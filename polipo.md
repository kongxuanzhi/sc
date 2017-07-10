#polipo 代理服务器源码研究
    > 别问为什么，先迈出第一步是最重要的

##查看代码记录
eventLoop->FdEventHandlerPtr->reopenLog()
                            ->discardObjects()
###函数调用树状图
> 一个函数可以被多个函数调用 不采用
> 多个函数被一个函数调用 采用
> 一般不会出现循环调用
event.c:
1. registerFdEvent :: dns.c|io.c|server.c
       makeFdEvent
	   registerFdEventHelper
		   allocateFdEventNum
2. unregisterFdEvent
	   unregisterFdEventI:loop
		   deallocateFdEventNum
		   recomputePollEvents
3. pokeFdEvent： 
	   scheduleTimeEvent
	   pokeFdEventHandler：
		   unregisterFdEventI
		   unregisterFdEvent
4. conditionWait:: client.c | dns.c | server.c

5. abortConditionHandler:: client.c
		unregisterConditionHandler :: client.c
6. signalCondition :: object.c


##规则
1. 函数是对变量进行操作，变量是核心
2. 所有的函数api都在.h文件中

## 代码组织【初步】
1. atom                     
2. auth                             
3. chunk   
4. client                               
5. config                               
6. config.sample                                
7. dirent_compat                                
8. diskcache                                
9. dns                              
10. event                                
11. forbidden                                
12. forbidden.sample                             
13. ftsimport                                            
14. fts_compat                                            
15. http                                              
16. http_parse           
17. io
18. local
19. log
20. main.c
21. md5
22. md5import
23. mingw
24. object
25. parse_time
26. polipo.h
27. polipo.man
28. polipo.texi
29. server
30. socks
31. tunnel
32. util
33. Makefile

main.c ```#include "polipo.h"```

    initAtoms, releaseAtom
    CONFIG_VARIABLE
    preinitChunks, initChunks
    preinitLog, initLog, loggingToStderr
    preinitIo, initIo
    preinitDns, initDns
    preinitServer, initServer
    preinitHttp, initHttp
    preinitDiskcache, initDiskcache
    preinitLocal
    preinitForbidden, initForbidden
    preinitSocks, initSocks

    initObject, expireDiskObjects
    initEvents

    expandTilde
    access

    parseConfigFile, printConfigVariables

    do_daemonise 
    writePid
    create_listener
    unlink
    eventLoop

## 数据结构【核心】
AtomPtr configFile = NULL;
AtomPtr pidFile = NULL;
FdEventHandlerPtr listener;
static AtomPtr *atomHashTable;


```c
typedef struct _Atom {
    unsigned int refcount;
    struct _Atom *next;
    unsigned short length;
    char string[1];
} AtomRec, *AtomPtr;

typedef struct _AtomList {
    int length;
    int size;
    AtomPtr *list;
} AtomListRec, *AtomListPtr;

typedef struct _ConfigVariable {
    AtomPtr name;
    int type;
    union {
        int *i;
        float *f;
        struct _Atom **a;
        struct _AtomList **al;
        struct _IntList **il;
    } value;
    int (*setter)(struct _ConfigVariable*, void*);
    char *help;
    struct _ConfigVariable *next;
} ConfigVariableRec, *ConfigVariablePtr;

typedef struct _DnsQuery {
    unsigned id;
    AtomPtr name;
    ObjectPtr object;
    AtomPtr inet4, inet6;
    time_t ttl4, ttl6;
    time_t time;
    int timeout;
    TimeEventHandlerPtr timeout_handler;
    struct _DnsQuery *next;
} DnsQueryRec, *DnsQueryPtr;

```

## 函数用途【归类】
initAtoms
## 主要流程【业务】

## 全局内容


## c语言知识
1. [static](http://www.cnblogs.com/stoneJin/archive/2011/09/21/2183313.html)
    * 首先static的最主要功能是隐藏，其次因为static变量存放在静态存储区，所以它具备持久性和默认值0。
2. [void * memcpy ( void * destination, const void * source, size_t num );](http://www.cplusplus.com/reference/cstring/memcpy/)
3. [void * memchr ( const void *, int, size_t ); ](http://www.cplusplus.com/reference/cstring/memchr/?kw=memchr)
## 技巧
1. 【sublime】如果文件不多的話，把所有用到的文件打开，按F12可以跳转函数
2. [Sublime注释插件-Doc​Blockr](http://www.cnblogs.com/chris-oil/p/5387129.html)
3. 看源码就是要不停的添加注释‘
4. 以变量为中心，搞清楚那些函数对该变量有哪些操作。
5. 重点研究一个函数的输入和输出，即到底函数做了什么
6. 先把最重要的数据关系搞清楚
7. 倒着看一个函数
8. 先研究这个软件怎么用,从实现的功能入手，逐步了解软件的实现
9. 对小概念，要及时查找，然后总结学习知识点
10. 提出需求问题，然后找实现的方法

### ref:
1. [C语言open()函数：打开文件函数](http://c.biancheng.net/cpp/html/238.html)
2. [详解C语言中的fopen()函数和fdopen()函数](http://www.jb51.net/article/71714.htm)
3. [linux信号处理函数 利用Linux信号SIGUSR1,SIGUSR2调试嵌入式程序](http://blog.csdn.net/u010133805/article/details/53899667)
4. [linux信号 sigaction函数解析](http://blog.chinaunix.net/uid-1877180-id-3011232.html)
5. [linux信号 sigset_t](http://blog.sina.com.cn/s/blog_4b226b92010119j9.html)
6. [IO多路复用之poll总结](http://www.cnblogs.com/Anker/archive/2013/08/15/3261006.html)
7. [select、poll、epoll之间的区别总结[整理]](http://www.cnblogs.com/Anker/p/3265058.html)
8. [struct timeval结构体](http://blog.csdn.net/lyc_daniel/article/details/11733715)
9. [关于PF_INET和AF_INET的区别](http://blog.csdn.net/xiongmaojiayou/article/details/7584211)	
10. [socket函数](http://baike.baidu.com/link?url=2ONtGsISBrTjzghCOdirh6TZbCcfNkqFjNnt_Q-U0enbGtdrWuT6Q3NsVvVYDpQ5BWZtZYjGlIEZ8AwHowBgosZoUi7Qr79MbbILF2W8-C_)
11. [SOCKADDR_IN](http://baike.baidu.com/link?url=fY6xAgcuoJQl2frfWOM4LDdAQQSNydb76yQaQ8gWnZolJ2Zj2HBCNg7wrSgqsVC32jx7wAynyb2JFGEl4YJIwmkFNCPyC6zjm21I_GIUWQy)
12. [socket接口详解](http://www.cnblogs.com/yuqiao/p/5786427.html)
13. [socket通信简介](http://blog.csdn.net/xiaoweige207/article/details/6211577/)
13. [socket与文件描述符](http://blog.chinaunix.net/uid-23146151-id-3084687.html)
14. [函数malloc()和calloc()的区别](http://blog.csdn.net/zhongjiekangping/article/details/6162748)

1. memchr函数原型extern void *memchr(const void *buf, int ch, size_t count)，功能：从buf所指内存区域的前count个字节查找字符ch。


• Client connections:	  	Speaking to clients
• Contacting servers:	  	Contacting servers.
• HTTP tuning:	  	Tuning at the HTTP level.
• Offline browsing:	  	Browsing with poor connectivity.
• Server statistics:	  	Polipo keeps statistics about servers.
• Server-side behaviour:	  	Tuning the server-side behaviour.
• PMM:	  	Poor Man’s Multiplexing.
• Forbidden:	  	You can forbid some URLs.
• DNS:	  	How Polipo finds hosts.
• Parent proxies:	  	Fetching data from other proxies.
• Tuning POST and PUT:	  	Tuning POST and PUT requests.
• Tunnelling connections:	  	Tunnelling foreign protocols and https.


#know
1. Note that both IP-based authentication and [HTTP basic authentication](https://www.irif.fr/~jch/software/polipo/polipo.html#Access-control) are insecure: the former is vulnerable to IP address spoofing, the latter to replay attacks. If you need to access Polipo over the public Internet, the only secure option is to have it listen over the loopback interface only and use an ssh tunnel (see Parent proxies)4.
2. A server can have multiple addresses, for example if it is multihomed (connected to multiple networks) or if it can speak both IPv4 and IPv6. Polipo will try all of a hosts addresses in turn; once it has found one that works, it will stick to that address until it fails again.
3. A TCP service is identified not only by the IP address of the machine it is running on, but also by a small integer, the TCP port it is listening on. Normally, web servers listen on port 80, but it is not uncommon to have them listen on different ports; Polipo’s internal web server, for example, listens on port 8123 by default.
4. Tuning at the HTTP level, Tuning the HTTP parser, Censoring headers
5. As a number of HTTP servers and CGI scripts serve incorrect HTTP headers, Polipo uses a lax parser, meaning that incorrect HTTP headers will be ignored (a warning will be logged by default).
6. Polipo offers the option to censor given HTTP headers in both client requests and server replies. The main application of this feature is to very slightly improve the user’s privacy by eliminating cookies and some content-negotiation headers.
7. Why censor Accept-Language

    Recent versions of HTTP include a mechanism known as content negotiation which allows a user-agent and a server to negotiate the best representation (instance) for a given resource. For example, a server that provides both PNG and GIF versions of an image will serve the PNG version to user-agents that support PNG, and the GIF version to Internet Explorer.

    Content negotiation requires that a client should send with every single request a number of headers specifying the user’s cultural and technical preferences. Most of these headers do not expose sensitive information (who cares whether your browser supports PNG?). The ‘Accept-Language’ header, however, is meant to convey the user’s linguistic preferences. In some cases, this information is sufficient to pinpoint with great precision the user’s origins and even his political or religious opinions; think, for example, of the implications of sending ‘Accept-Language: yi’ or ‘ar_PS’.

    At any rate, ‘Accept-Language’ is not useful. Its design is based on the assumption that language is merely another representation for the same information, and ‘Accept-Language’ simply carries a prioritised list of languages, which is not enough to usefully describe a literate user’s preferences. A typical French user, for example, will prefer an English-language original to a French (mis-)translation, while still wanting to see French language texts when they are original. Such a situation cannot be described by the simple-minded ‘Accept-Language’ header.
8. Implementors of intermediate caches (proxies) have found it useful to convert the media type of certain entity bodies. A non-transparent proxy might, for example, convert between image formats in order to save cache space or to reduce the amount of traffic on a slow link.
9. In order to decide when to pipeline requests (see Pipelining) and whether to perform Poor Man’s Multiplexing (see Poor Mans Multiplexing), Polipo needs to keep statistics about servers. These include the server’s ability to handle persistent connections, the server’s ability to handle pipelined requests, the round-trip time to the server, and the server’s transfer rate. The statistics are accessible from Polipo’s web interface (see Web interface).
10. By default, Polipo does not use Poor Man’s Multiplexing
11. PMM is an intrinsically unreliable technique. Polipo makes heroic efforts to make it at least usable, requesting that the server disable PMM when not useful (by using the ‘If-Range’ header) and disabling it on its own if a resource turns out to be dynamic. Notwithstanding these precautions, unless the server cooperates6, you will see failures when using PMM, which will usually result in blank pages and broken image icons; hitting Reload on your browser will usually cause Polipo to notice that something went wrong and correct the problem.
12. Polipo can be configured to prevent certain URLs from reaching the browser, either by returning a forbidden error message to the user, or by redirecting such URLs to some other URL.
Some content providers attempt to subvert content filtering as well as malware scans by tunnelling their questionable content as https or other encrypted protocols. Other content providers are so clueless as to inject content from external providers into supposedly safe webpages. Polipo has therefore the ability to selectively block tunneled connections based on hostname and port information.  
13. Obviously the web browser (and other software) must be configured to use polipo as tunneling proxy for this to work. The tunnelled traffic is neither touched nor inspected in any way by polipo, thus encryption, certification and all other security and integrity guarantees implemented in the browser are not in any way affected.
14. The domain name service

The low-level protocols beneath HTTP identify machines by IP addresses, sequences of four 8-bit integers such as ‘199.232.41.10’7. HTTP, on the other hand, and most application protocols, manipulate host names, strings such as ‘www.polipo.org’.

The domain name service (DNS) is a distributed database that maps host names to IP addresses. When an application wants to make use of the DNS, it invokes a resolver, a local library or process that contacts remote name servers.
15. Polipo usually tries to speak the DNS protocol itself rather than using the system resolver


[accept()函数](http://www.360doc.com/content/13/0908/17/13253385_313070996.shtml)
http://ticktick.blog.51cto.com/823160/779866/
　　准备好了，系统调用 accept() 会有点古怪的地方的！你可以想象发生 这样的事情：有人从很远的地方通过一个你在侦听 (listen()) 的端口连接 (connect()) 到你的机器。它的连接将加入到等待接受 (accept()) 的队列 中。你调用 accept() 告诉它你有空闲的连接。它将返回一个新的套接字文 件描述符！这样你就有两个套接字了，原来的一个还在侦听你的那个端口， 新的在准备发送 (send()) 和接收 ( recv()) 数据。这就是这个过程！
函数是这样定义的： 
#include <sys/socket.h>
int accept(int sockfd, void *addr, int *addrlen); 
sockfd 相当简单，是和 listen() 中一样的套接字描述符。addr 是个指 向局部的数据结构 sockaddr_in 的指针。这是要求接入的信息所要去的地 方（你可以测定那个地址在那个端口呼叫你）。在它的地址传递给 accept 之 前，addrlen 是个局部的整形变量，设置为 sizeof(struct sockaddr_in)。 accept 将不会将多余的字节给 addr。如果你放入的少些，那么它会通过改
变 addrlen 的值反映出来。

[socket05---recv && send使用，回射客户端](http://blog.csdn.net/tingyun_say/article/details/51907687)
[Socket send函数和recv函数详解](http://www.cnblogs.com/jianqiang2010/archive/2010/08/20/1804598.html)
