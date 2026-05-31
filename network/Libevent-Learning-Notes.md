# Libevent 学习笔记

这份笔记整理了 `sample/hello-world.c` 示例背后的 Libevent 核心机制，覆盖
`event_base`、`event`、`eventop`、`evconnlistener`、`bufferevent`、
`evbuffer`、`evbuffer_chain` 和 timer 机制。

*code repo: master 1b7cc9b58d6a5fdd5f33a43168245c2e1f35291f*

## 目录

1. [总体模型：Libevent 的核心组件如何分层？](#1-总体模型libevent-的核心组件如何分层)
2. [hello-world.c 执行流程：一个 TCP 连接如何被接收、写回并关闭？](#2-hello-worldc-执行流程一个-tcp-连接如何被接收写回并关闭)
3. [evconnlistener：如何创建 listen socket 并 accept 新连接？](#3-evconnlistener如何创建-listen-socket-并-accept-新连接)
   - [3.1 evconnlistener_new_bind() 做了什么？](#31-evconnlistener_new_bind-做了什么)
   - [3.2 evconnlistener_new() 做了什么？](#32-evconnlistener_new-做了什么)
   - [3.3 listen socket 和连接 socket 有什么区别？](#33-listen-socket-和连接-socket-有什么区别)
4. [event：一个事件注册项保存了哪些信息？](#4-event一个事件注册项保存了哪些信息)
5. [event_base：事件循环如何管理所有状态？](#5-event_base事件循环如何管理所有状态)
6. [eventop：Libevent 如何屏蔽 epoll/kqueue/poll/select 差异？](#6-eventoplibevent-如何屏蔽-epollkqueuepollselect-差异)
7. [event_assign() 和 event_add()：初始化事件和注册事件有什么区别？](#7-event_assign-和-event_add初始化事件和注册事件有什么区别)
   - [7.1 event_assign()](#71-event_assign)
   - [7.2 event_add()](#72-event_add)
8. [event loop：事件循环每一轮做什么？](#8-event-loop事件循环每一轮做什么)
9. [timer 机制：timeout 如何集成进事件循环？](#9-timer-机制timeout-如何集成进事件循环)
   - [9.1 timeout_next()](#91-timeout_next)
   - [9.2 timeout_process()](#92-timeout_process)
   - [9.3 IO 和 timeout 同时存在时怎么处理？](#93-io-和-timeout-同时存在时怎么处理)
   - [9.4 common timeout](#94-common-timeout)
10. [bufferevent：如何封装单条 stream 连接的异步读写？](#10-bufferevent如何封装单条-stream-连接的异步读写)
    - [10.1 bufferevent_socket_new()](#101-bufferevent_socket_new)
    - [10.2 bufferevent_setcb()](#102-bufferevent_setcb)
    - [10.3 bufferevent_enable()](#103-bufferevent_enable)
    - [10.4 bufferevent_write()](#104-bufferevent_write)
11. [evbuffer：bufferevent 的输入/输出缓冲区如何设计？](#11-evbufferbufferevent-的输入输出缓冲区如何设计)
12. [evbuffer_chain：为什么 evbuffer 使用链表结构？](#12-evbuffer_chain为什么-evbuffer-使用链表结构)
    - [12.1 misalign 和 off](#121-misalign-和-off)
    - [12.2 为什么 evbuffer_chain 要组织成链表？](#122-为什么-evbuffer_chain-要组织成链表)
13. [hello-world.c 的组件关系：所有组件如何串起来？](#13-hello-worldc-的组件关系所有组件如何串起来)

## 1. 总体模型：Libevent 的核心组件如何分层？

Libevent 的核心可以分成几层：

```text
event_base
    一个事件循环实例。保存后端状态、fd 映射、signal 映射、active 队列、
    timeout 小顶堆等。

event
    一个事件注册项。描述“关心哪个 fd / signal / timeout，以及触发后调用谁”。

eventop
    底层 IO 多路复用后端的操作表。用于屏蔽 epoll/kqueue/poll/select/IOCP
    等平台差异。

evconnlistener
    TCP/Unix stream listener 封装。负责 listen socket 的事件注册和 accept。

bufferevent
    面向流式连接的读写封装。内部包含读事件、写事件、输入缓冲区、输出缓冲区。

evbuffer
    链式字节缓冲区。用于 bufferevent 的 input/output buffer，也被其他模块使用。
```

一条典型调用链是：

```text
event_assign()
    初始化 struct event，但不注册到底层后端。

event_add()
    把 event 加入 event_base，并注册到底层后端；如果传了 timeout，也加入 timer。

event_base_dispatch()
    启动事件循环。

backend dispatch
    等待 IO，就近的 timeout 会作为最大等待时间。

event_active_nolock_()
    把就绪事件放入 active 队列。

event_process_active()
    执行用户 callback。
```

## 2. hello-world.c 执行流程：一个 TCP 连接如何被接收、写回并关闭？

`sample/hello-world.c` 是一个最小 TCP server：

```text
监听 TCP 9995 端口
每来一个客户端连接，就写入 "Hello, World!\n"
写完并 flush 后关闭该客户端连接
收到 Ctrl-C 后延迟 2 秒退出
```

核心初始化：

```c
base = event_base_new();
```

创建一个 `event_base`，也就是事件循环对象。

```c
sin.sin_family = AF_INET;
sin.sin_port = htons(PORT);
```

设置 IPv4 和端口 `9995`。没有设置 `sin_addr`，所以默认绑定
`0.0.0.0:9995`。

```c
listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
    LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
    (struct sockaddr *)&sin, sizeof(sin));
```

创建 TCP 监听 socket，绑定地址，调用 `listen()`，并把监听 fd 注册到
`event_base`。有新连接时调用 `listener_cb`。

```c
signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);
event_add(signal_event, NULL);
```

注册 `SIGINT` 信号事件，也就是 Ctrl-C。

```c
event_base_dispatch(base);
```

进入事件循环。

有客户端连接时的链路：

```text
listen fd 可读
-> epoll/poll/select 后端返回 EV_READ
-> Libevent 调 listener_read_cb()
-> listener_read_cb() 调 accept()
-> 得到 conn_fd
-> 调用户的 listener_cb(listener, conn_fd, ...)
```

`listener_cb` 里：

```c
bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
bufferevent_setcb(bev, NULL, conn_writecb, conn_eventcb, NULL);
bufferevent_enable(bev, EV_WRITE);
bufferevent_disable(bev, EV_READ);
bufferevent_write(bev, MESSAGE, strlen(MESSAGE));
```

这里的 `fd` 已经是 `accept()` 出来的连接 socket，不是 listen socket。

执行过程：

```text
创建 socket bufferevent
-> 设置用户 write callback 和 event callback
-> 启用写方向
-> 禁用读方向
-> 把 "Hello, World!\n" 追加到 output evbuffer
-> socket 可写时由 bufferevent_writecb 刷到 fd
-> output 清空后调用 conn_writecb
-> conn_writecb 释放 bufferevent
-> BEV_OPT_CLOSE_ON_FREE 关闭 conn_fd
```

## 3. evconnlistener：如何创建 listen socket 并 accept 新连接？

相关实现主要在 `listener.c`。

### 3.1 evconnlistener_new_bind() 做了什么？

`evconnlistener_new_bind()` 是高级接口，内部大致做：

```text
socket()
setsockopt()
bind()
evconnlistener_new()
```

关键代码：

```c
int family = sa ? sa->sa_family : AF_UNSPEC;
int socktype = SOCK_STREAM | EVUTIL_SOCK_NONBLOCK;

fd = evutil_socket_(family, socktype, 0);
```

`SOCK_STREAM` 表示它创建的是流式 socket。对于 `AF_INET` / `AF_INET6`，
通常就是 TCP socket。UDP 使用的是 `SOCK_DGRAM`，所以
`evconnlistener_new_bind()` 不是 UDP 接收接口。

之后它会按 flags 设置选项：

```text
LEV_OPT_REUSEABLE
LEV_OPT_REUSEABLE_PORT
LEV_OPT_DEFERRED_ACCEPT
LEV_OPT_BIND_IPV6ONLY
LEV_OPT_BIND_IPV4_AND_IPV6
LEV_OPT_CLOSE_ON_EXEC
```

然后绑定地址：

```c
bind(fd, sa, socklen)
```

最后调用：

```c
evconnlistener_new(base, cb, ptr, flags, backlog, fd);
```

### 3.2 evconnlistener_new() 做了什么？

`evconnlistener_new()` 接收一个已有 fd，把它变成 Libevent listener。

如果 `backlog > 0` 或 `backlog < 0`，它会调用：

```c
listen(fd, backlog);
```

然后分配并初始化内部结构 `struct evconnlistener_event`，其中包含一个
`struct event`：

```c
event_assign(&lev->listener, base, fd, EV_READ | EV_PERSIST,
    listener_read_cb, lev);
```

这里的含义是：

```text
当 listen fd 可读时，调用 listener_read_cb
EV_PERSIST 表示触发后继续监听，不要自动删除
```

启用 listener 时，会调用：

```c
event_add(&lev_e->listener, NULL);
```

### 3.3 listen socket 和连接 socket 有什么区别？

listen socket：

```text
socket(AF_INET, SOCK_STREAM, 0)
bind(0.0.0.0:9995)
listen()
```

职责：

```text
占住本地端口
接收 TCP 握手
维护 accept 队列
在有连接可 accept 时变为可读
```

连接 socket：

```text
conn_fd = accept(listen_fd, ...)
```

职责：

```text
代表一个具体客户端连接
用于 read/write/send/recv
关闭它只影响这一条连接
```

如果客户端用 UDP 发到 `9995`，这个 TCP listener 不会收到。TCP 9995 和
UDP 9995 是不同协议上的不同端点。

## 4. event：一个事件注册项保存了哪些信息？

`struct event` 定义在 `include/event2/event_struct.h`。

核心字段：

```c
struct event {
    struct event_callback ev_evcallback;

    evutil_socket_t ev_fd;
    short ev_events;
    short ev_res;

    struct event_base *ev_base;

    union {
        struct {
            LIST_ENTRY(event) ev_io_next;
            struct timeval ev_timeout;
        } ev_io;

        struct {
            LIST_ENTRY(event) ev_signal_next;
            short ev_ncalls;
            short *ev_pncalls;
        } ev_signal;
    } ev_;

    struct timeval ev_timeout;
};
```

重要含义：

```text
ev_fd
    IO event 中是 fd；signal event 中是信号编号。

ev_events
    用户关心的事件，例如 EV_READ、EV_WRITE、EV_SIGNAL、EV_PERSIST。

ev_res
    实际触发 callback 的原因，例如 EV_READ、EV_TIMEOUT。

ev_base
    所属 event_base。

ev_callback / ev_arg
    用户 callback 和用户参数，通过内部宏映射到 ev_evcallback 中。

ev_flags
    当前 event 在哪些内部队列里，例如 EVLIST_INSERTED、EVLIST_ACTIVE、
    EVLIST_TIMEOUT。

ev_timeout
    绝对到期时间。只有带 timeout 的 event 才会使用。
```

`event` 不是“已经发生的事件记录”，而是一个注册项：

```text
当 fd X 出现 EV_READ，或者 timeout 到期时，调用 callback。
```

## 5. event_base：事件循环如何管理所有状态？

`struct event_base` 定义在 `event-internal.h`。

核心字段：

```c
const struct eventop *evsel;
void *evbase;

struct event_io_map io;
struct event_signal_map sigmap;
struct min_heap timeheap;

struct evcallback_list *activequeues;
int nactivequeues;

int event_gotterm;
int event_break;
int running_loop;
```

含义：

```text
evsel
    当前 IO 后端的操作表，例如 epoll。

evbase
    后端私有数据。例如 epoll fd、事件数组等。

io
    fd 到 event 列表的映射。

sigmap
    signal number 到 event 列表的映射。

timeheap
    timeout event 的小顶堆，堆顶是最近到期事件。

activequeues
    已触发、等待执行 callback 的事件队列，支持优先级。

event_gotterm / event_break
    控制事件循环退出。
```

所以 `event_base` 是一个事件循环的状态中心。

## 6. eventop：Libevent 如何屏蔽 epoll/kqueue/poll/select 差异？

`struct eventop` 定义在 `event-internal.h`。

它是底层后端接口：

```c
struct eventop {
    const char *name;
    void *(*init)(struct event_base *);
    int (*add)(struct event_base *, evutil_socket_t fd,
        short old, short events, void *fdinfo);
    int (*del)(struct event_base *, evutil_socket_t fd,
        short old, short events, void *fdinfo);
    int (*dispatch)(struct event_base *, struct timeval *);
    void (*dealloc)(struct event_base *);
    ...
};
```

Libevent 上层统一调用：

```text
event_add()
event_del()
event_base_loop()
```

底层通过 `eventop` 适配：

```text
Linux       epoll
BSD/macOS   kqueue
通用        poll/select
Windows     IOCP/select
```

在 Linux epoll 后端里，`dispatch()` 最终会调用：

```c
epoll_wait(epollop->epfd, events, epollop->nevents, timeout);
```

## 7. event_assign() 和 event_add()：初始化事件和注册事件有什么区别？

### 7.1 event_assign()

`event_assign()` 在 `event.c` 中实现。它只初始化 event，不注册到底层后端。

简化逻辑：

```c
ev->ev_base = base;
ev->ev_callback = callback;
ev->ev_arg = arg;
ev->ev_fd = fd;
ev->ev_events = events;
ev->ev_res = 0;
ev->ev_flags = EVLIST_INIT;
```

它还会根据事件类型设置 closure：

```text
EV_SIGNAL       -> EV_CLOSURE_EVENT_SIGNAL
EV_PERSIST      -> EV_CLOSURE_EVENT_PERSIST
普通 event      -> EV_CLOSURE_EVENT
```

### 7.2 event_add()

`event_add(ev, tv)` 会真正注册事件。

如果 event 包含 IO / signal：

```text
EV_READ / EV_WRITE / EV_CLOSED -> evmap_io_add_()
EV_SIGNAL                      -> evmap_signal_add_()
```

最终会调用底层后端的 `add()`。

如果 `tv != NULL`，还会设置 timeout：

```text
当前时间 now + 相对时间 tv = ev->ev_timeout
插入 event_base->timeheap
```

如果 `tv == NULL`，就没有 timeout。

## 8. event loop：事件循环每一轮做什么？

`event_base_dispatch(base)` 本质上调用 `event_base_loop(base, 0)`。

主循环简化为：

```text
while not done:
    计算最近 timeout
    调 backend dispatch 等待 IO
    处理到期 timer
    执行 active callback
```

源码中的关键顺序：

```c
timeout_next(base, &tv_p);
res = evsel->dispatch(base, tv_p);
timeout_process(base);
event_process_active(base);
```

对 epoll 来说，`tv_p` 会变成 `epoll_wait()` 的 timeout：

```c
timeout = evutil_tv_to_msec_(tv);
res = epoll_wait(epollop->epfd, events, epollop->nevents, timeout);
```

如果 IO 先发生，`epoll_wait()` 提前返回；如果没有 IO，则最多等到最近
timer 到期。

## 9. timer 机制：timeout 如何集成进事件循环？

不是所有 event 加入 `event_base` 后都会有到期时间。

只有这样调用才有 timeout：

```c
event_add(ev, &tv);
```

这样调用没有 timeout：

```c
event_add(ev, NULL);
```

普通 timeout 事件保存在：

```c
base->timeheap
```

这是一个小顶堆，堆顶是最早到期的 event。

### 9.1 timeout_next()

`timeout_next()` 做：

```text
取 timeheap 堆顶
如果没有 timer，tv_p = NULL，后端可以无限等待 IO
如果堆顶已经到期，timeout = 0
否则 timeout = 堆顶到期时间 - 当前时间
```

### 9.2 timeout_process()

`timeout_process()` 做：

```text
while 堆顶 timer 已经过期:
    从 timeout heap 删除
    标记为 active，触发原因是 EV_TIMEOUT
```

之后 `event_process_active()` 执行 callback。

### 9.3 IO 和 timeout 同时存在时怎么处理？

一个 event 可以同时是 IO event 和 timeout event：

```c
event_assign(ev, base, fd, EV_READ, cb, arg);
event_add(ev, &tv);
```

含义：

```text
fd 可读，或者 timeout 到期，哪个先发生就触发 callback
```

callback 里要用 bit 判断：

```c
if (events & EV_READ) {
    ...
}

if (events & EV_TIMEOUT) {
    ...
}
```

普通非持久 event 如果 IO 先触发，`event_process_active()` 会在 callback
之前调用：

```c
event_del_nolock_(ev, EVENT_DEL_NOBLOCK);
```

它会从 IO map 和 timeout heap 中都删除该 event，所以这个 timeout 后续不会
再单独触发。

但有一个边界情况：如果同一轮 loop 中 IO 和 timeout 都已经发生，Libevent 可能
合并结果，让 callback 收到：

```text
EV_READ | EV_TIMEOUT
```

对于 `EV_PERSIST` event，触发后不会被整体删除。如果它带 timeout，Libevent 会
重新安排下一次 timeout：

```text
如果这次是 IO 触发，下一次 timeout 通常从 now 开始计算
如果这次是 timeout 触发，下一次 timeout 尽量从上次计划时间开始计算
```

### 9.4 common timeout

`event_base_init_common_timeout()` 是大量相同 timeout 的优化。

普通 timer 使用小顶堆：

```text
插入 / 删除 O(log n)
```

common timeout 把同一 timeout duration 的事件放入专门队列，并用一个代表性
timeout event 挂到 heap 里，适合大量连接共享同一超时时间的场景。

## 10. bufferevent：如何封装单条 stream 连接的异步读写？

`bufferevent` 是面向 stream socket 的高级封装。

普通 `event` 只告诉你：

```text
fd 可读 / fd 可写
```

`bufferevent` 进一步帮你做：

```text
fd 可读 -> 自动 read 到 input evbuffer -> 调用户 readcb
用户 write -> 追加到 output evbuffer -> fd 可写时自动 flush -> 调用户 writecb
EOF/error/timeout/connect 完成 -> 调用户 eventcb
```

`struct bufferevent` 定义在 `include/event2/bufferevent_struct.h`：

```c
struct bufferevent {
    struct event_base *ev_base;
    const struct bufferevent_ops *be_ops;

    struct event ev_read;
    struct event ev_write;

    struct evbuffer *input;
    struct evbuffer *output;

    struct event_watermark wm_read;
    struct event_watermark wm_write;

    bufferevent_data_cb readcb;
    bufferevent_data_cb writecb;
    bufferevent_event_cb errorcb;
    void *cbarg;

    struct timeval timeout_read;
    struct timeval timeout_write;

    short enabled;
};
```

关键字段：

```text
ev_read
    内部读事件，监听 fd 的 EV_READ。

ev_write
    内部写事件，监听 fd 的 EV_WRITE。

input
    输入缓冲区。Libevent 从 socket 读到的数据放这里。

output
    输出缓冲区。用户要写的数据先放这里。

readcb / writecb / errorcb
    用户回调。

be_ops
    bufferevent 类型操作表。socket/filter/pair/ssl 等类型各有实现。
```

### 10.1 bufferevent_socket_new()

socket bufferevent 实现在 `bufferevent_sock.c`。

`bufferevent_socket_new(base, fd, options)` 做：

```text
分配 struct bufferevent_private
初始化通用 bufferevent 字段
创建 input/output evbuffer
初始化 ev_read
初始化 ev_write
给 output evbuffer 加 callback
```

内部事件：

```c
event_assign(&bufev->ev_read, bufev->ev_base, fd,
    EV_READ | EV_PERSIST | EV_FINALIZE, bufferevent_readcb, bufev);

event_assign(&bufev->ev_write, bufev->ev_base, fd,
    EV_WRITE | EV_PERSIST | EV_FINALIZE, bufferevent_writecb, bufev);
```

这里的 `bufferevent_readcb` / `bufferevent_writecb` 是 Libevent 内部回调，
不是用户回调。它们负责真正 read/write socket，然后再触发用户 callback。

### 10.2 bufferevent_setcb()

保存用户回调：

```c
bufev->readcb = readcb;
bufev->writecb = writecb;
bufev->errorcb = eventcb;
bufev->cbarg = cbarg;
```

`hello-world.c` 中：

```c
bufferevent_setcb(bev, NULL, conn_writecb, conn_eventcb, NULL);
```

表示不关心读，只关心写完和连接事件。

### 10.3 bufferevent_enable()

`bufferevent_enable(bev, EV_READ | EV_WRITE)` 会记录 enabled 状态，并调用
对应类型的 `be_ops->enable()`。

对于 socket bufferevent：

```c
if (event & EV_READ)
    bufferevent_add_event_(&bufev->ev_read, &bufev->timeout_read);

if (event & EV_WRITE)
    bufferevent_add_event_(&bufev->ev_write, &bufev->timeout_write);
```

本质还是把内部 `struct event` 加入 `event_base`。

### 10.4 bufferevent_write()

`bufferevent_write()` 不直接调用 `write(fd, ...)`。

它只做：

```c
evbuffer_add(bufev->output, data, size);
```

之后 output evbuffer 的 callback 会发现有新数据，如果写方向启用，就添加
`ev_write` 事件。fd 可写时，`bufferevent_writecb()` 调：

```c
evbuffer_write_atmost(bufev->output, fd, atmost);
```

把 output 中的数据真正写入 socket。

写完后，如果 output 为空，会删除写事件并触发用户 write callback。

## 11. evbuffer：bufferevent 的输入/输出缓冲区如何设计？

`evbuffer` 是链式字节缓冲区。

它不是 evbuffer 自己实现的一套 slab/arena 内存分配器。它使用 Libevent 的
内存包装层：

```text
mm_malloc
mm_calloc
mm_realloc
mm_free
```

这些宏通常映射到 `event_mm_*` 函数；如果禁用了内存替换机制，则直接映射到：

```text
malloc
calloc
realloc
free
```

evbuffer 自己实现的是：

```text
链式 buffer 管理
尾部追加
头部消费
buffer 拼接
外部内存引用
文件段 / sendfile / mmap
scatter/gather IO
buffer 变化 callback
```

`struct evbuffer` 在 `evbuffer-internal.h` 中：

```c
struct evbuffer {
    struct evbuffer_chain *first;
    struct evbuffer_chain *last;
    struct evbuffer_chain **last_with_datap;

    size_t total_len;
    size_t max_read;

    size_t n_add_for_cb;
    size_t n_del_for_cb;

    int refcnt;

    LIST_HEAD(evbuffer_cb_queue, evbuffer_cb_entry) callbacks;
    struct bufferevent *parent;
};
```

## 12. evbuffer_chain：为什么 evbuffer 使用链表结构？

`evbuffer_chain` 是 evbuffer 链表中的一个节点：

```c
struct evbuffer_chain {
    struct evbuffer_chain *next;
    size_t buffer_len;
    ev_misalign_t misalign;
    size_t off;
    unsigned flags;
    int refcnt;
    unsigned char *buffer;
};
```

普通内存 chain 的分配方式是：

```text
+-----------------------+-------------------------+
| struct evbuffer_chain | byte storage            |
+-----------------------+-------------------------+
^ chain                 ^ chain->buffer
```

也就是 chain 头和数据区一次性分配。

### 12.1 misalign 和 off

`off` 表示当前 chain 里有效数据长度。

`misalign` 表示有效数据前面已经被消费掉的空间。

例如：

```text
buffer:   [ A B C D E F G . . . ]
misalign: 0
off:      7
```

消费掉 `ABC` 后，不一定马上移动内存，而是变成：

```text
buffer:   [ A B C D E F G . . . ]
misalign: 3
off:      4
有效数据: D E F G
```

这样可以避免每次从头部 drain 都做 `memmove()`。

### 12.2 为什么 evbuffer_chain 要组织成链表？

对于 `hello-world.c`，单个可扩容 buffer 就够了。但 Libevent 是通用网络库，
evbuffer 需要支持复杂场景。

网络 buffer 的常见操作是：

```text
从 socket 读入 -> 尾部追加
协议解析       -> 头部消费
继续读入       -> 尾部追加
写到 socket    -> 头部消费
```

如果只有一个连续数组，头部消费会产生前部空洞；后续要么频繁 `memmove()`，
要么在扩容/整理时复制大量数据。

链表结构可以做到：

```text
chain1 -> chain2 -> chain3
```

如果 `chain1` 被完全消费，直接释放它：

```text
chain2 -> chain3
```

不需要移动后续数据。

链表还有几个关键优势：

```text
1. 扩容时可以追加新 chain，避免复制已有大块数据。
2. evbuffer_add_buffer() 可以通过改链表指针完成 O(1) 拼接。
3. 可以混合普通内存、外部引用、文件段、sendfile chain。
4. 可以天然映射到 writev/readv 的多个 iovec。
5. pinned/reference/immutable 数据不需要强行拷贝到一个连续数组。
```

`evbuffer_chain` 的 flags 支持多种存储：

```text
EVBUFFER_REFERENCE
EVBUFFER_IMMUTABLE
EVBUFFER_FILESEGMENT
EVBUFFER_SENDFILE
EVBUFFER_MEM_PINNED_R
EVBUFFER_MEM_PINNED_W
EVBUFFER_MULTICAST
```

所以 `evbuffer` 不是简单字符串 buffer，而是通用流式 IO buffer。

## 13. hello-world.c 的组件关系：所有组件如何串起来？

最终把这些组件串起来：

```text
event_base
    管理 epoll/poll/select 后端
    管理 active callback 队列
    管理 timeout heap

evconnlistener
    包含一个 event，监听 listen_fd 的 EV_READ

listen event
    callback 是 listener_read_cb
    触发后 accept() 得到 conn_fd

用户 listener_cb
    用 conn_fd 创建 bufferevent

bufferevent
    包含 ev_read 和 ev_write
    包含 input evbuffer 和 output evbuffer

bufferevent_write
    把 "Hello, World!\n" 放入 output evbuffer

output evbuffer
    触发 output callback
    添加 ev_write

ev_write
    fd 可写时触发 bufferevent_writecb
    把 output 刷到 socket

用户 conn_writecb
    看到 output 为空
    bufferevent_free()
    关闭 conn_fd
```

一句话总结：

```text
event_base 是调度器。
event 是底层事件注册项。
eventop 是平台 IO 后端适配层。
evconnlistener 负责 accept 新连接。
bufferevent 负责单条 stream 连接上的异步读写。
evbuffer 负责保存输入/输出字节。
evbuffer_chain 让 evbuffer 支持高效追加、消费、拼接和零拷贝场景。
timer 通过 event_base 的小顶堆和后端 wait timeout 融入事件循环。
```
