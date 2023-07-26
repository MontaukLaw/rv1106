#include "rk_common.h"
#include "rk_comm_vpss.h"
#include "utils.h"
#include "rk_mpi_sys.h"
#include "test_mod_vpss.h"
#include "test_comm_imgproc.h"
#include "test_comm_sys.h"
#include "test_comm_utils.h"
#include "test_comm_vpss.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "rk_debug.h"
#include "rk_mpi_vpss.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_cal.h"
static void *TEST_VPSS_ModSingleTest(void *arg)
{
    RK_S32 s32Ret = RK_SUCCESS;
    TEST_VPSS_CTX_S *pstCtx = reinterpret_cast<TEST_VPSS_CTX_S *>(arg);
    void *retArg = RK_NULL;
    VIDEO_FRAME_INFO_S stChnFrameInfos[VPSS_MAX_CHN_NUM];
    VIDEO_FRAME_INFO_S stGrpFrameInfo;
    char cWritePath[128] = {0};

    memset(stChnFrameInfos, 0, sizeof(VIDEO_FRAME_INFO_S) * VPSS_MAX_CHN_NUM);
    memset(&stGrpFrameInfo, 0, sizeof(VIDEO_FRAME_INFO_S));

    s32Ret = TEST_VPSS_ModInit(pstCtx);
    if (s32Ret != RK_SUCCESS)
    {
        goto __FAILED;
    }

    for (RK_S32 loopCount = 0; loopCount < pstCtx->s32LoopCount; loopCount++)
    {
        s32Ret = TEST_VPSS_ModSendFrame(pstCtx);
        if (s32Ret != RK_SUCCESS)
        {
            goto __FAILED;
        }
        s32Ret = TEST_VPSS_ModGetChnFrame(pstCtx, stChnFrameInfos);
        if (s32Ret != RK_SUCCESS)
        {
            goto __FAILED;
        }
        for (RK_S32 i = 0; i < pstCtx->s32ChnNum; i++)
        {
            if (pstCtx->dstFilePath)
            {
                snprintf(cWritePath, sizeof(cWritePath), "%schn_out_%dx%d_%d_%d.bin",
                         pstCtx->dstFilePath, stChnFrameInfos[i].stVFrame.u32VirWidth,
                         stChnFrameInfos[i].stVFrame.u32VirHeight, pstCtx->s32GrpIndex, i);
                s32Ret = TEST_COMM_FileWriteOneFrame(cWritePath, &(stChnFrameInfos[i]));
                if (s32Ret != RK_SUCCESS)
                {
                    goto __FAILED;
                }
            }
            s32Ret = RK_MPI_VPSS_ReleaseChnFrame(pstCtx->s32GrpIndex, i, &(stChnFrameInfos[i]));
            if (s32Ret != RK_SUCCESS)
            {
                goto __FAILED;
            }
        }
    }

    retArg = arg;
__FAILED:
    TEST_VPSS_ModDeInit(pstCtx);
    if (retArg == RK_NULL)
    {
        RK_LOGE("single vpss test %d running failed.", pstCtx->s32GrpIndex);
    }
    return retArg;
}

RK_S32 main(int argc, const char **argv)
{
    RK_S32 s32Ret;
    TEST_VPSS_CTX_S ctx;
    memset(&ctx, 0, sizeof(TEST_VPSS_CTX_S));

    //  set default params.
    ctx.dstFilePath = RK_NULL;
    ctx.s32LoopCount = 1;
    ctx.s32VProcDevType = VIDEO_PROC_DEV_RGA;// VIDEO_PROC_DEV_RGA;
    ctx.s32GrpNum = 1;
    ctx.s32ChnNum = 1;
    ctx.bGrpCropEn = RK_FALSE;
    ctx.bChnCropEn = RK_FALSE;
    ctx.s32GrpCropRatio = 1000;
    ctx.s32ChnCropRatio = 1000;
    ctx.s32SrcCompressMode = 0;
    ctx.s32SrcPixFormat = RK_FMT_YUV420SP;
    ctx.s32DstCompressMode = 0;
    ctx.s32DstPixFormat = RK_FMT_RGB888;
    ctx.s32GrpIndex = 0;
    ctx.u32ChnDepth = 8;
    ctx.s32SrcChnRate = -1;
    ctx.s32DstChnRate = -1;
    ctx.s32SrcGrpRate = -1;
    ctx.s32DstGrpRate = -1;

    ctx.srcFileName = "/data/vi_to_npu.yuv";
    ctx.s32SrcWidth = 640;
    ctx.s32SrcHeight = 640;
    ctx.s32SrcVirWidth = 640;
    ctx.s32SrcVirHeight = 640;
    ctx.s32DstWidth = 640;
    ctx.s32DstHeight = 640;
    ctx.dstFilePath = "/data/vpss/";

    s32Ret = RK_MPI_SYS_Init();
    if (s32Ret != RK_SUCCESS)
    {
        return s32Ret;
    }

    int64_t start_us = getCurrentTimeUs();
    int64_t elapse_us = 0;
    s32Ret = TEST_VPSS_ModTest(&ctx);
    if (s32Ret != RK_SUCCESS)
    {
        goto __FAILED;
    }
    elapse_us = getCurrentTimeUs() - start_us;
    printf("vpss spend  = %.2fms, FPS = %.2f\n", elapse_us / 1000.f, 1000.f * 1000.f / elapse_us);

    s32Ret = RK_MPI_SYS_Exit();
    if (s32Ret != RK_SUCCESS)
    {
        return s32Ret;
    }
    RK_LOGI("test running seems ok.");
    return RK_SUCCESS;

__FAILED:
    RK_MPI_SYS_Exit();
    RK_LOGE("test running failed!");
    return s32Ret;
}