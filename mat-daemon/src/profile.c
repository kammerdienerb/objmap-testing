#include "profile.h"
#include "common.h"
#include "config.h"
#include "process.h"
#include "array.h"
#include "tree.h"

#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <linux/perf_event.h>
#include <perfmon/pfmlib_perf_event.h>

typedef struct __attribute__ ((__packed__)) {
    u32 pid;
    u32 tid;
    u64 addr;
} md_profile_sample;

typedef void *addr_t;

typedef struct {
    pid_t pid;
    u64   accesses;
} md_profile_info;

typedef struct {
    int                          fd;
    size_t                       size;
    struct perf_event_attr       pe;
    struct perf_event_mmap_page *metadata;
    pfm_perf_encode_arg_t        pfm;
} md_cpu_profiler;

static pthread_t       profiling_pthread;
static int             n_cpus;
static md_cpu_profiler cpu_profilers[2048];
static array_t         collected_profile;


static int profile_get_event(int cpu, md_cpu_profiler *cpu_profiler) {
    int err;

    pfm_initialize();

    cpu_profilers[cpu].pe.size  = sizeof(struct perf_event_attr);

    cpu_profilers[cpu].pfm.size = sizeof(pfm_perf_encode_arg_t);
    cpu_profilers[cpu].pfm.attr = &cpu_profilers[cpu].pe;
    err = pfm_get_os_event_encoding(config.profile_event_string, PFM_PLM2 | PFM_PLM3, PFM_OS_PERF_EVENT, &cpu_profilers[cpu].pfm);

    if (err != PFM_SUCCESS) {
        fprintf(stderr, "Couldn't find an appropriate event to use. (error %d) Aborting.\n", err);
        return 0;
    }

    /* perf_event_open */
    cpu_profilers[cpu].pe.sample_type    = PERF_SAMPLE_TID | PERF_SAMPLE_ADDR;
    cpu_profilers[cpu].pe.mmap           = 1;
    cpu_profilers[cpu].pe.exclude_kernel = 1;
    cpu_profilers[cpu].pe.exclude_hv     = 1;
    cpu_profilers[cpu].pe.precise_ip     = 2;
    cpu_profilers[cpu].pe.task           = 1;
    cpu_profilers[cpu].pe.use_clockid    = 1;
    cpu_profilers[cpu].pe.clockid        = CLOCK_MONOTONIC;
    cpu_profilers[cpu].pe.sample_freq    = MAX(1000 / config.profile_period_ms, 1);
    cpu_profilers[cpu].pe.disabled       = 1;

    return 1;
}

int init_profiling(void) {
    int status;
    int fd;

    collected_profile = array_make(md_profile_info);

    status = 0;
    fd     = 0;
    n_cpus = 0;

    while (fd != -1) {
        profile_get_event(n_cpus, cpu_profilers);

        /* Open the perf file descriptor */
        fd = syscall(__NR_perf_event_open, &cpu_profilers[n_cpus].pe, -1, n_cpus, -1, 0);

        if (fd == -1) {
            if (errno != EINVAL) {
                fprintf(stderr, "Error opening perf event 0x%llx (%d -- %s).\n", cpu_profilers[n_cpus].pe.config, errno, strerror(errno));
                fprintf(stderr, "attr = 0x%lx\n", (u64)(cpu_profilers[n_cpus].pe.config));
                status = errno;
                errno  = 0;
                goto out;
            }
        } else {
            cpu_profilers[n_cpus].fd = fd;

            /* mmap the file */
            cpu_profilers[n_cpus].metadata = mmap(NULL, PAGE_SIZE + (PAGE_SIZE * config.profile_max_sample_pages),
                                PROT_READ | PROT_WRITE, MAP_SHARED, cpu_profilers[n_cpus].fd, 0);
            if (cpu_profilers[n_cpus].metadata == MAP_FAILED) {
                fprintf(stderr, "Failed to mmap room (%d bytes) for perf samples (fd = %d). Aborting with:\n(%d) %s\n",
                        PAGE_SIZE + (PAGE_SIZE * config.profile_max_sample_pages), cpu_profilers[n_cpus].fd, errno, strerror(errno));
                status = errno;
                errno  = 0;
                goto out;
            }

            /* Initialize */
            ioctl(cpu_profilers[n_cpus].fd, PERF_EVENT_IOC_RESET, 0);
            ioctl(cpu_profilers[n_cpus].fd, PERF_EVENT_IOC_ENABLE, 0);

            n_cpus += 1;
        }
    }

    printf(":: Profiling events for %d threads.\n", n_cpus);

out:;
    return status;
}

static md_profile_info *get_or_make_info_for_pid(pid_t pid) {
    int              found;
    md_profile_info *info;
    md_profile_info  new_info;

    found = 0;
    info  = NULL;
    array_traverse(collected_profile, info) {
        if (info->pid == pid) {
            found = 1;
            break;
        }
    }

    if (!found) {
        memset(&new_info, 0, sizeof(new_info));
        new_info.pid = pid;
        info = array_push(collected_profile, new_info);
    }

    return info;
}

static void do_profiling_for_cpu(md_cpu_profiler *prof) {
    u64                       head, tail, buf_size;
    void                     *addr;
    char                     *base, *begin, *end;
    struct perf_event_header *header;
    md_profile_sample        *sample;
    md_proc                  *proc;
    md_profile_info          *info;

    /* Get ready to read */
    head     = prof->metadata->data_head;
    tail     = prof->metadata->data_tail;
    buf_size = PAGE_SIZE * config.profile_max_sample_pages;
    asm volatile("" ::: "memory"); /* Block after reading data_head, per perf docs */

    base  = (char *)prof->metadata + PAGE_SIZE;
    begin = base + tail % buf_size;
    end   = base + head % buf_size;

    /* Read all of the samples */
    while (begin < end) {
        header = (struct perf_event_header *)begin;
        if (header->size == 0) {
            break;
        } else if (header->type != PERF_RECORD_SAMPLE) {
            goto inc;
        }

        sample = (md_profile_sample*) (begin + sizeof(struct perf_event_header)); (void)sample;

        proc = acquire_proccess(sample->pid);
        if (proc == NULL) { goto inc; }
        release_proccess(proc);


        (void)addr;

        info = get_or_make_info_for_pid(sample->pid);
        info->accesses += 1;

inc:
        /* Increment begin by the size of the sample */
        if (((char *)header + header->size) == base + buf_size) {
            begin = base;
        } else {
            begin = begin + header->size;
        }
    }

    /* Let perf know that we've read this far */
    prof->metadata->data_tail = head;
    __sync_synchronize();
}

static void clear_profiling_info(void) {
    array_clear(collected_profile);
}

static void *profiling_thread(void *arg) {
    int cpu;
    u64 start;
    u64 elapsed;

    for (;;) {
        printf("begin prof interval\n");
        start = measure_time_now_ms();

        clear_profiling_info();

        for (cpu = 0; cpu < n_cpus; cpu += 1) {
            do_profiling_for_cpu(&cpu_profilers[cpu]);
        }

        printf("  %d\n", array_len(collected_profile));

        fflush(stdout);
        elapsed = measure_time_now_ms() - start;
        if (elapsed < config.profile_period_ms) {
            usleep(1000 * (config.profile_period_ms - elapsed));
        }
    }

    return NULL;
}

int start_profiling_thread(void) {
    int status;
    status = pthread_create(&profiling_pthread, NULL, profiling_thread, NULL);
    if (status != 0) {
        fprintf(stderr, "failed to create profiling thread\n");
    }
    return status;
}
