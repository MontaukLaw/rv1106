/*
 * Copyright 2021 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* End of #ifdef __cplusplus */

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

    rtsp_demo_handle g_rtsplive = NULL;
    static rtsp_session_handle g_rtsp_session;

    typedef struct _rkMpiCtx
    {
        SAMPLE_VI_CTX_S vi;
        SAMPLE_VO_CTX_S vo;
        SAMPLE_VPSS_CTX_S vpss;
        SAMPLE_VENC_CTX_S venc;
        SAMPLE_RGN_CTX_S rgn[2];
    } SAMPLE_MPI_CTX_S;

    static bool quit = false;
    static void sigterm_handler(int sig)
    {
        fprintf(stderr, "signal %d\n", sig);
        quit = true;
    }

    /******************************************************************************
     * function : venc thread
     ******************************************************************************/
    static void *venc_get_stream(void *pArgs)
    {
        SAMPLE_VENC_CTX_S *ctx = (SAMPLE_VENC_CTX_S *)(pArgs);
        RK_S32 s32Ret = RK_FAILURE;
        void *pData = RK_NULL;
        RK_S32 loopCount = 0;

        while (!quit)
        {
            s32Ret = SAMPLE_COMM_VENC_GetStream(ctx, &pData);
            if (s32Ret == RK_SUCCESS)
            {

                PrintStreamDetails(ctx->s32ChnId, ctx->stFrame.pstPack->u32Len);
                rtsp_tx_video(g_rtsp_session, pData, ctx->stFrame.pstPack->u32Len,
                              ctx->stFrame.pstPack->u64PTS);
                rtsp_do_event(g_rtsplive);

                RK_LOGD("chn:%d, loopCount:%d wd:%d\n", ctx->s32ChnId, loopCount,
                        ctx->stFrame.pstPack->u32Len);

                SAMPLE_COMM_VENC_ReleaseStream(ctx);
                loopCount++;
            }
            usleep(1000);
        }

        return RK_NULL;
    }

    static void *vpss_get_stream(void *pArgs)
    {

        VIDEO_FRAME_INFO_S pstVideoFrame;

        memset(&pstVideoFrame, 0, sizeof(pstVideoFrame));

        void *pData = RK_NULL;
        char name[256] = {0};
        RK_S32 s32Ret = RK_FAILURE;
        RK_S32 loopCount = 0;
        PIC_BUF_ATTR_S stBufAttr;
        MB_PIC_CAL_S stPicCal;

        while (!quit)
        {
            s32Ret = RK_MPI_VPSS_GetChnFrame(0, 1, &pstVideoFrame, 1000);
            if (s32Ret != RK_SUCCESS)
            {
                RK_LOGE("RK_MPI_VPSS_GetChnFrame fail %x", s32Ret);
                usleep(1000);
                continue;
            }

            // RK_MPI_SYS_MmzFlushCache(ctx->stChnFrameInfos.stVFrame.pMbBlk, RK_TRUE);

            // pData = RK_MPI_MB_Handle2VirAddr(ctx->stChnFrameInfos.stVFrame.pMbBlk);

            SAMPLE_COMM_VPSS_ReleaseChnFrame(pstVideoFrame);
            loopCount++;
        }

        RK_LOGE("-----------vpss_get_stream thread exit!!!");
        return RK_NULL;
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
            s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, 1000);
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

                if (loopCount % 10 == 0)
                {
                    printf("loopCount:%d, fd:%d, data:%p\n", loopCount, fd, data);
                }

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
    /******************************************************************************
     * function    : main()
     * Description : main
     ******************************************************************************/
    int main(int argc, char *argv[])
    {
        SAMPLE_MPI_CTX_S *ctx;
        int video_width = 1920;
        int video_height = 1080;
        int venc_width = 1920;
        int venc_height = 1080;
        int disp_width = 1920;
        int disp_height = 1080;
        RK_CHAR *pDeviceName = "rkisp_mainpath";
        RK_CHAR *pInPathBmp = NULL;
        RK_CHAR *pOutPathVenc = NULL;
        CODEC_TYPE_E enCodecType = RK_CODEC_TYPE_H264;
        VENC_RC_MODE_E enRcMode = VENC_RC_MODE_H264CBR;
        RK_CHAR *pCodecName = "H264";
        RK_S32 s32CamId = 0;
        RK_S32 s32DisId = 1;
        RK_S32 s32DisLayerId = 0;
        RK_S32 s32loopCnt = -1;
        RK_S32 s32BitRate = 4 * 1024;
        MPP_CHN_S stSrcChn, stDestChn;

        ctx = (SAMPLE_MPI_CTX_S *)(malloc(sizeof(SAMPLE_MPI_CTX_S)));
        memset(ctx, 0, sizeof(SAMPLE_MPI_CTX_S));

        signal(SIGINT, sigterm_handler);

        RK_BOOL bMultictx = RK_FALSE;
        int c;
        char *iq_file_dir = NULL;

        printf("#CameraIdx: %d\n", s32CamId);
        printf("#pDeviceName: %s\n", pDeviceName);
        printf("#CodecName:%s\n", pCodecName);
        printf("#Output Path: %s\n", pOutPathVenc);
        printf("#IQ Path: %s\n", iq_file_dir);
        printf("#Rkaiq XML DirPath: %s\n", iq_file_dir);
        printf("#bMultictx: %d\n\n", bMultictx);
        rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;

        SAMPLE_COMM_ISP_Init(s32CamId, hdr_mode, bMultictx, iq_file_dir);
        SAMPLE_COMM_ISP_Run(s32CamId);

        // init rtsp
        g_rtsplive = create_rtsp_demo(554);
        g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/0");
        rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
        rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());

        if (RK_MPI_SYS_Init() != RK_SUCCESS)
        {
            goto __FAILED;
        }

        // Init VI[1]
        ctx->vi.u32Width = video_width;
        ctx->vi.u32Height = video_height;
        ctx->vi.s32DevId = s32CamId;
        ctx->vi.u32PipeId = ctx->vi.s32DevId;
        ctx->vi.s32ChnId = 1;
        ctx->vi.stChnAttr.stIspOpt.u32BufCount = 3;
        ctx->vi.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
        ctx->vi.stChnAttr.u32Depth = 1;
        ctx->vi.stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
        ctx->vi.stChnAttr.stFrameRate.s32SrcFrameRate = -1;
        ctx->vi.stChnAttr.stFrameRate.s32DstFrameRate = -1;
        strcpy(ctx->vi.stChnAttr.stIspOpt.aEntityName, "rkisp_selfpath");
        SAMPLE_COMM_VI_CreateChn(&ctx->vi);

        // Init VI[0]
        ctx->vi.u32Width = video_width;
        ctx->vi.u32Height = video_height;
        ctx->vi.s32DevId = s32CamId;
        ctx->vi.u32PipeId = ctx->vi.s32DevId;
        ctx->vi.s32ChnId = 0;
        ctx->vi.stChnAttr.stIspOpt.u32BufCount = 3;
        ctx->vi.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
        ctx->vi.stChnAttr.u32Depth = 1;
        ctx->vi.stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
        ctx->vi.stChnAttr.stFrameRate.s32SrcFrameRate = -1;
        ctx->vi.stChnAttr.stFrameRate.s32DstFrameRate = -1;
        strcpy(ctx->vi.stChnAttr.stIspOpt.aEntityName, "rkisp_mainpath");
        SAMPLE_COMM_VI_CreateChn(&ctx->vi);

        // Init VPSS[0]
        ctx->vpss.s32GrpId = 0;
        ctx->vpss.s32ChnId = 0;
        // RGA_device: VIDEO_PROC_DEV_RGA GPU_device: VIDEO_PROC_DEV_GPU
        ctx->vpss.enVProcDevType = VIDEO_PROC_DEV_RGA;
        ctx->vpss.stGrpVpssAttr.enPixelFormat = RK_FMT_YUV420SP;
        ctx->vpss.stGrpVpssAttr.enCompressMode = COMPRESS_MODE_NONE; // no compress

        // ctx->vpss.stCropInfo.bEnable = RK_FALSE;
        // ctx->vpss.stCropInfo.enCropCoordinate = VPSS_CROP_RATIO_COOR;
        // ctx->vpss.stCropInfo.stCropRect.s32X = 0;
        // ctx->vpss.stCropInfo.stCropRect.s32Y = 0;
        // ctx->vpss.stCropInfo.stCropRect.u32Width = video_width;
        // ctx->vpss.stCropInfo.stCropRect.u32Height = video_height;

        ctx->vpss.stVpssChnAttr[0].enChnMode = VPSS_CHN_MODE_USER;
        ctx->vpss.stVpssChnAttr[0].enCompressMode = COMPRESS_MODE_NONE;
        ctx->vpss.stVpssChnAttr[0].enDynamicRange = DYNAMIC_RANGE_SDR8;
        ctx->vpss.stVpssChnAttr[0].enPixelFormat = RK_FMT_YUV420SP;
        ctx->vpss.stVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
        ctx->vpss.stVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
        ctx->vpss.stVpssChnAttr[0].u32Width = venc_width;
        ctx->vpss.stVpssChnAttr[0].u32Height = venc_height;

        ctx->vpss.stVpssChnAttr[1].enChnMode = VPSS_CHN_MODE_USER;
        ctx->vpss.stVpssChnAttr[1].enCompressMode = COMPRESS_MODE_NONE;
        ctx->vpss.stVpssChnAttr[1].enDynamicRange = DYNAMIC_RANGE_SDR8;
        ctx->vpss.stVpssChnAttr[1].enPixelFormat = RK_FMT_RGB888;
        ctx->vpss.stVpssChnAttr[1].stFrameRate.s32SrcFrameRate = -1;
        ctx->vpss.stVpssChnAttr[1].stFrameRate.s32DstFrameRate = -1;
        ctx->vpss.stVpssChnAttr[1].u32Width = video_width;
        ctx->vpss.stVpssChnAttr[1].u32Height = video_height;

        SAMPLE_COMM_VPSS_CreateChn(&ctx->vpss);

        // Init VENC[0]
        ctx->venc.s32ChnId = 0;
        ctx->venc.u32Width = venc_width;
        ctx->venc.u32Height = venc_height;
        int u32BufSize = venc_width * venc_height / 4;
        ctx->venc.stChnAttr.stVencAttr.u32BufSize = u32BufSize;
        ctx->venc.u32Fps = 30;
        ctx->venc.u32Gop = 50;
        ctx->venc.u32BitRate = 4 * 1024;
        ctx->venc.enCodecType = RK_CODEC_TYPE_H264;
        ctx->venc.enRcMode = VENC_RC_MODE_H264CBR;
        ctx->venc.getStreamCbFunc = venc_get_stream;
        ctx->venc.s32loopCount = -1;
        ctx->venc.dstFilePath = "/data/";
        // H264  66：Baseline  77：Main Profile 100：High Profile
        // H265  0：Main Profile  1：Main 10 Profile
        // MJPEG 0：Baseline
        ctx->venc.stChnAttr.stVencAttr.u32Profile = 100;
        ctx->venc.stChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP; // VENC_GOPMODE_SMARTP
        SAMPLE_COMM_VENC_CreateChn(&ctx->venc);

        // 开始接收数据
        VENC_RECV_PIC_PARAM_S stRecvParam;
        stRecvParam.s32RecvPicNum = -1;
        RK_MPI_VENC_StartRecvFrame(0, &stRecvParam);

        printf("create venc\n");

#if 0
        // Init RGN[0]
        ctx->rgn[0].rgnHandle = 0;
        ctx->rgn[0].stRgnAttr.enType = COVER_RGN;
        ctx->rgn[0].stMppChn.enModId = RK_ID_VENC;
        ctx->rgn[0].stMppChn.s32ChnId = 0;
        ctx->rgn[0].stMppChn.s32DevId = ctx->venc.s32ChnId;
        ctx->rgn[0].stRegion.s32X = 1328;    // must be 16 aligned
        ctx->rgn[0].stRegion.s32Y = 976;     // must be 16 aligned
        ctx->rgn[0].stRegion.u32Width = 576; // must be 16 aligned
        ctx->rgn[0].stRegion.u32Height = 96; // must be 16 aligned
        ctx->rgn[0].u32Color = 0x00f800;
        ctx->rgn[0].u32Layer = 1;
        SAMPLE_COMM_RGN_CreateChn(&ctx->rgn[0]);

        // Init RGN[1]
        ctx->rgn[1].rgnHandle = 1;
        ctx->rgn[1].stRgnAttr.enType = OVERLAY_RGN;
        ctx->rgn[1].stMppChn.enModId = RK_ID_VENC;
        ctx->rgn[1].stMppChn.s32ChnId = 0;
        ctx->rgn[1].stMppChn.s32DevId = ctx->venc.s32ChnId;
        ctx->rgn[1].stRegion.s32X = 0;       // must be 16 aligned
        ctx->rgn[1].stRegion.s32Y = 0;       // must be 16 aligned
        ctx->rgn[1].stRegion.u32Width = 160; // must be 16 aligned
        ctx->rgn[1].stRegion.u32Height = 96; // must be 16 aligned
        ctx->rgn[1].u32BmpFormat = RK_FMT_BGRA5551;
        ctx->rgn[1].u32BgAlpha = 128;
        ctx->rgn[1].u32FgAlpha = 128;
        ctx->rgn[1].u32Layer = 1;
        ctx->rgn[1].srcFileBmpName = pInPathBmp;
        SAMPLE_COMM_RGN_CreateChn(&ctx->rgn[1]);
#endif

        // Init VO[0]
        ctx->vo.s32DevId = s32DisId;
        ctx->vo.s32ChnId = 0;
        ctx->vo.s32LayerId = s32DisLayerId;
        ctx->vo.Volayer_mode = VO_LAYER_MODE_GRAPHIC;
        ctx->vo.u32DispBufLen = 3;
        ctx->vo.stLayerAttr.stDispRect.s32X = 0;
        ctx->vo.stLayerAttr.stDispRect.s32Y = 0;
        ctx->vo.stLayerAttr.stDispRect.u32Width = disp_width;
        ctx->vo.stLayerAttr.stDispRect.u32Height = disp_height;
        ctx->vo.stLayerAttr.stImageSize.u32Width = disp_width;
        ctx->vo.stLayerAttr.stImageSize.u32Height = disp_height;
        ctx->vo.stLayerAttr.u32DispFrmRt = 30;
        ctx->vo.stLayerAttr.enPixFormat = RK_FMT_RGB888;
        // ctx->vo.stLayerAttr.bDoubleFrame = RK_FALSE;
        ctx->vo.stChnAttr.stRect.s32X = 0;
        ctx->vo.stChnAttr.stRect.s32Y = 0;
        ctx->vo.stChnAttr.stRect.u32Width = disp_width;
        ctx->vo.stChnAttr.stRect.u32Height = disp_height;
        ctx->vo.stChnAttr.u32Priority = 1;
        ctx->vo.stVoPubAttr.enIntfType = VO_INTF_HDMI;
        ctx->vo.stVoPubAttr.enIntfSync = VO_OUTPUT_1080P60;
        SAMPLE_COMM_VO_CreateChn(&ctx->vo);

        printf("binding ctx->vi.s32DevId %d, ctx->vi.s32ChnId %d\n", ctx->vi.s32DevId, ctx->vi.s32ChnId);
        printf("binding ctx->vpss.s32GrpId %d, ctx->vpss.s32ChnId %d\n", ctx->vpss.s32GrpId, ctx->vpss.s32ChnId);

        // Bind VI[0] and VPSS[0]
        stSrcChn.enModId = RK_ID_VI;
        stSrcChn.s32DevId = 0;
        stSrcChn.s32ChnId = 0;
        stDestChn.enModId = RK_ID_VPSS;
        stDestChn.s32DevId = 0;
        stDestChn.s32ChnId = 0;
        SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);

        stSrcChn.s32ChnId = 1;
        stDestChn.s32ChnId = 1;
        SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);
        printf("bind vi to vpss \n");

        printf("binding ctx->vpss.s32GrpId %d, ctx->vpss.s32ChnId %d\n", ctx->vpss.s32GrpId, ctx->vpss.s32ChnId);
        printf("binding ctx->venc.s32ChnId %d\n", ctx->venc.s32ChnId);
        // getchar();
        // Bind VPSS[0] and VENC[0]
        stSrcChn.enModId = RK_ID_VPSS;
        stSrcChn.s32DevId = 0;
        stSrcChn.s32ChnId = 0;
        stDestChn.enModId = RK_ID_VENC;
        stDestChn.s32DevId = 0;
        stDestChn.s32ChnId = 0;
        SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);
        printf("bind vpss to venc \n");
        // getchar();
        // Bind VPSS[1] and VO[0]
        printf("binding ctx->vpss.s32GrpId %d, ctx->vpss.s32ChnId %d\n", ctx->vpss.s32GrpId, ctx->vpss.s32ChnId);
        printf("binding ctx->vo.s32LayerId %d, ctx->vo.s32ChnId %d\n", ctx->vo.s32LayerId, ctx->vo.s32ChnId);

        memset(&stSrcChn, 0, sizeof(stSrcChn));
        memset(&stDestChn, 0, sizeof(stDestChn));

        stSrcChn.enModId = RK_ID_VPSS;
        stSrcChn.s32DevId = ctx->vpss.s32GrpId;
        stSrcChn.s32ChnId = 1;
        stDestChn.enModId = RK_ID_VO;
        stDestChn.s32DevId = ctx->vo.s32LayerId;
        stDestChn.s32ChnId = ctx->vo.s32ChnId;
        SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);
        printf("bind vpss 1 to vo \n");

        // sleep(5);
        pthread_t vpss_thread_id;
        // pthread_create(vpss_thread_id, 0, vpss_get_stream, NULL);

        pthread_t get_vi_to_npu_thread;
        // pthread_create(&get_vi_to_npu_thread, NULL, rkipc_get_vi_to_npu, NULL);
        printf("%s initial finish\n", __func__);

        while (!quit)
        {
            sleep(1);
        }

        printf("%s exit!\n", __func__);

        if (ctx->venc.getStreamCbFunc)
        {
            pthread_join(ctx->venc.getStreamThread, NULL);
        }

        if (g_rtsplive)
            rtsp_del_demo(g_rtsplive);

        // UnBind VPSS[0] and VO[0]
        stSrcChn.enModId = RK_ID_VPSS;
        stSrcChn.s32DevId = ctx->vpss.s32GrpId;
        stSrcChn.s32ChnId = 1;
        stDestChn.enModId = RK_ID_VO;
        stDestChn.s32DevId = ctx->vo.s32LayerId;
        stDestChn.s32ChnId = ctx->vo.s32ChnId;
        SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);

        // UnBind VPSS[0] and VENC[0]
        stSrcChn.enModId = RK_ID_VPSS;
        stSrcChn.s32DevId = ctx->vpss.s32GrpId;
        stSrcChn.s32ChnId = ctx->vpss.s32ChnId;
        stDestChn.enModId = RK_ID_VENC;
        stDestChn.s32DevId = 0;
        stDestChn.s32ChnId = ctx->venc.s32ChnId;
        SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);

        // UnBind VI[0] and VPSS[0]
        stSrcChn.enModId = RK_ID_VI;
        stSrcChn.s32DevId = ctx->vi.s32DevId;
        stSrcChn.s32ChnId = ctx->vi.s32ChnId;
        stDestChn.enModId = RK_ID_VPSS;
        stDestChn.s32DevId = ctx->vpss.s32GrpId;
        stDestChn.s32ChnId = ctx->vpss.s32ChnId;
        SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);

        // Destroy VO[0]
        SAMPLE_COMM_VO_DestroyChn(&ctx->vo);
        // Destroy RGN[1]
        SAMPLE_COMM_RGN_DestroyChn(&ctx->rgn[1]);
        // Destroy RGN[0]
        SAMPLE_COMM_RGN_DestroyChn(&ctx->rgn[0]);
        // Destroy VENC[0]
        SAMPLE_COMM_VENC_DestroyChn(&ctx->venc);
        // Destroy VPSS[0]
        SAMPLE_COMM_VPSS_DestroyChn(&ctx->vpss);
        // Destroy VI[0]
        SAMPLE_COMM_VI_DestroyChn(&ctx->vi);
    __FAILED:
        RK_MPI_SYS_Exit();
        if (iq_file_dir)
        {
#ifdef RKAIQ
            SAMPLE_COMM_ISP_Stop(s32CamId);
#endif
        }
        if (ctx)
        {
            free(ctx);
            ctx = RK_NULL;
        }

        return 0;
    }

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
