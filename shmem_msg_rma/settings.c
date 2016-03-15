
#include <assert.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "settings.h"

int pp_options;
int pp_iterations;
int pp_transfer_size;
int pp_size_option;

struct timespec pstart, pend;

char pp_test_name[10] = "custom";

struct pp_test_size_param pp_test_size[] = {
    { 1 <<  1, 0 }, { (1 <<  1) + (1 <<  0), 2},
    { 1 <<  2, 0 }, { (1 <<  2) + (1 <<  1), 2},
    { 1 <<  3, 0 }, { (1 <<  3) + (1 <<  2), 2},
    { 1 <<  4, 0 }, { (1 <<  4) + (1 <<  3), 2},
    { 1 <<  5, 0 }, { (1 <<  5) + (1 <<  4), 2},
    { 1 <<  6, 0 }, { (1 <<  6) + (1 <<  5), 1},
    { 1 <<  7, 0 }, { (1 <<  7) + (1 <<  6), 1},
    { 1 <<  8, 0 }, { (1 <<  8) + (1 <<  7), 1},
    { 1 <<  9, 0 }, { (1 <<  9) + (1 <<  8), 1},
    { 1 << 10, 0 }, { (1 << 10) + (1 <<  9), 1},
    { 1 << 11, 0 }, { (1 << 11) + (1 << 10), 1},
    { 1 << 12, 0 }, { (1 << 12) + (1 << 11), 1},
    { 1 << 13, 0 }, { (1 << 13) + (1 << 12), 1},
    { 1 << 14, 0 }, { (1 << 14) + (1 << 13), 1},
    { 1 << 15, 0 }, { (1 << 15) + (1 << 14), 1},
    { 1 << 16, 0 }, { (1 << 16) + (1 << 15), 1},
    { 1 << 17, 0 }, { (1 << 17) + (1 << 16), 1},
    { 1 << 18, 0 }, { (1 << 18) + (1 << 17), 1},
    { 1 << 19, 0 }, { (1 << 19) + (1 << 18), 1},
    { 1 << 20, 0 }, { (1 << 20) + (1 << 19), 1},
    { 1 << 21, 0 }, { (1 << 21) + (1 << 20), 1},
    { 1 << 22, 0 }, { (1 << 22) + (1 << 21), 1},
    { 1 << 23, 1 },
};

const unsigned int pp_test_cnt = (sizeof pp_test_size / sizeof pp_test_size[0]);

char *pp_size_str(char str[PP_STR_LEN], long long size)
{
    long long base, fraction = 0;
    char mag;

    memset(str, '\0', PP_STR_LEN);

    if (size >= (1 << 30)) {
        base = 1 << 30;
        mag = 'g';
    } else if (size >= (1 << 20)) {
        base = 1 << 20;
        mag = 'm';
    } else if (size >= (1 << 10)) {
        base = 1 << 10;
        mag = 'k';
    } else {
        base = 1;
        mag = '\0';
    }

    if (size / base < 10)
        fraction = (size % base) * 10 / base;

    if (fraction)
        snprintf(str, PP_STR_LEN, "%lld.%lld%c", size / base, fraction, mag);
    else
        snprintf(str, PP_STR_LEN, "%lld%c", size / base, mag);

    return str;
}

char *pp_cnt_str(char str[PP_STR_LEN], long long cnt)
{
    if (cnt >= 1000000000)
        snprintf(str, PP_STR_LEN, "%lldb", cnt / 1000000000);
    else if (cnt >= 1000000)
        snprintf(str, PP_STR_LEN, "%lldm", cnt / 1000000);
    else if (cnt >= 1000)
        snprintf(str, PP_STR_LEN, "%lldk", cnt / 1000);
    else
        snprintf(str, PP_STR_LEN, "%lld", cnt);

    return str;
}

int pp_size_to_count(int size)
{
    if (size >= (1 << 20))
        return 100;
    else if (size >= (1 << 16))
        return 1000;
    else if (size >= (1 << 10))
        return 10000;
    else
        return 100000;
}

void pp_init_test(char *test_name, size_t test_name_len)
{
    char sstr[PP_STR_LEN];

    size_str(sstr, pp_transfer_size);
    snprintf(test_name, test_name_len, "%s_lat", sstr);
    if (!(pp_options & PP_OPT_ITER))
        pp_iterations = pp_size_to_count(pp_transfer_size);
}

int64_t pp_get_elapsed(const struct timespec *b, const struct timespec *a,
        enum precision p)
{
    int64_t elapsed;

    elapsed = (a->tv_sec - b->tv_sec) * 1000 * 1000 * 1000;
    elapsed += a->tv_nsec - b->tv_nsec;
    return elapsed / p;
}

void pp_show_perf(char *name, int tsize, int iters, struct timespec *start,
        struct timespec *end, int xfers_per_iter)
{
    static int header = 1;
    char str[PP_STR_LEN];
    int64_t elapsed = pp_get_elapsed(start, end, MICRO);
    long long bytes = (long long) iters * tsize * xfers_per_iter;

    if (header) {
        printf("%-10s%-8s%-8s%-8s%8s %10s%13s\n",
                "name", "bytes", "iters", "total", "time", "Gb/sec", "usec/xfer");
        header = 0;
    }

    printf("%-10s", name);

    printf("%-8s", pp_size_str(str, tsize));

    printf("%-8s", pp_cnt_str(str, iters));

    printf("%-8s", pp_size_str(str, bytes));

    printf("%8.2fs%10.2f%11.2f\n",
            elapsed / 1000000.0, (bytes * 8) / (1000.0 * elapsed),
            ((float)elapsed / iters / xfers_per_iter));
}

void pp_show_perf_mr(int tsize, int iters, struct timespec *start,
        struct timespec *end, int xfers_per_iter, int argc, char *argv[])
{
    static int header = 1;
    int64_t elapsed = pp_get_elapsed(start, end, MICRO);
    long long total = (long long) iters * tsize * xfers_per_iter;
    int i;

    if (header) {
        printf("---\n");

        for (i = 0; i < argc; ++i)
            printf("%s ", argv[i]);

        printf(":\n");
        header = 0;
    }

    printf("- { ");
    printf("xfer_size: %d, ", tsize);
    printf("iterations: %d, ", iters);
    printf("total: %lld, ", total);
    printf("time: %f, ", elapsed / 1000000.0);
    printf("Gb/sec: %f, ", (total * 8) / (1000.0 * elapsed));
    printf("usec/xfer: %f", ((float)elapsed / iters / xfers_per_iter));
    printf(" }\n");
}
