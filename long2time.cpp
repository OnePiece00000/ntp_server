#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <cutils/properties.h>
#include "long2time.h"

#define UTC_BASE_YEAR 1970
#define MONTH_PER_YEAR 12
#define DAY_PER_YEAR 365
#define SEC_PER_DAY 86400
#define SEC_PER_HOUR 3600
#define SEC_PER_MIN 60

/* 每个月的天数 */
const unsigned char g_day_per_mon[MONTH_PER_YEAR] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/*
 * 功能：
 *     判断是否是闰年
 * 参数：
 *     year：需要判断的年份数
 *
 * 返回值：
 *     闰年返回1，否则返回0
 */
unsigned char applib_dt_is_leap_year(unsigned short year)
{

    if ((year % 400) == 0)
    {
        return 1;
    }
    else if ((year % 100) == 0)
    {
        return 0;
    }
    else if ((year % 4) == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/*
 * 功能：
 *     得到每个月有多少天
 * 参数：
 *     month：需要得到天数的月份数
 *     year：该月所对应的年份数
 *
 * 返回值：
 *     该月有多少天
 *
 */
unsigned char applib_dt_last_day_of_mon(unsigned char month, unsigned short year)
{
    if ((month == 0) || (month > 12))
    {
        return g_day_per_mon[1] + applib_dt_is_leap_year(year);
    }

    if (month != 2)
    {
        return g_day_per_mon[month - 1];
    }
    else
    {
        return g_day_per_mon[1] + applib_dt_is_leap_year(year);
    }
}

long getRtcTime()
{
    int fd, size;
    char buffer[50];
    int hour, minunte, second;
    long time;
    fd = open("/sys/class/rtc/rtc0/time", O_RDONLY);
    if (fd < 0)
    {
        INFO("open /sys/class/rtc/rtc0/time failed");
    }
    size = read(fd, buffer, sizeof(buffer));
    close(fd);
    sscanf(buffer, "%d:%d:%d", &hour, &minunte, &second);
    time = hour * 3600 + minunte * 60 + second;
    //INFO("hour = %d minunte = %d second = %d time = %ld",hour,minunte,second,time);
    return time;
}

int setRtcTime(long millis)
{
    char time[25], rtcTime[25];
    sprintf(time, "%ld", millis);
    property_set("persist.sys.time", time);
    long rtcSettings = getRtcTime();
    sprintf(rtcTime, "%ld", rtcSettings);
    property_set("persist.sys.settingsrtc", rtcTime);
    return 0;
}

void change(long ts)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    char str[80];

    //将时间戳值转化成天数。通过天数可以比较方便地算出年、月、日。
    int days = ts / SEC_PER_DAY;
    //这个时间戳值的年。
    int yearTmp = 0;
    int dayTmp = 0;
    //使用夹逼法计算 days 天中包含的年数。
    for (yearTmp = UTC_BASE_YEAR; days > 0; yearTmp++)
    {
        dayTmp = (DAY_PER_YEAR + applib_dt_is_leap_year(yearTmp)); //这一年有多少天？
        if (days >= dayTmp)                                        //条件成立，则 yearTmp 即是这个时间戳值所代表的年数。
        {
            days -= dayTmp;
        }
        else
        {
            break;
        }
    }
    year = yearTmp;

    //这个时间戳值的月
    int monthTmp = 0;
    for (monthTmp = 1; monthTmp < MONTH_PER_YEAR; monthTmp++)
    {
        dayTmp = applib_dt_last_day_of_mon(monthTmp, year);
        if (days >= dayTmp)
        {
            days -= dayTmp;
        }
        else
        {
            break;
        }
    }
    month = monthTmp;

    day = days + 1;

    //转化成秒。
    int secs = ts % SEC_PER_DAY;
    //这个时间戳值的小时数。
    hour = secs / SEC_PER_HOUR;
    //这个时间戳值的分钟数。
    secs %= SEC_PER_HOUR;
    minute = secs / SEC_PER_MIN;
    //这个时间戳的秒钟数。
    second = secs % SEC_PER_MIN;

    sprintf(str, "date %d%d%d%d%d.%d set", month, day, hour, minute, year, second);
    if (system(str) < 0)
    {
        printf("cmd : %s\t error\n", str);
    }
    // if (system("busybox hwclock -w") < 0) {
    //     printf("cmd : busybox hwclock -w\t error\n");
    // }
}
