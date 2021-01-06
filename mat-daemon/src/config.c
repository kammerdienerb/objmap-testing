#include "config.h"

#define SIMPLE_CONFIG_IMPL
#include "simple_config.h"

md_config config;

int init_config(int argc, char **argv) {
    int status;

    status = 0;

    config._scfg = scfg_make();

    scfg_add_int(        config._scfg, "tier-1-node",              &config.tier_1_node);
        scfg_default_int(config._scfg, "tier-1-node",              -1);
    scfg_add_int(        config._scfg, "tier-2-node",              &config.tier_2_node);
        scfg_default_int(config._scfg, "tier-2-node",              -1);
    scfg_add_int(        config._scfg, "tier-3-node",              &config.tier_3_node);
        scfg_default_int(config._scfg, "tier-3-node",              -1);
    scfg_add_int(        config._scfg, "tier-1-capacity",          &config.tier_1_capacity);
        scfg_default_int(config._scfg, "tier-1-capacity",          -1);
    scfg_add_int(        config._scfg, "tier-2-capacity",          &config.tier_2_capacity);
        scfg_default_int(config._scfg, "tier-2-capacity",          -1);
    scfg_add_int(        config._scfg, "tier-3-capacity",          &config.tier_3_capacity);
        scfg_default_int(config._scfg, "tier-3-capacity",          -1);

    scfg_add_int(        config._scfg, "proc-discovery-period-ms", &config.proc_discovery_period_ms);
        scfg_default_int(config._scfg, "proc-discovery-period-ms", 1000);

    scfg_add_int(        config._scfg, "profile-period-ms",        &config.profile_period_ms);
        scfg_default_int(config._scfg, "profile-period-ms",        1000);
    scfg_add_int(        config._scfg, "profile-overflow-thresh",  &config.profile_overflow_thresh);
        scfg_default_int(config._scfg, "profile-overflow-thresh",  16);
    scfg_add_int(        config._scfg, "profile-max-sample-pages", &config.profile_max_sample_pages);
        scfg_default_int(config._scfg, "profile-max-sample-pages", 128);
    scfg_add_int(        config._scfg, "profile-max-history",      &config.profile_max_history);
        scfg_default_int(config._scfg, "profile-max-history",      1024);
    scfg_add_string(     config._scfg, "profile-event-string",     &config.profile_event_string);
        scfg_require(    config._scfg, "profile-event-string");

    status = scfg_parse(config._scfg, "test.config");

    if (status != SCFG_ERR_NONE) {
        fprintf(stderr, "%s\n", scfg_err_msg(config._scfg));
    }

    return status;
}

void print_config(void) {
    printf("============================ CURRENT CONFIG ============================\n");
    printf("%-30s %d\n",   "tier-1-node",              config.tier_1_node);
    printf("%-30s %d\n",   "tier-1-capacity",          config.tier_1_capacity);
    printf("%-30s %d\n",   "tier-2-node",              config.tier_2_node);
    printf("%-30s %d\n",   "tier-2-capacity",          config.tier_2_capacity);
    printf("%-30s %d\n",   "tier-3-node",              config.tier_3_node);
    printf("%-30s %d\n",   "tier-3-capacity",          config.tier_3_capacity);
    printf("%-30s %d\n",   "proc-discovery-period-ms", config.proc_discovery_period_ms);
    printf("%-30s %d\n",   "profile-period-ms",        config.profile_period_ms);
    printf("%-30s %d\n",   "profile-overflow-thresh",  config.profile_overflow_thresh);
    printf("%-30s %d\n",   "profile-max-sample-pages", config.profile_max_sample_pages);
    printf("%-30s %d\n",   "profile-max-history",      config.profile_max_history);
    printf("%-30s '%s'\n", "profile-event-string",     config.profile_event_string);
    printf("========================================================================\n");
}
