#include "process.h"
#include "common.h"
#include "config.h"
#include "array.h"

#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

hash_table(pid_t, md_proc) process_table;
pthread_rwlock_t           process_table_rwlock = PTHREAD_RWLOCK_INITIALIZER;

static pthread_t process_discovery_pthread;

#define WLOCK()  (pthread_rwlock_wrlock(&process_table_rwlock))

static u64 pid_hash(pid_t pid) { return (u64)pid; }

int init_process_table(void) {
    process_table = hash_table_make(pid_t, md_proc, pid_hash);
    return 0;
}

md_proc *acquire_proccess(pid_t pid) {
    md_proc *proc;

    RLOCK();

    proc = hash_table_get_val(process_table, pid);

    if (proc == NULL) {
        UNLOCK();
        return NULL;
    }

    return proc;
}

void release_proccess(md_proc *proc) {
    if (proc == NULL) { return; }

    UNLOCK();
}

static void *process_discovery_thread(void *arg) {
    array_t        dead;
    u64            start;
    DIR           *slash_proc_dir;
    struct dirent *proc_dent;
    pid_t          pid;
    char           objmap_path[PATH_MAX];
    DIR           *objmap_dir;
    struct dirent *objmap_dent;
    md_proc       *proc;
    md_proc        new_proc;
    pid_t         *pit;
    u64            elapsed;

    dead = array_make(pid_t);

    for (;;) {
        array_clear(dead);

        start = measure_time_now_ms();

        /* Mark all processes as dead. We will check if they're still alive below. */
        PROCESS_FOR(proc, { proc->alive = 0; });

        slash_proc_dir = opendir("/proc");
        if (slash_proc_dir == NULL) {
            fprintf(stderr, "error opening /proc -- errno = %d\n", errno);
            errno = 0;
            goto out;
        }

        while ((proc_dent = readdir(slash_proc_dir)) != NULL) {
            if (sscanf(proc_dent->d_name, "%d", &pid) != 1) { continue; }

            snprintf(objmap_path, sizeof(objmap_path),
                     "/proc/%s/object_map", proc_dent->d_name);

            if ((objmap_dir = opendir(objmap_path)) != NULL) {
                while ((objmap_dent = readdir(objmap_dir))) {
                    if (strcmp(".",          objmap_dent->d_name) == 0
                    ||  strcmp("..",         objmap_dent->d_name) == 0
                    ||  strcmp("controller", objmap_dent->d_name) == 0) {
                        continue;
                    }

                    WLOCK(); {
                        proc = hash_table_get_val(process_table, (pid_t)pid);
                        if (proc != NULL) {
                            /* The process is still running. Keep it for another round. */
                            proc->alive = 1;
                        } else {
                            /* A new process.. add it. */
                            new_proc.pid   = pid;
                            new_proc.alive = 1;
                            hash_table_insert(process_table, new_proc.pid, new_proc);
                        }
                    } UNLOCK();

                    break;
                }
            }

            closedir(objmap_dir);
        }

        closedir(slash_proc_dir);

        /* Collect process IDs that aren't alive any more. */
        PROCESS_FOR(proc, {
            if (!proc->alive) {
                array_push(dead, proc->pid);
            }
        });

        /* Clear them out of the table. */
        if (array_len(dead)) {
            WLOCK(); {
                array_traverse(dead, pit) {
                    hash_table_delete(process_table, *pit);
                }
            } UNLOCK();
        }

        elapsed = measure_time_now_ms() - start;
        if (elapsed < config.proc_discovery_period_ms) {
            usleep(1000 * (config.proc_discovery_period_ms - elapsed));
        }
    }

out:;
    return NULL;
}

int start_process_discovery_thread(void) {
    int status;
    status = pthread_create(&process_discovery_pthread, NULL, process_discovery_thread, NULL);
    if (status != 0) {
        fprintf(stderr, "failed to create process discovery thread\n");
    }
    return status;
}
