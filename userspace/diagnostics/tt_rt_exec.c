#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: tt-rt-exec PRIORITY PROGRAM [ARGS...]\n");
        return 2;
    }

    int prio = atoi(argv[1]);
    struct sched_param sp = { .sched_priority = prio };
    if (sched_setscheduler(0, SCHED_FIFO, &sp) == -1)
        fprintf(stderr, "tt-rt-exec: SCHED_FIFO %d failed: %s; continuing normally\n",
            prio, strerror(errno));
    else
        fprintf(stderr, "tt-rt-exec: SCHED_FIFO %d enabled\n", prio);

    execv(argv[2], &argv[2]);
    fprintf(stderr, "tt-rt-exec: exec %s failed: %s\n", argv[2], strerror(errno));
    return 127;
}
