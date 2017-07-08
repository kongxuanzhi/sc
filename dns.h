extern char *nameServer; //无引用
extern int useGethostbyname; //无引用

#define DNS_A 0
#define DNS_CNAME 1

//数组
//ref：dns.c | server.h | socks.h | tunnel.c |
typedef struct _GethostbynameRequest {
    AtomPtr name; //host name 
    AtomPtr addr; //地址
    AtomPtr error_message; //错误消息
    int count; //个数
    ObjectPtr object;
    int (*handler)(int, struct _GethostbynameRequest*);
    void *data;
} GethostbynameRequestRec, *GethostbynameRequestPtr;

/* Note that this requires no alignment */
//ref: io.c | dns.c 
typedef struct _HostAddress {
    char af;                     /* 4 or 6 */
    char data[16];
} HostAddressRec, *HostAddressPtr;

//ref: main.c
void preinitDns(void);
void initDns(void);
//ref: server.c | sock.c | tunnel.c
int do_gethostbyname(char *name, int count,
                     int (*handler)(int, GethostbynameRequestPtr), void *data);
