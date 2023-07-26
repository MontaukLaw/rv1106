/*
 * Copyright 2023 Rockchip Electronics Co. LTD
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
#include <semaphore.h>
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

    typedef struct _rkModeTest
    {
        RK_BOOL bIfMainThreadQuit;
        RK_BOOL bIfVpssTHreadQuit;
        RK_BOOL bModuleTestThreadQuit;
        RK_BOOL bModuleTestIfopen;
        RK_S32 s32ModuleTestType;
        RK_S32 s32ModuleTestLoop;
        RK_U32 u32TestFrameCount;
        RK_U32 u32VpssGetFrameCount;
        RK_U32 u32VpssModeTestDstWidth;
        RK_U32 u32VpssModeTestDstHeight;
        pthread_t vpss_thread_id;
        pthread_t venc_thread_id;
    } g_mode_test;

    typedef struct _rkMpiCtx
    {
        SAMPLE_VI_CTX_S vi;
        SAMPLE_VPSS_CTX_S vpss;
        // SAMPLE_VPSS_CTX_S vpss2Venc;
    } SAMPLE_MPI_CTX_S;

    /* global param */
    g_mode_test *gModeTest = {0};
    SAMPLE_MPI_CTX_S *ctx = {0};
    RK_S32 g_exit_result = RK_SUCCESS;
    sem_t g_sem_module_test = {0};
    pthread_mutex_t g_frame_count_mutex = {0};
    static rtsp_demo_handle g_rtsplive = NULL;
    static rtsp_session_handle g_rtsp_session_0;

    void init_rtsp(void)
    {
        // init rtsp
        g_rtsplive = create_rtsp_demo(554);
        g_rtsp_session_0 = rtsp_new_session(g_rtsplive, "/live/0");
        rtsp_set_video(g_rtsp_session_0, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
        rtsp_sync_video_ts(g_rtsp_session_0, rtsp_get_reltime(), rtsp_get_ntptime());
    }

    static void program_handle_error(const char *func, RK_U32 line)
    {
        RK_LOGE("func: <%s> line: <%d> error exit!", func, line);
        g_exit_result = RK_FAILURE;
        gModeTest->bIfMainThreadQuit = RK_TRUE;
    }

    static void program_normal_exit(const char *func, RK_U32 line)
    {
        RK_LOGE("func: <%s> line: <%d> normal exit!", func, line);
        gModeTest->bIfMainThreadQuit = RK_TRUE;
    }

    static void sigterm_handler(int sig)
    {
        fprintf(stderr, "signal %d\n", sig);
        program_normal_exit(__func__, __LINE__);
    }

    static RK_S32 test_venc_init(int chnId, int width, int height, RK_CODEC_ID_E enType)
    {
        printf("================================%s==================================\n",
               __func__);
        VENC_RECV_PIC_PARAM_S stRecvParam;
        VENC_CHN_ATTR_S stAttr;
        memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));

        stAttr.stVencAttr.enType = enType;
        stAttr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
        stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
        stAttr.stRcAttr.stH264Cbr.u32BitRate = 10 * 1024;
        stAttr.stRcAttr.stH264Cbr.u32Gop = 60;
        stAttr.stVencAttr.u32PicWidth = width;
        stAttr.stVencAttr.u32PicHeight = height;
        stAttr.stVencAttr.u32VirWidth = width;
        stAttr.stVencAttr.u32VirHeight = height;
        stAttr.stVencAttr.u32StreamBufCnt = 2;
        stAttr.stVencAttr.u32BufSize = width * height * 3 / 2;
        RK_MPI_VENC_CreateChn(chnId, &stAttr);

        memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
        stRecvParam.s32RecvPicNum = -1;
        RK_MPI_VENC_StartRecvFrame(chnId, &stRecvParam);

        return 0;
    }
    /******************************************************************************
     * function : vpss thread
     ******************************************************************************/
    static void *venc_get_stream(void *pArgs)
    {
        printf("#Start %s , arg:%p\n", __func__, pArgs);
        SAMPLE_VENC_CTX_S *ctx = (SAMPLE_VENC_CTX_S *)(pArgs);
        RK_S32 s32Ret = RK_FAILURE;
        char name[256] = {0};
        FILE *fp = RK_NULL;
        void *pData = RK_NULL;
        RK_S32 loopCount = 0;

        while (!gModeTest->bIfVpssTHreadQuit)
        {
            s32Ret = SAMPLE_COMM_VENC_GetStream(ctx, &pData);
            if (s32Ret == RK_SUCCESS)
            {
                printf("chn:%d, loopCount:%d wd:%d\n", ctx->s32ChnId, loopCount, ctx->stFrame.pstPack->u32Len);
                // exit when complete
                if (ctx->s32loopCount > 0)
                {
                    if (loopCount >= ctx->s32loopCount)
                    {
                        SAMPLE_COMM_VENC_ReleaseStream(ctx);
                        gModeTest->bIfVpssTHreadQuit = true;
                        break;
                    }
                }

                PrintStreamDetails(ctx->s32ChnId, ctx->stFrame.pstPack->u32Len);
                // rtsp_tx_video(g_rtsp_session_0, (const uint8_t *)pData, ctx->stFrame.pstPack->u32Len,
                // ctx->stFrame.pstPack->u64PTS);
                // rtsp_do_event(g_rtsplive);

                SAMPLE_COMM_VENC_ReleaseStream(ctx);
                loopCount++;
            }
            usleep(1000);
        }

        return RK_NULL;
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
        venc.u32Fps = 30;

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

    static void *vpss_get_stream(void *pArgs)
    {
        SAMPLE_VPSS_CTX_S *ctx = (SAMPLE_VPSS_CTX_S *)(pArgs);
        void *pData = RK_NULL;
        char name[256] = {0};
        RK_S32 s32Ret = RK_FAILURE;
        RK_S32 loopCount = 0;
        FILE *fp = RK_NULL;
        PIC_BUF_ATTR_S stBufAttr;
        MB_PIC_CAL_S stPicCal;

        if (ctx->dstFilePath)
        {
            snprintf(name, sizeof(name), "/%s/vpss_%d.bin", ctx->dstFilePath, ctx->s32ChnId);
            fp = fopen(name, "wb");
            if (fp == RK_NULL)
            {
                printf("chn %d can't open %s file !\n", ctx->s32ChnId, ctx->dstFilePath);
                program_handle_error(__func__, __LINE__);
                return RK_NULL;
            }
        }

        while (!gModeTest->bIfVpssTHreadQuit)
        {
            s32Ret = SAMPLE_COMM_VPSS_GetChnFrame(ctx, &pData);
            if (s32Ret == RK_SUCCESS)
            {
                // exit when complete
                if (ctx->s32loopCount > 0)
                {
                    if (loopCount >= ctx->s32loopCount)
                    {
                        SAMPLE_COMM_VPSS_ReleaseChnFrame(ctx);
                        program_normal_exit(__func__, __LINE__);
                        break;
                    }
                }

                if (fp)
                {
                    /* cal frame size */
                    stBufAttr.u32Width = ctx->stChnFrameInfos.stVFrame.u32Width;
                    stBufAttr.u32Height = ctx->stChnFrameInfos.stVFrame.u32Height;
                    stBufAttr.enCompMode = ctx->stChnFrameInfos.stVFrame.enCompressMode;
                    stBufAttr.enPixelFormat = ctx->stChnFrameInfos.stVFrame.enPixelFormat;
                    memset(&stPicCal, 0, sizeof(MB_PIC_CAL_S));
                    s32Ret = RK_MPI_CAL_VGS_GetPicBufferSize(&stBufAttr, &stPicCal);
                    if (s32Ret != RK_SUCCESS)
                    {
                        RK_LOGE("RK_MPI_CAL_VGS_GetPicBufferSize failure s32Ret:%#X", s32Ret);
                    }
                    else
                    {
                        printf("got from vpss 0");
                        // fwrite(pData, 1, stPicCal.u32MBSize, fp);
                        // fflush(fp);
                    }
                }

                if (ctx->stVpssChnAttr[0].u32Width == ctx->stChnFrameInfos.stVFrame.u32Width)
                {
                    if (ctx->stChnFrameInfos.stVFrame.u32Height != ctx->stVpssChnAttr[0].u32Height ||
                        ctx->stChnFrameInfos.stVFrame.u32VirWidth != ctx->stVpssChnAttr[0].u32Width ||
                        ctx->stChnFrameInfos.stVFrame.u32VirHeight != ctx->stVpssChnAttr[0].u32Height ||
                        ctx->stChnFrameInfos.stVFrame.enPixelFormat != ctx->stVpssChnAttr[0].enPixelFormat)
                    {
                        RK_LOGE("Avs Current resolution is:%dX%d, get frame's resolution is "
                                ":%dX%d, the frame's PixelFormat"
                                "is :%d need to equal to %d,error exit!!!",
                                ctx->stVpssChnAttr[0].u32Width,
                                ctx->stVpssChnAttr[0].u32Height,
                                ctx->stChnFrameInfos.stVFrame.u32Width,
                                ctx->stChnFrameInfos.stVFrame.u32Height,
                                ctx->stChnFrameInfos.stVFrame.enPixelFormat,
                                ctx->stVpssChnAttr[0].enPixelFormat);
                        program_handle_error(__func__, __LINE__);
                    }
                }
                else if (gModeTest->u32VpssModeTestDstWidth == ctx->stChnFrameInfos.stVFrame.u32Width)
                {
                    if (ctx->stChnFrameInfos.stVFrame.u32Height != gModeTest->u32VpssModeTestDstHeight ||
                        ctx->stChnFrameInfos.stVFrame.u32VirWidth != gModeTest->u32VpssModeTestDstWidth ||
                        ctx->stChnFrameInfos.stVFrame.u32VirHeight != gModeTest->u32VpssModeTestDstHeight ||
                        ctx->stChnFrameInfos.stVFrame.enPixelFormat != ctx->stVpssChnAttr[0].enPixelFormat)
                    {
                        RK_LOGE("Avs Current resolution is:%dX%d, get frame's resolution is "
                                ":%dX%d, the frame's PixelFormat"
                                "is :%d need to equal to %d,error exit!!!",
                                gModeTest->u32VpssModeTestDstWidth,
                                gModeTest->u32VpssModeTestDstHeight,
                                ctx->stChnFrameInfos.stVFrame.u32Width,
                                ctx->stChnFrameInfos.stVFrame.u32Height,
                                ctx->stChnFrameInfos.stVFrame.enPixelFormat,
                                ctx->stVpssChnAttr[0].enPixelFormat);
                        program_handle_error(__func__, __LINE__);
                    }
                }
                else
                {
                    RK_LOGE("avs frame's resolution:%xX%d isn't equal to Setting's",
                            ctx->stChnFrameInfos.stVFrame.u32VirWidth,
                            ctx->stChnFrameInfos.stVFrame.u32VirHeight);
                    program_handle_error(__func__, __LINE__);
                }

                SAMPLE_COMM_VPSS_ReleaseChnFrame(ctx);
                loopCount++;

                if (gModeTest->bModuleTestIfopen)
                {

                    pthread_mutex_lock(&g_frame_count_mutex);
                    gModeTest->u32VpssGetFrameCount++;
                    pthread_mutex_unlock(&g_frame_count_mutex);

                    if (gModeTest->u32VpssGetFrameCount == gModeTest->u32TestFrameCount)
                    {
                        sem_post(&g_sem_module_test);
                    }
                }
                RK_LOGE("vpss get_stream count: %d", loopCount);
            }
            usleep(1000);
        }

        if (fp)
        {
            fclose(fp);
            fp = RK_NULL;
        }
        RK_LOGE("-----------vpss_get_stream thread exit!!!");
        return RK_NULL;
    }

    static RK_S32 global_param_init(void)
    {

        ctx = (SAMPLE_MPI_CTX_S *)malloc(sizeof(SAMPLE_MPI_CTX_S));
        if (ctx == RK_NULL)
        {
            RK_LOGE("malloc for ctx failure");
            goto INIT_FAIL;
        }
        memset(ctx, 0, sizeof(SAMPLE_MPI_CTX_S));

        gModeTest = (g_mode_test *)malloc(sizeof(g_mode_test));
        if (gModeTest == RK_NULL)
        {
            RK_LOGE("malloc for gModeTest failure");
            goto INIT_FAIL;
        }
        memset(gModeTest, 0, sizeof(g_mode_test));

        gModeTest->s32ModuleTestLoop = -1;
        gModeTest->u32TestFrameCount = 10;
        gModeTest->u32VpssModeTestDstWidth = 1280;
        gModeTest->u32VpssModeTestDstHeight = 800;

        sem_init(&g_sem_module_test, 0, 0);

        if (pthread_mutex_init(&g_frame_count_mutex, NULL) != 0)
        {
            RK_LOGE("mutex init failure \n");
            goto INIT_FAIL;
        }

        return RK_SUCCESS;

    INIT_FAIL:
        if (ctx)
        {
            free(ctx);
            ctx = RK_NULL;
        }
        if (gModeTest)
        {
            free(gModeTest);
            gModeTest = RK_NULL;
        }

        return RK_FAILURE;
    }

    static RK_S32 global_param_deinit(void)
    {

        if (ctx)
        {
            free(ctx);
            ctx = RK_NULL;
        }

        if (gModeTest)
        {
            free(gModeTest);
            gModeTest = RK_NULL;
        }
        sem_destroy(&g_sem_module_test);
        pthread_mutex_destroy(&g_frame_count_mutex);

        return RK_SUCCESS;
    }

    /******************************************************************************
     * function    : main()
     * Description : main
     ******************************************************************************/
    int main(int argc, char *argv[])
    {
        MPP_CHN_S stvpssChn, stvencChn;
        RK_S32 s32Ret = RK_FAILURE;
        RK_U32 u32ViWidth = 1920;
        RK_U32 u32ViHeight = 1080;
        RK_U32 u32VpssWidth = 1920;
        RK_U32 u32VpssHeight = 1080;
        RK_CHAR *pOutPath = "/data/nfs";
        RK_S32 s32CamId = 0;
        RK_S32 s32loopCnt = -1;
        MPP_CHN_S stSrcChn, stDestChn;
        pthread_t modeTest_thread_id = 0;

        s32Ret = global_param_init();
        if (s32Ret != RK_SUCCESS)
        {
            RK_LOGE("global_param_init %#X", s32Ret);
            return s32Ret;
        }

        signal(SIGINT, sigterm_handler);
        RK_BOOL bMultictx = RK_FALSE;
        int c;
        char *iq_file_dir = "/etc/iqfiles/";

        printf("#CameraIdx: %d\n", s32CamId);
        printf("#Output Path: %s\n", pOutPath);
        printf("#IQ Path: %s\n", iq_file_dir);

        printf("#Rkaiq XML DirPath: %s\n", iq_file_dir);
        rk_aiq_working_mode_t eHdrMode = RK_AIQ_WORKING_MODE_NORMAL;

        // init_rtsp();

        s32Ret = SAMPLE_COMM_ISP_Init(s32CamId, eHdrMode, bMultictx, iq_file_dir);
        s32Ret |= SAMPLE_COMM_ISP_Run(s32CamId);
        if (s32Ret != RK_SUCCESS)
        {
            RK_LOGE("ISP init failure:#%X", s32Ret);
            g_exit_result = RK_FAILURE;
            goto __FAILED2;
        }

        /* SYS Init */
        if (RK_MPI_SYS_Init() != RK_SUCCESS)
        {
            goto __FAILED;
        }

        /* Init VI */
        ctx->vi.u32Width = u32ViWidth;
        ctx->vi.u32Height = u32ViHeight;
        ctx->vi.s32DevId = s32CamId;
        ctx->vi.u32PipeId = ctx->vi.s32DevId;
        ctx->vi.s32ChnId = 1;
        ctx->vi.stChnAttr.stIspOpt.u32BufCount = 2;
        ctx->vi.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
        ctx->vi.stChnAttr.u32Depth = 2;
        ctx->vi.stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
        ctx->vi.stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
        ctx->vi.stChnAttr.stFrameRate.s32SrcFrameRate = -1;
        ctx->vi.stChnAttr.stFrameRate.s32DstFrameRate = -1;
        SAMPLE_COMM_VI_CreateChn(&ctx->vi);

        printf("111111111111111111111111111111111111111111111\n");
        /* Init VPSS */
        ctx->vpss.s32GrpId = 0;
        ctx->vpss.s32ChnId = 0;
        ctx->vpss.s32loopCount = s32loopCnt;
        ctx->vpss.dstFilePath = pOutPath;
        /* RGA_device: VIDEO_PROC_DEV_RGA */
        ctx->vpss.enVProcDevType = VIDEO_PROC_DEV_RGA;
        ctx->vpss.stGrpVpssAttr.enPixelFormat = RK_FMT_YUV420SP;
        ctx->vpss.stGrpVpssAttr.enCompressMode = COMPRESS_MODE_NONE; /* no compress */
        ctx->vpss.stVpssChnAttr[0].enChnMode = VPSS_CHN_MODE_USER;
        ctx->vpss.stVpssChnAttr[0].enCompressMode = COMPRESS_MODE_NONE;
        ctx->vpss.stVpssChnAttr[0].enDynamicRange = DYNAMIC_RANGE_SDR8;
        ctx->vpss.stVpssChnAttr[0].enPixelFormat = RK_FMT_RGB888;
        ctx->vpss.stVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
        ctx->vpss.stVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
        ctx->vpss.stVpssChnAttr[0].u32Width = 640;
        ctx->vpss.stVpssChnAttr[0].u32Height = 640;

        ctx->vpss.stVpssChnAttr[1].u32Width = 1920;
        ctx->vpss.stVpssChnAttr[1].u32Height = 1080;
        ctx->vpss.stVpssChnAttr[1].enPixelFormat = RK_FMT_YUV420SP;
        s32Ret = SAMPLE_COMM_VPSS_CreateChn(&ctx->vpss);
        if (s32Ret != RK_SUCCESS)
        {
            RK_LOGE("SAMPLE_COMM_VPSS_CreateChn failure:%#X", s32Ret);
            program_handle_error(__func__, __LINE__);
        }

        printf("2222222222222222222222222222222222222222222222222222\n");

        /* launch vpss get frame thread */
        pthread_create(&gModeTest->vpss_thread_id, 0, vpss_get_stream, (void *)(&ctx->vpss));

        // 初始化venc通道0
        // test_venc_init(0, 1920, 1080, RK_VIDEO_ID_AVC);
        init_venc(1920, 1080, 0, venc_get_stream);

        printf("333333333333333333333333333333333333333333\n");

        ////////////////////////////////////////////////
        /* Bind VI and VPSS */
        stSrcChn.enModId = RK_ID_VI;
        stSrcChn.s32DevId = ctx->vi.s32DevId;
        stSrcChn.s32ChnId = ctx->vi.s32ChnId;
        stDestChn.enModId = RK_ID_VPSS;
        stDestChn.s32DevId = ctx->vpss.s32GrpId;
        stDestChn.s32ChnId = ctx->vpss.s32ChnId;
        SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);

        stvpssChn.enModId = RK_ID_VPSS;
        stvpssChn.s32DevId = 0;
        stvpssChn.s32ChnId = 1; // VPSS 通道1

        stvencChn.enModId = RK_ID_VENC;
        stvencChn.s32DevId = 0;
        stvencChn.s32ChnId = 0; // VENC通道0
        SAMPLE_COMM_Bind(&stvpssChn, &stvencChn);

        /* launch vpss get frame thread */
        // pthread_create(&gModeTest->venc_thread_id, 0, venc_get_stream, NULL);

        printf("%s initial finish\n", __func__);

        printf("4444444444444444444444444444444444444444444444444444444444444444444\n");

        while (!gModeTest->bIfMainThreadQuit)
        {
            sleep(1);
        }

    finshed:
        printf("%s exit!\n", __func__);
        rtsp_del_demo(g_rtsplive);

        /* vpss get frame thread exit */
        gModeTest->bIfVpssTHreadQuit = RK_TRUE;
        pthread_join(gModeTest->vpss_thread_id, NULL);

        /* UnBind Bind VI and VPSS */
        stSrcChn.enModId = RK_ID_VI;
        stSrcChn.s32DevId = ctx->vi.s32DevId;
        stSrcChn.s32ChnId = ctx->vi.s32ChnId;
        stDestChn.enModId = RK_ID_VPSS;
        stDestChn.s32DevId = ctx->vpss.s32GrpId;
        stDestChn.s32ChnId = ctx->vpss.s32ChnId;
        SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);
        SAMPLE_COMM_UnBind(&stvpssChn, &stvencChn);

        /* Destroy VPSS */
        SAMPLE_COMM_VPSS_DestroyChn(&ctx->vpss);
        /* Destroy VI[0] */
        SAMPLE_COMM_VI_DestroyChn(&ctx->vi);
        /* Destroy VPSS[1] */
        // SAMPLE_COMM_VPSS_DestroyChn(&ctx->vpss2Venc);

    __FAILED:
        RK_MPI_SYS_Exit();
        SAMPLE_COMM_ISP_Stop(0);
    __FAILED2:
        global_param_deinit();

        return g_exit_result;
    }

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
