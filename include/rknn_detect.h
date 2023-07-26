#ifndef _RKNN_DETECT_H_
#define _RKNN_DETECT_H_

void init_model(char *model_path);

int detect(unsigned char *input_data);

void quit_rknn(void);

#endif // DEBUG