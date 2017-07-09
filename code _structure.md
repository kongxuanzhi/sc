 * io.c: schedule_stream-> event.c:registerFdEventHelper
    * io.c: do_stream -> io.c: schedule_stream
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
        #httpAccept 作为回调函数使用

    * io.c: do_accept-> io.c: schedule_accept
        #do_accept 未添加引用

    * io.c: create_listener-> io.c: schedule_accept

    * main.c: main -> io.c: create_listener

    * io.c: lingeringClose-> event.c: registerFdEvent
    * client.c: httpClientFinish-> io.c: lingeringClose

    * server.c: httpServerTrigger-> event.c: registerFdEvent
    * server.c: httpServerFinish-> server.c: httpServerTrigger
    * server.c: httpMakeServerRequest-> server.c: httpServerTrigger
    * server.c: httpServerConnectionHandlerCommon-> server.c: httpServerTrigger
