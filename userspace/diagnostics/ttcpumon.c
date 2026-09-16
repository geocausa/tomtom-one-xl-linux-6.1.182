#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct cpu_sample {
    unsigned long long total;
    unsigned long long idle;
};

struct proc_sample {
    unsigned long long bluealsa;
    unsigned long long aplay;
    unsigned long long bluetoothd;
    unsigned long long dbus;
    unsigned long long agent;
};

static int read_cpu(struct cpu_sample *s) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;
    char line[512];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    fclose(f);

    unsigned long long u=0,n=0,sy=0,id=0,iw=0,irq=0,sirq=0,st=0;
    int nr = sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
        &u,&n,&sy,&id,&iw,&irq,&sirq,&st);
    if (nr < 4) return -1;
    s->idle = id + iw;
    s->total = u + n + sy + id + iw + irq + sirq + st;
    return 0;
}

static int read_proc_ticks(const char *pid, unsigned long long *ticks) {
    char path[128], buf[2048];
    snprintf(path, sizeof(path), "/proc/%.20s/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    fclose(f);

    char *rparen = strrchr(buf, ')');
    if (!rparen || rparen[1] != ' ') return -1;
    char *p = rparen + 2;

    int field = 3;
    unsigned long long utime=0, stime=0;
    char *save = NULL;
    for (char *tok = strtok_r(p, " ", &save); tok; tok = strtok_r(NULL, " ", &save), field++) {
        if (field == 14) utime = strtoull(tok, NULL, 10);
        else if (field == 15) { stime = strtoull(tok, NULL, 10); break; }
    }
    *ticks = utime + stime;
    return 0;
}

static int read_comm(const char *pid, char *comm, size_t n) {
    char path[128];
    snprintf(path, sizeof(path), "/proc/%.20s/comm", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(comm, n, f)) { fclose(f); return -1; }
    fclose(f);
    comm[strcspn(comm, "\r\n")] = 0;
    return 0;
}

static void read_procs(struct proc_sample *s) {
    memset(s, 0, sizeof(*s));
    DIR *d = opendir("/proc");
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d))) {
        if (!isdigit((unsigned char)de->d_name[0])) continue;
        char comm[64];
        unsigned long long ticks;
        if (read_comm(de->d_name, comm, sizeof(comm)) != 0) continue;
        if (read_proc_ticks(de->d_name, &ticks) != 0) continue;

        if (strncmp(comm, "bluealsa-aplay", strlen("bluealsa-aplay")) == 0)
            s->aplay += ticks;
        else if (strncmp(comm, "bluealsa", strlen("bluealsa")) == 0)
            s->bluealsa += ticks;
        else if (strncmp(comm, "bluetoothd", strlen("bluetoothd")) == 0)
            s->bluetoothd += ticks;
        else if (strncmp(comm, "dbus-daemon", strlen("dbus-daemon")) == 0)
            s->dbus += ticks;
        else if (strncmp(comm, "ttbluez-agent", strlen("ttbluez-agent")) == 0)
            s->agent += ticks;
    }
    closedir(d);
}

static double pct(unsigned long long now, unsigned long long prev,
                  unsigned long long total_delta) {
    if (total_delta == 0 || now < prev) return 0.0;
    return 100.0 * (double)(now - prev) / (double)total_delta;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "/run/TTCPU.LOG";
    FILE *out = fopen(path, "w");
    if (!out) {
        fprintf(stderr, "ttcpumon: %s: %s\n", path, strerror(errno));
        return 2;
    }
    setvbuf(out, NULL, _IOLBF, 0);

    struct cpu_sample c0, c1;
    struct proc_sample p0, p1;
    if (read_cpu(&c0) != 0) return 3;
    read_procs(&p0);

    fprintf(out, "sec cpu_busy bluealsa aplay bluetoothd dbus agent\n");

    unsigned long sec = 0;
    for (;;) {
        sleep(1);
        if (read_cpu(&c1) != 0) break;
        read_procs(&p1);

        unsigned long long td = c1.total - c0.total;
        unsigned long long idle = c1.idle - c0.idle;
        double busy = td ? 100.0 * (double)(td - idle) / (double)td : 0.0;

        fprintf(out, "%lu %.1f %.1f %.1f %.1f %.1f %.1f\n",
            ++sec, busy,
            pct(p1.bluealsa, p0.bluealsa, td),
            pct(p1.aplay, p0.aplay, td),
            pct(p1.bluetoothd, p0.bluetoothd, td),
            pct(p1.dbus, p0.dbus, td),
            pct(p1.agent, p0.agent, td));

        c0 = c1;
        p0 = p1;
    }
    fclose(out);
    return 0;
}
