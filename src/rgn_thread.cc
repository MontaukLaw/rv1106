
#include "comm.h"
#define TIME_STAMP_WATER_MARK_STR_LEN 19
#define FIRST_DASH_POS 4
#define SECOND_DASH_POS 7
#define FIRST_COLON_POS 13
#define SECOND_COLON_POS 16
#define SPACE_POST 10
#define SPACE_WIDTH 16
#define WATER_MARK_ALPHA 200

extern bool quitApp;
// 这个文件里面很多东西是从海思移植过来的, 因为RK的RGN连函数名字都是抄袭海思的, 所以直接拿过来改个函数名就能用
extern OSD_COMP_INFO s_OSDCompInfo[];

char *NUMBER_PICS[10] = {
    "/userdata/res/0_1080p.bmp",
    "/userdata/res/1_1080p.bmp",
    "/userdata/res/2_1080p.bmp",
    "/userdata/res/3_1080p.bmp",
    "/userdata/res/4_1080p.bmp",
    "/userdata/res/5_1080p.bmp",
    "/userdata/res/6_1080p.bmp",
    "/userdata/res/7_1080p.bmp",
    "/userdata/res/8_1080p.bmp",
    "/userdata/res/9_1080p.bmp",
};


// 通过数字获取对应的图片
char *RGN_GetBMPSrc(int num)
{
    char *file = "/userdata/res/0_1080p.bmp";

    switch (num)
    {
    case 0:
        file = "/userdata/res/0_1080p.bmp";
        break;
    case 1:
        file = "/userdata/res/1_1080p.bmp";
        break;
    case 2:
        file = "/userdata/res/2_1080p.bmp";
        break;
    case 3:
        file = "/userdata/res/3_1080p.bmp";
        break;
    case 4:
        file = "/userdata/res/4_1080p.bmp";
        break;
    case 5:
        file = "/userdata/res/5_1080p.bmp";
        break;
    case 6:
        file = "/userdata/res/6_1080p.bmp";
        break;
    case 7:
        file = "/userdata/res/7_1080p.bmp";
        break;
    case 8:
        file = "/userdata/res/8_1080p.bmp";
        break;
    case 9:
        file = "/userdata/res/9_1080p.bmp";
        break;
    default:
        file = "/userdata/res/0_1080p.bmp";
        break;
    }

    return file;
}

// 根据时间数字获取对应的图片
char *get_number_bmp_filename(HI_U16 idx, struct tm *pLocalTime)
{
    char *filename = NULL;

    switch (idx) // YYYY-MM-DD    HH:MM:SS
    {
    // YYYY
    case 0:
        filename = NUMBER_PICS[(1900 + pLocalTime->tm_year) / 1000]; // 2018
        break;
    case 1:
        filename = NUMBER_PICS[((1900 + pLocalTime->tm_year) / 100) % 10];
        break;
    case 2:
        filename = NUMBER_PICS[((1900 + pLocalTime->tm_year) % 100) / 10];
        break;
    case 3:
        filename = NUMBER_PICS[(1900 + pLocalTime->tm_year) % 10];
        break;
    // MM
    case 5:
        filename = NUMBER_PICS[(1 + pLocalTime->tm_mon) / 10];
        break;
    case 6:
        filename = NUMBER_PICS[(1 + pLocalTime->tm_mon) % 10];
        break;
    // DD
    case 8:
        filename = NUMBER_PICS[(pLocalTime->tm_mday) / 10];
        break;
    case 9:
        filename = NUMBER_PICS[(pLocalTime->tm_mday) % 10];
        break;

    // HH
    case 11:
        filename = NUMBER_PICS[(pLocalTime->tm_hour) / 10];
        break;
    case 12:
        filename = NUMBER_PICS[(pLocalTime->tm_hour) % 10];
        break;
    // MM
    case 14:
        filename = NUMBER_PICS[(pLocalTime->tm_min) / 10];
        break;
    case 15:
        filename = NUMBER_PICS[(pLocalTime->tm_min) % 10];
        break;
    // SS
    case 17:
        filename = NUMBER_PICS[(pLocalTime->tm_sec) / 10];
        break;
    case 18:
        filename = NUMBER_PICS[(pLocalTime->tm_sec) % 10];
        break;
    }

    return filename;
}

// wasted
HI_U16 OSD_MAKECOLOR_U16(HI_U8 r, HI_U8 g, HI_U8 b, OSD_COMP_INFO compinfo)
{
    HI_U8 r1, g1, b1;
    HI_U16 pixel = 0;
    HI_U32 tmp = 15;

    r1 = g1 = b1 = 0;
    r1 = r >> (8 - compinfo.rlen);
    g1 = g >> (8 - compinfo.glen);
    b1 = b >> (8 - compinfo.blen);
    while (compinfo.alen)
    {
        pixel |= (1 << tmp);
        tmp--;
        compinfo.alen--;
    }

    pixel |= (r1 | (g1 << compinfo.blen) | (b1 << (compinfo.blen + compinfo.glen)));
    return pixel;
}

void fill_bitmap_buf(HI_U8 *pRGBBuf, HI_U16 bytesPerPixel, HI_U32 bmpWidth, HI_U32 bmpHeight, HI_U8 *pStart, HI_U16 *pDst,
                     OSD_LOGO_T *pVideoLogo, HI_U8 *pOrigBMPBuf, HI_U32 stride)
{
    const static HI_U8 spa = 16;
    HI_U16 i, j;

    // HI_U8 r, g, b;
    // printf("fill_bitmap_buf >>>>>>>>>>>>>>>>>>>>>>  bmpWidth:%d, bmpHeight:%d, stride:%d\n", bmpWidth, bmpHeight, stride);
    // printf("pRGBBuf: %p, pOrigBMPBuf: %p\n", pRGBBuf, pOrigBMPBuf);

    for (i = 0; i < bmpHeight; i++) // Bpp:3,enFmt:4
    {
        // printf("k:%d,i:%d,j:%d,h:%d,w:%d \n", k, i, j, h, w);
        for (j = 0; j < bmpWidth; j++)
        {
            memcpy(pRGBBuf + i * pVideoLogo->stride + j * 4, pOrigBMPBuf + ((bmpHeight - 1) - i) * stride + j * bytesPerPixel, bytesPerPixel);
            *(pRGBBuf + i * pVideoLogo->stride + j * 4 + 3) = WATER_MARK_ALPHA; /*alpha*/
            // 如果是白色, 就改成透明色
            if (0xff == *(pRGBBuf + i * pVideoLogo->stride + j * 4) && 0xff == *(pRGBBuf + i * pVideoLogo->stride + j * 4 + 1) && 0xff == *(pRGBBuf + i * pVideoLogo->stride + j * 4 + 2))
            {
                *(pRGBBuf + i * pVideoLogo->stride + j * 4 + 3) = 0x00; /*alpha*/
            }
        }
    }
}

/// @brief 是否支持对应的bmp格式
/// @param bmpInfo
/// @return
bool if_support_bmp(OSD_BITMAPINFO bmpInfo)
{
    HI_U16 Bpp;

    Bpp = bmpInfo.bmiHeader.biBitCount / 8;
    if (Bpp < 2)
    {
        /* only support 1555 8888 888 bitmap */
        printf("bitmap format not supported!\n");
        return -1;
    }

    if (bmpInfo.bmiHeader.biCompression != 0)
    {
        printf("not support compressed bitmap file!\n");
        return -1;
    }

    if (bmpInfo.bmiHeader.biHeight < 0)
    {
        printf("bmpInfo.bmiHeader.biHeight < 0\n");
        return -1;
    }

    return 0;
}

void print_time(tm *pLocalTime)
{
    printf("pLocalTime->tm_year:%d\n", pLocalTime->tm_year + 1900);
    printf("pLocalTime->tm_mon:%d\n", pLocalTime->tm_mon + 1);
    printf("pLocalTime->tm_mday:%d\n", pLocalTime->tm_mday);
    printf("pLocalTime->tm_hour:%d\n", pLocalTime->tm_hour);
    printf("pLocalTime->tm_min:%d\n", pLocalTime->tm_min);
    printf("pLocalTime->tm_sec:%d\n", pLocalTime->tm_sec);
}

char *get_filename_by_idx(HI_U16 charIdx, tm *pLocalTime)
{
    char *filename = NULL;
    // 画"-""
    if (charIdx == FIRST_DASH_POS || charIdx == SECOND_DASH_POS)
    {
        filename = "/userdata/res/-_1080p.bmp";
    }
    // 画":"
    else if (charIdx == FIRST_COLON_POS || charIdx == SECOND_COLON_POS)
    {
        filename = "/userdata/res/colon_1080p.bmp";
    }
    // 画空格
    else if (charIdx == SPACE_POST)
    {
        filename = "/userdata/res/blank_1080p.bmp";
    }
    else
    {
        // 获取图片文件名
        filename = get_number_bmp_filename(charIdx, pLocalTime);
    }
    return filename;
}

HI_U32 get_bmp_stride(HI_U32 bmpWidth, HI_U16 bytesPerPixel)
{
    HI_U32 bmpStride;
    bmpStride = bmpWidth * bytesPerPixel; // Bpp:3, enFmt:4

    if (bmpStride % 4)
    {
        bmpStride = (bmpStride & 0xfffc) + 4;
    }
    return bmpStride;
}

// 通过数字获取对应的图片
int RGN_LoadBMPCanvas_TimeStamp(OSD_LOGO_T *pVideoLogo)
{
    // char filename[32] = {0};
    FILE *pFile;
    HI_U16 i, j, charIdx;

    HI_U32 bmpWidth, bmpHeight;
    HI_U16 bytesPerPixel = 3; // 表示每像素字符数

    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    HI_U8 *pOrigBMPBuf; // 保存bmp文件数据
    HI_U8 *pRGBBuf;
    HI_U32 bmpStride;
    HI_U8 r, g, b;
    HI_U8 *pStart;
    HI_U16 *pDst;

    time_t timep;
    struct tm *pLocalTime;
    time(&timep);

    pLocalTime = localtime(&timep);

    char *filename;

    // 拿到rgn的虚拟内存地址
    pRGBBuf = pVideoLogo->pRGBBuffer;

    // YYYY-MM-DD HH:MM:SS
    // 图片尺寸16*24 因为有18个字符, 所以总宽度是16*18=288
    for (charIdx = 0; charIdx < TIME_STAMP_WATER_MARK_STR_LEN; charIdx++)
    {

        filename = get_filename_by_idx(charIdx, pLocalTime);
        if (NULL == filename)
        {
            printf("OSD_LoadBMP: filename=NULL\n");
            return -1;
        }

        // printf("bmp filename: is %s \n", filename);
        // 获取bmp图片信息, bmp文件头和bmp信息头
        if (get_bmp_info(filename, &bmpFileHeader, &bmpInfo) < 0)
        {
            printf("get_bmp_info error!\n");
            return -1;
        }

        if (if_support_bmp(bmpInfo) < 0)
        {
            printf("if_support_bmp error!\n");
            return -1;
        }

        if ((pFile = fopen((char *)filename, "rb")) == NULL)
        {
            printf("Open file faild:%s!\n", filename);
            return -1;
        }

        bmpWidth = (HI_U16)bmpInfo.bmiHeader.biWidth;
        bmpHeight = (HI_U16)((bmpInfo.bmiHeader.biHeight > 0) ? bmpInfo.bmiHeader.biHeight : (-bmpInfo.bmiHeader.biHeight));

        bmpStride = get_bmp_stride(bmpWidth, bytesPerPixel);
        // printf("filename: %s stride: %d w: %d, h: %d \n", filename, stride, w, h);
        RK_U32 bmpPicSize = bmpHeight * bmpStride;

        /* RGB8888 or RGB1555 */
        // 申请内存用于保存每个bmp原始图像数据, 即4*长宽高
        pOrigBMPBuf = (HI_U8 *)malloc(bmpPicSize);
        if (NULL == pOrigBMPBuf)
        {
            printf("not enough memory to malloc!\n");
            fclose(pFile);
            return -1;
        }

        // printf("pVideoLogo->height:%d, pVideoLogo->width:%d\n", pVideoLogo->height, pVideoLogo->width);

        // 从bmp文件的偏移位置开始读取数据
        // 把bmp图片文件数据转成RGB数据, 因为bmp文件有包头,颜色索引之类的若干(可能54byte)内容, 所以要跳过
        fseek(pFile, bmpFileHeader.bfOffBits, 0);
        // 将每张图片数据读到pOrigBMPBuf里面
        if (fread(pOrigBMPBuf, 1, bmpPicSize, pFile) != (bmpPicSize))
        {
            perror("fread error!");
            goto _FAILED;
        }
        // printf("read bmp success \n");

        // HI_U8 *pRGBBuf, HI_U16 Bpp, HI_U32 w, HI_U32 h, OSD_COLOR_FMT_E enFmt,
        // HI_U8 *pStart, HI_U16 *pDst, OSD_LOGO_T *pVideoLogo, HI_U8 *pOrigBMPBuf,
        // HI_U32 stride
        // fill_bitmap_buf(, bytesPerPixel, bmpWidth, bmpHeight, pStart, pDst, pVideoLogo, pRGBBuf, bmpStride, charIdx);
        fill_bitmap_buf(pRGBBuf, bytesPerPixel, bmpWidth, bmpHeight, pStart, pDst, pVideoLogo, pOrigBMPBuf, bmpStride);
        pRGBBuf += bmpWidth * 4;

#if 0       
        // Bpp:3,enFmt:4
        for (i = 0; i < h; i++)
        {
            // printf("k:%d,i:%d,j:%d,h:%d,w:%d \n", k, i, j, h, w);
            for (j = 0; j < w; j++)
            {
                if (Bpp == 3) /*.....*/
                {
                    switch (enFmt)
                    {
                    case OSD_COLOR_FMT_RGB444:
                    case OSD_COLOR_FMT_RGB555:
                    case OSD_COLOR_FMT_RGB565:
                    case OSD_COLOR_FMT_RGB4444:
                        /* start color convert */
                        // h:48,w:32,stride:96  Canvas_h:48,Can_w:704,Can_Str:1408
                        pStart = pOrigBMPBuf + ((h - 1) - i) * stride + j * Bpp;

                        // pDst = (HI_U16*)(pRGBBuf + i * pVideoLogo->stride + j * 2 + k*w*2);
                        //  YYYY-MM-DD HH:MM:SS  k==4 || k==7 || k==13 || k==16

                        if (k < 4)
                        { // YYYY-MM-DD
                            pDst = (HI_U16 *)(pRGBBuf + i * pVideoLogo->stride + j * 2 + k * w * 2);
                        }
                        else if (k == 4)
                        {
                            pDst = (HI_U16 *)(pRGBBuf + i * pVideoLogo->stride + j * 2 + k * 32);
                        }
                        else if (k > 4 && k < 7)
                        {
                            pDst = (HI_U16 *)(pRGBBuf + i * pVideoLogo->stride + j * 2 + ((k - 1) * 32 + 16));
                        }
                        else if (k == 7)
                        {
                            pDst = (HI_U16 *)(pRGBBuf + i * pVideoLogo->stride + j * 2 + (6 * 32 + 16));
                        }
                        else if (k > 7 && k < 10)
                        {
                            pDst = (HI_U16 *)(pRGBBuf + i * pVideoLogo->stride + j * 2 + (k - 1) * 32);
                        }
                        else if (k == 10 || k == 11 || k == 12)
                        { // HH:**:**
                            pDst = (HI_U16 *)(pRGBBuf + i * pVideoLogo->stride + j * 2 + ((k - 1) * 32 + spa));
                        }
                        else if (k == 13 || k == 14 || k == 15)
                        { //***HH:**
                            pDst = (HI_U16 *)(pRGBBuf + i * pVideoLogo->stride + j * 2 + ((k - 2) * 32 + spa + 16));
                        }
                        else if (k == 16 || k == 17)
                        { //***HH:**
                            pDst = (HI_U16 *)(pRGBBuf + i * pVideoLogo->stride + j * 2 + ((k - 2) * 32 + spa));
                        }

                        r = *(pStart);
                        g = *(pStart + 1);
                        b = *(pStart + 2);
                        // printf("Func: %s, line:%d, Bpp: %d, bmp stride: %d, Canvas stride: %d, h:%d, w:%d.\n",
                        //     __FUNCTION__, __LINE__, Bpp, stride, pVideoLogo->stride, i, j);
                        *pDst = OSD_MAKECOLOR_U16(r, g, b, s_OSDCompInfo[enFmt]);

                        break;

                    case OSD_COLOR_FMT_RGB888:
                    case OSD_COLOR_FMT_ARGB8888:
                        memcpy(pRGBBuf + i * pVideoLogo->stride + j * 4, pOrigBMPBuf + ((h - 1) - i) * stride + j * Bpp, Bpp);
                        *(pRGBBuf + i * pVideoLogo->stride + j * 4 + 3) = 0xff; /*alpha*/
                        break;

                    default:
                        printf("file(%s), line(%d), no such format!\n", __FILE__, __LINE__);
                        break;
                    }
                }
                else if ((Bpp == 2) || (Bpp == 4)) /*..............*/
                {
                    memcpy(pRGBBuf + i * pVideoLogo->stride + j * Bpp, pOrigBMPBuf + ((h - 1) - i) * stride + j * Bpp, Bpp);
                }
            }
        }
#endif

    _FAILED:

        free(pOrigBMPBuf);
        pOrigBMPBuf = NULL;

        fclose(pFile);
    }

    return 0;
}

// 加载若干bmp图片文件, 并转成RGB数据
// int load_canvas(OSD_LOGO_T *pVideoLogo)
// {
//     int nRet = 0;
//     nRet = RGN_LoadBMPCanvas_TimeStamp(pVideoLogo);
//     if (nRet != HI_SUCCESS)
//     {
//         printf("OSD_LoadBMP error!\n");
//         return -1;
//     }

//     // printf("pVideoLogo->width:%d, pVideoLogo->height:%d, pVideoLogo->stride:%d\n", pVideoLogo->width, pVideoLogo->height, pVideoLogo->stride);

//     return 0;
// }

HI_S32 create_surface_by_canvas(OSD_SURFACE_S *pstSurface, HI_U8 *pu8Virt, HI_U32 u32Width, HI_U32 u32Height)
{
    OSD_LOGO_T stLogo;
    memset(&stLogo, 0, sizeof(OSD_LOGO_T));
    stLogo.pRGBBuffer = pu8Virt;
    stLogo.width = u32Width;
    stLogo.height = u32Height;

    printf("create_surface_by_canvas >>>>>>>>>>>>>>>>>>>>>> u32Width:%d, u32Height:%d\n", u32Width, u32Height);

    // printf("pVideoLogo->width:%d, pVideoLogo->height:%d, pVideoLogo->stride:%d\n", pVideoLogo->width, pVideoLogo->height, pVideoLogo->stride);

    // stLogo.stride = u32Stride;
    // if (load_canvas(&stLogo) < 0)
    // {
    //     printf("load bmp error!\n");
    //     return -1;
    // }

    pstSurface->u16Height = u32Height;
    pstSurface->u16Width = u32Width;
    // pstSurface->u16Stride = u32Stride;

    return 0;
}

// fillColor就是canvas在没有被覆盖的区域的颜色
// pstBitmap是canvas的虚拟内存所在位置
static HI_S32 update_canvas(BITMAP_S *pstBitmap, HI_U32 u16FilColor, RK_U32 canvasHeight, RK_U32 canvasWidth)
{
    HI_S32 nRet = 0;
    OSD_SURFACE_S surface;
    OSD_LOGO_T stLogo;
    if (NULL == pstBitmap->pData)
    {
        printf("malloc osd memroy err!\n");
        return HI_FAILURE;
    }

    memset(&stLogo, 0, sizeof(OSD_LOGO_T));
    stLogo.pRGBBuffer = (RK_U8 *)pstBitmap->pData;
    stLogo.width = canvasWidth;
    stLogo.height = canvasHeight;
    // BRGBA8888每像素字节数为4
    stLogo.stride = canvasWidth * 4;

    // printf("create_surface_by_canvas >>>>>>>>>>>>>>>>>>>>>> u32Width:%d, u32Height:%d\n", canvasWidth, canvasHeight);
    // printf("pstBitmap->u32Width:%d, pstBitmap->u32Height:%d, pstBitmap->enPixelFormat:%d\n", pstBitmap->u32Width, pstBitmap->u32Height, pstBitmap->enPixelFormat);
    // printf("stLogo.width: %d, stLogo.height: %d\n", stLogo.width, stLogo.height);

    nRet = RGN_LoadBMPCanvas_TimeStamp(&stLogo);
    if (nRet != HI_SUCCESS)
    {
        printf("OSD_LoadBMP error!\n");
        return -1;
    }

    // printf("pVideoLogo->width:%d, pVideoLogo->height:%d, pVideoLogo->stride:%d\n", pVideoLogo->width, pVideoLogo->height, pVideoLogo->stride);

    // stLogo.stride = u32Stride;
    // if (load_canvas(&stLogo) < 0)
    // {
    //     printf("load bmp error!\n");
    //     return -1;
    // }

    surface.u16Height = canvasHeight;
    surface.u16Width = canvasWidth;
    // create_surface_by_canvas(&surface, (HI_U8 *)(pstBitmap->pData), canvasWidth, canvasHeight);

    pstBitmap->u32Width = surface.u16Width;
    pstBitmap->u32Height = surface.u16Height;
    // pstBitmap->enPixelFormat = enPixelFmt;

    int i = 0, j = 0;
    HI_U16 *pu16Temp = NULL;
    pu16Temp = (HI_U16 *)pstBitmap->pData;

// 针对15555格式的图片, 改成黑白色改成透明色
#if 0
    // printf("@@@@----H:%d, W:%d \n", pstBitmap->u32Height,pstBitmap->u32Width);
    for (i = 0; i < pstBitmap->u32Height; i++)
    {
        for (j = 0; j < pstBitmap->u32Width; j++)
        {
            // printf("(%d,%d): %04x!\n", i,j,*pu16Temp);

            // if (u16FilColor == *pu16Temp)
            if (0x0000 == *pu16Temp || 0xFFFF == *pu16Temp) // TODO fixed value, Fun para not work
            {
                *pu16Temp &= 0x7FFF;
            }

            pu16Temp++;
        }
    }
#endif

    return HI_SUCCESS;
}

void canvas_drawing(void)
{

    HI_S32 s32Ret = HI_SUCCESS;
    RGN_CANVAS_INFO_S stCanvasInfo;
    memset(&stCanvasInfo, 0, sizeof(RGN_CANVAS_INFO_S));

    s32Ret = RK_MPI_RGN_GetCanvasInfo(VENC_RECORD_TIME_OSD_HANDLE, &stCanvasInfo);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_RGN_GetCanvasInfo failed with %#x!", s32Ret);
        return;
    }

    RK_U64 canvasSize = (RK_U64)stCanvasInfo.u32VirWidth * (RK_U64)stCanvasInfo.u32VirHeight * 4;
    // printf("canvasSize: %ld\n", canvasSize);

    // draw_rect_2bpp((RK_U8 *)stCanvasInfo.u64VirAddr, stCanvasInfo.stSize.u32Width,
    // stCanvasInfo.stSize.u32Height, 0, 0, 10, 10, 2, RGN_COLOR_LUT_INDEX_1);
    printf("u32Width:%d,u32Height:%d canvasSize: %d\n", stCanvasInfo.stSize.u32Width, stCanvasInfo.stSize.u32Height, canvasSize);
    memset(reinterpret_cast<void *>(stCanvasInfo.u64VirAddr), 0xff, stCanvasInfo.stSize.u32Width * stCanvasInfo.stSize.u32Height * 4);

    // memset(reinterpret_cast<void *>(stCanvasInfo.u64VirAddr), 0xff, stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight >> 2);

    s32Ret = RK_MPI_RGN_UpdateCanvas(VENC_RECORD_TIME_OSD_HANDLE);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_RGN_UpdateCanvas failed with %#x!", s32Ret);
        return;
    }
}

HI_S32 rgn_add(unsigned int Handle)
{
    // printf("-------------------%s add rgn %d --------------------\n",__func__,Type);

    HI_S32 s32Ret = HI_SUCCESS;
    RGN_ATTR_S stRgnAttrSet;
    RGN_CANVAS_INFO_S stCanvasInfo;
    memset(&stRgnAttrSet, 0, sizeof(RGN_ATTR_S));

    BITMAP_S stBitmap;
    memset(&stBitmap, 0, sizeof(BITMAP_S));
    SIZE_S stSize;

    /* Photo logo */
    s32Ret = RK_MPI_RGN_GetAttr(Handle /*VencOsdHandle*/, &stRgnAttrSet);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_RGN_GetAttr failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }

    s32Ret = RK_MPI_RGN_GetCanvasInfo(Handle /*VencOsdHandle*/, &stCanvasInfo);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_RGN_GetCanvasInfo failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }

    stBitmap.pData = reinterpret_cast<void *>(stCanvasInfo.u64VirAddr);
    // stBitmap.pData = (void *)stCanvasInfo.u64VirAddr; // u64VirAddr
    stSize.u32Width = stCanvasInfo.stSize.u32Width;
    stSize.u32Height = stCanvasInfo.stSize.u32Height;

    RK_U32 canvasHeight = stCanvasInfo.stSize.u32Height;
    RK_U32 canvasWidth = stCanvasInfo.stSize.u32Width;

    // printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>> canvas.u32Width:%d canvas.u32Height:%d \n", stSize.u32Width, stSize.u32Height);
    s32Ret = update_canvas(&stBitmap, 0x0000, canvasHeight, canvasWidth);
    // s32Ret = update_canvas(Type, &stBitmap, HI_TRUE, 0x0000, &stSize, stCanvasInfo.u32Stride, stRgnAttrSet.unAttr.stOverlayEx.enPixelFmt);

    if (HI_SUCCESS != s32Ret)
    {
        printf("SAMPLE_RGN_UpdateCanvas failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }

    s32Ret = RK_MPI_RGN_UpdateCanvas(Handle /*VencOsdHandle*/);
    if (HI_SUCCESS != s32Ret)
    {
        printf("RK_MPI_RGN_UpdateCanvas failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }

    return HI_SUCCESS;
}

RK_VOID *add_ts_thread(RK_VOID *p)
{
    RK_S32 s32Ret = RK_SUCCESS;
    time_t timep;
    struct tm *pLocalTime;
    RK_U8 seconds = 80; // just for make difference for first load

    // RGN_HANDLE Handle;
    // Handle = VENC_RECORD_TIME_OSD_HANDLE;
    // s32Ret = rgn_add(Handle, VENC_RECORD_TIME_OSD_HANDLE);
    while (!quitApp)
    {
        time(&timep);
        pLocalTime = localtime(&timep);
        if (seconds == pLocalTime->tm_sec)
        {
            usleep(100 * 1000);
            // usleep(150 * 1000);
            continue;
        }
        else
        {
            seconds = pLocalTime->tm_sec;
        }
        // 每秒刷新
        // printf(" >>>>>>>>>>>>>>>>>>>>>>>>> adding rgn\n");
        // canvas_drawing();
        s32Ret = rgn_add(VENC_RECORD_TIME_OSD_HANDLE);
        if (RK_SUCCESS != s32Ret)
        {
            printf("RGN_Add line %d  failed! s32Ret: 0x%x.\n", __LINE__, s32Ret);
            break;
        }
    }

    // pthread_detach(pthread_self());

    return NULL;
}
