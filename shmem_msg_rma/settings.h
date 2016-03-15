
#pragma once

#include <time.h>

extern struct timespec pstart, pend;

extern int pp_options;
extern int pp_iterations;
extern int pp_transfer_size;
extern int pp_size_option;

extern char pp_test_name[10];

const unsigned int pp_test_cnt;
#define PP_TEST_CNT pp_test_cnt
#define PP_STR_LEN 32

struct pp_test_size_param {
	int size;
	int option;
};

//struct pp_test_size_param pp_test_size;
extern struct pp_test_size_param pp_test_size[];

enum precision {
	NANO = 1,
	MICRO = 1000,
	MILLI = 1000000,
};

enum {
    PP_OPT_SIZE = 1 << 2,
    PP_OPT_ITER = 1 << 3,
    PP_OPT_ACTIVE = 1 << 1,
};

static inline void pp_start(void)
{
	pp_options |= PP_OPT_ACTIVE;
	clock_gettime(CLOCK_MONOTONIC, &pstart);
}
static inline void pp_stop(void)
{
	clock_gettime(CLOCK_MONOTONIC, &pend);
	pp_options &= ~PP_OPT_ACTIVE;
}

int64_t pp_get_elapsed(const struct timespec *b, const struct timespec *a,
		enum precision p);
void pp_show_perf(char *name, int tsize, int iters, struct timespec *start,
		struct timespec *end, int xfers_per_iter);
void pp_show_perf_mr(int tsize, int iters, struct timespec *start,
		struct timespec *end, int xfers_per_iter, int argc, char *argv[]);


