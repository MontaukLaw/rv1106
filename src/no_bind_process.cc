#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>

#include "rtsp_demo.h"
#include "sample_comm.h"
#include "rknn_detect.h"
#include "utils.h"

#define VI_CHN_0 0
#define VENC_CHN_0 0
#define VI_CHN_1 1
#define VPSS_CHN_0 0
#define VPSS_GRP_0 0
#define SEND_FRAME_TIMEOUT 2000
#define GET_FRAME_TIMEOUT 2000
#define MODLE_WIDTH 640

#define RTSP_INPUT_VI_WIDTH 1920
#define RTSP_INPUT_VI_HEIGHT 1080

#define X_START ((RTSP_INPUT_VI_WIDTH - MODLE_WIDTH) / 2)
#define Y_START ((RTSP_INPUT_VI_HEIGHT - MODLE_WIDTH) / 2)

typedef struct _rkMpiCtx
{
    SAMPLE_VI_CTX_S vi;
    SAMPLE_VO_CTX_S vo;
    SAMPLE_VPSS_CTX_S vpss;
    SAMPLE_VENC_CTX_S venc;
    SAMPLE_RGN_CTX_S rgn[2];
} SAMPLE_MPI_CTX_S;

static rknn_list_t *rknn_list_;

static bool quit = false;

rtsp_demo_handle g_rtsplive = NULL;
static rtsp_session_handle g_rtsp_session;

static void sigterm_handler(int sig)
{
    fprintf(stderr, "signal %d\n", sig);
    quit = true;
}

typedef struct g_box_info_t
{
    int x;
    int y;
    int w;
    int h;
} box_info_t;

box_info_t boxInfoList[10];
int boxInfoListNumber = 0;

// 直接在nv12的内存上画框
static int nv12_border(char *pic, int pic_w, int pic_h, int rect_x, int rect_y, int rect_w, int rect_h, int R, int G, int B)
{
    /* Set up the rectangle border size */
    const int border = 5;

    /* RGB convert YUV */
    int Y, U, V;
    Y = 0.299 * R + 0.587 * G + 0.114 * B;
    U = -0.1687 * R + 0.3313 * G + 0.5 * B + 128;
    V = 0.5 * R - 0.4187 * G - 0.0813 * B + 128;
    /* Locking the scope of rectangle border range */
    int j, k;
    for (j = rect_y; j < rect_y + rect_h; j++)
    {
        for (k = rect_x; k < rect_x + rect_w; k++)
        {
            if (k < (rect_x + border) || k > (rect_x + rect_w - border) ||
                j < (rect_y + border) || j > (rect_y + rect_h - border))
            {
                /* Components of YUV's storage address index */
                int y_index = j * pic_w + k;
                int u_index =
                    (y_index / 2 - pic_w / 2 * ((j + 1) / 2)) * 2 + pic_w * pic_h;
                int v_index = u_index + 1;
                /* set up YUV's conponents value of rectangle border */
                pic[y_index] = Y;
                pic[u_index] = U;
                pic[v_index] = V;
            }
        }
    }

    return 0;
}

void count_box_info(char *data)
{
    if (rknn_list_size(rknn_list_))
    {

        long time_before;
        detect_result_group_t detect_result_group;
        memset(&detect_result_group, 0, sizeof(detect_result_group));

        // pick up the first one
        rknn_list_pop(rknn_list_, &time_before, &detect_result_group);
        // printf("result count:%d \n", detect_result_group.count);

        for (int j = 0; j < detect_result_group.count; j++)
        {
            // int x = detect_result_group.results[j].box.left + X_START;
            // int y = detect_result_group.results[j].box.top + Y_START;
            // int w = (detect_result_group.results[j].box.right - detect_result_group.results[j].box.left);
            // int h = (detect_result_group.results[j].box.bottom - detect_result_group.results[j].box.top);
            int x = detect_result_group.results[j].box.left * RTSP_INPUT_VI_WIDTH / MODLE_WIDTH;
            int y = detect_result_group.results[j].box.top * RTSP_INPUT_VI_HEIGHT / MODLE_WIDTH;
            int w = (detect_result_group.results[j].box.right * RTSP_INPUT_VI_WIDTH / MODLE_WIDTH) - x;
            int h = (detect_result_group.results[j].box.bottom * RTSP_INPUT_VI_HEIGHT / MODLE_WIDTH) - y;
            while ((uint32_t)(x + w) >= RTSP_INPUT_VI_WIDTH)
            {
                w -= 16;
            }
            while ((uint32_t)(y + h) >= RTSP_INPUT_VI_HEIGHT)
            {
                h -= 16;
            }
            printf("border=(%d %d %d %d)\n", x, y, w, h);
            boxInfoList[j] = {x, y, w, h};
            boxInfoListNumber++;

            nv12_border(data, RTSP_INPUT_VI_WIDTH, RTSP_INPUT_VI_HEIGHT,
                        boxInfoList[j].x, boxInfoList[j].y, boxInfoList[j].w, boxInfoList[j].h, 0, 0, 255);
        }
    }
}

static void *get_vi_stream(void *arg)
{
    printf("#Start %s thread, arg:%p\n", __func__, arg);
    int s32Ret;
    int32_t loopCount = 0;
    VIDEO_FRAME_INFO_S stViFrame;

    int ret = 0;

    while (!quit)
    {
        // 从vi chn 0拿数据帧
        s32Ret = RK_MPI_VI_GetChnFrame(0, VI_CHN_0, &stViFrame, GET_FRAME_TIMEOUT);

        if (s32Ret == RK_SUCCESS)
        {

            void *data = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);
            // fd为dma buf的fd
            int32_t fd = RK_MPI_MB_Handle2Fd(stViFrame.stVFrame.pMbBlk);

            if (loopCount == 10)
            {
                printf("creating data\n");
                // 写成一个文件
                FILE *fp = fopen("/userdata/vi_to_npu.yuv", "wb");
                if (fp)
                {
                    // fwrite(data, 1, stViFrame.stVFrame.u32Width * stViFrame.stVFrame.u32Height * 3 / 2, fp);
                    fclose(fp);
                }
            }

            if (loopCount % 10 == 0)
            {
                printf("loopCount:%d, fd:%d, data:%p\n", loopCount, fd, data);
            }

            count_box_info((char *)data);

            /* send frame to venc */
            s32Ret = RK_MPI_VENC_SendFrame(VENC_CHN_0, &stViFrame, SEND_FRAME_TIMEOUT);
            if (s32Ret != RK_SUCCESS)
            {
                printf("RK_MPI_VENC_SendFrame timeout:%#X vi index:%d", s32Ret, VI_CHN_0);
            }

            s32Ret = RK_MPI_VI_ReleaseChnFrame(0, VI_CHN_0, &stViFrame);
            if (s32Ret != RK_SUCCESS)
            {
                printf("RK_MPI_VI_ReleaseChnFrame fail %x\n", s32Ret);
            }
            loopCount++;
        }
    }
    return NULL;
}

static RK_VOID *venc_get_stream(RK_VOID *pArgs)
{
    SAMPLE_VENC_CTX_S *ctx = (SAMPLE_VENC_CTX_S *)pArgs;
    RK_S32 s32Ret = RK_FAILURE;
    RK_S32 s32Fd = 0;
    RK_S32 s32LoopCount = 0;
    void *pData = RK_NULL;
    FILE *fp = RK_NULL;

    while (!quit)
    {
        s32Ret = SAMPLE_COMM_VENC_GetStream(ctx, &pData);
        if (s32Ret == RK_SUCCESS)
        {
            rtsp_tx_video(g_rtsp_session, (uint8_t *)pData, ctx->stFrame.pstPack->u32Len, ctx->stFrame.pstPack->u64PTS);
            rtsp_do_event(g_rtsplive);
            SAMPLE_COMM_VENC_ReleaseStream(ctx);
            s32LoopCount++;
        }
    }

    RK_LOGD("chnId:%d venc_get_stream exit!!!", ctx->s32ChnId);
    return RK_NULL;
}

static void *vpss_get_starem(void *arg)
{
    RK_S32 s32Ret = RK_SUCCESS;
    VIDEO_FRAME_INFO_S pstVideoFrame;
    memset(&pstVideoFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
    void *pData = RK_NULL;
    RK_U32 counter = 0;

    while (!quit)
    {
        s32Ret = RK_MPI_VPSS_GetChnFrame(VPSS_GRP_0, VPSS_CHN_0, &pstVideoFrame, 200);
        if (s32Ret != RK_SUCCESS)
        {
            usleep(1000);
            continue;
        }

        RK_MPI_SYS_MmzFlushCache(pstVideoFrame.stVFrame.pMbBlk, RK_TRUE);
        pData = RK_MPI_MB_Handle2VirAddr(pstVideoFrame.stVFrame.pMbBlk);

        if (counter % 10 == 0)
        {
            printf("VPSS running: %d\n", counter);
        }
        detect_result_group_t detect_result_group;
        memset(&detect_result_group, 0, sizeof(detect_result_group_t));
        int64_t start_us = getCurrentTimeUs();
        int64_t elapse_us = 0;
        rknn_detect((unsigned char *)pData, &detect_result_group);
        elapse_us = getCurrentTimeUs() - start_us;
        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>detect spend = %.2fms, FPS = %.2f\n", elapse_us / 1000.f, 1000.f * 1000.f / elapse_us);

        // put detect result to list
        if (detect_result_group.count > 0)
        {

            rknn_list_push(rknn_list_, get_current_time_ms(), detect_result_group);
            int size = rknn_list_size(rknn_list_);
            if (size >= MAX_RKNN_LIST_NUM)
            {
                rknn_list_drop(rknn_list_);
            }
            printf("size is %d\n", size);
        }
        // if (counter == 10)
        // {
        //     printf("creating data\n");
        //     // 写成一个文件
        //     FILE *fp = fopen("/userdata/640.rgb", "wb");
        //     if (fp)
        //     {
        //         fwrite(pData, 1, MODLE_WIDTH * MODLE_WIDTH * 3, fp);
        //         fclose(fp);
        //     }
        // }

        counter++;
        s32Ret = RK_MPI_VPSS_ReleaseChnFrame(VPSS_GRP_0, VPSS_CHN_0, &(pstVideoFrame));
        if (s32Ret != RK_SUCCESS)
        {
            printf("%d RK_MPI_VPSS_ReleaseChnFrame failed with 0x%x", s32Ret);
            continue;
        }
    }
}

int create_vi_vpss(void)
{
    // RK_S32 chnIndex = VPSS_CHN_0;
    MPP_CHN_S stViChn, stVpssChn;
    RK_S32 s32Ret = RK_SUCCESS;
    VIDEO_PROC_DEV_TYPE_E enTmpVProcDevType;
    VPSS_GRP_ATTR_S stGrpVpssAttr;

    stGrpVpssAttr.u32MaxW = 4096;
    stGrpVpssAttr.u32MaxH = 4096;
    stGrpVpssAttr.stFrameRate.s32SrcFrameRate = -1;
    stGrpVpssAttr.stFrameRate.s32DstFrameRate = -1;
    stGrpVpssAttr.enCompressMode = COMPRESS_MODE_NONE;
    stGrpVpssAttr.enPixelFormat = RK_FMT_YUV420SP;

    s32Ret = RK_MPI_VPSS_CreateGrp(VPSS_GRP_0, &stGrpVpssAttr);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_VPSS_CreateGrp failed with %#x!\n", s32Ret);
        return s32Ret;
    }
    SAMPLE_VPSS_CTX_S vpssCtx;
    memset(&vpssCtx, 0, sizeof(SAMPLE_VPSS_CTX_S));

    // ctx->vpss.stGrpVpssAttr.enPixelFormat = RK_FMT_YUV420SP;
    // ctx->vpss.stGrpVpssAttr.enCompressMode = COMPRESS_MODE_NONE; // no compress

    vpssCtx.stVpssChnAttr[0].enChnMode = VPSS_CHN_MODE_USER;
    vpssCtx.stVpssChnAttr[0].enCompressMode = COMPRESS_MODE_NONE;
    vpssCtx.stVpssChnAttr[0].enDynamicRange = DYNAMIC_RANGE_SDR8;
    vpssCtx.stVpssChnAttr[0].enPixelFormat = RK_FMT_RGB888;
    vpssCtx.stVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    vpssCtx.stVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    vpssCtx.stVpssChnAttr[0].u32Width = 640;
    vpssCtx.stVpssChnAttr[0].u32Height = 640;

    // 设置vpss处理方式
    s32Ret = RK_MPI_VPSS_SetVProcDev(VPSS_GRP_0, VIDEO_PROC_DEV_RGA);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_VPSS_SetVProcDev(grp:%d) failed with %#x!", VPSS_GRP_0, s32Ret);
        return s32Ret;
    }

    s32Ret = RK_MPI_VPSS_GetVProcDev(VPSS_GRP_0, &enTmpVProcDevType);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_VPSS_GetVProcDev(grp:%d) failed with %#x!", VPSS_GRP_0, s32Ret);
        return s32Ret;
    }
    RK_LOGI("vpss Grp %d's work unit is %d", VPSS_GRP_0, enTmpVProcDevType);

    s32Ret = RK_MPI_VPSS_ResetGrp(VPSS_GRP_0);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_VPSS_ResetGrp failed with %#x!\n", s32Ret);
        return s32Ret;
    }
    s32Ret = RK_MPI_VPSS_SetChnAttr(VPSS_GRP_0, VPSS_CHN_0, &vpssCtx.stVpssChnAttr[VPSS_CHN_0]);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_VPSS_SetChnAttr failed with %#x!\n", s32Ret);
        return s32Ret;
    }
    s32Ret = RK_MPI_VPSS_GetChnAttr(VPSS_GRP_0, VPSS_CHN_0, &vpssCtx.stVpssChnAttr[VPSS_CHN_0]);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_VPSS_GetChnAttr failed with %#x!\n", s32Ret);
        return s32Ret;
    }
    s32Ret = RK_MPI_VPSS_EnableChn(VPSS_GRP_0, VPSS_CHN_0);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_VPSS_EnableChn failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = RK_MPI_VPSS_StartGrp(VPSS_GRP_0);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_VPSS_StartGrp failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    // return RK_SUCCESS;

    // bind vi to vpss
    stViChn.enModId = RK_ID_VI;
    stViChn.s32DevId = 0;
    stViChn.s32ChnId = VI_CHN_1;
    stVpssChn.enModId = RK_ID_VPSS;
    stVpssChn.s32DevId = 0;
    stVpssChn.s32ChnId = VPSS_CHN_0;

    RK_LOGD("vi to vpss ch %d vpss group %d", stVpssChn.s32ChnId, stVpssChn.s32DevId);
    s32Ret = RK_MPI_SYS_Bind(&stViChn, &stVpssChn);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("vi and vpss bind error 0x%x", s32Ret);
    }

    return s32Ret;
}

void unbind_vi_vpss(void)
{
    RK_S32 s32Ret = RK_SUCCESS;

    MPP_CHN_S stViChn, stVpssChn;

    // bind vi to vpss
    stViChn.enModId = RK_ID_VI;
    stViChn.s32DevId = 0;
    stViChn.s32ChnId = VI_CHN_1;
    stVpssChn.enModId = RK_ID_VPSS;
    stVpssChn.s32DevId = 0;
    stVpssChn.s32ChnId = VPSS_CHN_0;

    s32Ret = RK_MPI_SYS_UnBind(&stViChn, &stVpssChn);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("vi and vpss unbind error 0x%x", s32Ret);
    }
}

int main(int argc, char *argv[])
{
    int s32Ret = RK_SUCCESS;
    SAMPLE_VI_CTX_S viCtx[2];
    SAMPLE_VENC_CTX_S vencCtx;

    int video_width = RTSP_INPUT_VI_WIDTH;
    int video_height = RTSP_INPUT_VI_HEIGHT;

    int venc_width = video_width;
    int venc_height = video_height;

    // int disp_width = 1920;
    // int disp_height = 1080;

    memset(viCtx, 0, sizeof(SAMPLE_VI_CTX_S) * 2);
    memset(&vencCtx, 0, sizeof(SAMPLE_VENC_CTX_S));

    signal(SIGINT, sigterm_handler);
    RK_BOOL bMultictx = RK_FALSE;
    SAMPLE_COMM_ISP_Init(0, RK_AIQ_WORKING_MODE_NORMAL, RK_FALSE, "/etc/iqfiles/");
    SAMPLE_COMM_ISP_Run(0);

    // 创建rknn推理结果的列表
    create_rknn_list(&rknn_list_);

    // init rtsp
    g_rtsplive = create_rtsp_demo(554);
    g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/0");
    rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
    rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());

    if (RK_MPI_SYS_Init() != RK_SUCCESS)
    {
        goto __FAILED;
    }

    if (argc < 2)
    {
        printf("Usage: %s model_path\n", argv[0]);
        return -1;
    }

    printf("model path :%s\n", argv[1]);
    init_model(argv[1]);

    viCtx[0].u32Width = video_width;
    viCtx[0].u32Height = video_height;
    viCtx[0].s32DevId = 0;
    viCtx[0].u32PipeId = viCtx[0].s32DevId;
    viCtx[0].s32ChnId = VI_CHN_0;
    viCtx[0].stChnAttr.stIspOpt.u32BufCount = 3;
    viCtx[0].stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
    viCtx[0].stChnAttr.u32Depth = 1;
    viCtx[0].stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
    viCtx[0].stChnAttr.stFrameRate.s32SrcFrameRate = -1;
    viCtx[0].stChnAttr.stFrameRate.s32DstFrameRate = -1;
    s32Ret = SAMPLE_COMM_VI_CreateChn(&viCtx[0]);
    if (s32Ret != RK_SUCCESS)
    {
        printf("SAMPLE_COMM_VI_CreateChn:$#X ", s32Ret);
        goto __FINISHED;
    }

    viCtx[1].u32Width = video_width;
    viCtx[1].u32Height = video_height;
    viCtx[1].s32DevId = 0;
    viCtx[1].u32PipeId = viCtx[0].s32DevId;
    viCtx[1].s32ChnId = VI_CHN_1;
    viCtx[1].stChnAttr.stIspOpt.u32BufCount = 3;
    viCtx[1].stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
    viCtx[1].stChnAttr.u32Depth = 1;
    viCtx[1].stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
    viCtx[1].stChnAttr.stFrameRate.s32SrcFrameRate = -1;
    viCtx[1].stChnAttr.stFrameRate.s32DstFrameRate = -1;
    s32Ret = SAMPLE_COMM_VI_CreateChn(&viCtx[1]);
    if (s32Ret != RK_SUCCESS)
    {
        printf("SAMPLE_COMM_VI_CreateChn:$#X ", s32Ret);
        goto __FINISHED;
    }

    s32Ret = RK_MPI_VI_StartPipe(0);
    if (s32Ret != RK_SUCCESS)
    {
        printf("RK_MPI_VI_StartPipe failure:$#X pipe:%d", s32Ret, 0);
        goto __FINISHED;
    }

    /* init venc */
    vencCtx.s32ChnId = VENC_CHN_0;
    vencCtx.u32Width = video_width;
    vencCtx.u32Height = video_height;
    vencCtx.u32Gop = 50;
    vencCtx.u32BitRate = 4 * 1024;

    vencCtx.enCodecType = RK_CODEC_TYPE_H264;
    vencCtx.enRcMode = VENC_RC_MODE_H264CBR;
    vencCtx.enable_buf_share = 1;
    vencCtx.getStreamCbFunc = venc_get_stream;
    vencCtx.dstFilePath = "/data/";
    /*
    H264  66：Baseline  77：Main Profile 100：High Profile
    H265  0：Main Profile  1：Main 10 Profile
    MJPEG 0：Baseline
    */
    vencCtx.stChnAttr.stVencAttr.u32Profile = 100;
    /* VENC_GOPMODE_SMARTP */
    vencCtx.stChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    s32Ret = SAMPLE_COMM_VENC_CreateChn(&vencCtx);

    // vi 取流的线程
    pthread_t get_vi_stream_thread;
    pthread_create(&get_vi_stream_thread, NULL, get_vi_stream, NULL);

    s32Ret = create_vi_vpss();
    if (s32Ret != RK_SUCCESS)
    {
        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> create_vi_vpss failed\n");
        goto __FINISHED;
    }

    // vpss 取流线程
    pthread_t vpss_get_starem_thread;
    pthread_create(&vpss_get_starem_thread, NULL, vpss_get_starem, NULL);

    getchar();

__FINISHED:
    quit = true;

    pthread_join(vpss_get_starem_thread, RK_NULL);

    pthread_join(get_vi_stream_thread, RK_NULL);

    pthread_join(vencCtx.getStreamThread, RK_NULL);

    // 解绑
    unbind_vi_vpss();

    SAMPLE_COMM_VENC_DestroyChn(&vencCtx);
    SAMPLE_COMM_VI_DestroyChn(&viCtx[0]);

    rtsp_del_demo(g_rtsplive);
__FAILED:

    RK_MPI_SYS_Exit();
    SAMPLE_COMM_ISP_Stop(0);

    destory_rknn_list(&rknn_list_);

    return 0;
}