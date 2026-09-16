/*
 * Robust C Master Supervisor for TomTom ONE XL (Linux 6.1 LTS)
 * Mounts filesystems, configures network & daemons, supervises ttlauncher.
 */

#include "tinylib.h"

static int kmsg_fd = -1;

static inline void kmsg_print(const char *s) {
    int len = 0;
    while (s[len]) len++;
    if (kmsg_fd >= 0) sys_write(kmsg_fd, s, len);
}

static char *const default_envp[] = {
    "PATH=/bin:/sbin:/usr/bin:/usr/sbin",
    "HOME=/root",
    "TERM=linux",
    0
};

static void run_command(char *const argv[]) {
    int pid = sys_fork();
    if (pid == 0) {
        sys_execve(argv[0], argv, default_envp);
        sys_exit(1);
    } else if (pid > 0) {
        int status = 0;
        sys_wait4(pid, &status, 0, 0);
    }
}

static inline void print_int(int val) {
    if (val < 0) {
        kmsg_print("-");
        val = -val;
    }
    if (val == 0) {
        kmsg_print("0");
        return;
    }
    char buf[16];
    char tmp[16];
    int pos = 0, tpos = 0;
    while (val > 0) {
        tmp[tpos++] = '0' + int_mod(val, 10);
        val = int_div(val, 10);
    }
    while (tpos > 0) {
        buf[pos++] = tmp[--tpos];
    }
    buf[pos] = 0;
    kmsg_print(buf);
}

static int spawn_daemon(char *const argv[]) {
    int pid = sys_fork();
    if (pid == 0) {
        int null_fd = sys_open("/dev/null", O_RDWR, 0);
        if (null_fd >= 0) {
            sys_dup2(null_fd, 0);
            if (kmsg_fd >= 0) {
                sys_dup2(kmsg_fd, 1);
                sys_dup2(kmsg_fd, 2);
            }
        }
        sys_execve(argv[0], argv, default_envp);
        sys_exit(1);
    }
    return pid;
}

void _start() {
    /* 1. Mount virtual filesystems */
    sys_mkdir("/proc", 0755);
    sys_mkdir("/sys", 0755);
    sys_mkdir("/dev", 0755);
    sys_mkdir("/var", 0755);
    sys_mkdir("/var/run", 0755);
    sys_mkdir("/var/log", 0755);
    sys_mkdir("/tmp", 0777);
    sys_mkdir("/etc", 0755);
    sys_mkdir("/etc/dropbear", 0755);
    sys_mkdir("/root", 0755);
    sys_mkdir("/root/.ssh", 0700);
    sys_mkdir("/sdcard", 0755);
    sys_mkdir("/mnt", 0755);
    sys_mkdir("/mnt/flash", 0755);
    sys_mkdir("/mnt/flash/sysfile", 0755);

    sys_mount("proc", "/proc", "proc", 0, 0);
    sys_mount("sysfs", "/sys", "sysfs", 0, 0);
    sys_mount("devtmpfs", "/dev", "devtmpfs", 0, 0);

    sys_mkdir("/dev/pts", 0755);
    sys_mount("devpts", "/dev/pts", "devpts", 0, 0);

    kmsg_fd = sys_open("/dev/kmsg", O_WRONLY, 0);

    kmsg_print("\n\n=======================================================\n");
    kmsg_print("<7>  TOMTOM ONE XL: LINUX 6.1 SYSTEM BOOT (PID 1)\n");
    kmsg_print("=======================================================\n");

    /* Guarantee device nodes in /dev */
    sys_mknod("/dev/console",  0020600, (5 << 8) | 1);
    sys_mknod("/dev/null",     0020666, (1 << 8) | 3);
    sys_mknod("/dev/zero",     0020666, (1 << 8) | 5);
    sys_mknod("/dev/urandom",  0020666, (1 << 8) | 9);
    sys_mknod("/dev/fb0",      0020666, (29 << 8) | 0);
    sys_mknod("/dev/tty0",     0020666, (4 << 8) | 0);
    sys_mknod("/dev/ptmx",     0020666, (5 << 8) | 2);
    sys_mknod("/dev/pts/ptmx", 0020666, (5 << 8) | 2);
    sys_mknod("/dev/ttySAC0",  0020660, (204 << 8) | 64);
    sys_mknod("/dev/ttySAC1",  0020660, (204 << 8) | 65);
    sys_mknod("/dev/ttySAC2",  0020660, (204 << 8) | 66);
    sys_mknod("/dev/hwstatus", 0020666, (240 << 8) | 0);
    sys_symlink("/dev/fb0", "/dev/fb");
    sys_symlink("/dev/ttySAC2", "/dev/bt");

    /* Completely disable VT blinking text cursor on framebuffer console */
    int tty_fd = sys_open("/dev/tty0", O_RDWR, 0);
    if (tty_fd >= 0) {
        sys_write(tty_fd, "\033[?25l", 6);
        sys_ioctl(tty_fd, 0x4B3A, 1); /* KDSETMODE KD_GRAPHICS */
        sys_close(tty_fd);
    }
    int con_fd = sys_open("/dev/console", O_RDWR, 0);
    if (con_fd >= 0) {
        sys_write(con_fd, "\033[?25l", 6);
        sys_ioctl(con_fd, 0x4B3A, 1);
        sys_close(con_fd);
    }

    sys_mkdir("/dev/input", 0755);
    sys_mknod("/dev/input/event0", 0020660, (13 << 8) | 64);
    sys_mknod("/dev/input/event1", 0020660, (13 << 8) | 65);
    sys_mknod("/dev/input/event2", 0020660, (13 << 8) | 66);
    sys_mknod("/dev/input/event3", 0020660, (13 << 8) | 67);
    sys_symlink("/dev/input/event0", "/dev/ts");

    /* Storage device nodes (MMC/SD block major 179) */
    sys_mknod("/dev/mmcblk0",   0060660, (179 << 8) | 0);
    sys_mknod("/dev/mmcblk0p1", 0060660, (179 << 8) | 1);
    sys_mknod("/dev/mmcblk0p2", 0060660, (179 << 8) | 2);
    sys_mknod("/dev/mmcblk1",   0060660, (179 << 8) | 8);
    sys_mknod("/dev/mmcblk1p1", 0060660, (179 << 8) | 9);

    /* Try mounting SD card */
    int sd_pid = sys_fork();
    if (sd_pid == 0) {
        int mounted = 0;
        for (int retry = 0; retry < 20; retry++) {
            sleep_ms(300);
            if (sys_mount("/dev/mmcblk0p1", "/sdcard", "vfat", 0, 0) == 0 ||
                sys_mount("/dev/mmcblk0", "/sdcard", "vfat", 0, 0) == 0 ||
                sys_mount("/dev/mmcblk1p1", "/sdcard", "vfat", 0, 0) == 0 ||
                sys_mount("/dev/mmcblk1", "/sdcard", "vfat", 0, 0) == 0) {
                mounted = 1;
                break;
            }
        }
        if (mounted) {
            kmsg_print("<7>[SD] SD card mounted at /sdcard\n");
        } else {
            kmsg_print("<7>[SD] SD card mount timed out\n");
        }
        sys_exit(0);
    }

    /* Start Handheld App Launcher on Framebuffer */
    sleep_ms(100);
    kmsg_print("<7>[UI] Spawning /usr/bin/ttlauncher...\n");
    char *launcher_args[] = { "/usr/bin/ttlauncher", 0 };
    int launcher_pid = spawn_daemon(launcher_args);
    kmsg_print("<7>[UI] launcher spawned with pid=");
    print_int(launcher_pid);
    kmsg_print("\n");

    /* Master Supervision Loop */
    while (1) {
        int status = 0;
        int dead_pid = 0;
        while ((dead_pid = sys_wait4(-1, &status, WNOHANG, 0)) > 0) {
            if (dead_pid == launcher_pid) {
                kmsg_print("<1>[UI] ttlauncher died with status=");
                print_int(status);
                kmsg_print("\n");
                sleep_ms(1000);
                launcher_pid = spawn_daemon(launcher_args);
            }
        }
        sleep_ms(1000);
    }
}
