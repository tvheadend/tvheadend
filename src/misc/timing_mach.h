#ifndef TIMING_MACH_H
#define TIMING_MACH_H
/* ************* */
/* TIMING_MACH_H */

/* C99 check */
#if defined(__STDC__)
# if defined(__STDC_VERSION__)
#  if (__STDC_VERSION__ >= 199901L)
#   define TIMING_C99
#  endif
# endif
#endif

#include <time.h>

#define TIMING_GIGA (1000000000)
#define TIMING_NANO (1e-9)

/* inline functions - maintain ANSI C compatibility */
#ifndef TIMING_C99
/* this is a bad hack that makes the functions static in a header file.
   Compiler warnings about unused functions will plague anyone using this code with ANSI C. */
#define inline static
#endif

/* timespec to double */
inline double timespec2secd(const struct timespec *ts_in) {
    return ((double) ts_in->tv_sec) + ((double) ts_in->tv_nsec ) * TIMING_NANO;
}

/* double sec to timespec */
inline void secd2timespec(struct timespec *ts_out, const double sec_d) {
    ts_out->tv_sec = (time_t) (sec_d);
    ts_out->tv_nsec = (long) ((sec_d - (double) ts_out->tv_sec) * TIMING_GIGA);
}

/* timespec difference (monotonic) left - right */
inline void timespec_monodiff_lmr(struct timespec *ts_out,
                                    const struct timespec *ts_in) {
    /* out = out - in,
       where out > in
     */
    ts_out->tv_sec = ts_out->tv_sec - ts_in->tv_sec;
    ts_out->tv_nsec = ts_out->tv_nsec - ts_in->tv_nsec;
    if (ts_out->tv_nsec < 0) {
        ts_out->tv_sec = ts_out->tv_sec - 1;
        ts_out->tv_nsec = ts_out->tv_nsec + TIMING_GIGA;
    }
}

/* timespec difference (monotonic) right - left */
inline void timespec_monodiff_rml(struct timespec *ts_out,
                                    const struct timespec *ts_in) {
    /* out = in - out,
       where in > out
     */
    ts_out->tv_sec = ts_in->tv_sec - ts_out->tv_sec;
    ts_out->tv_nsec = ts_in->tv_nsec - ts_out->tv_nsec;
    if (ts_out->tv_nsec < 0) {
        ts_out->tv_sec = ts_out->tv_sec - 1;
        ts_out->tv_nsec = ts_out->tv_nsec + TIMING_GIGA;
    }
}

/* timespec addition (monotonic) */
inline void timespec_monoadd(struct timespec *ts_out,
                             const struct timespec *ts_in) {
    /* out = in + out */
    ts_out->tv_sec = ts_out->tv_sec + ts_in->tv_sec;
    ts_out->tv_nsec = ts_out->tv_nsec + ts_in->tv_nsec;
    if (ts_out->tv_nsec >= TIMING_GIGA) {
        ts_out->tv_sec = ts_out->tv_sec + 1;
        ts_out->tv_nsec = ts_out->tv_nsec - TIMING_GIGA;
    }
}

#ifndef TIMING_C99
#undef inline
#endif

#ifdef __MACH__
/* ******** */
/* __MACH__ */

/* only CLOCK_REALTIME and CLOCK_MONOTONIC are emulated */
#ifndef CLOCK_REALTIME
# define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
# define CLOCK_MONOTONIC 1
#endif

/* typdef POSIX clockid_t */
/* typedef int clockid_t; */

/* initialize mach timing */
int timing_mach_init (void);

/* clock_gettime - emulate POSIX */
int clock_gettime(const clockid_t id, struct timespec *tspec);

/* clock_nanosleep for CLOCK_MONOTONIC and TIMER_ABSTIME */
int clock_nanosleep_abstime(const struct timespec *req);

/* __MACH__ */
/* ******** */
#else
/* ***** */
/* POSIX */

/* clock_nanosleep for CLOCK_MONOTONIC and TIMER_ABSTIME */
# define clock_nanosleep_abstime(req) \
         clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, (req), NULL)

/* POSIX */
/* ***** */
#endif

/* timer functions that make use of clock_nanosleep_abstime
   For POSIX systems, it is recommended to use POSIX timers and signals.
   For Mac OSX (mach), there are no POSIX timers so these functions are very helpful.
*/

/* Sets absolute time ts_target to ts_step after current time */
int itimer_start (struct timespec *ts_target, const struct timespec *ts_step);

/* Nanosleeps to ts_target then adds ts_step to ts_target for next iteration */
int itimer_step (struct timespec *ts_target, const struct timespec *ts_step);

/* TIMING_MACH_H */
/* ************* */
#endif
