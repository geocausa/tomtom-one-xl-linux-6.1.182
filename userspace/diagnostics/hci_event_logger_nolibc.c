typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef unsigned int size_t;

#define AF_BLUETOOTH 31
#define SOCK_RAW 3
#define BTPROTO_HCI 1
#define SOL_HCI 0
#define HCI_FILTER 2
#define HCI_CHANNEL_RAW 0
#define HCI_EVENT_PKT 4
#define NR_exit 1
#define NR_read 3
#define NR_write 4
#define NR_socket 281
#define NR_bind 282
#define NR_setsockopt 294

struct sockaddr_hci { u16 family; u16 dev; u16 channel; };
struct hci_filter { u32 type_mask; u32 event_mask[2]; u16 opcode; };

static long sc1(long n,long a){ register long r0 asm("r0")=a; register long r7 asm("r7")=n; asm volatile("svc 0":"+r"(r0):"r"(r7):"memory"); return r0; }
static long sc3(long n,long a,long b,long c){ register long r0 asm("r0")=a; register long r1 asm("r1")=b; register long r2 asm("r2")=c; register long r7 asm("r7")=n; asm volatile("svc 0":"+r"(r0):"r"(r1),"r"(r2),"r"(r7):"memory"); return r0; }
static long sc5(long n,long a,long b,long c,long d,long e){ register long r0 asm("r0")=a; register long r1 asm("r1")=b; register long r2 asm("r2")=c; register long r3 asm("r3")=d; register long r4 asm("r4")=e; register long r7 asm("r7")=n; asm volatile("svc 0":"+r"(r0):"r"(r1),"r"(r2),"r"(r3),"r"(r4),"r"(r7):"memory"); return r0; }
static unsigned slen(const char*s){ unsigned n=0; while(s[n])n++; return n; }
static void out(const char*s){ sc3(NR_write,1,(long)s,slen(s)); }
static char hx(unsigned x){ x&=15; return x<10?'0'+x:'A'+x-10; }
static void event_line(const u8*b,int n){ char o[1100]; int p=0,i; const char pre[]="HCI "; for(i=0;pre[i];i++)o[p++]=pre[i]; if(n>1){o[p++]=hx(b[1]>>4);o[p++]=hx(b[1]);} o[p++]=' '; for(i=0;i<n && p<1096;i++){o[p++]=hx(b[i]>>4);o[p++]=hx(b[i]);} o[p++]='\n'; sc3(NR_write,1,(long)o,p); }
void _start(void){
 long s=sc3(NR_socket,AF_BLUETOOTH,SOCK_RAW,BTPROTO_HCI); if(s<0){out("LOGGER socket_fail\n");sc1(NR_exit,1);}
 struct sockaddr_hci a; a.family=AF_BLUETOOTH;a.dev=0;a.channel=HCI_CHANNEL_RAW;
 if(sc3(NR_bind,s,(long)&a,sizeof(a))<0){out("LOGGER bind_fail\n");sc1(NR_exit,2);}
 struct hci_filter f; f.type_mask=(1u<<HCI_EVENT_PKT);f.event_mask[0]=0xffffffffu;f.event_mask[1]=0xffffffffu;f.opcode=0;
 if(sc5(NR_setsockopt,s,SOL_HCI,HCI_FILTER,(long)&f,sizeof(f))<0){out("LOGGER filter_fail\n");sc1(NR_exit,3);}
 out("HCI_EVENT_LOGGER_READY\n");
 for(;;){u8 b[512]; long n=sc3(NR_read,s,(long)b,sizeof(b)); if(n>0 && b[0]==HCI_EVENT_PKT) event_line(b,(int)n);}
}
