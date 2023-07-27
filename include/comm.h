#ifndef __COMM_H__
#define __COMM_H__

#ifdef __cplusplus
extern "C"
{
#endif

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

#ifdef __cplusplus
}
#endif

#endif