#ifndef _RKNN_DETECT_H_
#define _RKNN_DETECT_H_

#include "postprocess.h"

int init_model(char *model_path);

int rknn_detect(unsigned char *input_data, detect_result_group_t *detect_result_group);

void quit_rknn(void);

void destory_rknn_list(rknn_list_t **s);

void rknn_list_push(rknn_list_t *s, long timeval, detect_result_group_t detect_result_group);

int rknn_list_size(rknn_list_t *s);

void rknn_list_drop(rknn_list_t *rknnList);

void create_rknn_list(rknn_list_t **s);

void rknn_list_pop(rknn_list_t* s, long* timeval, detect_result_group_t* detect_result_group);

#endif // DEBUG

