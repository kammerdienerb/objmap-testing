#include "config.h"
#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int main(int argc, char **argv) {
    if (init_config(argc, argv)          != 0) { return 1; }
        print_config();
    if (init_process_table()             != 0) { return 1; }
    if (start_process_discovery_thread() != 0) { return 1; }
        printf(":: Started process discovery thread.\n");
    if (init_profiling()                 != 0) { return 1; }
    if (start_profiling_thread()         != 0) { return 1; }
        printf(":: Started profiling thread.\n");

    printf(":: Everything is initialized.\n\n");

    for (;;) {
        sleep(1);
    }

    return 0;
}
