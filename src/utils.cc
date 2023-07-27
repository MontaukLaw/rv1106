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

int64_t get_current_time_ms(void)
{
    int64_t msec = 0;
    char str[20] = {0};
    struct timeval stuCurrentTime;

    gettimeofday(&stuCurrentTime, NULL);
    sprintf(str, "%ld%03ld", stuCurrentTime.tv_sec,
            (stuCurrentTime.tv_usec) / 1000);
    for (size_t i = 0; i < strlen(str); i++)
    {
        msec = msec * 10 + (str[i] - '0');
    }

    return msec;
}