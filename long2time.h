#ifndef LONG2TIME_H
#define LONG2TIME_H

#include <utils/Log.h>

#define TAG "vclusters"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

#define debugprintf 1
#ifdef debugprintf
#define debugpri(mesg, args...) fprintf(stderr, "[NetRate print:%s:%d:] " mesg "\n", __FILE__, __LINE__, ##args)
#else
#define debugpri(mesg, args...)
#endif

#define INFO(...)            \
    do                       \
    {                        \
        printf(__VA_ARGS__); \
        printf("\n");        \
        LOGI(__VA_ARGS__);   \
    } while (0)

const char DEFAULT_NTP_SERVER[16] = "ntp1.aliyun.com";
unsigned char applib_dt_is_leap_year(unsigned short year);
unsigned char applib_dt_last_day_of_mon(unsigned char month, unsigned short year);
void change(long ts);
long getRtcTime();
int setRtcTime(long millis);

#endif