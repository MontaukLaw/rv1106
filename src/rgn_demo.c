
#include "comm.h"

#define TEST_ARGB32_PIX_SIZE 4
#define TEST_ARGB32_GREEN 0x00FF00FF
#define TEST_ARGB32_BLUE 0x0000FFFF
#define TEST_ARGB32_TRANS 0x00000000
#define TEST_ARGB32_BLACK 0x000000FF
#define TEST_ARGB32_RED 0xFF0000FF

// RK_U64 TEST_COMM_GetNowUs(void)
// {
//     struct timespec time = {0, 0};
//     clock_gettime(CLOCK_MONOTONIC, &time);
//     return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
// }

RK_S32 load_file_osdmem(const RK_CHAR *filename, RK_U8 *pu8Virt, RK_U32 u32Width, RK_U32 u32Height, RK_U32 pixel_size, RK_U32 shift_value)
{
    RK_U32 mem_len = u32Width;
    RK_U32 read_len = mem_len * pixel_size >> shift_value;
    RK_U32 read_height;
    FILE *file = NULL;

    file = fopen(filename, "rb");
    if (file == NULL)
    {
        RK_LOGE("open filename: %s file failed!", filename);
        return RK_FAILURE;
    }
    for (read_height = 0; read_height < u32Height; read_height++)
    {
        fread((pu8Virt + (u32Width * read_height * pixel_size >> shift_value)), 1, read_len, file);
    }
    fclose(file);
    return RK_SUCCESS;
}

static void set_argb8888_buffer(RK_U32 *buf, RK_U32 size, RK_U32 color)
{
    for (RK_U32 i = 0; buf && (i < size); i++)
    {
        *(buf + i) = color;
    }
}

// 将rgn通道帮到venc上, 因为rgn只能绑定到venc或者vi
RK_S32 bind_rgn_to_venc(void)
{
    RK_S32 s32Ret = RK_FAILURE;
    RK_CHAR *pOutPath = NULL;
    RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC;
    RK_CHAR *pCodecName = "H264";
    RK_S32 s32chnlId = 0;

    RGN_HANDLE RgnHandle = VENC_RECORD_TIME_OSD_HANDLE;
    BITMAP_S stBitmap;
    RGN_ATTR_S stRgnAttr;
    RGN_CHN_ATTR_S stRgnChnAttr;

    // int u32Width = 128;
    // int u32Height = 128;
    int u32Width = 384; // 16 * 24;
    int u32Height = 32;

    int s32X = 100;
    int s32Y = 100;

    // RK_CHAR *filename = "/data/res/rgn/44";

    MPP_CHN_S stMppChn;

    stMppChn.enModId = RK_ID_VENC;
    stMppChn.s32DevId = 0;
    stMppChn.s32ChnId = VPSS_CHN_0;

    /****************************************
     step 1: create overlay regions
    ****************************************/
    memset(&stRgnAttr, 0, sizeof(stRgnAttr));

    // stRgnAttr.unAttr.stOverlay.u32CanvasNum = 1;
    // RGN的类型是OVERLAY方式
    stRgnAttr.enType = OVERLAY_RGN;
    // stRgnAttr.unAttr.stOverlay.enPixelFmt = (PIXEL_FORMAT_E)RK_FMT_ARGB8888;
    // RGN格式为BGRA8888, 4字节.
    stRgnAttr.unAttr.stOverlay.enPixelFmt = (PIXEL_FORMAT_E)RK_FMT_BGRA8888;
    // RGN的宽高为364x32
    stRgnAttr.unAttr.stOverlay.stSize.u32Width = u32Width;
    stRgnAttr.unAttr.stOverlay.stSize.u32Height = u32Height;

    stRgnAttr.unAttr.stOverlay.u32ClutNum = 0;

    // 创建RGN区域
    s32Ret = RK_MPI_RGN_Create(RgnHandle, &stRgnAttr);
    if (RK_SUCCESS != s32Ret)
    {
        RK_LOGE("RK_MPI_RGN_Create (%d) failed with %#x!", RgnHandle, s32Ret);
        RK_MPI_RGN_Destroy(RgnHandle);
        return RK_FAILURE;
    }
    RK_LOGI("The handle: %d, create success!", RgnHandle);

    /*********************************************
     step 2: display overlay regions to groups
     *********************************************/
    memset(&stRgnChnAttr, 0, sizeof(stRgnChnAttr));
    // RGN通道属性设置
    // 可见
    stRgnChnAttr.bShow = RK_TRUE;
    // 通道类型为OVERLAY
    stRgnChnAttr.enType = OVERLAY_RGN;
    //
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = s32X;
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = s32Y;
    // 前景色透明度
    stRgnChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 0;
    // 背景色透明度
    stRgnChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 128;
    stRgnChnAttr.unChnAttr.stOverlayChn.u32Layer = 0;

    // qpInfo设置
    stRgnChnAttr.unChnAttr.stOverlayChn.stQpInfo.bEnable = RK_FALSE;
    stRgnChnAttr.unChnAttr.stOverlayChn.stQpInfo.bForceIntra = RK_TRUE;
    stRgnChnAttr.unChnAttr.stOverlayChn.stQpInfo.bAbsQp = RK_FALSE;
    stRgnChnAttr.unChnAttr.stOverlayChn.stQpInfo.s32Qp = RK_FALSE;

    stRgnChnAttr.unChnAttr.stOverlayChn.u32ColorLUT[0] = 0x00;
    stRgnChnAttr.unChnAttr.stOverlayChn.u32ColorLUT[1] = 0xFFFFFF;

    // 反色设置
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = RK_FALSE;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Width = 16;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Height = 16;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUM_THRESH;

    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.u32LumThresh = 100;
    s32Ret = RK_MPI_RGN_AttachToChn(RgnHandle, &stMppChn, &stRgnChnAttr);
    if (RK_SUCCESS != s32Ret)
    {
        RK_LOGE("RK_MPI_RGN_AttachToChn (%d) failed with %#x!", RgnHandle, s32Ret);
        return RK_FAILURE;
    }
    RK_LOGI("Display region to chn success!");

    return RK_SUCCESS;

    /*********************************************
        step 3: show bitmap
    *********************************************/

#if 0
    RK_S64 s64ShowBmpStart = TEST_COMM_GetNowUs();
    stBitmap.enPixelFormat = (PIXEL_FORMAT_E)RK_FMT_ARGB8888;
    stBitmap.u32Width = u32Width;
    stBitmap.u32Height = u32Height;

    RK_U16 ColorBlockSize = stBitmap.u32Height * stBitmap.u32Width;
    stBitmap.pData = malloc(ColorBlockSize * TEST_ARGB32_PIX_SIZE);
    RK_U8 *ColorData = (RK_U8 *)stBitmap.pData;

    if (filename)
    {
        s32Ret = load_file_osdmem(filename, stBitmap.pData, u32Width, u32Height, TEST_ARGB32_PIX_SIZE, 0);

        if (RK_SUCCESS != s32Ret)
        {
            set_argb8888_buffer((RK_U32 *)ColorData, ColorBlockSize / 4, TEST_ARGB32_RED);
            set_argb8888_buffer((RK_U32 *)(ColorData + ColorBlockSize), ColorBlockSize / 4, TEST_ARGB32_GREEN);
            set_argb8888_buffer((RK_U32 *)(ColorData + 2 * ColorBlockSize), ColorBlockSize / 4, TEST_ARGB32_BLUE);
            set_argb8888_buffer((RK_U32 *)(ColorData + 3 * ColorBlockSize), ColorBlockSize / 4, TEST_ARGB32_BLACK);
        }
    }

    s32Ret = RK_MPI_RGN_SetBitMap(RgnHandle, &stBitmap);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_RGN_SetBitMap failed with %#x!", s32Ret);
        return RK_FAILURE;
    }
    RK_S64 s64ShowBmpEnd = TEST_COMM_GetNowUs();
    RK_LOGI("Handle:%d, space time %lld us, load bmp success!", RgnHandle, s64ShowBmpEnd - s64ShowBmpStart);
#endif

    return 0;
}