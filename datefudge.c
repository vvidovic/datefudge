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

static time_t fudge = 0;
static bool dostatic = false;
static bool fudge_set = false;

static void init_fudge (void)
{
    const char * const fud = getenv("DATEFUDGE");
    if(fud == NULL) return;
    if (sizeof(time_t) <= sizeof(int))
        fudge = atoi(fud);
    else
        fudge = atoll(fud);
    dostatic = getenv("DATEFUDGE_DOSTATIC") != NULL;
    fudge_set = true;
}

static void set_fudge(time_t *seconds)
{
    if (!seconds)
        return;

    if (!fudge_set)
        init_fudge();

    if (dostatic)
        *seconds = fudge;
    else
        *seconds -= fudge;
}

time_t time(time_t *x) {
    static time_t (*libc_time)(time_t *) = NULL;
    time_t res;

    if(!libc_time)
        libc_time = (typeof(libc_time))dlsym (RTLD_NEXT, __func__);
    res = (*libc_time)(x);
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
