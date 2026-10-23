#pragma once
#if defined(_WIN32)
struct timeval { long tv_sec; long tv_usec; };
#else
#include <sys/time.h>
#endif
using sntp_sync_time_cb_t = void (*)(struct timeval *);
void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t callback);
void configTime(long offset, int daylight, const char *first, const char *second);
