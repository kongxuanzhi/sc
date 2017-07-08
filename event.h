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

extern struct timeval current_time;
extern struct timeval null_time;
extern int diskIsClean;

typedef struct _TimeEventHandler {
    struct timeval time;
    struct _TimeEventHandler *previous, *next;
    int (*handler)(struct _TimeEventHandler*);
    char data[1];
} TimeEventHandlerRec, *TimeEventHandlerPtr;

typedef struct _FdEventHandler {
    short fd; //文件描述符
    short poll_events; //文件轮询发生的事件s
    struct _FdEventHandler *previous, *next; //双向链表
    int (*handler)(int, struct _FdEventHandler*); //处理该事件的函数
    char data[1]; //申请内存的时候，这个内存先减去再加上实际data的大小。
} FdEventHandlerRec, *FdEventHandlerPtr;

typedef struct _ConditionHandler {
    struct _Condition *condition; //每一个元素又可以纵向扩展
    struct _ConditionHandler *previous, *next;
    int (*handler)(int, struct _ConditionHandler*);
    char data[1];
} ConditionHandlerRec, *ConditionHandlerPtr;

typedef struct _Condition { //作为一个链表头指针
    ConditionHandlerPtr handlers;
} ConditionRec, *ConditionPtr;

void initEvents(void);
void uninitEvents(void);
#ifdef HAVE_FORK
void interestingSignals(sigset_t *ss);
#endif

TimeEventHandlerPtr scheduleTimeEvent(int seconds,
                                      int (*handler)(TimeEventHandlerPtr),
                                      int dsize, void *data);

int timeval_minus_usec(const struct timeval *s1, const struct timeval *s2)
     ATTRIBUTE((pure));
void cancelTimeEvent(TimeEventHandlerPtr);
int allocateFdEventNum(int fd);
void deallocateFdEventNum(int i);
void timeToSleep(struct timeval *);
void runTimeEventQueue(void);
FdEventHandlerPtr makeFdEvent(int fd, int poll_events, 
                              int (*handler)(int, FdEventHandlerPtr), 
                              int dsize, void *data);
FdEventHandlerPtr registerFdEvent(int fd, int poll_events,
                                  int (*handler)(int, FdEventHandlerPtr),
                                  int dsize, void *data);
FdEventHandlerPtr registerFdEventHelper(FdEventHandlerPtr event);
void unregisterFdEvent(FdEventHandlerPtr event);
void pokeFdEvent(int fd, int status, int what);
int workToDo(void);
void eventLoop(void);
ConditionPtr makeCondition(void);
void initCondition(ConditionPtr);
void signalCondition(ConditionPtr condition);
ConditionHandlerPtr 
conditionWait(ConditionPtr condition,
              int (*handler)(int, ConditionHandlerPtr),
              int dsize, void *data);
void unregisterConditionHandler(ConditionHandlerPtr);
void abortConditionHandler(ConditionHandlerPtr);
void polipoExit(void);
