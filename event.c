/*
Copyright (c) 2003-2006 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "polipo.h"

#ifdef HAVE_FORK
static volatile sig_atomic_t exitFlag = 0;
#else
static int exitFlag = 0;
#endif
static int in_signalCondition = 0;

//event队列  -> 事件时间从左到右增大， 最先发生的事件在最前面
static TimeEventHandlerPtr timeEventQueue;
//指向event队列的最后，最晚发生的事件
static TimeEventHandlerPtr timeEventQueueLast;

struct timeval current_time;
struct timeval null_time = {0,0};

static int fdEventSize = 0;
static int fdEventNum = 0;
static struct pollfd *poll_fds = NULL; //结构体数组
//FdEventHandlerPtr： 处理poll_fds 文件发生的事件
static FdEventHandlerPtr *fdEvents = NULL, *fdEventsLast = NULL;
int diskIsClean = 1;

static int fds_invalid = 0; //poll_fds fdEvents数组统一向前移动了一位
 
static inline int
timeval_cmp(struct timeval *t1, struct timeval *t2)
{
    if(t1->tv_sec < t2->tv_sec)
        return -1;
    else if(t1->tv_sec > t2->tv_sec)
        return +1;
    else if(t1->tv_usec < t2->tv_usec)
        return -1;
    else if(t1->tv_usec > t2->tv_usec)
        return +1;
    else
        return 0;
}

//时间相减
static inline void
timeval_minus(struct timeval *d,
              const struct timeval *s1, const struct timeval *s2)
{
	//s1 > s2  s1-s2
    if(s1->tv_usec >= s2->tv_usec) {
        d->tv_usec = s1->tv_usec - s2->tv_usec;
        d->tv_sec = s1->tv_sec - s2->tv_sec;
    } else {
        d->tv_usec = s1->tv_usec + 1000000 - s2->tv_usec;
        d->tv_sec = s1->tv_sec - s2->tv_sec - 1;
    }
}

int
timeval_minus_usec(const struct timeval *s1, const struct timeval *s2)
{
    return (s1->tv_sec - s2->tv_sec) * 1000000 + s1->tv_usec - s2->tv_usec;
}

#ifdef HAVE_FORK
static void
sigexit(int signo)
{	
	//If Polipo receives the SIGUSR1 signal, 
	//it will write out all the in - memory data to disk(but won’t discard them), 
	//reopen the log file, 
	//and then reload the forbidden URLs file
    if(signo == SIGUSR1) //http://blog.csdn.net/u010133805/article/details/53899667
        exitFlag = 1;
	//Finally, if Polipo receives the SIGUSR2 signal, 
	//it will write out all the in-memory data to disk 
	//and discard as much of the memory cache as possible
	//It will then reopen the log file and reload the forbidden URLs file.

	//Polipo assumes that the local web tree doesn’t change behind its back. 
	//If you change any of the local files,
	//you will need to notify Polipo by sending it a SIGUSR2 signal
    else if(signo == SIGUSR2)
        exitFlag = 2;
    else
        exitFlag = 3;
}
#endif

//ref: main.c
void
initEvents()
{
#ifdef HAVE_FORK
    struct sigaction sa; //检查或修改与指定信号相关联的处理动作http://blog.chinaunix.net/uid-1877180-id-3011232.html
    sigset_t ss; //信号集 http://blog.sina.com.cn/s/blog_4b226b92010119j9.html
	// 为信号SIGPIPE,SIGTERM,SIGHUP,SIGINT,SIGUSR1,SIGUSR2制定信号处理函数
	//Polipo will shut down cleanly if it receives SIGHUP, SIGTERM or SIGINT signals;
	//this will normally happen when a Polipo in the foreground receives a ^C key press, 
	//when your system shuts down, or 
	//when you use the kill command with no flags.
	//Polipo will then write-out all its in-memory data to disk and quit.
    sigemptyset(&ss);
    sa.sa_handler = SIG_IGN;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, NULL);

    sigemptyset(&ss);
    sa.sa_handler = sigexit;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);

    sigemptyset(&ss);
    sa.sa_handler = sigexit;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGHUP, &sa, NULL);

    sigemptyset(&ss);
    sa.sa_handler = sigexit;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

	//SIGUSR1 
	//If Polipo receives the SIGUSR1 signal, 
	//it will write out all the in - memory data to disk(but won’t discard them), 
	//reopen the log file, 
	//and then reload the forbidden URLs file
    sigemptyset(&ss);
    sa.sa_handler = sigexit;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

	//Finally, if Polipo receives the SIGUSR2 signal, 
	//it will write out all the in-memory data to disk 
	//and discard as much of the memory cache as possible
	//It will then reopen the log file and reload the forbidden URLs file.
    sigemptyset(&ss);
    sa.sa_handler = sigexit;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);
#endif

    timeEventQueue = NULL;  //初始时间队列为空
    timeEventQueueLast = NULL;
    fdEventSize = 0;
    fdEventNum = 0;
	
    poll_fds = NULL; //初始化文件描述符队列
    fdEvents = NULL; //文件事件队列
    fdEventsLast = NULL; //最后一个事件
}

//ref: forbidden.c | local.c
//解绑信号和相应的信号处理函数
void
uninitEvents(void)
{
#ifdef HAVE_FORK //只有linux上才有FORK
    struct sigaction sa;
    sigset_t ss;

    sigemptyset(&ss);
    sa.sa_handler = SIG_DFL;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);

    sigemptyset(&ss);
    sa.sa_handler = SIG_DFL;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGHUP, &sa, NULL);

    sigemptyset(&ss);
    sa.sa_handler = SIG_DFL;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    sigemptyset(&ss);
    sa.sa_handler = SIG_DFL;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    sigemptyset(&ss);
    sa.sa_handler = SIG_DFL;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);
#endif
}

//ref： forbidden.c | local.c
#ifdef HAVE_FORK
void
interestingSignals(sigset_t *ss)
{
    sigemptyset(ss);
    sigaddset(ss, SIGTERM);
    sigaddset(ss, SIGHUP);
    sigaddset(ss, SIGINT);
    sigaddset(ss, SIGUSR1);
    sigaddset(ss, SIGUSR2);
}
#endif

//去event队列
void
timeToSleep(struct timeval *time)
{
    if(!timeEventQueue) { //no event.
        time->tv_sec = ~0L; //长整型取反 为-1
        time->tv_usec = ~0L; // -1
    } else {
        *time = timeEventQueue->time; //取出（查看）最近发生的事件的时间
    }
}

//进事件处理队列
//called only when schedule TimeEvent
//staticref: scheduleTimeEvent
static TimeEventHandlerPtr
enqueueTimeEvent(TimeEventHandlerPtr event)
{
	//use to deal with the event which time is between the start and the end of the queue.
    TimeEventHandlerPtr otherevent;

    /* We try to optimise two cases -- the event happens very soon, or
       it happens after most of the other events. */
	//一开始如果没有事件 或者 新加入的事件在目前最早事件之前
	//就将该event放到队列的最前面，优先执行
    if(timeEventQueue == NULL ||  
       timeval_cmp(&event->time, &timeEventQueue->time) < 0) {
        /* It's the first event */
        event->next = timeEventQueue;
        event->previous = NULL;
        if(timeEventQueue) {
            timeEventQueue->previous = event;
        } else {
            timeEventQueueLast = event;
        }
        timeEventQueue = event;
    } else if(timeval_cmp(&event->time, &timeEventQueueLast->time) >= 0) {
        /* It's the last one */
        event->next = NULL;
        event->previous = timeEventQueueLast;
        timeEventQueueLast->next = event;
        timeEventQueueLast = event;
    } else {
        /*big than the first one and less than the last one; Walk from the end */
        otherevent = timeEventQueueLast;
        while(otherevent->previous &&
              timeval_cmp(&event->time, &otherevent->previous->time) < 0) {
            otherevent = otherevent->previous;
        }
        event->next = otherevent;
        event->previous = otherevent->previous;
        if(otherevent->previous) {
            otherevent->previous->next = event;
        } else {
            timeEventQueue = event;
        }
        otherevent->previous = event;
    }
    return event;
}

/**
 * seconds: 设定几秒后执行
 * TimeEventHandlerPtr：事件处理的handler函数
 * dsize: data的大小
 * data：事件处理所需要的数据
 * ref: chunk.c | client.c | dns.c | http.c | io.c | object.c | server.c 
 * staticref: pokeFdEvent
 */
TimeEventHandlerPtr
scheduleTimeEvent(int seconds,
                  int (*handler)(TimeEventHandlerPtr), int dsize, void *data)
{
    struct timeval when;
    TimeEventHandlerPtr event;
	
    if(seconds >= 0) { //if set seconds. set the event occur time as current_time + seconds
        when = current_time; 
        when.tv_sec += seconds;
    } else { // when second<0 = -1, set event time as zero.
        when.tv_sec = 0;
        when.tv_usec = 0;
    }
	//分配事件内存
    event = malloc(sizeof(TimeEventHandlerRec) - 1 + dsize);
    if(event == NULL) {
        do_log(L_ERROR, "Couldn't allocate time event handler -- "
               "discarding all objects.\n");
        exitFlag = 2; //不能分配内存，就异常退出.
        return NULL;
    }

    event->time = when;
    event->handler = handler;
    /* Let the compiler optimise the common case */
    if(dsize == sizeof(void*)) //x86就是4，x64就是8.
        memcpy(event->data, data, sizeof(void*));
    else if(dsize > 0)
        memcpy(event->data, data, dsize);
	
	//event data可能为空
    return enqueueTimeEvent(event); //进队列
}

//取消事件的发生
//ref: dns.c | client.c | http.c | io.c | server.c
void
cancelTimeEvent(TimeEventHandlerPtr event)
{
	//1. event在最前面，指针后移
	//2. event在最后面，指针前移
	//3. event在中间，直接删除
    if(event == timeEventQueue)
        timeEventQueue = event->next;
    if(event == timeEventQueueLast)
        timeEventQueueLast = event->previous;
    if(event->next)
        event->next->previous = event->previous;
    if(event->previous)
        event->previous->next = event->next;
    free(event);
}

// 下面是关于FdEvent双链表的操作
//ref : registerFdEventHelper
int
allocateFdEventNum(int fd)
{
    int i;
    if(fdEventNum < fdEventSize) { //计算插入的队列的最后面
        i = fdEventNum;
        fdEventNum++;
    } else { //一开始为空，分配新的内存，插入
        struct pollfd *new_poll_fds;
        FdEventHandlerPtr *new_fdEvents, *new_fdEventsLast;
        int new_size = 3 * fdEventSize / 2 + 1; //1，2，4，5，7，8，10
		//重新创建分配空间， 并且分配同样的大小
        new_poll_fds = realloc(poll_fds, new_size * sizeof(struct pollfd));
        if(!new_poll_fds)
            return -1;
        new_fdEvents = realloc(fdEvents, new_size * sizeof(FdEventHandlerPtr));
        if(!new_fdEvents)
            return -1;
        new_fdEventsLast = realloc(fdEventsLast, 
                                   new_size * sizeof(FdEventHandlerPtr));
        if(!new_fdEventsLast)
            return -1;
		//申请的是三个数组，数组元素是双向链表结构
        poll_fds = new_poll_fds;
        fdEvents = new_fdEvents;
        fdEventsLast = new_fdEventsLast;
        fdEventSize = new_size;
        i = fdEventNum;
        fdEventNum++;
    }

    poll_fds[i].fd = fd;
	//POLLHUP： A POLLHUP means the socket is no longer connected. In TCP, this means FIN has been received and sent
    //POLLERR：A POLLERR means the socket got an asynchronous error. In TCP, this typically means a RST has been received or sent. If the file descriptor is not a socket, POLLERR might mean the device does not support polling.
	//For both of the conditions above, the socket file descriptor is still open, and has not yet been closed (but shutdown() may have already been called). A close() on the file descriptor will release resources that are still being reserved on behalf of the socket. In theory, it should be possible to reuse the socket immediately (e.g., with another connect() call).
	//POLLNVAL: A POLLNVAL means the socket file descriptor is not open. It would be an error to close() it.
	poll_fds[i].events = POLLERR | POLLHUP | POLLNVAL;
    poll_fds[i].revents = 0;
    fdEvents[i] = NULL;
    fdEventsLast[i] = NULL;
    fds_invalid = 1;
    return i;
}

//删除主链上的元素，后面的往前移动 
//ref: unregisterFdEventI
void
deallocateFdEventNum(int i)
{
	//假设i=0，fdEventNum = 2.
    if(i < fdEventNum - 1) { // 如果这个元素不是倒数第2个，将三个数组都向前移动
        memmove(&poll_fds[i], &poll_fds[i + 1], 
                (fdEventNum - i - 1) * sizeof(struct pollfd));
        memmove(&fdEvents[i], &fdEvents[i + 1],
                (fdEventNum - i - 1) * sizeof(FdEventHandlerPtr));
        memmove(&fdEventsLast[i], &fdEventsLast[i + 1],
                (fdEventNum - i - 1) * sizeof(FdEventHandlerPtr));
    }
    fdEventNum--;
    fds_invalid = 1; //删除后第i个元素无效了
}
//申请内存，创建对象，设置参数
//ref: registerFdEvent | io.c
FdEventHandlerPtr 
makeFdEvent(int fd, int poll_events, 
            int (*handler)(int, FdEventHandlerPtr), int dsize, void *data)
{
    FdEventHandlerPtr event;
	//这里的-1是将原FdEventHandlerRec中的属性data[1]去掉，再申请dsize到data中
    event = malloc(sizeof(FdEventHandlerRec) - 1 + dsize);
    if(event == NULL) {
        do_log(L_ERROR, "Couldn't allocate fd event handler -- "
               "discarding all objects.\n");
        exitFlag = 2; //discarding all objects.
        return NULL;
    }
    event->fd = fd;
    event->poll_events = poll_events;
    event->handler = handler;
    /* Let the compiler optimise the common cases */
	// 让编译器优化常见的情况
    if(dsize == sizeof(void*))
        memcpy(event->data, data, sizeof(void*));
    else if(dsize == sizeof(StreamRequestRec)) //在io.h 中定义 request
        memcpy(event->data, data, sizeof(StreamRequestRec));
    else if(dsize > 0)
        memcpy(event->data, data, dsize);
    return event;
}
//注册（增加）file descriptor事件
//ref: io.c
//staticref: registerFdEvent
FdEventHandlerPtr
registerFdEventHelper(FdEventHandlerPtr event)
{
    int i;
    int fd = event->fd;// SOCKET 整数

    for(i = 0; i < fdEventNum; i++)
        if(poll_fds[i].fd == fd) //如果已经添加了对fd的事件
            break;
	// 如果poll_fds中不存在该event
    if(i >= fdEventNum)
        i = allocateFdEventNum(fd);// 返回插入到的列表的索引
    if(i < 0) { // 出现分配内存失败，注册事件失败
        free(event);
        return NULL;
    }
	//这里的fdEventsLast和fdEvents到底是是什么关系？？！！
	//应该是数组链表结构，每个数组都有一个链表结构, 每个链表的event的fd都相同
	//fdEvents[i] 是水平方向的游标
	//fdEventsLast[i] 是垂直方向的指向最后一个元素的游标
    event->next = NULL;
    event->previous = fdEventsLast[i];
    if(fdEvents[i] == NULL) { //最开始fdEvents[i]==NULL
        fdEvents[i] = event; //数组链表的开始
    } else { //再次在第i个位置插入，会插入到fdEvents[i]延伸的链表中.
        fdEventsLast[i]->next = event;
    }
    fdEventsLast[i] = event; //移动游标指向最后一个元素（新加入的元素）
    poll_fds[i].events |= event->poll_events; //加入事件, 保存fdEvents[i]这条链上所有元素的事件的

    return event;
}
//最终把事件放到了一个数组链表中
//ref: dns.c｜io.c | server.c
FdEventHandlerPtr 
registerFdEvent(int fd, int poll_events, 
                int (*handler)(int, FdEventHandlerPtr), int dsize, void *data)
{
    FdEventHandlerPtr event;

    event = makeFdEvent(fd, poll_events, handler, dsize, data);
    if(event == NULL)
        return NULL;
	//返回event说明注册成功
    return registerFdEventHelper(event);
}

//重新计算poll_fds[i]的events
//staticref: unregisterFdEventI
static int
recomputePollEvents(FdEventHandlerPtr event) 
{
    int pe = 0;
    while(event) {
        pe |= event->poll_events; //合并该链上所有的poll_events.
        event = event->next;
    }
    return pe | POLLERR | POLLHUP | POLLNVAL;
}

//删除event，并且重新计算poll_fds[i]的events
//这个event是在已经 events队列中的引用了吗？？（是的） 猜测应该是通过查找获得引用
//staticref: unregisterFdEvent | pokeFdEventHandler | eventLoop
static void
unregisterFdEventI(FdEventHandlerPtr event, int i)
{
    assert(i < fdEventNum && poll_fds[i].fd == event->fd);
	
    if(fdEvents[i] == event) { //如果数组链表的第一个元素等于event
        assert(!event->previous); //event->previous 一定等于null
        fdEvents[i] = event->next;//移动光标到下一个
    } else {
        event->previous->next = event->next; //断开event前面的链接
    }

    if(fdEventsLast[i] == event) { //如果最后一个元素等于event
        assert(!event->next); //那event->next 一定等于null
        fdEventsLast[i] = event->previous; //向上移动fdEventsLast[i]
    } else {
        event->next->previous = event->previous; //断开event后面的链接
    }

    free(event); //释放event

    if(fdEvents[i] == NULL) { //如果event等于fdEvents[i]，并且第i条链上只有一个元素被删了
        deallocateFdEventNum(i); //删除这个元素，把三个数组统一前移一位
    } else { //因为删除了一个元素，重新计算fdEvents[i]上的事件总和 -> poll_fds[i]的events
        poll_fds[i].events = recomputePollEvents(fdEvents[i]) | 
            POLLERR | POLLHUP | POLLNVAL; //这里貌似不必须了，在recomputePollEvents里设置过了
    }
}

//ref：io.c | server.c 
//staticref: pokeFdEventHandler | eventLoop
void 
unregisterFdEvent(FdEventHandlerPtr event)
{
    int i;

    for(i = 0; i < fdEventNum; i++) {
        if(poll_fds[i].fd == event->fd) { //poll_fds[i].fd = fdEvents[i].fd
            unregisterFdEventI(event, i);
            return;
        }
    }
	//event不存在，中断程序运行
    abort();
}

//同步，从前往后执行队列中的到时的event， 并从队列中删除
//staticref: eventLoop
void
runTimeEventQueue()
{
    TimeEventHandlerPtr event;
    int done;
	//事件发生的时间到了，执行事件
    while(timeEventQueue && 
          timeval_cmp(&timeEventQueue->time, &current_time) <= 0) {
        event = timeEventQueue;
        timeEventQueue = event->next;
        if(timeEventQueue)
            timeEventQueue->previous = NULL; //not the last one.
        else
            timeEventQueueLast = NULL; //last one
        done = event->handler(event); // call handler function to handler event.
        assert(done);
        free(event);
    }
}

//staticref: findEvent
static FdEventHandlerPtr
findEventHelper(int revents, FdEventHandlerPtr events)
{
    FdEventHandlerPtr event = events;
    while(event) {
        if(revents & event->poll_events)
            return event;
        event = event->next;
    }
    return NULL;
}

//staticref: eventLoop
static FdEventHandlerPtr
findEvent(int revents, FdEventHandlerPtr events)
{
    FdEventHandlerPtr event;

    assert(!(revents & POLLNVAL));
    
    if((revents & POLLHUP) || (revents & POLLERR)) {
        event = findEventHelper(POLLOUT, events);
        if(event) return event;

        event = findEventHelper(POLLIN, events);
        if(event) return event;
        return NULL;
    }

    if(revents & POLLOUT) {
        event = findEventHelper(POLLOUT, events);
        if(event) return event;
    }

    if(revents & POLLIN) {
        event = findEventHelper(POLLIN, events);
        if(event) return event;
    }
    return NULL;
}

typedef struct _FdEventHandlerPoke {
    int fd;
    int what; //poll_event 
    int status;
} FdEventHandlerPokeRec, *FdEventHandlerPokePtr;

//定时执行这个函数，处理fdevent   what
//staticref: pokeFdEvent
static int
pokeFdEventHandler(TimeEventHandlerPtr tevent)
{
    FdEventHandlerPokePtr poke = (FdEventHandlerPokePtr)tevent->data;
    int fd = poke->fd;
    int what = poke->what;
    int status = poke->status;
    int done;
    FdEventHandlerPtr event, next;
    int i;

    for(i = 0; i < fdEventNum; i++) {
        if(poll_fds[i].fd == fd)
            break;
    }
	//未找到
    if(i >= fdEventNum)
        return 1;
	//指向当前第i个fdEvents
    event = fdEvents[i];
    while(event) { //纵向遍历链表
        next = event->next;
        if(event->poll_events & what) { //相等->某个事件发生了
            done = event->handler(status, event); //处理
            if(done) { //处理成功就销毁这个event从fdEvents[i]中
				//默认为0
                if(fds_invalid)
                    unregisterFdEvent(event); 
                else
                    unregisterFdEventI(event, i); //如果第i个元素fdEvents[i]无效了，fds_invalid=1
            }
            if(fds_invalid) //第i条链没有元素了
                break;
        }
        event = next; //指向下一个
    }
    return 1;
}

//构造FdEventHandlerPokeRec，设置TimeEventHandlerPtr处理FdEventHandlerPokeRec
//ref：client.c | http.c | io.c | server.c
void 
pokeFdEvent(int fd, int status, int what)
{
    TimeEventHandlerPtr handler;
    FdEventHandlerPokeRec poke;

    poke.fd = fd;
    poke.status = status;
    poke.what = what;
	//seconds 设置为0，立即执行
    handler = scheduleTimeEvent(0, pokeFdEventHandler,
                                sizeof(poke), &poke);
    if(!handler) {
        do_log(L_ERROR, "Couldn't allocate handler.\n");
    }
}

//ref: object.c
//func： 检查是time event队列中是否还有事件 返回bool类型
//再分析
int
workToDo()
{
    struct timeval sleep_time;
    int rc;

    if(exitFlag)
        return 1;

    timeToSleep(&sleep_time);
    gettimeofday(&current_time, NULL);
    if(timeval_cmp(&sleep_time, &current_time) <= 0) //过时的任务
        return 1;
    rc = poll(poll_fds, fdEventNum, 0);//对文件描述符进行查询，返回准备好的文件描述符
    if(rc < 0) { //没有准备好的
        do_log_error(L_ERROR, errno, "Couldn't poll");
        return 1;
    }
    return(rc >= 1); //如果rc==0说明没有准备好的文件描述符，false
}

//不断的轮询socket，检查读写，然后执行读写
//ref: main.c 
void
eventLoop()
{
	//1 sec = 1000000 usec
    struct timeval sleep_time, timeout;
    int rc, i, done, n;
    FdEventHandlerPtr event;
    int fd0;
	
    gettimeofday(&current_time, NULL);
	
    while(1) {
    again:
		/*exitFlag：
		 * 1: reopenLog()
			  ->writeoutObjects(1)
			  ->initForbidden()
			  ->exitFlag=0;
		 * 2: reopenLog()
			  ->discardObjects(1,0)
			  ->free_chunk_arenas()
			  ->initForbidden()
			  ->exitFlag=0;
	     * 3: discardObjects(1, 0)
			  ->return;
		 */
        if(exitFlag) {
            if(exitFlag < 3)
                reopenLog();
            if(exitFlag >= 2) {
                discardObjects(1, 0);
                if(exitFlag >= 3)
                    return;
                free_chunk_arenas();
            } else {
                writeoutObjects(1);
            }
            initForbidden();
            exitFlag = 0;
        }
		//sleep_time等于时间队列的最后一个
        timeToSleep(&sleep_time);
		
        if(sleep_time.tv_sec == -1) {//timeEventQueue 为空
			//轮询poll_fds，查看有rc个准备好的socket
			//idleTime定义在object.c 中，超时时间设置为20s 空闲时间
            //rc为0，超时前未发生任何事件
			//rc>0 有rc个socket发生了事件
			//rc=-1：轮询失败
			//总之这里要停20s （maybe）
			rc = poll(poll_fds, fdEventNum, 
                      diskIsClean ? -1 : idleTime * 1000);
        } else if(timeval_cmp(&sleep_time, &current_time) <= 0) {//任务需要执行
            runTimeEventQueue(); //执行TimeEventQueue中到时的任务
            continue;
        } else { //如果没有超时事件
            gettimeofday(&current_time, NULL); //更新current_time
            if(timeval_cmp(&sleep_time, &current_time) <= 0) {
                runTimeEventQueue(); //执行
                continue;
            } else { //还没有event需要执行 sleep_time > current_time
                int t; //记录毫秒数
                timeval_minus(&timeout, &sleep_time, &current_time);
                t = timeout.tv_sec * 1000 + (timeout.tv_usec + 999) / 1000; //向上取整
				//没有event到时，再次轮询，如果磁盘清空，就轮询t毫秒，否则，轮询MIN(idleTime * 1000, t) 
                rc = poll(poll_fds, fdEventNum,
                          diskIsClean ? t : MIN(idleTime * 1000, t));
            }
        }

        gettimeofday(&current_time, NULL);  //更新current_time

        if(rc < 0) { //轮询失败
            if(errno == EINTR) { //请求的事件之前产生一个信号，调用可以重新发起。
                continue;
            } else if(errno == ENOMEM) { //可用内存不足，无法完成请求。
                free_chunk_arenas();
                do_log(L_ERROR, 
                       "Couldn't poll: out of memory.  "
                       "Sleeping for one second.\n");
                sleep(1);
            } else {
                do_log_error(L_ERROR, errno, "Couldn't poll");
                exitFlag = 3;
            }
            continue; // 不执行后面，如果exitFlag=3，退出
        }

        if(rc == 0) { //超时前未发生任何事件
            if(!diskIsClean) { //磁盘未清空，并且下一个event的时间还未到
                timeToSleep(&sleep_time);
                if(timeval_cmp(&sleep_time, &current_time) > 0)
                    writeoutObjects(0); //将object写出
            }
            continue;
        }

		//并不是要检测所有的改变，当看到有活动，我们认为是有事物发生了改变。
        /* Rather than tracking all changes to the in-memory cache, we
           assume that something changed whenever we see any activity. */
        diskIsClean = 0; //设置磁盘状态为未清空
		//随机产生一个索引
        fd0 = 
            (current_time.tv_usec ^ (current_time.tv_usec >> 16)) % fdEventNum;
        n = rc;
		//遍历所有的fdEvents，查找轮询poll时文件描述符发生的可读或可写事件
        for(i = 0; i < fdEventNum; i++) {
            int j = (i + fd0) % fdEventNum;
            if(n <= 0)
                break;
            if(poll_fds[j].revents) { // poll返回的时候会设置
                n--; //减少一个event
                event = findEvent(poll_fds[j].revents, fdEvents[j]);  //在fdEvents[i]中查找发生的事件revents
                if(!event) //向下一条查找
                    continue;
                done = event->handler(0, event); //httpAccept处理event， 0代表未发生错误
                if(done) {
                    if(fds_invalid)
                        unregisterFdEvent(event);
                    else
                        unregisterFdEventI(event, j);
                }
                if(fds_invalid) { //poll_fds fdEvents数组统一向前移动了一位
                    fds_invalid = 0;
                    goto again;
                } 
            }
        }
    }
}

/*
ref: object.c
staticref: makeCondition
func: 初始化ConditionPtr
*/
void
initCondition(ConditionPtr condition)
{
    condition->handlers = NULL;
}

/*
func: 创建ConditionPtr
*/
ConditionPtr
makeCondition(void)
{
    ConditionPtr condition;
    condition = malloc(sizeof(ConditionRec));
    if(condition == NULL)
        return NULL;
    initCondition(condition);
    return condition;
}

/*
ref: client.c | dns.c | server.c
func: 创建ConditionHandlerPtr，并加入链表最前面
*/
ConditionHandlerPtr
conditionWait(ConditionPtr condition,
              int (*handler)(int, ConditionHandlerPtr),
              int dsize, void *data)
{
    ConditionHandlerPtr chandler;

    assert(!in_signalCondition);

    chandler = malloc(sizeof(ConditionHandlerRec) - 1 + dsize);
    if(!chandler)
        return NULL;

    chandler->condition = condition;
    chandler->handler = handler;
    /* Let the compiler optimise the common case */
    if(dsize == sizeof(void*))
        memcpy(chandler->data, data, sizeof(void*));
    else if(dsize > 0)
        memcpy(chandler->data, data, dsize);

    if(condition->handlers)
        condition->handlers->previous = chandler;
    chandler->next = condition->handlers;
    chandler->previous = NULL;
    condition->handlers = chandler; //作为一个链表头指针
    return chandler;
}

/*
ref: client.c
func: 删除ConditionHandlerPtr元素
staticref: abortConditionHandler
*/
void
unregisterConditionHandler(ConditionHandlerPtr handler)
{
    ConditionPtr condition = handler->condition; //链表头指针所在结构体

    assert(!in_signalCondition); //不是信号条件

    if(condition->handlers == handler) //位于第一个
        condition->handlers = condition->handlers->next;
    if(handler->next) //断开后面的关系
        handler->next->previous = handler->previous;
    if(handler->previous) //断开前面的关系
        handler->previous->next = handler->next;

    free(handler); //释放handler
}

/*
ref: client.c
func: 中断执行
callr: unregisterConditionHandler
*/
//
void 
abortConditionHandler(ConditionHandlerPtr handler)
{
    int done;
    done = handler->handler(-1, handler); //设置为-1，大概是终止执行handler的意思
    assert(done);
    unregisterConditionHandler(handler); //删除该handler
}

/*
ref: object.c
func: 是信号condition
*/
void
signalCondition(ConditionPtr condition)
{
    ConditionHandlerPtr handler;
    int done;

    assert(!in_signalCondition);
    in_signalCondition++; //处理之中，其他的都会因assert而中断

    handler = condition->handlers; //头指针
    while(handler) { //循环处理condition事件
        ConditionHandlerPtr next = handler->next;
        done = handler->handler(0, handler); //statue为0
        if(done) {
            if(handler == condition->handlers) //头指针移动到下一个
                condition->handlers = next;
            if(next) //断开后面的关系
                next->previous = handler->previous;
            if(handler->previous) //断开前面的关系
                handler->previous->next = next;
            else
                condition->handlers = next;
            free(handler); //删除该handler
        }
        handler = next; //处理下一个
    }
    in_signalCondition--;
}

/*
ref: client.c | local.c | server.c
*/
void
polipoExit()
{
    exitFlag = 3;
}
