#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "simple_config.h"

#include <stdio.h>

typedef struct {
    struct scfg *_scfg;

    int         tier_1_node;
    int         tier_1_capacity;
    int         tier_2_node;
    int         tier_2_capacity;
    int         tier_3_node;
    int         tier_3_capacity;

    int         proc_discovery_period_ms;

    int         profile_period_ms;
    int         profile_overflow_thresh;
    int         profile_max_sample_pages;
    int         profile_max_history;
    const char *profile_event_string;
} md_config;

extern md_config config;

int  init_config(int argc, char **argv);
void print_config(void);

#endif
