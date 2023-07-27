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
#define BOX_THRESH 0.25
#define PROP_BOX_SIZE (5 + OBJ_CLASS_NUM)
#define MAX_RKNN_LIST_NUM 10

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