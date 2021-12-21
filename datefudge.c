/* vim:ts=4:sts=4:sw=4:et:cindent
 * datefudge.c: Twist system date back N seconds
 *
 * Copyright (C) 2001-2003, Matthias Urlichs <smurf@noris.de>
 * Copyright (C) 2008-2011, Robert Luberda <robert@debian.org>
 *
 */
#define _GNU_SOURCE

#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <assert.h>
#include <features.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>

static bool fudge_from_file = false;
static time_t fudge = 0;
static bool dostatic = false;
static bool fudge_set = false;
static char * fudge_file_path = 0;
static time_t fudge_file_mtime = 0;
static struct timeval fudge_file_checked_tv = (struct timeval){0};
static struct timeval fudge_file_cache_tv = (struct timeval){0};

time_t real_time(time_t *x);
time_t real_gettimeofday(struct timeval *x, void *y);

static void init_fudge (void) {
    dostatic = getenv("DATEFUDGE_DOSTATIC") != NULL;

    const char * const fud = getenv("DATEFUDGE");
    if(fud != NULL) {
        if (sizeof(time_t) <= sizeof(int))
            fudge = atoi(fud);
        else
            fudge = atoll(fud);
        fudge_set = true;
        return;
    }

    // Read datetime value (yyyy-MM-dd HH:mm:ss) from a file - can be changed
    // dynamically during a life of process.
    fudge_file_path = getenv("DATEFUDGE_FILE");
    if(fudge_file_path != NULL) {
        // Convert number of milliseconds to timeval struct (secs & microsecs).
        long fudge_file_cache_ms = atoi(getenv("DATEFUDGE_FILE_CACHE_MS"));
        fudge_file_cache_tv.tv_sec = fudge_file_cache_ms / 1000;
        fudge_file_cache_tv.tv_usec = (fudge_file_cache_ms % 1000) * 1000;

        fudge_from_file = true;
        char * datetime_str = 0;
        long length;
        FILE * f = fopen (fudge_file_path, "rb");
        if(f) {
            fseek(f, 0, SEEK_END);
            length = ftell (f);
            fseek(f, 0, SEEK_SET);
            datetime_str = malloc(length);
            if(datetime_str) {
                fread(datetime_str, 1, length, f);
            }
            fclose(f);
        }
        if(datetime_str) {
            time_t current_time;
            real_time(&current_time);
            struct tm current_tm;
            localtime_r(&current_time, &current_tm);

            // Initialize tm struct to 1900-01-01 00:00:00 & tells mktime() to
            // determine whether daylight saving time is in effect
            struct tm tm = (struct tm){.tm_mday = 1, .tm_isdst = -1};
            strptime(datetime_str, "%Y-%m-%d %H:%M:%S", &tm);
            time_t config_time = mktime(&tm);
            if(dostatic) {
              fudge = config_time;
            }
            else {
              fudge = current_time - config_time;
            }
        }
        free(datetime_str);
        fudge_set = true;
    }
}

static bool is_fudge_set() {
    if(!fudge_from_file) {
        return fudge_set;
    }
    struct timeval current_tv;
    struct timeval diff_tv;
    real_gettimeofday(&current_tv, NULL);
    timersub(&current_tv, &fudge_file_checked_tv, &diff_tv);
    // Don't check if time is changed more than predefined cache time.
    if(diff_tv.tv_sec > fudge_file_cache_tv.tv_sec
      || (diff_tv.tv_sec == fudge_file_cache_tv.tv_sec && diff_tv.tv_usec > fudge_file_cache_tv.tv_usec)) {
        fudge_file_checked_tv = current_tv;
        struct stat attrib;
        stat(fudge_file_path, &attrib);
        if(fudge_file_mtime != attrib.st_mtime) {
            fudge_file_mtime = attrib.st_mtime;
            fudge_set = false;
        }
    }
    return fudge_set;
}

static void set_fudge(time_t *seconds) {
    if (!seconds)
        return;

    if (!is_fudge_set())
        init_fudge();

    if (dostatic)
        *seconds = fudge;
    else
        *seconds -= fudge;
}

// Proxy to libc time function, setting/returning the "real" time.
time_t real_time(time_t *x) {
    static time_t (*libc_time)(time_t *) = NULL;

    if(!libc_time)
        libc_time = (typeof(libc_time))dlsym (RTLD_NEXT, "time");
    return libc_time(x);
}

time_t time(time_t *x) {
    time_t res;

    res = real_time(x);
    set_fudge(x);
    set_fudge(&res);
    return res;
}

// Proxy to libc __gettimeofday function, setting/returning the "real" time.
time_t real_gettimeofday(struct timeval *x, void *y) {
    static int (*libc_gettimeofday)(struct timeval *, void *) = NULL;

    if(!libc_gettimeofday)
        libc_gettimeofday = (typeof(libc_gettimeofday))dlsym (RTLD_NEXT, "gettimeofday");
    return libc_gettimeofday(x, y);
}

int __gettimeofday(struct timeval *x, void *y) {
    int res;

    res = real_gettimeofday(x,y);
    if(res) return res;
    set_fudge(&x->tv_sec);
    return 0;
}

int gettimeofday(struct timeval *x, void *y) {
    return __gettimeofday(x,y);
}

int clock_gettime(clockid_t x, struct timespec *y) {
    static int (*libc_clock_gettime)(clockid_t, struct timespec*);
    int res;

    if (!libc_clock_gettime)
        libc_clock_gettime =  (typeof(libc_clock_gettime))dlsym (RTLD_NEXT, __func__);
    res = (*libc_clock_gettime)(x,y);
    if (res || CLOCK_REALTIME != x) return res;
    set_fudge(&y->tv_sec);
    return 0;
}
