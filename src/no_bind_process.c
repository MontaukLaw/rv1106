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

#define VI_CHN_0 0
#define VENC_CHN_0 0
#define SEND_FRAME_TIMEOUT 2000
#define GET_FRAME_TIMEOUT 2000

typedef struct _rkMpiCtx
{
    SAMPLE_VI_CTX_S vi;
    SAMPLE_VO_CTX_S vo;
    SAMPLE_VPSS_CTX_S vpss;
    SAMPLE_VENC_CTX_S venc;
    SAMPLE_RGN_CTX_S rgn[2];
} SAMPLE_MPI_CTX_S;

static bool quit = false;

rtsp_demo_handle g_rtsplive = NULL;
static rtsp_session_handle g_rtsp_session;

static void sigterm_handler(int sig)
{
    fprintf(stderr, "signal %d\n", sig);
    quit = true;
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
                    fwrite(data, 1, stViFrame.stVFrame.u32Width * stViFrame.stVFrame.u32Height * 3 / 2, fp);
                    fclose(fp);
                }
            }

            if (loopCount % 10 == 0)
            {
                printf("loopCount:%d, fd:%d, data:%p\n", loopCount, fd, data);
            }

            /* send frame to venc */
            s32Ret = RK_MPI_VENC_SendFrame(VENC_CHN_0, &stViFrame, SEND_FRAME_TIMEOUT);
            if (s32Ret != RK_SUCCESS)
            {
                printf("RK_MPI_VENC_SendFrame timeout:%#X vi index:%d", s32Ret, VI_CHN_0);
            }

            s32Ret = RK_MPI_VI_ReleaseChnFrame(0, VI_CHN_0, &stViFrame);
            if (s32Ret != RK_SUCCESS)
                printf("RK_MPI_VI_ReleaseChnFrame fail %x\n", s32Ret);
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
    RK_U8 *pData = RK_NULL;
    FILE *fp = RK_NULL;

    while (!quit)
    {
        s32Ret = SAMPLE_COMM_VENC_GetStream(ctx, &pData);
        if (s32Ret == RK_SUCCESS)
        {
            rtsp_tx_video(g_rtsp_session, pData, ctx->stFrame.pstPack->u32Len, ctx->stFrame.pstPack->u64PTS);
            rtsp_do_event(g_rtsplive);
            SAMPLE_COMM_VENC_ReleaseStream(ctx);
            s32LoopCount++;
        }
    }

    RK_LOGD("chnId:%d venc_get_stream exit!!!", ctx->s32ChnId);
    return RK_NULL;
}

int main(int argc, char *argv[])
{
    int s32Ret = 0;
    SAMPLE_VI_CTX_S viCtx[2];
    SAMPLE_VENC_CTX_S vencCtx;
    int video_width = 1920;
    int video_height = 1080;
    int venc_width = 1920;
    int venc_height = 1080;
    int disp_width = 1920;
    int disp_height = 1080;

    memset(viCtx, 0, sizeof(SAMPLE_VI_CTX_S) * 2);
    memset(&vencCtx, 0, sizeof(SAMPLE_VENC_CTX_S));

    signal(SIGINT, sigterm_handler);
    RK_BOOL bMultictx = RK_FALSE;
    SAMPLE_COMM_ISP_Init(0, RK_AIQ_WORKING_MODE_NORMAL, RK_FALSE, "/etc/iqfiles/");
    SAMPLE_COMM_ISP_Run(0);

    // init rtsp
    g_rtsplive = create_rtsp_demo(554);
    g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/0");
    rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
    rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());

    if (RK_MPI_SYS_Init() != RK_SUCCESS)
    {
        goto __FAILED;
    }

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
    SAMPLE_COMM_VI_CreateChn(&viCtx[0]);

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
    ;
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
    SAMPLE_COMM_VENC_CreateChn(&vencCtx);

    pthread_t get_vi_stream_thread;
    pthread_create(&get_vi_stream_thread, NULL, get_vi_stream, NULL);

    getchar();

    pthread_join(get_vi_stream_thread, RK_NULL);

    SAMPLE_COMM_VENC_DestroyChn(&vencCtx);
    SAMPLE_COMM_VI_DestroyChn(&viCtx[0]);
    pthread_join(vencCtx.getStreamThread, RK_NULL);

__FINISHED:
    quit = true;
    rtsp_del_demo(g_rtsplive);
__FAILED:

    RK_MPI_SYS_Exit();
    SAMPLE_COMM_ISP_Stop(0);

    return 0;
}