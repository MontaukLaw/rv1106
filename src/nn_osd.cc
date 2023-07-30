#include "comm.h"
#include "utils.h"

extern bool quitApp;
extern detect_result_group_t detectResultGroup;

static pthread_t get_nn_update_osd_thread_id;

static RK_U8 rgn_color_lut_0_left_value[4] = {0x03, 0xf, 0x3f, 0xff};
static RK_U8 rgn_color_lut_0_right_value[4] = {0xc0, 0xf0, 0xfc, 0xff};
static RK_U8 rgn_color_lut_1_left_value[4] = {0x02, 0xa, 0x2a, 0xaa};
static RK_U8 rgn_color_lut_1_right_value[4] = {0x80, 0xa0, 0xa8, 0xaa};
RK_S32 draw_rect_2bpp(RK_U8 *buffer, RK_U32 width, RK_U32 height, int rgn_x, int rgn_y, int rgn_w,
                      int rgn_h, int line_pixel, COLOR_INDEX_E color_index)
{
    int i;
    RK_U8 *ptr = buffer;
    RK_U8 value = 0;
    if (color_index == RGN_COLOR_LUT_INDEX_0)
        value = 0xff;
    if (color_index == RGN_COLOR_LUT_INDEX_1)
        value = 0xaa;

    if (line_pixel > 4)
    {
        printf("line_pixel > 4, not support\n", line_pixel);
        return -1;
    }

    // printf("YUV %dx%d, rgn (%d,%d,%d,%d), line pixel %d\n", width, height, rgn_x, rgn_y, rgn_w,
    // rgn_h, line_pixel); draw top line
    ptr += (width * rgn_y + rgn_x) >> 2;
    for (i = 0; i < line_pixel; i++)
    {
        memset(ptr, value, (rgn_w + 3) >> 2);
        ptr += width >> 2;
    }
    // draw letft/right line
    for (i = 0; i < (rgn_h - line_pixel * 2); i++)
    {
        if (color_index == RGN_COLOR_LUT_INDEX_1)
        {
            *ptr = rgn_color_lut_1_left_value[line_pixel - 1];
            *(ptr + ((rgn_w + 3) >> 2)) = rgn_color_lut_1_right_value[line_pixel - 1];
        }
        else
        {
            *ptr = rgn_color_lut_0_left_value[line_pixel - 1];
            *(ptr + ((rgn_w + 3) >> 2)) = rgn_color_lut_0_right_value[line_pixel - 1];
        }
        ptr += width >> 2;
    }
    // draw bottom line
    for (i = 0; i < line_pixel; i++)
    {
        memset(ptr, value, (rgn_w + 3) >> 2);
        ptr += width >> 2;
    }
    return 0;
}

static void *rkipc_get_nn_update_osd(void *arg)
{
    // LOG_DEBUG("#Start %s thread, arg:%p\n", __func__, arg);
    // prctl(PR_SET_NAME, "RkipcNpuOsd", 0, 0, 0);

    int ret = 0;
    int line_pixel = 4;
    int change_to_nothing_flag = 0;
    int video_width = 0;
    int video_height = 0;
    int rotation = 0;
    long long last_ba_result_time;
    RGN_HANDLE RgnHandle = DRAW_NN_OSD_ID;
    RGN_CANVAS_INFO_S stCanvasInfo;
    detect_result_t detect_object;

    memset(&stCanvasInfo, 0, sizeof(RGN_CANVAS_INFO_S));
    while (!quitApp)
    {
        usleep(40 * 1000);
        video_width = RTSP_INPUT_VI_WIDTH;
        video_height = RTSP_INPUT_VI_HEIGHT;

        // ret = rkipc_rknn_object_get(&ba_result);
        //
        // print("ret is %d, ba_result.objNum is %d\n", ret, ba_result.objNum);

        if (detectResultGroup.obj_num == 0)
        {
            printf("no rknn result\n");
            continue;
        }

        // printf("detectResultGroup.obj_num is %d\n", detectResultGroup.obj_num);

        ret = RK_MPI_RGN_GetCanvasInfo(RgnHandle, &stCanvasInfo);
        if (ret != RK_SUCCESS)
        {
            RK_LOGE("RK_MPI_RGN_GetCanvasInfo failed with %#x!", ret);
            continue;
        }
        if ((stCanvasInfo.stSize.u32Width != UPALIGNTO16(video_width)) ||
            (stCanvasInfo.stSize.u32Height != UPALIGNTO16(video_height)))
        {
            printf("canvas is %d*%d, not equal %d*%d, maybe in the process of switching,"
                   "skip this time\n",
                   stCanvasInfo.stSize.u32Width, stCanvasInfo.stSize.u32Height,
                   UPALIGNTO16(video_width), UPALIGNTO16(video_height));
            continue;
        }
        memset((void *)stCanvasInfo.u64VirAddr, 0,
               stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight >> 2);
        // draw
        for (int i = 0; i < detectResultGroup.obj_num; i++)
        {
            int x, y, w, h;
            // object = &ba_result.triggerObjects[i];
            detect_result_t detect_object = detectResultGroup.results[i];

            // LOG_INFO("topLeft:[%d,%d], bottomRight:[%d,%d],"
            // 			"objId is %d, frameId is %d, score is %d, type is %d\n",
            // 			object->objInfo.rect.topLeft.x, object->objInfo.rect.topLeft.y,
            // 			object->objInfo.rect.bottomRight.x,
            // 			object->objInfo.rect.bottomRight.y, object->objInfo.objId,
            // 			object->objInfo.frameId, object->objInfo.score, object->objInfo.type);
            // x = video_width * detect_object.box.left / 10000;
            x = detect_object.box.left * RTSP_INPUT_VI_WIDTH / RKNN_VI_WIDTH;
            y = (detect_object.box.top - DETECT_X_START) * RTSP_INPUT_VI_HEIGHT / RKNN_VI_HEIGHT;
            // y = video_height * object->objInfo.rect.topLeft.y / 10000;
            // w = video_width * (object->objInfo.rect.bottomRight.x - object->objInfo.rect.topLeft.x) / 10000;
            // h = video_height * (object->objInfo.rect.bottomRight.y - object->objInfo.rect.topLeft.y) / 10000;

            w = detect_object.box.right * RTSP_INPUT_VI_WIDTH / RKNN_VI_WIDTH - x;
            h = (detect_object.box.bottom - DETECT_X_START) * RTSP_INPUT_VI_HEIGHT / RKNN_VI_HEIGHT - y;

            x = x / 16 * 16;
            y = y / 16 * 16;
            w = (w + 3) / 16 * 16;
            h = (h + 3) / 16 * 16;

            while (x + w + line_pixel >= video_width)
            {
                w -= 8;
            }
            while (y + h + line_pixel >= video_height)
            {
                h -= 8;
            }
            if (x < 0 || y < 0 || w < 0 || h < 0)
            {
                continue;
            }
            // LOG_DEBUG("i is %d, x,y,w,h is %d,%d,%d,%d\n", i, x, y, w, h);
            draw_rect_2bpp((RK_U8 *)stCanvasInfo.u64VirAddr, stCanvasInfo.u32VirWidth,
                           stCanvasInfo.u32VirHeight, x, y, w, h, line_pixel,
                           RGN_COLOR_LUT_INDEX_0);
            // LOG_INFO("draw rect time-consuming is %lld\n",(rkipc_get_curren_time_ms() -
            // 	last_ba_result_time));
            // LOG_INFO("triggerRules is %d, ruleID is %d, triggerType is %d\n",
            // 			object->triggerRules,
            // 			object->firstTrigger.ruleID,
            // 			object->firstTrigger.triggerType);
        }
        ret = RK_MPI_RGN_UpdateCanvas(RgnHandle);
        if (ret != RK_SUCCESS)
        {
            RK_LOGE("RK_MPI_RGN_UpdateCanvas failed with %#x!", ret);
            continue;
        }
    }

    return NULL;
}

int rkipc_osd_draw_nn_init(void)
{
    printf("start\n");
    int ret = 0;
    RGN_HANDLE RgnHandle = DRAW_NN_OSD_ID;
    RGN_ATTR_S stRgnAttr;
    MPP_CHN_S stMppChn;
    RGN_CHN_ATTR_S stRgnChnAttr;
    BITMAP_S stBitmap;
    int rotation = 0;

    // create overlay regions
    memset(&stRgnAttr, 0, sizeof(stRgnAttr));
    stRgnAttr.enType = OVERLAY_RGN;
    stRgnAttr.unAttr.stOverlay.enPixelFmt = RK_FMT_2BPP;
    // stRgnAttr.unAttr.stOverlay.u32CanvasNum = 1;
    stRgnAttr.unAttr.stOverlay.stSize.u32Width = 1920;
    stRgnAttr.unAttr.stOverlay.stSize.u32Height = 1080;

    ret = RK_MPI_RGN_Create(RgnHandle, &stRgnAttr);
    if (RK_SUCCESS != ret)
    {
        printf("RK_MPI_RGN_Create (%d) failed with %#x\n", RgnHandle, ret);
        RK_MPI_RGN_Destroy(RgnHandle);
        return RK_FAILURE;
    }
    printf("The handle: %d, create success\n", RgnHandle);
    // after malloc max size, it needs to be set to the actual size
    stRgnAttr.unAttr.stOverlay.stSize.u32Width = 1920;
    stRgnAttr.unAttr.stOverlay.stSize.u32Height = 1080;

    ret = RK_MPI_RGN_SetAttr(RgnHandle, &stRgnAttr);
    if (RK_SUCCESS != ret)
    {
        printf("RK_MPI_RGN_SetAttr (%d) failed with %#x!", RgnHandle, ret);
        return RK_FAILURE;
    }

    // display overlay regions to venc groups
    memset(&stRgnChnAttr, 0, sizeof(stRgnChnAttr));
    stRgnChnAttr.bShow = RK_TRUE;
    stRgnChnAttr.enType = OVERLAY_RGN;
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 0;
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = 0;
    stRgnChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 0;
    stRgnChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 255;
    stRgnChnAttr.unChnAttr.stOverlayChn.u32Layer = 2; // DRAW_NN_OSD_ID;
    stRgnChnAttr.unChnAttr.stOverlayChn.u32ColorLUT[RGN_COLOR_LUT_INDEX_0] = BLUE_COLOR;
    stRgnChnAttr.unChnAttr.stOverlayChn.u32ColorLUT[RGN_COLOR_LUT_INDEX_1] = RED_COLOR;

    stMppChn.enModId = RK_ID_VENC;
    stMppChn.s32DevId = 0;
    stMppChn.s32ChnId = VENC_CHN_0;
    ret = RK_MPI_RGN_AttachToChn(RgnHandle, &stMppChn, &stRgnChnAttr);
    if (RK_SUCCESS != ret)
    {
        printf("RK_MPI_RGN_AttachToChn (%d) to venc0 failed with %#x\n", RgnHandle, ret);
        return RK_FAILURE;
    }
    printf("RK_MPI_RGN_AttachToChn to venc0 success\n");
    pthread_create(&get_nn_update_osd_thread_id, NULL, rkipc_get_nn_update_osd, NULL);
    printf("end\n");

    return ret;
}