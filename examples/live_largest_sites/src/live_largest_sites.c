#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <math.h>
#include <stdint.h>
#include <sys/time.h>

#define u64 uint64_t

#include "tree.h"
#define PROC_OBJECT_MAP_IMPL
#include "proc_object_map.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

typedef char *str;
use_tree(str, u64);
use_tree(u64, str);

int            pid;
int            i_ms;
tree(str, u64) sites;

void   usage(char *argv0);
int    do_args(int argc, char **argv);
char * pretty_bytes(u64 n_bytes);
u64    measure_time_now_ms(void);
int    do_loop(void);


int main(int argc, char **argv) {
    if (!do_args(argc, argv)) { return 1; }

    sites = tree_make_c(str, u64, strcmp);

    if (!do_loop()) { return 1; }

    tree_free(sites);

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

char * pretty_bytes(uint64_t n_bytes) {
    uint64_t    s;
    double      count;
    char       *r;
    const char *suffixes[]
        = { " B", " KB", " MB", " GB", " TB", " PB", " EB" };

    s     = 0;
    count = (double)n_bytes;

    while (count >= 1024 && s < 7) {
        s     += 1;
        count /= 1024;
    }

    r = calloc(64, 1);

    if (count - floor(count) == 0.0) {
        sprintf(r, "%d", (int)count);
    } else {
        sprintf(r, "%.2f", count);
    }

    strcat(r, suffixes[s]);

    return r;
}

u64 measure_time_now_ms(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return 1000ULL * tv.tv_sec + (tv.tv_usec / 1000ULL);
}

int do_loop(void) {
    u64                              start_ms;
    u64                              elap_ms;
    char                             objmap_path[4096 - 256];
    int                              did_once;
    DIR                             *dir;
    tree_it(str, u64)                site_it;
    struct dirent                   *dirent;
    char                             entry_path[4096];
    int                              fd;
    int                              err;
    struct proc_object_map_record_t  record;
    char                             site_buff[4096];
    int                              n_coop_read;
    int                              i;
    char                            *pretty;

    snprintf(objmap_path, sizeof(objmap_path), "/proc/%d/object_map", pid);

    did_once = 0;
    i        = 0;
    while ((dir = opendir(objmap_path)) != NULL) {
        did_once = 1;

        start_ms = measure_time_now_ms();

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
                n_coop_read = MIN(record.coop_buff_n_bytes, sizeof(site_buff) - 1);
                if (n_coop_read) {
                    err = objmap_entry_read_record_coop_buff(entry_path, site_buff, n_coop_read);

                    if (err == 0) {
                        site_buff[n_coop_read] = 0;

                        site_it = tree_lookup(sites, site_buff);
                        if (!tree_it_good(site_it)) {
                            site_it = tree_insert(sites, strdup(site_buff), 0);
                        }
                        tree_it_val(site_it) += record.n_resident_pages;
                    }
                }
            }

            close(fd);
        }

        closedir(dir);

        printf("interval %d\n", i);
        tree_traverse(sites, site_it) {
            printf("  %s,%d\n", tree_it_key(site_it), tree_it_val(site_it) * 4096);

            /* Reset the data. */
            char *tmp = tree_it_key(site_it);
            tree_delete(sites, tmp);
            free(tmp);
        }


        i += 1;

        elap_ms = measure_time_now_ms() - start_ms;

        if (elap_ms <= i_ms) {
            usleep(1000 * (i_ms - elap_ms));
        }
    }

    if (!did_once) {
        fprintf(stderr, "error opening object_map path: '%s' -- error %d\n", objmap_path, errno);
    }

    return 1;
}
