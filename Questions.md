1. httpAccept如何处理event，处理了哪些事件。 0代表未发生错误
2. 什么时候，并且怎么把事件放入fdEvents数组
3. fdEvents[i].fd = i，那么时候会有新的fd生成，加入进来，新的事件加入进来