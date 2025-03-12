
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_TIMES_H_INCLUDED_
#define _NGX_TIMES_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct {
    time_t      sec; // 自epoch 以来的秒数，即时间戳
    ngx_uint_t  msec; // 秒数后的小数部分，单位是毫秒
    ngx_int_t   gmtoff; // GMT 时区偏移量
} ngx_time_t;

// 初始化时间系统，应在Nginx启动时调用
void ngx_time_init(void);
// 初始化时间系统，应在Nginx启动时调用
void ngx_time_update(void);

void ngx_time_sigsafe_update(void);
// 将时间戳转换为符合HTTP协议要求的日期字符串（如Fri, 08 Sep 2023 14:30:00 GMT），用于生成HTTP响应头。
u_char *ngx_http_time(u_char *buf, time_t t);
u_char *ngx_http_cookie_time(u_char *buf, time_t t);
// 将时间戳转换为UTC时间的结构体ngx_tm_t，用于时间格式化。
void ngx_gmtime(time_t t, ngx_tm_t *tp);

time_t ngx_next_time(time_t when);
#define ngx_next_time_n      "mktime()"

// 全局指针指示当前缓存的时间
extern volatile ngx_time_t  *ngx_cached_time;
// 获取当前时间的秒数（时间戳）
#define ngx_time()           ngx_cached_time->sec
// 获取完整的时间数据结构
#define ngx_timeofday()      (ngx_time_t *) ngx_cached_time

extern volatile ngx_str_t    ngx_cached_err_log_time;
extern volatile ngx_str_t    ngx_cached_http_time;
extern volatile ngx_str_t    ngx_cached_http_log_time; // 错误日志格式
extern volatile ngx_str_t    ngx_cached_http_log_iso8601; // ISO8601标准格式
extern volatile ngx_str_t    ngx_cached_syslog_time; // 系统日志格式

/*
 * milliseconds elapsed since some unspecified point in the past
 * and truncated to ngx_msec_t, used in event timers
 * 用途：记录自服务器启动后的毫秒数，用于事件循环超时管理（如epoll_wait的超时参数）。
 * 设计意义：避免频繁调用系统调用gettimeofday()，提升事件驱动模型的效率。
 * 
 */
extern volatile ngx_msec_t  ngx_current_msec;


#endif /* _NGX_TIMES_H_INCLUDED_ */
