1. httpAccept如何处理event，处理了哪些事件。 0代表未发生错误
2. 什么时候，并且怎么把事件放入fdEvents数组, 即 fdEvents[i].fd，那么时候会有新的fd生成，加入进来，新的事件加入进来
    * 程序在调用registerFdEvent和registerFdEventHelper时会将event加入数组，
    * 根据fd来分配数组fd是文件描述符，这里应该指的是客户端的socket的请求。
    * fd是一个小于1024的整数，所以3中的poll_fds长度一定小于等于1024
    * io.c: schedule_stream-> event.c:registerFdEventHelper
    * io.c: do_stream_1 -> io.c: schedule_stream
    * io.c: do_stream_2 -> io.c: schedule_stream
    * io.c: do_stream_3 -> io.c: schedule_stream
    * io.c: do_stream_h -> io.c: schedule_stream
    * io.c: do_stream_buf -> io.c: schedule_stream
   
    * dns.c: establishDnsSocket -> event.c: registerFdEvent
    * dns.c: really_do_dns-> dns.c: establishDnsSocket
    * dns.c: dnsReplyHandler-> dns.c: establishDnsSocket
    
    * io.c: do_connect-> event.c: registerFdEvent
    * server.c:httpServerConnectionDnsHandler ->io.c: do_connect
    * socks.c:do_socks_connect_common ->io.c: do_connect
    * tunnel.c:tunnelDnsHandler ->io.c: do_connect

    * io.c: schedule_accept-> event.c: registerFdEvent
    * client.c: httpAcceptAgain -> io.c: schedule_accept
    * client.c: httpAccept -> client.c: httpAcceptAgain
    * io.c: do_accept-> io.c: schedule_accept
    * io.c: create_listener-> io.c: schedule_accept
    * main.c: main -> io.c: create_listener

    * io.c: lingeringClose-> event.c: registerFdEvent
    * client.c: httpClientFinish-> io.c: lingeringClose

    * server.c: httpServerTrigger-> event.c: registerFdEvent
    * server.c: httpServerFinish-> server.c: httpServerTrigger
    * server.c: httpMakeServerRequest-> server.c: httpServerTrigger
    * server.c: httpServerConnectionHandlerCommon-> server.c: httpServerTrigger
    
3. poll_fds,fdEventNum 和fdEventsLast，三个链表数组的关系
   poll_fds是一个数组，每个数组元素延伸出一个fdEventNum的双向链表，对应的第i个元素有一个fdEventsLast
   指向第i个fdEventNum的链表尾部，以方便追加新的元素。
   poll_fds的第i个元素以及延伸出的第fdEventNum双向链表，拥有同样的fd，并且在poll_fds[i]中存放着该
   链表中所有元素发生的事件的总和。
   