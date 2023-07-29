#ifndef __COMM_H__
#define __COMM_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>

#include "rk_debug.h"
#include "rk_defines.h"
#include "rk_mpi_adec.h"
#include "rk_mpi_aenc.h"
#include "rk_mpi_ai.h"
#include "rk_mpi_ao.h"
#include "rk_mpi_avs.h"
#include "rk_mpi_cal.h"
#include "rk_mpi_ivs.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_rgn.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_tde.h"
#include "rk_mpi_vdec.h"
#include "rk_mpi_venc.h"
#include "rk_mpi_vi.h"
#include "rk_mpi_vo.h"
#include "rk_mpi_vpss.h"
#include "loadbmp.h"

#define VENC_RECORD_TIME_OSD_HANDLE 0
#define DRAW_NN_OSD_ID 0
#define OBJ_NAME_MAX_SIZE 16
#define OBJ_NUMB_MAX_SIZE 64
#define OBJ_CLASS_NUM 80
#define NMS_THRESH 0.45
#define BOX_THRESH 0.35
#define PROP_BOX_SIZE (5 + OBJ_CLASS_NUM)
#define MAX_RKNN_LIST_NUM 10

#define RTSP_INPUT_VI_WIDTH 1920
#define RTSP_INPUT_VI_HEIGHT 1080

#define RKNN_VI_WIDTH 640
#define RKNN_VI_HEIGHT 360
#define MODLE_WIDTH 640

// 140
#define DETECT_X_START ((MODLE_WIDTH - RKNN_VI_HEIGHT) / 2)

#define VI_CHN_0 0
#define VENC_CHN_0 0
#define VI_CHN_1 1
#define VPSS_CHN_0 0
#define VPSS_GRP_0 0
#define SEND_FRAME_TIMEOUT 2000
#define GET_FRAME_TIMEOUT 2000

#define RTSP_INPUT_VI_WIDTH 1920
#define RTSP_INPUT_VI_HEIGHT 1080

#define HI_U16 RK_U16
#define HI_U32 RK_U32
#define HI_U8 RK_U8
#define HI_S32 RK_S32
#define HI_SUCCESS RK_SUCCESS
#define HI_TRUE RK_TRUE
#define HI_BOOL RK_BOOL
#define HI_FAILURE RK_FAILURE

    typedef struct _BOX_RECT
    {
        int left;
        int right;
        int top;
        int bottom;
    } BOX_RECT;

    typedef struct __detect_result_t
    {
        char name[OBJ_NAME_MAX_SIZE];
        BOX_RECT box;
        float prop;
    } detect_result_t;

    typedef struct _detect_result_group_t
    {
        int id;
        int count;
        detect_result_t results[OBJ_NUMB_MAX_SIZE];
    } detect_result_group_t;

    typedef struct node
    {
        long timeval;
        detect_result_group_t detect_result_group;
        struct node *next;
    } Node;

    typedef struct my_stack
    {
        int size;
        Node *top;
    } rknn_list_t;

    RK_S32 bind_rgn_to_venc(void);
    RK_VOID *add_ts_thread(RK_VOID *p);

#ifdef __cplusplus
}
#endif

#endif