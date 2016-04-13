
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <netdb.h>

#include "shmem_impl.h"
#include "settings.h"

int pp_run_test(void)
{
	int ret, i;

    ret = shmem_ft_sync();
	if (ret)
		return ret;

	pp_start(); // Timing

	for (i = 0; i < pp_iterations; i++) {
        shmem_put(pp_transfer_size);
        /* TODO: Other shmem functions? */
	}

	pp_stop(); // Timing stop

	pp_show_perf(pp_test_name, pp_transfer_size, pp_iterations,
				&pstart, &pend, 1);

	return 0;
}

static int pp_run(void)
{
	int i, ret;

	if (!(pp_options & PP_OPT_SIZE)) {
		for (i = 0; i < PP_TEST_CNT; i++) {
			if (pp_test_size[i].option > pp_size_option)
				continue;
			pp_transfer_size = pp_test_size[i].size;
			pp_init_test(pp_test_name, sizeof(pp_test_name));
			ret = pp_run_test();
			if (ret)
				goto out;
		}
	} else {
		pp_init_test(pp_test_name, sizeof(pp_test_name));
		ret = pp_run_test();
		if (ret)
			goto out;
	}

    shmem_finalize();
out:
    shutdown_lf();
	return ret;
}

int main(int argc, char **argv)
{
    int op, ret;

    pp_options = 0;

/*	while ((op = getopt(argc, argv, "I:S:" )) != -1) {
        switch (op) {
        case 'I':
            pp_options |= PP_OPT_ITER;
            pp_iterations = atoi(optarg);
            break;
        case 'S':
            if (!strncasecmp("all", optarg, 3)) {
                pp_size_option = 1;
            } else {
                pp_options |= PP_OPT_SIZE;
                pp_transfer_size = atoi(optarg);
            }
            break;
        default:
            break;
        }
    }
*/
    	
    pp_options |= PP_OPT_ITER;
    pp_iterations = 1;
            
    init_opts(argc, argv);   

    shmem_init();
    
    pp_run();



    free_res_lf();

    return -ret;
}
