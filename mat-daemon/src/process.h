#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "hash_table.h"
#include "profile.h"

#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>

typedef struct {
    int             alive;
    pid_t           pid;
    char            objmap_path[PATH_MAX];
} md_proc;

use_hash_table(pid_t, md_proc);

extern hash_table(pid_t, md_proc) process_table;
extern pthread_rwlock_t           process_table_rwlock;

#define RLOCK()  (pthread_rwlock_rdlock(&process_table_rwlock))
#define UNLOCK() (pthread_rwlock_unlock(&process_table_rwlock))

int      init_process_table(void);
md_proc *acquire_proccess(pid_t pid);
void     release_proccess(md_proc *proc);
int      start_process_discovery_thread(void);

#define PROCESS_FOR(proc, ...)                                       \
do {                                                                 \
    pid_t _pid;                                                      \
    (void)_pid;                                                      \
    RLOCK();                                                         \
        hash_table_traverse(process_table, _pid, (proc)) __VA_ARGS__ \
    UNLOCK();                                                        \
} while (0)

#endif
