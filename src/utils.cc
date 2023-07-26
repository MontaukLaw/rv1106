#include <sys/time.h>
#include <stdint.h>
#include "utils.h"
#include "rknn_api.h"
#include "stdio.h"
#include <string.h>
#include "utils.h"
#include <float.h>
#include <stdlib.h>

int64_t getCurrentTimeUs(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}