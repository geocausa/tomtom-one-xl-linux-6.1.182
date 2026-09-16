#define __EXPORTED_HEADERS__ 1
#include <sys/ioctl.h>
#include <sound/asound.h>

typedef signed short s16;
#define NR_exit 1
#define NR_open 5
#define NR_close 6
#define NR_ioctl 54
#define NR_write 4
#define O_WRONLY 1

static long sc1(long n,long a){ register long r0 asm("r0")=a; register long r7 asm("r7")=n; asm volatile("svc 0":"+r"(r0):"r"(r7):"memory"); return r0; }
static long sc3(long n,long a,long b,long c){ register long r0 asm("r0")=a; register long r1 asm("r1")=b; register long r2 asm("r2")=c; register long r7 asm("r7")=n; asm volatile("svc 0":"+r"(r0):"r"(r1),"r"(r2),"r"(r7):"memory"); return r0; }
static unsigned slen(const char*s){ unsigned n=0; while(s[n]) n++; return n; }
static void out(const char*s){ sc3(NR_write,1,(long)s,slen(s)); }
static void zero(void *p, unsigned n){ unsigned char *q=p; while(n--) *q++=0; }

static const char *errname(long rc)
{
    if (rc == -5) return "EIO(-5)";
    if (rc == -6) return "ENXIO(-6)";
    if (rc == -11) return "EAGAIN(-11)";
    if (rc == -12) return "ENOMEM(-12)";
    if (rc == -14) return "EFAULT(-14)";
    if (rc == -16) return "EBUSY(-16)";
    if (rc == -19) return "ENODEV(-19)";
    if (rc == -22) return "EINVAL(-22)";
    if (rc == -32) return "EPIPE(-32)";
    if (rc == -77) return "EBADFD(-77)";
    return "UNKNOWN";
}
static void fail(const char *where,long rc,int code){ out("tone: "); out(where); out(" failed: "); out(errname(rc)); out("\n"); sc1(NR_exit,code); }

static void mask_one(struct snd_mask *m, unsigned bit){
    unsigned i; for(i=0;i<sizeof(m->bits)/sizeof(m->bits[0]);i++) m->bits[i]=0;
    m->bits[bit>>5] = 1U << (bit & 31);
}
static void interval_any(struct snd_interval *i){ zero(i,sizeof(*i)); i->min=0; i->max=~0U; }
static void interval_one(struct snd_interval *i, unsigned v){ zero(i,sizeof(*i)); i->min=v; i->max=v; i->integer=1; }
static struct snd_mask *pmask(struct snd_pcm_hw_params *p,int var){ return &p->masks[var-SNDRV_PCM_HW_PARAM_FIRST_MASK]; }
static struct snd_interval *pint(struct snd_pcm_hw_params *p,int var){ return &p->intervals[var-SNDRV_PCM_HW_PARAM_FIRST_INTERVAL]; }
static void params_any(struct snd_pcm_hw_params *p){
    int m,v; unsigned i;
    zero(p,sizeof(*p));
    for(m=0;m<=SNDRV_PCM_HW_PARAM_LAST_MASK-SNDRV_PCM_HW_PARAM_FIRST_MASK;m++)
        for(i=0;i<sizeof(p->masks[m].bits)/sizeof(p->masks[m].bits[0]);i++) p->masks[m].bits[i]=~0U;
    for(v=SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;v<=SNDRV_PCM_HW_PARAM_LAST_INTERVAL;v++) interval_any(pint(p,v));
    p->rmask = p->cmask = 0x000fff07U;
    p->info = ~0U;
}
static long ioc(int fd,unsigned long cmd,void *arg){ return sc3(NR_ioctl,fd,cmd,(long)arg); }

void _start(void)
{
    int fd=(int)sc3(NR_open,(long)"/dev/snd/pcmC0D0p",O_WRONLY,0);
    if(fd<0) fail("open pcm",fd,1);

    struct snd_pcm_hw_params p;
    params_any(&p);
    mask_one(pmask(&p,SNDRV_PCM_HW_PARAM_ACCESS), SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    mask_one(pmask(&p,SNDRV_PCM_HW_PARAM_FORMAT), SNDRV_PCM_FORMAT_S16_LE);
    mask_one(pmask(&p,SNDRV_PCM_HW_PARAM_SUBFORMAT), SNDRV_PCM_SUBFORMAT_STD);
    interval_one(pint(&p,SNDRV_PCM_HW_PARAM_SAMPLE_BITS),16);
    interval_one(pint(&p,SNDRV_PCM_HW_PARAM_FRAME_BITS),32);
    interval_one(pint(&p,SNDRV_PCM_HW_PARAM_CHANNELS),2);
    interval_one(pint(&p,SNDRV_PCM_HW_PARAM_RATE),48000);
    interval_one(pint(&p,SNDRV_PCM_HW_PARAM_PERIOD_SIZE),256);
    interval_one(pint(&p,SNDRV_PCM_HW_PARAM_PERIODS),4);
    interval_one(pint(&p,SNDRV_PCM_HW_PARAM_BUFFER_SIZE),1024);

    long rc=ioc(fd,SNDRV_PCM_IOCTL_HW_PARAMS,&p);
    if(rc<0) fail("HW_PARAMS",rc,2);

    /* Prevent immediate start after the first tiny write. Fill the whole
     * 1024-frame buffer first, then ALSA starts playback automatically. */
    struct snd_pcm_sw_params sw;
    zero(&sw,sizeof(sw));
    sw.tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
    sw.period_step = 1;
    sw.avail_min = 256;
    sw.start_threshold = 1024;
    sw.stop_threshold = 1024;
    sw.silence_threshold = 0;
    sw.silence_size = 0;
    rc=ioc(fd,SNDRV_PCM_IOCTL_SW_PARAMS,&sw);
    if(rc<0) fail("SW_PARAMS",rc,3);

    rc=ioc(fd,SNDRV_PCM_IOCTL_PREPARE,0);
    if(rc<0) fail("PREPARE",rc,4);

    s16 buf[256*2];
    const int amps = 8000; /* ~24% full scale: obvious but conservative */
    const int halfperiods[3] = { 48, 24, 12 }; /* 500 Hz, 1 kHz, 2 kHz */
    const int blocks_per_tone = 94; /* about 0.50 seconds each */
    int toneidx;

    out("tone: 500 Hz -> 1 kHz -> 2 kHz square, amplitude 8000/32767\n");

    for (toneidx=0; toneidx<3; toneidx++) {
        int count=0, sign=1, block;
        int halfperiod=halfperiods[toneidx];
        for(block=0; block<blocks_per_tone; block++) {
            int i;
            for(i=0;i<256;i++) {
                int v = sign ? amps : -amps;
                buf[i*2]=(s16)v; buf[i*2+1]=(s16)v;
                count++;
                if(count==halfperiod) { count=0; sign=!sign; }
            }
            {
                struct snd_xferi x;
                x.result=0; x.buf=buf; x.frames=256;
                rc=ioc(fd,SNDRV_PCM_IOCTL_WRITEI_FRAMES,&x);
                if(rc<0) fail("WRITEI ioctl",rc,5);
                if(x.result<0) fail("WRITEI result",x.result,6);
            }
        }
    }
    rc=ioc(fd,SNDRV_PCM_IOCTL_DRAIN,0);
    if(rc<0) fail("DRAIN",rc,7);
    sc1(NR_close,fd);
    out("tone: multitone ALSA playback completed\n");
    sc1(NR_exit,0);
}
