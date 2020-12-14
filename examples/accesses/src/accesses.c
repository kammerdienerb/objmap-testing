#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/perf_event.h>
#include <perfmon/pfmlib_perf_event.h>

#define u32 uint32_t
#define u64 uint64_t

#include "tree.h"
#include "hash_table.h"
#define PROC_OBJECT_MAP_IMPL
#include "proc_object_map.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

typedef char *str;

#define KEY_BYTES (1)
#define KEY_ADDR  (2)

typedef struct {
    void *addr_OR_coop_buff_bytes; /* malloc()ed when coop bytes */
    u32   n_coop_buff_bytes;
    int   kind;
} lookup_t;

typedef struct {
    u64 rss;
    u64 accesses;
} counter_t;

typedef struct {
    u64       end;
    lookup_t *look;
} object_t;

use_tree(u64, object_t);
use_hash_table(lookup_t, counter_t);

/* @bad
** We're assuming that the first 8 bytes in any coop
** buffer won't hash to something that looks like a valid
** heap address...
*/
u64 hash_addr(void *addr) { return (u64)addr; }
u64 hash_bytes(char *bytes, u32 n_bytes) {
    u64 hash;
    int i;

    hash = 5381;

    for (i = 0; i < n_bytes; i += 1) {
        hash = ((hash << 5) + hash) + bytes[i];
    }

    return hash;
}
u64 hash(const lookup_t l) { /* Use const so that the compiler will hopefully pass by ref. */
    if (l.kind == KEY_BYTES) {
        return hash_bytes(l.addr_OR_coop_buff_bytes, l.n_coop_buff_bytes);
    }
    return hash_addr(l.addr_OR_coop_buff_bytes);
}

int                             pid;
int                             i_ms;
hash_table(lookup_t, counter_t) counters;
tree(u64, object_t)             addr_tree;

#define PAGE_SIZE (4096)
int         profile_overflow_thresh  = 4;
int         profile_max_sample_pages = 128;

typedef struct {
    int                          fd;
    size_t                       size;
    struct perf_event_attr       pe;
    struct perf_event_mmap_page *metadata;
} cpu_profiler_t;

int            n_cpus;
cpu_profiler_t cpu_profilers[2048];

typedef struct __attribute__ ((__packed__)) {
    u32 pid;
    u32 tid;
    u64 addr;
} profile_sample_t;


void   usage(char *argv0);
int    do_args(int argc, char **argv);
u64    measure_time_now_ms(void);
int    setup_profiling(void);
int    do_loop(void);


int main(int argc, char **argv) {
    if (!do_args(argc, argv)) { return 1; }

    counters  = hash_table_make(lookup_t, counter_t, hash);
    addr_tree = tree_make(u64, object_t);

    if (!setup_profiling()) { return 1; }
    if (!do_loop())         { return 1; }

    return 0;
}

void usage(char *argv0) {
    fprintf(stderr, "usage: %s PID INTERVAL_TIME_IN_MS\n", argv0);
}

int do_args(int argc, char **argv) {
    if (argc != 3) {
        usage(argv[0]);
        return 0;
    }

    if (sscanf(argv[1], "%d", &pid) != 1) {
        usage(argv[0]);
        return 0;
    }

    if (sscanf(argv[2], "%d", &i_ms) != 1) {
        usage(argv[0]);
        return 0;
    }

    return 1;
}

u64 measure_time_now_ms(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return 1000ULL * tv.tv_sec + (tv.tv_usec / 1000ULL);
}

int profile_get_event(int cpu, cpu_profiler_t *cpu_profiler) {
    /* perf_event_open */
    cpu_profilers[cpu].pe.size           = sizeof(struct perf_event_attr);
    cpu_profilers[cpu].pe.type           = PERF_TYPE_HARDWARE;
    cpu_profilers[cpu].pe.config         = PERF_COUNT_HW_CACHE_MISSES;
    cpu_profilers[cpu].pe.sample_type    = PERF_SAMPLE_TID | PERF_SAMPLE_ADDR;
    cpu_profilers[cpu].pe.mmap           = 1;
    cpu_profilers[cpu].pe.exclude_kernel = 1;
    cpu_profilers[cpu].pe.exclude_hv     = 1;
    cpu_profilers[cpu].pe.precise_ip     = 2;
    cpu_profilers[cpu].pe.task           = 1;
    cpu_profilers[cpu].pe.use_clockid    = 1;
    cpu_profilers[cpu].pe.clockid        = CLOCK_MONOTONIC;
    cpu_profilers[cpu].pe.sample_period  = profile_overflow_thresh;
    cpu_profilers[cpu].pe.disabled       = 1;

    return 1;
}

int setup_profiling(void) {
    int fd;

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
                return 0;
            }
        } else {
            cpu_profilers[n_cpus].fd = fd;

            /* mmap the file */
            cpu_profilers[n_cpus].metadata = mmap(NULL, PAGE_SIZE + (PAGE_SIZE * profile_max_sample_pages),
                                PROT_READ | PROT_WRITE, MAP_SHARED, cpu_profilers[n_cpus].fd, 0);
            if (cpu_profilers[n_cpus].metadata == MAP_FAILED) {
                fprintf(stderr, "Failed to mmap room (%d bytes) for perf samples (fd = %d). Aborting with:\n(%d) %s\n",
                        PAGE_SIZE + (PAGE_SIZE * profile_max_sample_pages), cpu_profilers[n_cpus].fd, errno, strerror(errno));
                return 0;
            }

            /* Initialize */
            ioctl(cpu_profilers[n_cpus].fd, PERF_EVENT_IOC_RESET, 0);
            ioctl(cpu_profilers[n_cpus].fd, PERF_EVENT_IOC_ENABLE, 0);

            n_cpus += 1;
        }
    }

    return 1;
}

void get_objects(DIR *dir, char *objmap_path) {
    struct dirent                   *dirent;
    char                             entry_path[4096];
    int                              err;
    struct proc_object_map_record_t  record;
    u64                              obj_start;
    u64                              obj_end;
    char                            *coop_bytes;
    lookup_t                         key;
    counter_t                       *found_val;
    counter_t                        val;
    object_t                         new_obj;

    while ((dirent = readdir(dir)) != NULL) {
        if (strcmp(dirent->d_name, "controller") == 0
        ||  strcmp(dirent->d_name, ".")          == 0
        ||  strcmp(dirent->d_name, "..")         == 0) {
            continue;
        }

        snprintf(entry_path, sizeof(entry_path), "%s/%s", objmap_path, dirent->d_name);
        err = objmap_entry_read_record(entry_path, &record);

        if      (err == ENOENT) { continue; }
        else if (err == 0)      {
            sscanf(dirent->d_name, "%lx-%lx", (unsigned long*)&obj_start, (unsigned long*)&obj_end);

            if (record.coop_buff_n_bytes > 0) {
                coop_bytes = malloc(record.coop_buff_n_bytes);
                err = objmap_entry_read_record_coop_buff(entry_path, coop_bytes, record.coop_buff_n_bytes);

                if (err == 0) {
                    key.addr_OR_coop_buff_bytes = coop_bytes;
                    key.n_coop_buff_bytes       = record.coop_buff_n_bytes;
                    key.kind                    = KEY_BYTES;
                } else {
                    free(coop_bytes);
                }
            } else {
                key.addr_OR_coop_buff_bytes = (void*)obj_start;
                key.n_coop_buff_bytes       = 0;
                key.kind                    = KEY_ADDR;
            }

            found_val = hash_table_get_val(counters, key);
            if (found_val == NULL) {
                val.rss      = record.n_resident_pages * 4096;
                val.accesses = 0;
                hash_table_insert(counters, key, val);
            } else {
                free(coop_bytes);
                found_val->rss += record.n_resident_pages * 4096;
            }

            new_obj.end  = obj_end;
            new_obj.look = hash_table_get_key(counters, key);

            tree_insert(addr_tree, obj_start, new_obj);
        }
    }
}

void get_accesses_for_cpu(cpu_profiler_t *cpu_profiler) {
    u64                       count;
    u64                       head, tail, buf_size;
    void                     *addr;
    char                     *base, *begin, *end;
    profile_sample_t         *sample;
    struct perf_event_header *header;

    count = 0;

    /* Get ready to read */
    head     = cpu_profiler->metadata->data_head;
    tail     = cpu_profiler->metadata->data_tail;
    buf_size = PAGE_SIZE * profile_max_sample_pages;
    asm volatile("" ::: "memory"); /* Block after reading data_head, per perf docs */

    base  = (char *)cpu_profiler->metadata + PAGE_SIZE;
    begin = base + tail % buf_size;
    end   = base + head % buf_size;

    /* Read all of the samples */
    while (begin < end) {
        printf("in loop\n");
        header = (struct perf_event_header *)begin;
        if (header->size == 0) {
            printf("zero\n");
            break;
        } else if (header->type != PERF_RECORD_SAMPLE) {
            printf("not sample\n");
            goto inc;
        }

        sample = (profile_sample_t*) (begin + sizeof(struct perf_event_header));

        if (sample->pid != pid) {
            printf("wrong pid\n");
            goto inc;
        }

        addr = (void *) (sample->addr);

        (void)addr;
        count += 1;
inc:
        /* Increment begin by the size of the sample */
        if (((char *)header + header->size) == base + buf_size) {
            begin = base;
        } else {
            begin = begin + header->size;
        }
    }

    printf("%lu\n", count);

    /* Let perf know that we've read this far */
    cpu_profiler->metadata->data_tail = head;
    __sync_synchronize();
}

void get_accesses(void) {
    int cpu;

    for (cpu = 0; cpu < n_cpus; cpu += 1) {
        get_accesses_for_cpu(&cpu_profilers[cpu]);
    }
}

void report_interval(int i, u64 elap_ms) {
#if 0
    lookup_t   key;
    counter_t *val_p;

    printf("interval %d took %lums\n", i, elap_ms);
    hash_table_traverse(counters, key, val_p) {
        if (key.kind == KEY_BYTES) {
            printf("  %.*s,", key.n_coop_buff_bytes, (char*)key.addr_OR_coop_buff_bytes);
        } else {
            printf("  %p,", key.addr_OR_coop_buff_bytes);
        }
        printf("%lu,%lu\n", val_p->rss, val_p->accesses);
    }
#endif
}

int do_loop(void) {
    u64        start_ms;
    u64        elap_ms;
    char       objmap_path[4096 - 256];
    int        did_once;
    int        i;
    DIR       *dir;
    lookup_t   key;
    counter_t *val_p;

    snprintf(objmap_path, sizeof(objmap_path), "/proc/%d/object_map", pid);

    did_once = 0;
    i        = 0;
    while ((dir = opendir(objmap_path)) != NULL) {
        did_once = 1;

        start_ms = measure_time_now_ms();

        get_objects(dir, objmap_path);
        get_accesses();
        closedir(dir);

        elap_ms = measure_time_now_ms() - start_ms;
        report_interval(i, elap_ms);

        /* Reset the interval stores. */
        tree_free(addr_tree);
        hash_table_traverse(counters, key, val_p) {
            (void)val_p;
            if (key.kind == KEY_BYTES) {
                free(key.addr_OR_coop_buff_bytes);
            }
        }
        hash_table_free(counters);
        counters = hash_table_make(lookup_t, counter_t, hash);
        addr_tree = tree_make(u64, object_t);

        i += 1;

        elap_ms = measure_time_now_ms() - start_ms;

        /* Sleep for the remainder of the interval. */
        if (elap_ms <= i_ms) {
            usleep(1000 * (i_ms - elap_ms));
        }
    }

    if (!did_once) {
        fprintf(stderr, "error opening object_map path: '%s' -- error %d\n", objmap_path, errno);
    }

    return 1;
}
