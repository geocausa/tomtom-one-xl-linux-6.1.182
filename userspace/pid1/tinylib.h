#ifndef TINYLIB_H
#define TINYLIB_H

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define O_RDONLY   00
#define O_WRONLY   01
#define O_RDWR     02
#define O_CREAT    0100
#define O_TRUNC    01000
#define O_NONBLOCK 04000
#define O_DIRECTORY 00200000

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define WNOHANG 1

#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_ABS 0x03

#define BTN_TOUCH 0x14a

#define ABS_X 0x00
#define ABS_Y 0x01
#define ABS_PRESSURE 0x18

struct input_event {
    struct { long tv_sec; long tv_usec; } time;
    unsigned short type;
    unsigned short code;
    int value;
};

static inline long sys_write(int fd, const void *buf, unsigned long count) {
    register long r0 asm("r0") = (long)fd;
    register const void *r1 asm("r1") = buf;
    register unsigned long r2 asm("r2") = count;
    register int r7 asm("r7") = 4;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}

static inline long sys_read(int fd, void *buf, unsigned long count) {
    register long r0 asm("r0") = (long)fd;
    register void *r1 asm("r1") = buf;
    register unsigned long r2 asm("r2") = count;
    register int r7 asm("r7") = 3;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}

static inline int sys_open(const char *path, int flags, int mode) {
    register long r0 asm("r0") = (long)path;
    register int r1 asm("r1") = flags;
    register int r2 asm("r2") = mode;
    register int r7 asm("r7") = 5;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r2), "r"(r7) : "memory");
    return (int)r0;
}

static inline int sys_ioctl(int fd, unsigned int cmd, unsigned long arg) {
    register long r0 asm("r0") = (long)fd;
    register unsigned int r1 asm("r1") = cmd;
    register unsigned long r2 asm("r2") = arg;
    register int r7 asm("r7") = 54;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}

static inline int sys_close(int fd) {
    register long r0 asm("r0") = (long)fd;
    register int r7 asm("r7") = 6;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r7) : "memory");
    return (int)r0;
}

static inline long sys_lseek(int fd, long offset, int whence) {
    register long r0 asm("r0") = fd;
    register long r1 asm("r1") = offset;
    register int r2 asm("r2") = whence;
    register int r7 asm("r7") = 19; // __NR_lseek on ARM
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}

static inline int sys_mount(const char *src, const char *target, const char *fstype, unsigned long flags, const void *data) {
    register long r0 asm("r0") = (long)src;
    register long r1 asm("r1") = (long)target;
    register long r2 asm("r2") = (long)fstype;
    register unsigned long r3 asm("r3") = flags;
    register long r4 asm("r4") = (long)data;
    register int r7 asm("r7") = 21;
    asm volatile ("swi 0"
        : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3)
        : "r"(r4), "r"(r7)
        : "memory");
    return (int)r0;
}

static inline int sys_mkdir(const char *path, int mode) {
    register long r0 asm("r0") = (long)path;
    register int r1 asm("r1") = mode;
    register int r7 asm("r7") = 39;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r7) : "memory");
    return (int)r0;
}

static inline int sys_mknod(const char *pathname, int mode, unsigned dev) {
    register long r0 asm("r0") = (long)pathname;
    register long r1 asm("r1") = mode;
    register long r2 asm("r2") = dev;
    register int r7 asm("r7") = 14;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r2), "r"(r7) : "memory");
    return (int)r0;
}

static inline int sys_nanosleep(const void *req, void *rem) {
    register long r0 asm("r0") = (long)req;
    register long r1 asm("r1") = (long)rem;
    register int r7 asm("r7") = 162;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r7) : "memory");
    return (int)r0;
}

static inline int sys_fork(void) {
    register long r0 asm("r0");
    register int r7 asm("r7") = 2;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r7) : "memory");
    return (int)r0;
}

static inline int sys_execve(const char *filename, char *const argv[], char *const envp[]) {
    register long r0 asm("r0") = (long)filename;
    register long r1 asm("r1") = (long)argv;
    register long r2 asm("r2") = (long)envp;
    register int r7 asm("r7") = 11;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r2), "r"(r7) : "memory");
    return (int)r0;
}

static inline int sys_dup2(int oldfd, int newfd) {
    register long r0 asm("r0") = oldfd;
    register long r1 asm("r1") = newfd;
    register int r7 asm("r7") = 63; // __NR_dup2
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r7) : "memory");
    return (int)r0;
}

static inline int sys_pipe(int pipefd[2]) {
    register long r0 asm("r0") = (long)pipefd;
    register int r7 asm("r7") = 42; // __NR_pipe
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r7) : "memory");
    return (int)r0;
}

static inline int sys_wait4(int pid, int *status, int options, void *rusage) {
    register long r0 asm("r0") = pid;
    register long r1 asm("r1") = (long)status;
    register long r2 asm("r2") = options;
    register long r3 asm("r3") = (long)rusage;
    register int r7 asm("r7") = 114;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r7) : "memory");
    return (int)r0;
}

static inline int sys_symlink(const char *oldpath, const char *newpath) {
    register long r0 asm("r0") = (long)oldpath;
    register long r1 asm("r1") = (long)newpath;
    register int r7 asm("r7") = 83; // __NR_symlink
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r7) : "memory");
    return (int)r0;
}

static inline int sys_unlink(const char *pathname) {
    register long r0 asm("r0") = (long)pathname;
    register int r7 asm("r7") = 10;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r7) : "memory");
    return (int)r0;
}

struct linux_dirent64 {
    unsigned long long d_ino;
    long long          d_off;
    unsigned short     d_reclen;
    unsigned char      d_type;
    char               d_name[];
};

static inline int sys_getdents64(int fd, void *dirp, unsigned int count) {
    register long r0 asm("r0") = (long)fd;
    register void *r1 asm("r1") = dirp;
    register unsigned int r2 asm("r2") = count;
    register int r7 asm("r7") = 217;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r2), "r"(r7) : "memory");
    return (int)r0;
}

static inline int sys_reboot(int magic1, int magic2, int cmd, void *arg) {
    register long r0 asm("r0") = magic1;
    register long r1 asm("r1") = magic2;
    register long r2 asm("r2") = cmd;
    register void *r3 asm("r3") = (void *)arg;
    register int r7 asm("r7") = 88;
    asm volatile ("swi 0" : "=r"(r0) : "r"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r7) : "memory");
    return (int)r0;
}

static inline void sys_exit(int status) {
    register long r0 asm("r0") = status;
    register int r7 asm("r7") = 1;
    asm volatile ("swi 0" : : "r"(r0), "r"(r7) : "memory");
    while (1);
}

static inline void sleep_ms(int ms) {
    struct { long sec; long nsec; } ts = { ms / 1000, (ms % 1000) * 1000000 };
    sys_nanosleep(&ts, 0);
}

static inline int tiny_strlen(const char *s) {
    int len = 0;
    while (*s++) {
        len++;
        __asm__ __volatile__("" : "+r"(s));
    }
    return len;
}

static inline void *tiny_memcpy(void *dest, const void *src, unsigned long n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) {
        *d++ = *s++;
        __asm__ __volatile__("" : "+r"(d), "+r"(s));
    }
    return dest;
}

static inline void *tiny_memset(void *dest, int c, unsigned long n) {
    unsigned char *d = (unsigned char *)dest;
    unsigned char val = (unsigned char)c;
    while (n--) {
        *d++ = val;
        __asm__ __volatile__("" : "+r"(d));
    }
    return dest;
}

void *memcpy(void *dest, const void *src, unsigned long n) __attribute__((weak, alias("tiny_memcpy")));
void *memset(void *dest, int c, unsigned long n) __attribute__((weak, alias("tiny_memset")));
int strlen(const char *s) __attribute__((weak, alias("tiny_strlen")));

static inline int tiny_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static inline void sys_sync(void) {
    register int r7 asm("r7") = 36;
    asm volatile ("swi 0" : : "r"(r7) : "memory");
}

/* Fast Safe Software Integer Division for ARMv4T */
static inline unsigned int udiv(unsigned int num, unsigned int den) {
    if (den == 0) return 0;
    unsigned int quot = 0, qbit = 1;
    while (den < num && (den & 0x80000000) == 0) {
        den <<= 1;
        qbit <<= 1;
    }
    while (qbit > 0) {
        if (num >= den) {
            num -= den;
            quot |= qbit;
        }
        den >>= 1;
        qbit >>= 1;
    }
    return quot;
}

static inline int int_div(int num, int den) {
    if (den == 0) return 0;
    int sign = 1;
    if (num < 0) { num = -num; sign = -sign; }
    if (den < 0) { den = -den; sign = -sign; }
    int res = (int)udiv((unsigned int)num, (unsigned int)den);
    return (sign < 0) ? -res : res;
}

static inline int int_mod(int num, int den) {
    if (den == 0) return 0;
    return num - (int_div(num, den) * den);
}

#endif
