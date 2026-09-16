#define __EXPORTED_HEADERS__ 1
#include <sys/ioctl.h>
#include <sound/asound.h>

typedef unsigned int u32;
#define NR_exit 1
#define NR_read 3
#define NR_write 4
#define NR_open 5
#define NR_close 6
#define NR_ioctl 54
#define O_RDWR 2

static long sc1(long n,long a){ register long r0 asm("r0")=a; register long r7 asm("r7")=n; asm volatile("svc 0":"+r"(r0):"r"(r7):"memory"); return r0; }
static long sc3(long n,long a,long b,long c){ register long r0 asm("r0")=a; register long r1 asm("r1")=b; register long r2 asm("r2")=c; register long r7 asm("r7")=n; asm volatile("svc 0":"+r"(r0):"r"(r1),"r"(r2),"r"(r7):"memory"); return r0; }
static unsigned slen(const char*s){ unsigned n=0; while(s[n]) n++; return n; }
static void out(const char*s){ sc3(NR_write,1,(long)s,slen(s)); }
static void zero(void *p,unsigned n){ unsigned char*q=p; while(n--)*q++=0; }
static int seq(const unsigned char*a,const char*b){ int i=0; while(b[i]){ if(a[i]!=(unsigned char)b[i]) return 0; i++; } return a[i]==0; }
static int parse(const char*s){ int v=0; while(*s>='0'&&*s<='9'){v=v*10+(*s-'0');s++;} return v; }
static void outnum(int v){
    char b[4]; int n=0, tens=0;
    if(v>=100){ b[n++]='1'; v-=100; }
    while(v>=10){ v-=10; tens++; }
    if(n || tens) b[n++]='0'+tens;
    b[n++]='0'+v;
    b[n++]='\n';
    sc3(NR_write,1,(long)b,n);
}


static int get_control(int fd, struct snd_ctl_elem_id *found)
{
    struct snd_ctl_elem_id ids[32];
    struct snd_ctl_elem_list list;
    zero(&list,sizeof(list)); zero(ids,sizeof(ids));
    list.offset=0; list.space=32; list.pids=ids;
    if(sc3(NR_ioctl,fd,SNDRV_CTL_IOCTL_ELEM_LIST,(long)&list)<0) return -1;
    for(unsigned i=0;i<list.used;i++) {
        if(seq(ids[i].name,"Master Playback Volume")) { *found=ids[i]; return 0; }
    }
    return -1;
}

static int readvol(int fd, const struct snd_ctl_elem_id *id)
{
    struct snd_ctl_elem_value v; zero(&v,sizeof(v)); v.id=*id;
    if(sc3(NR_ioctl,fd,SNDRV_CTL_IOCTL_ELEM_READ,(long)&v)<0) return -1;
    return (int)v.value.integer.value[0];
}
static int writevol(int fd, const struct snd_ctl_elem_id *id, int vol)
{
    struct snd_ctl_elem_value v; zero(&v,sizeof(v)); v.id=*id;
    v.value.integer.value[0]=vol; v.value.integer.value[1]=vol;
    return sc3(NR_ioctl,fd,SNDRV_CTL_IOCTL_ELEM_WRITE,(long)&v)<0 ? -1 : 0;
}

void start_c(unsigned long *sp) __attribute__((noreturn,used,noinline));
__attribute__((naked,noreturn)) void _start(void)
{
    __asm__ volatile("mov r0, sp\n\tb start_c");
}

void start_c(unsigned long *sp)
{
    int argc=(int)sp[0];
    char **argv=(char **)&sp[1];
    int fd=(int)sc3(NR_open,(long)"/dev/snd/controlC0",O_RDWR,0);
    if(fd<0){out("volume: no controlC0\n");sc1(NR_exit,1);}
    struct snd_ctl_elem_id id; zero(&id,sizeof(id));
    if(get_control(fd,&id)<0){out("volume: control not found\n");sc1(NR_close,fd);sc1(NR_exit,2);}
    int cur=readvol(fd,&id); if(cur<0) cur=100;
    int next=cur;
    if(argc>=2){
        if(argv[1][0]=='u') next=cur+5;
        else if(argv[1][0]=='d') next=cur-5;
        else if(argv[1][0]=='s' && argc>=3) next=parse(argv[2]);
    }
    if(next<0) next=0; if(next>121) next=121;
    if(next!=cur && writevol(fd,&id,next)<0){out("volume: write failed\n");sc1(NR_close,fd);sc1(NR_exit,3);}
    sc1(NR_close,fd);
    outnum(next);
    sc1(NR_exit,0);
    for (;;) { }
}
