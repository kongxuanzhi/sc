1. httpAccept如何处理event，处理了哪些事件。 0代表未发生错误
2. 什么时候，并且怎么把事件放入fdEvents数组, 即 fdEvents[i].fd，那么时候会有新的fd生成，加入进来，新的事件加入进来
    * 程序在调用registerFdEvent和registerFdEventHelper时会将event加入数组，
    * 根据fd来分配数组fd是文件描述符，这里应该指的是客户端的socket的请求。
    * fd是一个小于1024的整数，所以3中的poll_fds长度一定小于等于1024
3. poll_fds,fdEventNum 和fdEventsLast，三个链表数组的关系
   poll_fds是一个数组，每个数组元素延伸出一个fdEventNum的双向链表，对应的第i个元素有一个fdEventsLast
   指向第i个fdEventNum的链表尾部，以方便追加新的元素。
   poll_fds的第i个元素以及延伸出的第fdEventNum双向链表，拥有同样的fd，并且在poll_fds[i]中存放着该
   链表中所有元素发生的事件的总和。
4. scheduleTimeEvent 如何做到定时执行？，执行的是什么东西？
5. 有哪些函数实现了```int (*handler)(struct _TimeEventHandler*); ```接口？
   实现了相同接口的函数，是否有相同的执行方式？
   * httpAcceptAgain(TimeEventHandlerPtr event)
   * httpClientDelayedShutdownHandler(TimeEventHandlerPtr event)
   * httpClientRequestDelayed(TimeEventHandlerPtr event)
   * httpClientDelayed(TimeEventHandlerPtr event)
   * httpClientNoticeRequestDelayed(TimeEventHandlerPtr event)
   * httpClientContinueDelayed(TimeEventHandlerPtr event)
   * httpServeObjectDelayed(TimeEventHandlerPtr event)
   * httpServeChunkDelayed(TimeEventHandlerPtr event)
   * dnsDelayedErrorNotifyHandler(TimeEventHandlerPtr event)
   * dnsDelayedDoneNotifyHandler(TimeEventHandlerPtr event)
   * dnsTimeoutHandler(TimeEventHandlerPtr event)
   * enqueueTimeEvent(TimeEventHandlerPtr event)
   * cancelTimeEvent(TimeEventHandlerPtr event)
   * httpTimeoutHandler(TimeEventHandlerPtr event)
   * lingeringCloseTimeoutHandler(TimeEventHandlerPtr event)
   * discardObjectsHandler(TimeEventHandlerPtr event)
   * expireServersHandler(TimeEventHandlerPtr event)
   * httpClientDelayedDoSideHandler(TimeEventHandlerPtr event)
   * httpServerDelayedFinishHandler(TimeEventHandlerPtr event)
    ```event
    typedef struct _TimeEventHandler {
        struct timeval time;
        struct _TimeEventHandler *previous, *next;
        int (*handler)(struct _TimeEventHandler*); //定义了接口
        char data[1];
    } TimeEventHandlerRec, *TimeEventHandlerPtr;
    ```
    什么时候执行？
    event.c: runTimeEventQueue 循环队列执行
    event.c: eventLoop->runTimeEventQueue
    关于timeEvents函数：
    scheduleTimeEvent， enqueueTimeEvent，cancelTimeEvent，runTimeEventQueue

6. 有哪些函数实现了这个接口？
 int (*handler)(int, FdEventHandlerPtr, struct _StreamRequest*);

   * httpClientShutdownHandler(int status,
                          FdEventHandlerPtr event, StreamRequestPtr request)
   * httpClientHandler(int status,
                  FdEventHandlerPtr event, StreamRequestPtr request)
   * httpClientDiscardHandler(int status,
                         FdEventHandlerPtr event, StreamRequestPtr request)

typedef struct _StreamRequest {
    short operation;
    short fd;
    int offset;
    int len;
    int len2;
    union {
        struct {
            int hlen;
            char *header;
        } h;
        struct {
            int len3;
            char *buf3;
        } b;
        struct {
            char **buf_location;
        } l;
    } u;
    char *buf;
    char *buf2;
    int (*handler)(int, FdEventHandlerPtr, struct _StreamRequest*);
    void *data;
} StreamRequestRec, *StreamRequestPtr;

7. int (*handler)(int, struct _FdEventHandler*); 
    * do_scheduled_stream(int status, FdEventHandlerPtr event)
    * do_scheduled_connect(int status, FdEventHandlerPtr event)
    * do_scheduled_accept(int status, FdEventHandlerPtr event)
    * lingeringCloseHandler(int status, FdEventHandlerPtr event)
typedef struct _FdEventHandler {
    short fd; //文件描述符
    short poll_events; //文件轮询发生的事件s
    struct _FdEventHandler *previous, *next; //双向链表
    int (*handler)(int, struct _FdEventHandler*); //处理该事件的函数
    char data[1]; //申请内存的时候，这个内存先减去再加上实际data的大小。
} FdEventHandlerRec, *FdEventHandlerPtr;
执行方式：
    done = event->handler(0, event);
    if(done) { //数组中移除
        unregisterFdEvent(event);
        return NULL;
    }


8. int (*handler)(int, FdEventHandlerPtr, struct _ConnectRequest*);

* tunnelConnectionHandler(int status,
                        FdEventHandlerPtr event,
                        ConnectRequestPtr request)

* socksDnsHandler(int status, GethostbynameRequestPtr grequest) 

* httpServerConnectionHandler(int status,
                            FdEventHandlerPtr event,
                            ConnectRequestPtr request)                       
typedef struct _ConnectRequest {
    int fd;
    int af;
    struct _Atom *addr;
    int firstindex;
    int index;
    int port;
    int (*handler)(int, FdEventHandlerPtr, struct _ConnectRequest*);
    void *data;
} ConnectRequestRec, *ConnectRequestPtr;

9. 程序是怎么运行的？
    1. io.c: create_listen(socket. bind, listen)
    2. io.c: schedule_accept (registerFdEvent) eventloop 轮询 runTimeEventQueue回调3
    3. io.c: do_scheduled_accept(accept阻塞)，accept client. goto 4
    4. client.c: httpAccept（阻塞结束，处理与客户端的连接）->httpMakeConnection（goto 6）->scheduleTimeEvent：120ms->回调5
    5. client.c: httpTimeoutHandler关闭连接（shutdown），删除事件pokeFdEvent
    6. io.c: do_stream_buf : 回调执行client.c httpClientHandler
    7. io.c: schedule_stream->makeFdEvent(operation)->registerFdEventHelper : eventloop 轮询event->handle 回调8
    8. io.c: do_scheduled_stream获得request执行 httpClientHandler
10. 什么时候回调httpClientHandler？ 
    在do_stream_buf中，设置回调函数httpClientHandler, 统一调用schedule_stream, 
    将httpClientHandle作为event的data设置，放进fdEvents事件数组中。
    在eventloop中轮询执行事件event->handle，回调do_scheduled_stream, 在它里面执行handle->httpClientHandler, 
    
