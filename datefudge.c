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
static time_t fudge_file_checked_time = 0;
time_t real_time(time_t *x);

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
            struct tm tm;
            if (strptime(datetime_str, "%Y-%m-%d %H:%M:%S", &tm) != NULL) {
                time_t config_time = mktime(&tm);
                if(dostatic) {
                    fudge = config_time;
                }
                else {
                    time_t current_time;
                    real_time(&current_time);
                    fudge = current_time - config_time;
                }
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
    time_t current_time;
    real_time(&current_time);
    // Since times used here are at 1 sec resolution don't check if value is
    // changed more than once in a second.
    if(current_time - fudge_file_checked_time > 1) {
        fudge_file_checked_time = current_time;
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

// Proxy to libc time function, setting/returning "real" time.
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

int __gettimeofday(struct timeval *x, void *y) {
    static int (*libc_gettimeofday)(struct timeval *, void *) = NULL;
    int res;

    if(!libc_gettimeofday)
        libc_gettimeofday = (typeof(libc_gettimeofday))dlsym (RTLD_NEXT, __func__);
    res = (*libc_gettimeofday)(x,y);
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
