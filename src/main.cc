#include "rockiva/rockiva_ba_api.h"
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
#include "im2d.h"

#define MODEL_INPUT_SIZE 640
#define YOLO_INPUT_SIZE (MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3)

#define CAM_FPS 30
pthread_mutex_t g_rtsp_mutex = PTHREAD_MUTEX_INITIALIZER;

static rtsp_demo_handle g_rtsplive = NULL;
static rtsp_session_handle g_rtsp_session_0, g_rtsp_session_1;
static int rociva_run_flag = 0;
static RockIvaHandle rkba_handle;
static RockIvaBaTaskParams initParams;
static RockIvaInitParam globalParams;

typedef struct _rkMpiCtx
{
    SAMPLE_VI_CTX_S vi[4]; // camera 0: 0,1; camera 1: 2,3
    SAMPLE_VENC_CTX_S venc[4];
} SAMPLE_MPI_CTX_S;

static bool quit = false;

static void sigterm_handler(int sig)
{
    fprintf(stderr, "signal %d\n", sig);
    quit = true;
}

static void *venc0_get_stream(void *pArgs)
{
    printf("#Start %s , arg:%p\n", __func__, pArgs);
    SAMPLE_VENC_CTX_S *ctx = (SAMPLE_VENC_CTX_S *)(pArgs);
    RK_S32 s32Ret = RK_FAILURE;
    char name[256] = {0};
    FILE *fp = RK_NULL;
    void *pData = RK_NULL;
    RK_S32 loopCount = 0;

    while (!quit)
    {
        s32Ret = SAMPLE_COMM_VENC_GetStream(ctx, &pData);
        if (s32Ret == RK_SUCCESS)
        {
            RK_LOGD("chn:%d, loopCount:%d wd:%d\n", ctx->s32ChnId, loopCount,
                    ctx->stFrame.pstPack->u32Len);
            // exit when complete
            if (ctx->s32loopCount > 0)
            {
                if (loopCount >= ctx->s32loopCount)
                {
                    SAMPLE_COMM_VENC_ReleaseStream(ctx);
                    quit = true;
                    break;
                }
            }

            PrintStreamDetails(ctx->s32ChnId, ctx->stFrame.pstPack->u32Len);
            pthread_mutex_lock(&g_rtsp_mutex);
            rtsp_tx_video(g_rtsp_session_0, (const uint8_t *)pData, ctx->stFrame.pstPack->u32Len,
                          ctx->stFrame.pstPack->u64PTS);
            rtsp_do_event(g_rtsplive);
            pthread_mutex_unlock(&g_rtsp_mutex);

            SAMPLE_COMM_VENC_ReleaseStream(ctx);
            loopCount++;
        }
        usleep(1000);
    }

    return RK_NULL;
}

static void *venc1_get_stream(void *pArgs)
{
    printf("#Start %s , arg:%p\n", __func__, pArgs);
    SAMPLE_VENC_CTX_S *ctx = (SAMPLE_VENC_CTX_S *)(pArgs);
    RK_S32 s32Ret = RK_FAILURE;
    char name[256] = {0};
    FILE *fp = RK_NULL;
    void *pData = RK_NULL;
    RK_S32 loopCount = 0;

    while (!quit)
    {
        s32Ret = SAMPLE_COMM_VENC_GetStream(ctx, &pData);
        if (s32Ret == RK_SUCCESS)
        {
            RK_LOGD("chn:%d, loopCount:%d wd:%d\n", ctx->s32ChnId, loopCount,
                    ctx->stFrame.pstPack->u32Len);
            // exit when complete
            if (ctx->s32loopCount > 0)
            {
                if (loopCount >= ctx->s32loopCount)
                {
                    SAMPLE_COMM_VENC_ReleaseStream(ctx);
                    quit = true;
                    break;
                }
            }

            PrintStreamDetails(ctx->s32ChnId, ctx->stFrame.pstPack->u32Len);
            pthread_mutex_lock(&g_rtsp_mutex);
            rtsp_tx_video(g_rtsp_session_1, (const uint8_t *)pData, ctx->stFrame.pstPack->u32Len,
                          ctx->stFrame.pstPack->u64PTS);
            rtsp_do_event(g_rtsplive);
            pthread_mutex_unlock(&g_rtsp_mutex);

            SAMPLE_COMM_VENC_ReleaseStream(ctx);
            loopCount++;
        }
        usleep(1000);
    }

    return RK_NULL;
}

void handle_pipe(int sig) { printf("%s sig = %d\n", __func__, sig); }

void init_rtsp(void)
{
    // init rtsp
    g_rtsplive = create_rtsp_demo(554);
    g_rtsp_session_0 = rtsp_new_session(g_rtsplive, "/live/0");
    g_rtsp_session_1 = rtsp_new_session(g_rtsplive, "/live/1");
    rtsp_set_video(g_rtsp_session_0, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
    rtsp_set_video(g_rtsp_session_1, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
    rtsp_sync_video_ts(g_rtsp_session_0, rtsp_get_reltime(), rtsp_get_ntptime());
    rtsp_sync_video_ts(g_rtsp_session_1, rtsp_get_reltime(), rtsp_get_ntptime());
}

void init_isp(void)
{
    RK_BOOL bMultictx = RK_FALSE;
    RK_CHAR *pCodecName = "H264";
    char *iq_file_dir = "/oem/usr/share/iqfiles";
    printf("#CodecName:%s\n", pCodecName);
    printf("#IQ Path: %s\n", iq_file_dir);
    printf("#Rkaiq XML DirPath: %s\n", iq_file_dir);
    printf("#bMultictx: %d\n\n", bMultictx);
    rk_aiq_working_mode_t hdr_mode_0 = RK_AIQ_WORKING_MODE_NORMAL;

    SAMPLE_COMM_ISP_Init(0, hdr_mode_0, bMultictx, iq_file_dir);
    SAMPLE_COMM_ISP_Run(0);
    SAMPLE_COMM_ISP_SetFrameRate(0, CAM_FPS);
}

static int nv12_to_rgb24_640x640(void *yuvBuffer, void *rgbBuffer)
{

    rga_buffer_t src, dst;
    memset(&src, 0, sizeof(rga_buffer_t));
    memset(&dst, 0, sizeof(rga_buffer_t));

    printf("yuvBuffer:%p, rgbBuffer:%p\n", yuvBuffer, rgbBuffer);

    src = wrapbuffer_physicaladdr(yuvBuffer, 640, 640, RK_FORMAT_YCbCr_420_SP);
    dst = wrapbuffer_physicaladdr(rgbBuffer, 640, 640, RK_FORMAT_RGB_888);

    src.format = RK_FORMAT_YCbCr_420_SP;
    dst.format = RK_FORMAT_RGB_888;

    // imcvtcolor(rga_buffer_t src, rga_buffer_t dst, int sfmt, int dfmt, int mode = IM_COLOR_SPACE_DEFAULT, int sync = 1, int *release_fence_fd = NULL);
    IM_STATUS status = imcvtcolor(src, dst, src.format, dst.format);
    // IM_STATUS status = imcvtcolor(src, dst, src.format, dst.format);
    if (status != IM_STATUS_SUCCESS)
    {
        printf("ERROR: imcvtcolor failed!\n");
        return -1;
    }

    return 0;
}

pthread_t get_vi_to_npu_thread;
static void *rkipc_get_vi_to_npu_tde(void *arg)
{
    TDE_HANDLE hHandle;
    TDE_SURFACE_S stSrc, stDst;
    TDE_RECT_S stSrcRect, stDstRect;
    RK_S32 s32Ret = RK_FAILURE;
    PIC_BUF_ATTR_S stTdeOutPicBufAttr;
    MB_PIC_CAL_S stMbPicCalResult;
    VIDEO_FRAME_INFO_S stViFrame, DstFrame;
    memset(&stTdeOutPicBufAttr, 0, sizeof(PIC_BUF_ATTR_S));
    memset(&stMbPicCalResult, 0, sizeof(MB_PIC_CAL_S));

    memset(&stViFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
    memset(&DstFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
    DstFrame.stVFrame.enPixelFormat = RK_FMT_YUV420SP;
    DstFrame.stVFrame.enCompressMode = COMPRESS_MODE_NONE;

    // stTdeOutPicBufAttr.u32Width = MODEL_INPUT_SIZE;
    // stTdeOutPicBufAttr.u32Height = MODEL_INPUT_SIZE;
    // stTdeOutPicBufAttr.enCompMode = COMPRESS_MODE_NONE;
    // stTdeOutPicBufAttr.enPixelFormat = RK_FMT_RGB888;
    // s32Ret = RK_MPI_CAL_TDE_GetPicBufferSize(&stTdeOutPicBufAttr, &stMbPicCalResult);
    // if (s32Ret != RK_SUCCESS)
    // {
    //     RK_LOGE("RK_MPI_CAL_TDE_GetPicBufferSize failure:%X", s32Ret);
    //     return RK_NULL;
    // }

    // printf("----------------------------1\n");
    // printf("stMbPicCalResult.u32MBSize:%d\n", stMbPicCalResult.u32MBSize);
    // 直接申请1128800内存
    s32Ret = RK_MPI_SYS_MmzAlloc(&DstFrame.stVFrame.pMbBlk, RK_NULL, RK_NULL, 1228800);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_SYS_MmzAlloc failure:%X", s32Ret);
        return RK_NULL;
    }
    printf("----------------------------2\n");

    stSrc.u32Width = MODEL_INPUT_SIZE;
    stSrc.u32Height = MODEL_INPUT_SIZE;
    stSrc.enColorFmt = RK_FMT_YUV420SP;
    stSrc.enComprocessMode = COMPRESS_MODE_NONE;
    stSrcRect.s32Xpos = 0;
    stSrcRect.s32Ypos = 0;
    stSrcRect.u32Width = MODEL_INPUT_SIZE;
    stSrcRect.u32Height = MODEL_INPUT_SIZE;

    stDst.u32Width = MODEL_INPUT_SIZE;
    stDst.u32Height = MODEL_INPUT_SIZE;
    stDst.enColorFmt = RK_FMT_RGB888;
    stDst.enComprocessMode = COMPRESS_MODE_NONE;
    stDstRect.s32Xpos = 0;
    stDstRect.s32Ypos = 0;
    stDstRect.u32Width = MODEL_INPUT_SIZE;
    stDstRect.u32Height = MODEL_INPUT_SIZE;
    printf("----------------------------3\n");
    /* tde open */
    s32Ret = RK_TDE_Open();
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_TDE_Open failure:%X", s32Ret);
        return NULL;
    }
    while (!quit)
    {
        // 从vi chn 1拿数据帧
        s32Ret = RK_MPI_VI_GetChnFrame(0, 1, &stViFrame, 1000);
        if (s32Ret == RK_SUCCESS)
        {
            hHandle = RK_TDE_BeginJob();
            if (RK_ERR_TDE_INVALID_HANDLE == hHandle)
            {
                RK_LOGE("RK_TDE_BeginJob Failure");
                RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
                return NULL;
            }

            stSrc.pMbBlk = stViFrame.stVFrame.pMbBlk;
            stDst.pMbBlk = DstFrame.stVFrame.pMbBlk;
            DstFrame.stVFrame.u32TimeRef = stViFrame.stVFrame.u32TimeRef;
            DstFrame.stVFrame.u64PTS = stViFrame.stVFrame.u64PTS;

            s32Ret = RK_TDE_QuickResize(hHandle, &stSrc, &stSrcRect, &stDst, &stDstRect);
            if (s32Ret != RK_SUCCESS)
            {
                RK_LOGE("RK_TDE_QuickCopy Failure %#X ", s32Ret);
                RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
                RK_TDE_CancelJob(hHandle);
                return NULL;
            }
            s32Ret = RK_TDE_EndJob(hHandle, RK_FALSE, RK_TRUE, -1);
            if (s32Ret != RK_SUCCESS)
            {
                RK_LOGE("RK_TDE_EndJob Failure %#X ", s32Ret);
                RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
                RK_TDE_CancelJob(hHandle);
                return NULL;
            }
            s32Ret = RK_TDE_WaitForDone(hHandle);
            if (s32Ret != RK_SUCCESS)
            {
                RK_LOGE("RK_TDE_WaitForDone Failure s32Ret: %#X", s32Ret);
                RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
                return NULL;
            }

            printf("ready to detect\n");

            // detect((unsigned char *)RK_MPI_MB_Handle2VirAddr(DstFrame.stVFrame.pMbBlk));

            s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
            if (s32Ret != RK_SUCCESS)
            {
                RK_LOGE("RK_MPI_VI_ReleaseChnFrame failure:%X pipe:%d chnid:%d", s32Ret, 0, 0);
            }
        }
    }
    return NULL;
}

static void *rkipc_get_vi_to_npu(void *arg)
{
    printf("#Start %s thread, arg:%p\n", __func__, arg);
    int s32Ret;
    int32_t loopCount = 0;
    VIDEO_FRAME_INFO_S stViFrame;
    int ret = 0;

    while (!quit)
    {
        // 从vi chn 1拿数据帧
        s32Ret = RK_MPI_VI_GetChnFrame(0, 1, &stViFrame, 1000);
        if (s32Ret == RK_SUCCESS)
        {

            void *data = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);
            // fd为dma buf的fd
            int32_t fd = RK_MPI_MB_Handle2Fd(stViFrame.stVFrame.pMbBlk);

            if (loopCount == 10)
            {
                printf(" creating data\n");
                // 写成一个文件
                FILE *fp = fopen("/userdata/vi_to_npu.yuv", "wb");
                if (fp)
                {
                    fwrite(data, 1, stViFrame.stVFrame.u32Width * stViFrame.stVFrame.u32Height * 3 / 2, fp);
                    fclose(fp);
                }
            }

            void *pRknnInputData = malloc(YOLO_INPUT_SIZE);
            ret = nv12_to_rgb24_640x640(data, pRknnInputData);
            if (ret < 0)
            {
                printf("nv12_to_rgb24_640x640 failed\n");
            }
            // printf("nv12_to_rgb24_640x640 success\n");
            free(pRknnInputData);

            // unsigned char *dataPtr = (unsigned char *)malloc(stViFrame.stVFrame.u32Width * stViFrame.stVFrame.u32Height * 3 / 2);
            // memcpy(dataPtr, data, stViFrame.stVFrame.u32Width * stViFrame.stVFrame.u32Height * 3 / 2);
            // detect((unsigned char *)pRknnInputData);

            s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 1, &stViFrame);
            if (s32Ret != RK_SUCCESS)
                printf("RK_MPI_VI_ReleaseChnFrame fail %x\n", s32Ret);
            loopCount++;
        }
        else
        {
            // printf("RK_MPI_VI_GetChnFrame timeout %x\n", s32Ret);
        }
    }
    return NULL;
}

// 创建vi通道
void create_vi_chn(int camWidth, int camHeight, AVS_CHN u32ChnId, RK_U32 u32BufCount, RK_U32 depth)
{
    SAMPLE_VI_CTX_S viCtx;
    memset(&viCtx, 0, sizeof(SAMPLE_VI_CTX_S));

    viCtx.u32Width = camWidth;
    viCtx.u32Height = camHeight;
    viCtx.s32DevId = 0;
    viCtx.u32PipeId = 0;
    viCtx.s32ChnId = u32ChnId;
    viCtx.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
    viCtx.stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
    viCtx.stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
    viCtx.stChnAttr.stFrameRate.s32SrcFrameRate = -1;
    viCtx.stChnAttr.stFrameRate.s32DstFrameRate = -1;
    viCtx.stChnAttr.stIspOpt.u32BufCount = u32BufCount;
    viCtx.stChnAttr.u32Depth = depth;

    SAMPLE_COMM_VI_CreateChn(&viCtx);
    if (u32ChnId == 1)
    {
        pthread_create(&get_vi_to_npu_thread, NULL, rkipc_get_vi_to_npu, NULL);
    }
}

void init_venc(int vencWidth, int vencHeight, VENC_CHN vencChnId, Thread_Func func)
{
    RK_S32 s32loopCnt = -1;
    CODEC_TYPE_E enCodecType = RK_CODEC_TYPE_H264;
    VENC_RC_MODE_E enRcMode = VENC_RC_MODE_H264CBR;
    RK_S32 s32BitRate = 4 * 1024;
    SAMPLE_VENC_CTX_S venc;
    RK_CHAR *pOutPathVenc = "/userdata";
    memset(&venc, 0, sizeof(SAMPLE_VENC_CTX_S));

    venc.s32ChnId = vencChnId;
    venc.u32Width = vencWidth;
    venc.u32Height = vencHeight;
    int u32BufSize = vencHeight * vencWidth / 4;
    venc.stChnAttr.stVencAttr.u32BufSize = u32BufSize;
    venc.u32Fps = CAM_FPS;

    venc.u32Gop = 50;
    venc.u32BitRate = s32BitRate;
    venc.enCodecType = enCodecType;
    venc.enRcMode = enRcMode;
    venc.getStreamCbFunc = func;

    venc.s32loopCount = s32loopCnt;

    venc.dstFilePath = pOutPathVenc;
    // H264  66：Baseline  77：Main Profile 100：High Profile
    // H265  0：Main Profile  1：Main 10 Profile
    // MJPEG 0：Baseline
    venc.stChnAttr.stVencAttr.u32Profile = 66;
    venc.stChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    venc.enable_buf_share = 1;

    SAMPLE_COMM_VENC_CreateChn(&venc);
}

int main(int argc, char *argv[])
{
    // 读取一个yuv文件
}

int main_bak(int argc, char *argv[])
{
    RK_S32 s32Ret = RK_FAILURE;
    SAMPLE_MPI_CTX_S *ctx;

    int cam_0_enable_hdr = 0;
    int cam_0_video_0_width = 1920;
    int cam_0_video_0_height = 1080;

    int cam_0_video_1_width = 640;
    int cam_0_video_1_height = 640;

    int enable_npu = 1;
    int enable_buf_share = 1;
    CODEC_TYPE_E enCodecType = RK_CODEC_TYPE_H264;
    VENC_RC_MODE_E enRcMode = VENC_RC_MODE_H264CBR;

    // RK_S32 s32CamId = -1;
    // MPP_CHN_S vi_chn[6], venc_chn[4];
    MPP_CHN_S vi_chn[2];
    MPP_CHN_S venc_chn[2];

    memset(vi_chn, 0, sizeof(vi_chn));
    memset(venc_chn, 0, sizeof(venc_chn));

    RK_CHAR *pOutPathVenc = "/userdata";
    RK_S32 s32CamNum = 1;
    RK_S32 s32loopCnt = -1;
    RK_S32 s32BitRate = 4 * 1024;
    RK_S32 i;

    RK_U32 u32BufSize = 0;

    struct sigaction action;
    action.sa_handler = handle_pipe;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGPIPE, &action, NULL);

    ctx = (SAMPLE_MPI_CTX_S *)(malloc(sizeof(SAMPLE_MPI_CTX_S)));
    memset(ctx, 0, sizeof(SAMPLE_MPI_CTX_S));

    signal(SIGINT, sigterm_handler);

    printf("model path :%s\n", argv[1]);
    init_model(argv[1]);

    int c;
    printf("sensor 0: main:%d*%d, sub:%d*%d\n", cam_0_video_0_width, cam_0_video_0_height, cam_0_video_1_width, cam_0_video_1_height);

    init_isp();

    init_rtsp();

    if (RK_MPI_SYS_Init() != RK_SUCCESS)
    {
        goto __FAILED;
    }

    // 通道id0, buffer数量2, depth 0
    // create_vi_chn(cam_0_video_0_width, cam_0_video_0_height, 0, 2, 0);
    create_vi_chn(cam_0_video_1_width, cam_0_video_1_height, 1, 3, 1);

    // init_venc(cam_0_video_0_width, cam_0_video_0_height, 0, venc0_get_stream);
    init_venc(cam_0_video_1_width, cam_0_video_1_height, 1, venc1_get_stream);

    vi_chn[1].enModId = RK_ID_VI;
    vi_chn[1].s32DevId = 0;
    vi_chn[1].s32ChnId = 1;
    venc_chn[1].enModId = RK_ID_VENC;
    venc_chn[1].s32DevId = 0;
    venc_chn[1].s32ChnId = 1;

    SAMPLE_COMM_Bind(&vi_chn[1], &venc_chn[1]);

    // for (i = 0; i < s32CamNum; i++)
    // {
    //     printf("bind vi_chn[%d] to venc_chn[%d]\n", i, i);
    //     vi_chn[i].enModId = RK_ID_VI;
    //     vi_chn[i].s32DevId = 0;
    //     vi_chn[i].s32ChnId = i;
    //     venc_chn[i].enModId = RK_ID_VENC;
    //     venc_chn[i].s32DevId = 0;
    //     venc_chn[i].s32ChnId = i;

    //     SAMPLE_COMM_Bind(&vi_chn[i], &venc_chn[i]);
    // }

    getchar();

#if 0
    for (i = 0; i < s32CamNum; i++)
    {
        if (i == 0)
        {
            ctx->vi[i].u32Width = cam_0_video_0_width;
            ctx->vi[i].u32Height = cam_0_video_0_height;
        }
        if (i == 1)
        {
            ctx->vi[i].u32Width = cam_0_video_1_width;
            ctx->vi[i].u32Height = cam_0_video_1_height;
        }

        ctx->vi[i].s32DevId = 0;
        ctx->vi[i].u32PipeId = 0;
        ctx->vi[i].s32ChnId = i;

        ctx->vi[i].stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
        ctx->vi[i].stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
        ctx->vi[i].stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
        ctx->vi[i].stChnAttr.stFrameRate.s32SrcFrameRate = -1;
        ctx->vi[i].stChnAttr.stFrameRate.s32DstFrameRate = -1;

        if (i == 1)
        {
            // NPU only 10 fps,  need anothor buffer
            ctx->vi[i].stChnAttr.stIspOpt.u32BufCount = 3;
            ctx->vi[i].stChnAttr.u32Depth = 1;
            pthread_create(&get_vi_to_npu_thread, NULL, rkipc_get_vi_to_npu, NULL);
        }
        else
        {
            ctx->vi[i].stChnAttr.stIspOpt.u32BufCount = 2;
            ctx->vi[i].stChnAttr.u32Depth = 0;
        }

        SAMPLE_COMM_VI_CreateChn(&ctx->vi[i]);

        // 这里留下记录方便解绑
        vi_chn[i].enModId = RK_ID_VI;
        vi_chn[i].s32DevId = ctx->vi[i].s32DevId;
        vi_chn[i].s32ChnId = ctx->vi[i].s32ChnId;

        // 分别用i给venc的chnId赋值
        ctx->venc[i].s32ChnId = i;

        if (i == 0)
        {
            ctx->venc[i].u32Width = cam_0_video_0_width;
            ctx->venc[i].u32Height = cam_0_video_0_height;
            u32BufSize = cam_0_video_0_width * cam_0_video_0_height / 4;
            ctx->venc[i].u32Fps = CAM_FPS;
        }
        if (i == 1)
        {
            ctx->venc[i].u32Width = cam_0_video_1_width;
            ctx->venc[i].u32Height = cam_0_video_1_height;
            u32BufSize = cam_0_video_1_width * cam_0_video_1_height / 4;
            ctx->venc[i].u32Fps = CAM_FPS;
        }

        ctx->venc[i].stChnAttr.stVencAttr.u32BufSize = u32BufSize;
        // printf("u32BufSize :%d\n", u32BufSize);
        printf("venc[%d]:u32BufSize:%ld\n", i, ctx->venc[i].stChnAttr.stVencAttr.u32BufSize);

        ctx->venc[i].u32Gop = 30;
        ctx->venc[i].u32BitRate = s32BitRate;
        ctx->venc[i].enCodecType = enCodecType;
        ctx->venc[i].enRcMode = enRcMode;
        if (i == 0)
            ctx->venc[i].getStreamCbFunc = venc0_get_stream;
        if (i == 1)
            ctx->venc[i].getStreamCbFunc = venc1_get_stream;

        ctx->venc[i].s32loopCount = s32loopCnt;
        ctx->venc[i].dstFilePath = pOutPathVenc;
        // H264  66：Baseline  77：Main Profile 100：High Profile
        // H265  0：Main Profile  1：Main 10 Profile
        // MJPEG 0：Baseline
        ctx->venc[i].stChnAttr.stVencAttr.u32Profile = 66;
        ctx->venc[i].stChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;
        ctx->venc[i].enable_buf_share = enable_buf_share;

        SAMPLE_COMM_VENC_CreateChn(&ctx->venc[i]);
        // printf("venc[%d]:u32BufSize:%ld\n", i, ctx->venc[i].stChnAttr.stVencAttr.u32BufSize);

        venc_chn[i].enModId = RK_ID_VENC;
        venc_chn[i].s32DevId = 0;
        venc_chn[i].s32ChnId = ctx->venc[i].s32ChnId;
    }

    // 绑定
    SAMPLE_COMM_Bind(&vi_chn[0], &venc_chn[0]);
    SAMPLE_COMM_Bind(&vi_chn[1], &venc_chn[1]);
    printf("%s initial finish\n", __func__);

    getchar();
#endif

    quit = false;

    printf("%s exit!\n", __func__);

    rtsp_del_demo(g_rtsplive);

    SAMPLE_COMM_UnBind(&vi_chn[0], &venc_chn[0]);
    SAMPLE_COMM_UnBind(&vi_chn[1], &venc_chn[1]);

    for (i = 0; i < s32CamNum; i++)
    {
        // SAMPLE_COMM_VENC_DestroyChn(&venc_chn[i]);
        // SAMPLE_COMM_VI_DestroyChn(&vi_chn[i]);
    }

    /////////////////////// clean process ////////////////////
    if (RK_MPI_SYS_Init() != RK_SUCCESS)
    {
        goto __FAILED;
    }

__FAILED:
    RK_MPI_SYS_Exit();
    SAMPLE_COMM_ISP_Stop(0);
__FAILED2:
    if (ctx)
    {
        free(ctx);
        ctx = RK_NULL;
    }

    return 0;
}