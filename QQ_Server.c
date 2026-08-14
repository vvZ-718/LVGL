#ifndef _WIN32                           // Linux 开发板需要 POSIX 特性声明，Windows 不使用该宏
#define _POSIX_C_SOURCE 200809L           // 在严格 C99 模式下公开 localtime_r、sigaction 等 POSIX 接口
#endif                                   // 结束 Linux POSIX 特性声明

/*
 * QQ 聊天室 TCP 服务器
 *
 * 编译：arm-linux-gcc QQ_Server.c -o QQ_Server -pthread
 * 运行：./QQ_Server
 * 指定端口：./QQ_Server 8888
 *
 * 客户端与服务器必须共同使用 qq_protocol.h，保证消息头和消息体格式一致。
 */

#include "qq_protocol.h"                 // 引入客户端与服务器共用的 QQ 聊天协议

#include <errno.h>                       // 提供 errno 以及 EINTR、EAGAIN 等错误码
#include <pthread.h>                     // 提供线程和互斥锁接口
#include <signal.h>                      // 提供 SIGINT、SIGTERM 和 SIGPIPE 信号处理
#include <stdarg.h>                      // 提供服务器日志使用的可变参数接口
#include <stdint.h>                      // 提供 uint16_t 和 uint32_t 等固定长度类型
#include <stdio.h>                       // 提供 printf、fprintf 和文件操作
#include <stdlib.h>                      // 提供 malloc、free 和 strtol
#include <string.h>                      // 提供 memset、memcpy、strcmp 和 snprintf
#include <time.h>                        // 提供日志时间戳所需的 time 和 localtime_r

#ifdef _WIN32                           // Windows 本机测试时使用 WinSock 网络接口
#include <winsock2.h>                   // 提供 socket、bind、listen、accept、send 和 recv
#include <ws2tcpip.h>                   // 提供 inet_ntop 和 IPv4 地址转换接口
typedef SOCKET qq_socket_t;             // Windows 套接字句柄可能宽于 int，必须使用 SOCKET 保存
typedef int qq_io_result_t;             // WinSock 的 send 和 recv 返回 int
typedef int qq_socklen_t;                // WinSock 的 accept 地址长度参数使用 int
#define QQ_SERVER_INVALID_SOCKET INVALID_SOCKET // Windows 无效套接字常量
#define QQ_SERVER_SOCKET_ERROR() WSAGetLastError() // 获取最近一次 WinSock 错误码
#define QQ_SERVER_INTERRUPTED WSAEINTR  // WinSock 调用被信号中断时的错误码
#define QQ_SERVER_SHUT_RDWR SD_BOTH     // Windows 同时关闭套接字读写方向的常量
#define qq_server_close_socket closesocket // Windows 使用 closesocket 释放套接字
#else                                   // Linux 开发板使用标准 POSIX 套接字接口
#include <arpa/inet.h>                  // 提供 htonl、ntohl、htons 和 inet_ntop
#include <netinet/in.h>                 // 提供 sockaddr_in 和 INADDR_ANY
#include <sys/socket.h>                 // 提供 socket、bind、listen、accept、send 和 recv
#include <sys/time.h>                   // 提供 timeval，用于设置套接字发送超时
#include <unistd.h>                     // 提供 close
typedef int qq_socket_t;                // Linux 套接字使用普通文件描述符整数
typedef ssize_t qq_io_result_t;         // Linux 的 send 和 recv 返回 ssize_t
typedef socklen_t qq_socklen_t;         // Linux 的 accept 地址长度使用 socklen_t
#define QQ_SERVER_INVALID_SOCKET (-1)   // Linux 无效套接字描述符使用 -1
#define QQ_SERVER_SOCKET_ERROR() errno  // Linux 网络函数通过 errno 返回最近错误
#define QQ_SERVER_INTERRUPTED EINTR     // POSIX 系统调用被信号中断时的错误码
#define QQ_SERVER_SHUT_RDWR SHUT_RDWR   // Linux 同时关闭套接字读写方向的常量
#define qq_server_close_socket close    // Linux 套接字使用 close 释放文件描述符
#endif                                  // 结束 Windows 与 Linux 网络接口选择

#ifndef MSG_NOSIGNAL                    // 某些 Linux C 库可能没有定义 MSG_NOSIGNAL
#define MSG_NOSIGNAL 0                  // 缺少该标志时使用 0 保证源码仍可编译
#endif                                  // 结束 MSG_NOSIGNAL 兼容定义

#define QQ_SERVER_MAX_CLIENTS 32        // 服务器允许同时在线的最大客户端数量
#define QQ_SERVER_LISTEN_BACKLOG 16     // 内核中等待 accept 的连接队列长度
#define QQ_SERVER_SEND_TIMEOUT_SEC 3    // 单个客户端发送阻塞的最长秒数
#define QQ_SERVER_LOG_DEFAULT "QQ_Server.log" // 未配置日志路径时使用的默认日志文件

/* 编译期确认协议结构体没有出现额外填充，避免客户端与服务器包长度不一致。 */
typedef char qq_header_size_must_be_16[(sizeof(qq_chat_message_header_t) == 16) ? 1 : -1];
typedef char qq_body_size_must_be_576[(sizeof(qq_chat_message_body_t) == 576) ? 1 : -1]; //new+ 私聊接收者字段使固定消息体增加为五百七十六字节

/* ===== 保存一个已接入客户端的共享状态 ===== */
typedef struct {
    qq_socket_t fd;                     // 此客户端对应的 TCP 连接套接字描述符或句柄
    int active;                         // 1 表示槽位正在使用，0 表示可以接收新客户端
    int joined;                         // 1 表示客户端已经发送并通过 JOIN 消息验证
    char name[QQ_CHAT_NAME_MAX];        // JOIN 后登记的客户端昵称
} qq_server_client_t;

/* ===== 传递给单个客户端线程的启动参数 ===== */
typedef struct {
    int slot_index;                     // 客户端在全局槽位数组中的索引
    qq_socket_t fd;                     // 创建线程时对应的连接描述符或句柄
    char address[INET_ADDRSTRLEN];       // 客户端 IPv4 地址文字，用于服务器日志
    uint16_t port;                      // 客户端源端口，用于服务器日志
} qq_server_thread_arg_t;

static qq_server_client_t qq_clients[QQ_SERVER_MAX_CLIENTS]; // 保存全部在线客户端的固定槽位数组
static pthread_mutex_t qq_clients_mutex = PTHREAD_MUTEX_INITIALIZER; // 保护客户端槽位、昵称和在线状态
static pthread_mutex_t qq_broadcast_mutex = PTHREAD_MUTEX_INITIALIZER; // 防止不同线程向同一 TCP 流交叉写入包头和包体
static pthread_mutex_t qq_log_mutex = PTHREAD_MUTEX_INITIALIZER; // 防止多个线程同时写日志造成内容交叉
static pthread_cond_t qq_clients_empty_cond = PTHREAD_COND_INITIALIZER; // 所有客户端线程退出后唤醒服务器主线程
static FILE * qq_log_file = NULL;        // 服务器聊天日志文件，打开失败时仍可只输出到终端
static volatile sig_atomic_t qq_server_running = 1; // 主循环运行标志，收到退出信号后改为 0
#ifdef _WIN32                           // Windows 控制台信号处理不在这里关闭监听句柄
static qq_socket_t qq_listen_fd = QQ_SERVER_INVALID_SOCKET; // Windows 监听套接字句柄
#else                                   // Linux 信号处理函数需要使用 sig_atomic_t 保存描述符
static volatile sig_atomic_t qq_listen_fd = QQ_SERVER_INVALID_SOCKET; // Linux 监听描述符，信号处理时关闭以唤醒 accept
#endif                                  // 结束监听套接字状态类型选择
static int qq_active_client_count = 0;    // 已占用客户端槽位数量，用于服务器退出时等待线程清理

/* ===== 输出并保存服务器日志 ===== */
static void qq_server_log(const char * level, const char * format, ...)
{
    char message[1024];                  // 保存格式化后的单条日志正文
    char time_text[32];                  // 保存 YYYY-MM-DD HH:MM:SS 格式时间
    time_t now = time(NULL);             // 获取当前系统时间
    struct tm local_time;                // 保存转换后的本地时间结构
    va_list arguments;                   // 保存可变参数遍历状态

#ifdef _WIN32                           // Windows 使用安全版本 localtime_s，参数顺序与 Linux 不同
    localtime_s(&local_time, &now);      // 把时间转换为线程安全的本地时间结构
#else                                   // Linux 开发板使用 POSIX localtime_r
    localtime_r(&now, &local_time);      // 使用线程安全接口把时间转换为本地时间
#endif                                  // 结束本地时间转换接口选择
    strftime(time_text, sizeof(time_text), "%Y-%m-%d %H:%M:%S", &local_time); // 生成人类可读时间戳

    va_start(arguments, format);         // 开始读取日志格式字符串之后的参数
    vsnprintf(message, sizeof(message), format, arguments); // 安全生成日志正文，过长内容自动截断
    va_end(arguments);                   // 结束可变参数读取

    pthread_mutex_lock(&qq_log_mutex);   // 串行化终端和文件日志输出
    fprintf(stdout, "[%s] [%s] %s\n", time_text, level, message); // 在服务器终端显示日志
    fflush(stdout);                      // 立即刷新标准输出，方便实时观察运行状态
    if(qq_log_file != NULL) {            // 日志文件成功打开时同步写入文件
        fprintf(qq_log_file, "[%s] [%s] %s\n", time_text, level, message); // 记录上线、聊天和下线事件
        fflush(qq_log_file);             // 每条日志立即落盘，降低异常退出时的记录损失
    }
    pthread_mutex_unlock(&qq_log_mutex); // 完成本条日志后允许其他线程继续输出
}

/* ===== 处理服务器退出信号 ===== */
static void qq_server_signal_handler(int signal_number)
{
    qq_socket_t listen_fd;               // 临时保存需要关闭的监听描述符或句柄

    (void)signal_number;                 // 不区分 SIGINT 和 SIGTERM，两者都执行相同停止流程
    qq_server_running = 0;               // 通知 accept 主循环结束，具体清理由主线程执行
    listen_fd = (qq_socket_t)qq_listen_fd; // 在清空全局状态前保存当前监听描述符
    qq_listen_fd = QQ_SERVER_INVALID_SOCKET; // 防止主线程退出流程再次关闭同一个描述符
    if(listen_fd != QQ_SERVER_INVALID_SOCKET) { // 监听套接字已创建时才需要执行关闭
        qq_server_close_socket(listen_fd); // 关闭监听套接字并唤醒阻塞中的 accept
    }
}

/* ===== 循环发送指定长度的 TCP 数据 ===== */
static int qq_server_send_all(qq_socket_t fd, const void * data, size_t length)
{
    const unsigned char * cursor = data; // 指向下一段尚未发送的数据

    while(length > 0) {                  // TCP 可能短写，因此持续发送直到全部完成
        qq_io_result_t sent = send(fd, (const char *)cursor, (int)length, MSG_NOSIGNAL); // 发送数据且避免断线引起 SIGPIPE 终止服务器
        if(sent > 0) {                   // 正数表示本次成功写入 sent 个字节
            cursor += sent;              // 游标移动到剩余数据的起始位置
            length -= (size_t)sent;      // 扣除本次已经写入的字节数
            continue;                    // 继续发送尚未写完的数据
        }
        if(sent < 0 && QQ_SERVER_SOCKET_ERROR() == QQ_SERVER_INTERRUPTED) { // send 被信号中断不属于连接错误
            continue;                    // 直接重试同一段数据
        }
        return -1;                       // 超时、断线或其他错误均向上层报告失败
    }

    return 0;                            // 所有字节均已完整写入 TCP 套接字
}

/* ===== 循环接收指定长度的 TCP 数据 ===== */
static int qq_server_recv_all(qq_socket_t fd, void * data, size_t length)
{
    unsigned char * cursor = data;       // 指向接收缓冲区中下一段可写位置
    size_t received_total = 0;           // 记录当前字段已经接收的总字节数

    while(received_total < length) {     // TCP 没有消息边界，必须主动拼满指定长度
        qq_io_result_t received = recv(fd, (char *)cursor + received_total,
                                       (int)(length - received_total), 0); // 接收剩余字节
        if(received > 0) {               // 正数表示成功接收到了数据
            received_total += (size_t)received; // 累加本次接收长度
            continue;                    // 未达到目标长度时继续读取
        }
        if(received == 0) {              // 返回 0 表示客户端正常关闭了 TCP 连接
            return 0;                    // 通知客户端线程进入离线清理流程
        }
        if(QQ_SERVER_SOCKET_ERROR() == QQ_SERVER_INTERRUPTED) { // recv 被信号中断时连接本身仍可能正常
            continue;                    // 重试当前接收操作
        }
        return -1;                       // 其他 recv 错误表示连接异常
    }

    return 1;                            // 指定长度的数据已经完整接收
}

/* ===== 构造并向一个套接字发送完整协议包 ===== */
static int qq_server_send_packet_unlocked(qq_socket_t fd, uint32_t type,
                                          const char * sender, const char * receiver, //new+ 为底层发包函数增加结构化私聊接收者参数
                                          const char * text)
{
    qq_chat_message_header_t header;     // 创建固定 16 字节的网络协议头
    qq_chat_message_body_t body;         //new+ 创建包含发送者、接收者和正文的固定五百七十六字节消息体

    header.magic = htonl(QQ_CHAT_MAGIC); // 魔数转换为网络字节序
    header.version = htonl(QQ_CHAT_VERSION); // 协议版本转换为网络字节序
    header.type = htonl(type);           // 消息类型转换为网络字节序
    header.body_length = htonl(sizeof(body)); // 固定消息体长度转换为网络字节序

    memset(&body, 0, sizeof(body));      // 清空消息体，避免发送未初始化内存
    snprintf(body.sender, sizeof(body.sender), "%s", sender != NULL ? sender : "SYSTEM"); // 安全写入发送者
    snprintf(body.receiver, sizeof(body.receiver), "%s", receiver != NULL ? receiver : ""); //new+ 安全写入私聊接收者，普通消息传入空字符串
    snprintf(body.text, sizeof(body.text), "%s", text != NULL ? text : ""); // 安全写入聊天或提示正文

    if(qq_server_send_all(fd, &header, sizeof(header)) != 0) { // 先完整发送协议头
        return -1;                       // 包头发送失败时不能继续发送包体
    }
    if(qq_server_send_all(fd, &body, sizeof(body)) != 0) { // 再完整发送固定长度消息体
        return -1;                       // 包体发送失败表示该连接已不可用
    }

    return 0;                            // 完整协议包发送成功
}

/* ===== 线程安全地向一个已连接客户端发送消息 ===== */
static int qq_server_send_packet(qq_socket_t fd, uint32_t type,
                                 const char * sender, const char * text)
{
    int result;                          // 保存底层完整发送函数的返回结果

    pthread_mutex_lock(&qq_broadcast_mutex); // 防止其他线程同时向该客户端写入另一个协议包
    result = qq_server_send_packet_unlocked(fd, type, sender, "", text); //new+ 普通单播消息不指定私聊接收者
    pthread_mutex_unlock(&qq_broadcast_mutex); // 当前完整包发送结束后释放全局发送锁

    return result;                       // 将成功或失败结果交给调用者处理
}

/* ===== 在持有发送锁时向聊天室广播消息 ===== */
static void qq_server_broadcast_with_lock_held(uint32_t type, const char * sender, //new+ 允许在线列表和上线、下线状态组成不可插入的发送事务
                                               const char * text, qq_socket_t excluded_fd) //new+ 调用者必须已经持有 qq_broadcast_mutex
{
    int index;                           //new+ 遍历固定客户端槽位数组使用的索引

    pthread_mutex_lock(&qq_clients_mutex); //new+ 广播期间固定客户端 fd，避免关闭后被系统复用

    for(index = 0; index < QQ_SERVER_MAX_CLIENTS; ++index) { //new+ 检查服务器中的每一个客户端槽位
        qq_socket_t client_fd = qq_clients[index].fd; //new+ 保存当前槽位的套接字描述符或句柄

        if(!qq_clients[index].active || !qq_clients[index].joined ||
           client_fd == QQ_SERVER_INVALID_SOCKET) { //new+ 跳过空闲、未完成 JOIN 或无效槽位
            continue;
        }
        if(client_fd == excluded_fd) {   //new+ 通常排除消息来源，避免客户端重复显示自己的事件
            continue;
        }

        if(qq_server_send_packet_unlocked(client_fd, type, sender, "", text) != 0) { //new+ 广播包接收者保持为空，避免被误判为私聊
            shutdown(client_fd, QQ_SERVER_SHUT_RDWR); //new+ 写入失败时唤醒对应接收线程，由它统一移除并关闭连接
        }
    }

    pthread_mutex_unlock(&qq_clients_mutex); //new+ 广播完成后允许客户端列表继续增删
}

/* ===== 向聊天室中的其他客户端广播消息 ===== */
static void qq_server_broadcast(uint32_t type, const char * sender,
                                const char * text, qq_socket_t excluded_fd)
{
    pthread_mutex_lock(&qq_broadcast_mutex); // 保证一次广播的每个包不会被其他发送线程插入
    qq_server_broadcast_with_lock_held(type, sender, text, excluded_fd); //new+ 在统一发送锁内执行实际客户端遍历和完整包转发
    pthread_mutex_unlock(&qq_broadcast_mutex); // 释放全局发送锁，允许下一条消息开始发送
}

/* ===== 按昵称路由并确认一条私聊消息 ===== */
static int qq_server_route_private(int source_slot, qq_socket_t source_fd, //new+ 使用来源槽位认证发送者并向来源连接返回路由结果
                                   const char * receiver, const char * text) //new+ receiver 是目标昵称，text 是私聊正文
{
    int index;                                                     //new+ 遍历在线客户端槽位查找目标用户
    qq_socket_t target_fd = QQ_SERVER_INVALID_SOCKET;              //new+ 保存匹配目标的套接字并由发送锁保护其生命周期
    char authenticated_sender[QQ_CHAT_NAME_MAX] = {0};             //new+ 保存服务器登记的真实发送者，忽略客户端伪造字段
    char error_text[QQ_CHAT_NAME_MAX + 40];                         //new+ 保存返回给发送方的明确离线错误文字
    int result = 1;                                                //new+ 1 表示业务失败但错误已回复，负一表示来源连接也不可写

    pthread_mutex_lock(&qq_broadcast_mutex);                       //new+ 私聊查找和发送期间阻止目标关闭与套接字复用
    pthread_mutex_lock(&qq_clients_mutex);                         //new+ 读取来源身份并按昵称查找目标前锁定共享数组

    if(source_slot >= 0 && source_slot < QQ_SERVER_MAX_CLIENTS && //new+ 首先确认发起私聊的槽位索引有效
       qq_clients[source_slot].active && qq_clients[source_slot].joined &&
       qq_clients[source_slot].fd == source_fd) {                  //new+ 来源槽位必须仍属于当前已登录连接
        snprintf(authenticated_sender, sizeof(authenticated_sender),
                 "%s", qq_clients[source_slot].name);             //new+ 复制可信昵称后不再使用客户端 body.sender
        for(index = 0; index < QQ_SERVER_MAX_CLIENTS; ++index) {    //new+ 在已登录连接中精确查找目标昵称
            if(qq_clients[index].active && qq_clients[index].joined &&
               qq_clients[index].fd != QQ_SERVER_INVALID_SOCKET &&
               strcmp(qq_clients[index].name, receiver) == 0) {    //new+ 找到与 receiver 完全一致的唯一在线用户
                target_fd = qq_clients[index].fd;                   //new+ 保存目标套接字，发送锁会阻止它被同时关闭
                break;                                             //new+ 服务端禁止重名，因此找到后立即停止遍历
            }
        }
    }
    pthread_mutex_unlock(&qq_clients_mutex);                       //new+ 身份和目标快照完成后释放客户端数组锁

    if(authenticated_sender[0] == '\0') {                          //new+ 来源槽位已经失效时不能继续使用该连接
        result = -1;                                               //new+ 通知客户端线程进入统一断线清理
    }
    else if(receiver == NULL || receiver[0] == '\0' ||             //new+ 空接收者和给自己发送都属于无效私聊请求
            strcmp(receiver, authenticated_sender) == 0) {
        snprintf(error_text, sizeof(error_text), "Invalid private chat target"); //new+ 生成不会导致断线的业务错误提示
        result = qq_server_send_packet_unlocked(source_fd, QQ_MSG_ERROR,
                                                 "SYSTEM", receiver, error_text) == 0 ? 1 : -1; //new+ 将目标写入 ERROR.receiver 供客户端归属会话
    }
    else if(target_fd == QQ_SERVER_INVALID_SOCKET) {                //new+ 当前在线数组中没有指定目标用户
        snprintf(error_text, sizeof(error_text), "User '%s' is offline", receiver); //new+ 返回包含目标昵称的离线提示
        result = qq_server_send_packet_unlocked(source_fd, QQ_MSG_ERROR,
                                                 "SYSTEM", receiver, error_text) == 0 ? 1 : -1; //new+ 只报告本次失败，不断开发送者
    }
    else if(qq_server_send_packet_unlocked(target_fd, QQ_MSG_PRIVATE,
                                            authenticated_sender, receiver, text) != 0) { //new+ 先把可信发送者和目标写入定向私聊包
        shutdown(target_fd, QQ_SERVER_SHUT_RDWR);                    //new+ 目标写入失败时唤醒其线程统一移除连接
        snprintf(error_text, sizeof(error_text), "User '%s' is offline", receiver); //new+ 将发送失败按目标离线反馈
        result = qq_server_send_packet_unlocked(source_fd, QQ_MSG_ERROR,
                                                 "SYSTEM", receiver, error_text) == 0 ? 1 : -1; //new+ 来源仍可用时保持当前聊天连接
    }
    else if(qq_server_send_packet_unlocked(source_fd, QQ_MSG_PRIVATE,
                                            authenticated_sender, receiver, text) != 0) { //new+ 目标写入成功后向发送方回送同一包作为路由确认
        result = -1;                                                //new+ 来源确认写入失败说明当前来源连接需要清理
    }
    else {
        result = 0;                                                 //new+ 目标已收到且发送方已收到服务器路由确认
    }

    pthread_mutex_unlock(&qq_broadcast_mutex);                      //new+ 私聊事务结束后允许其他消息和离线清理继续
    return result;                                                  //new+ 零成功，一业务失败已回复，负一来源连接失败
}

/* ===== 将新连接加入客户端槽位数组 ===== */
static int qq_server_add_client(qq_socket_t fd)
{
    int index;                           // 搜索空闲槽位使用的索引
    int result = -1;                    // 默认返回 -1，表示聊天室已满

    pthread_mutex_lock(&qq_clients_mutex); // 修改共享客户端数组前先加锁
    for(index = 0; index < QQ_SERVER_MAX_CLIENTS; ++index) { // 从头查找第一个空闲槽位
        if(!qq_clients[index].active) {  // active 为 0 表示该槽位可以复用
            qq_clients[index].fd = fd;   // 保存 accept 返回的新连接描述符
            qq_clients[index].active = 1; // 标记槽位已经被占用
            qq_clients[index].joined = 0; // 新连接尚未发送合法 JOIN 消息
            qq_clients[index].name[0] = '\0'; // 清除上一个客户端可能留下的昵称
            ++qq_active_client_count;     // 增加活动连接计数，退出时需要等待对应线程清理
            result = index;              // 返回成功分配的槽位索引
            break;                       // 一个连接只需要占用一个槽位
        }
    }
    pthread_mutex_unlock(&qq_clients_mutex); // 槽位分配结束后释放客户端数组锁

    return result;                       // 返回槽位索引，-1 表示已经达到人数上限
}

/* ===== 登记昵称并同步完整在线用户列表 ===== */
static int qq_server_register_sync_and_announce(int slot_index, qq_socket_t fd, //new+ 把昵称登记、在线列表同步和上线广播组成一个有序事务
                                                const char * name, const char * join_text) //new+ join_text 用于其他客户端的系统上线提示
{
    char online_names[QQ_SERVER_MAX_CLIENTS][QQ_CHAT_NAME_MAX];     //new+ 在锁内复制当前在线昵称，避免持客户端锁执行阻塞网络发送
    int online_count = 0;                                          //new+ 记录快照中已经完成 JOIN 的在线用户数量
    int index;                                                     //new+ 遍历客户端槽位和昵称快照使用的索引
    int result = 0;                                                //new+ 0 表示同步与登记成功，负数表示失败原因
    char count_text[16];                                           //new+ 保存在线列表开始包中的其他在线联系人数文本

    pthread_mutex_lock(&qq_broadcast_mutex);                       //new+ 锁住完整 BEGIN、ITEM、END 和 USER_JOIN 事务，防止消息插入
    pthread_mutex_lock(&qq_clients_mutex);                         //new+ 在同一临界区内完成槽位校验、昵称查重和在线快照复制

    if(slot_index < 0 || slot_index >= QQ_SERVER_MAX_CLIENTS ||
       !qq_clients[slot_index].active || qq_clients[slot_index].fd != fd) { //new+ 确认当前槽位仍属于发起 JOIN 的连接
        result = -1;                                               //new+ 槽位已经失效时拒绝继续同步和登记
    }
    else {
        for(index = 0; index < QQ_SERVER_MAX_CLIENTS; ++index) {    //new+ 检查全部已登记客户端并构造当前在线快照
            if(index == slot_index || !qq_clients[index].active ||
               !qq_clients[index].joined ||
               qq_clients[index].fd == QQ_SERVER_INVALID_SOCKET) { //new+ 跳过自己、空闲槽位和未完成 JOIN 的连接
                continue;                                          //new+ 仅已登记用户可以出现在新客户端联系人列表中
            }
            if(strcmp(qq_clients[index].name, name) == 0) {         //new+ 昵称与现有在线用户完全相同时拒绝重复登录
                result = -2;                                       //new+ -2 专门表示昵称已被占用
                break;                                             //new+ 找到重复昵称后无需继续复制快照
            }
            snprintf(online_names[online_count],                    //new+ 将有效昵称复制到线程局部快照中
                     sizeof(online_names[online_count]), "%s", qq_clients[index].name); //new+ 保证快照字符串不会越界
            ++online_count;                                        //new+ 统计需要发送给新客户端的在线联系人条目
        }
    }
    pthread_mutex_unlock(&qq_clients_mutex);                       //new+ 快照完成后释放客户端锁，避免网络发送阻塞其他状态读取

    if(result == 0) {                                              //new+ 槽位有效且昵称没有重复时开始发送列表事务
        snprintf(count_text, sizeof(count_text), "%d", online_count); //new+ 将不含新客户端自己的联系人数写入列表开始包正文
        if(qq_server_send_packet_unlocked(fd, QQ_MSG_USER_LIST_BEGIN, "SYSTEM", "", count_text) != 0) { //new+ 首先通知客户端清空旧联系人列表
            result = -3;                                           //new+ -3 表示在线列表发送失败，连接不能完成 JOIN
        }
        for(index = 0; result == 0 && index < online_count; ++index) { //new+ 按快照顺序逐个发送已在线用户名
            if(qq_server_send_packet_unlocked(fd, QQ_MSG_USER_LIST_ITEM,
                                              online_names[index], "", "") != 0) { //new+ 昵称放在 sender 字段且接收者保持为空
                result = -3;                                       //new+ 任一条目发送失败都取消本次 JOIN
            }
        }
        if(result == 0 &&
           qq_server_send_packet_unlocked(fd, QQ_MSG_USER_LIST_END, "SYSTEM", "", "") != 0) { //new+ 明确标记完整在线列表同步结束
            result = -3;                                           //new+ 结束包发送失败时客户端列表不完整，拒绝登记上线
        }
    }

    if(result == 0) {                                              //new+ 列表完整到达新客户端后才正式写入共享在线状态
        pthread_mutex_lock(&qq_clients_mutex);                      //new+ 再次锁定数组并确认发送期间槽位仍归当前连接
        if(slot_index < 0 || slot_index >= QQ_SERVER_MAX_CLIENTS ||
           !qq_clients[slot_index].active || qq_clients[slot_index].fd != fd) { //new+ 防止异常状态下把昵称写入错误槽位
            result = -1;                                           //new+ 槽位失效时不发布上线消息
        }
        else {
            snprintf(qq_clients[slot_index].name,
                     sizeof(qq_clients[slot_index].name), "%s", name); //new+ 保存服务器确认过的唯一昵称
            qq_clients[slot_index].joined = 1;                      //new+ 列表同步成功后标记客户端正式在线
        }
        pthread_mutex_unlock(&qq_clients_mutex);                    //new+ 完成共享在线状态登记后释放客户端锁
    }

    if(result == 0) {                                              //new+ 新客户端已正式在线时通知之前的所有用户添加名片
        qq_server_broadcast_with_lock_held(QQ_MSG_USER_JOIN, name, join_text, fd); //new+ sender 直接携带昵称，避免客户端解析提示字符串
    }
    pthread_mutex_unlock(&qq_broadcast_mutex);                     //new+ 完整登录事务结束后允许其他聊天、上线或下线消息发送
    return result;                                                 //new+ 0 成功，-1 槽位失效，-2 重名，-3 列表发送失败
}

/* ===== 从客户端数组中移除并关闭一个连接 ===== */
static void qq_server_remove_client(int slot_index, qq_socket_t fd, //new+ 统一移除连接并按需向其他客户端发布结构化离线通知
                                    int announce_leave, const char * leave_text) //new+ 未完成 JOIN 或服务器停止时可禁止发布离线消息
{
    int removed = 0;                    // 记录当前调用是否确实移除了对应客户端槽位
    int was_joined = 0;                 //new+ 保存被移除连接是否曾完成 JOIN，防止未登录连接产生离线名片事件
    char removed_name[QQ_CHAT_NAME_MAX] = {0}; //new+ 在清空共享槽位前复制需要广播的离线用户昵称

    pthread_mutex_lock(&qq_broadcast_mutex); // 等待正在进行的广播结束，避免关闭一个仍在发送的 fd
    pthread_mutex_lock(&qq_clients_mutex); // 修改客户端槽位前锁定共享数组

    if(slot_index >= 0 && slot_index < QQ_SERVER_MAX_CLIENTS &&
       qq_clients[slot_index].active && qq_clients[slot_index].fd == fd) { // 确认当前槽位仍属于该连接
        was_joined = qq_clients[slot_index].joined; //new+ 在清空状态前记录该连接是否属于在线用户
        snprintf(removed_name, sizeof(removed_name), "%s", qq_clients[slot_index].name); //new+ 保存结构化离线通知需要的昵称
        qq_clients[slot_index].fd = QQ_SERVER_INVALID_SOCKET; // 先从广播列表移除套接字
        qq_clients[slot_index].active = 0; // 将槽位恢复为空闲状态
        qq_clients[slot_index].joined = 0; // 清除 JOIN 状态
        qq_clients[slot_index].name[0] = '\0'; // 清除旧昵称，供下一连接复用
        removed = 1;                    // 标记活动连接计数需要在套接字关闭后递减
    }

    pthread_mutex_unlock(&qq_clients_mutex); // 客户端槽位状态已经更新完毕
    if(removed && was_joined && announce_leave && qq_server_running) { //new+ 正式在线用户退出且服务器仍运行时通知其他客户端
        qq_server_broadcast_with_lock_held(QQ_MSG_USER_LEAVE, removed_name,
                                           leave_text != NULL ? leave_text : "", fd); //new+ sender 精确携带下线昵称，正文保留系统提示
    }
    shutdown(fd, QQ_SERVER_SHUT_RDWR);   // 关闭连接的读写方向并唤醒可能阻塞的网络调用
    qq_server_close_socket(fd);          // 释放当前 TCP 连接描述符或句柄
    pthread_mutex_unlock(&qq_broadcast_mutex); // fd 关闭完成后允许下一次广播继续

    if(removed) {                         // 只有成功移除槽位时才更新活动连接计数
        pthread_mutex_lock(&qq_clients_mutex); // 套接字完全关闭后再更新仍在运行的客户端线程计数
        --qq_active_client_count;         // 当前客户端线程已经完成全部连接资源清理
        if(qq_active_client_count == 0) { // 最后一个客户端完成清理时
            pthread_cond_broadcast(&qq_clients_empty_cond); // 唤醒正在等待安全退出的服务器主线程
        }
        pthread_mutex_unlock(&qq_clients_mutex); // 线程计数更新完成，客户端线程即将返回
    }
}

/* ===== 接收并校验一个客户端协议包 ===== */
static int qq_server_receive_packet(qq_socket_t fd, uint32_t * type,
                                    qq_chat_message_body_t * body)
{
    qq_chat_message_header_t header;    // 保存从网络读取的固定协议头
    int result = qq_server_recv_all(fd, &header, sizeof(header)); // 先读取完整 16 字节包头
    uint32_t magic;                     // 保存转换为本机字节序后的协议魔数
    uint32_t version;                   // 保存转换为本机字节序后的协议版本
    uint32_t body_length;               // 保存转换为本机字节序后的消息体长度

    if(result <= 0) {                   // 0 表示正常断线，-1 表示网络接收错误
        return result;                  // 原样返回，让客户端线程决定下线提示
    }

    magic = ntohl(header.magic);        // 将魔数从网络字节序转换为本机字节序
    version = ntohl(header.version);    // 将协议版本从网络字节序转换为本机字节序
    *type = ntohl(header.type);         // 将消息类型转换后写入输出参数
    body_length = ntohl(header.body_length); // 将消息体长度转换为本机字节序

    if(magic != QQ_CHAT_MAGIC || version != QQ_CHAT_VERSION ||
       body_length != sizeof(*body)) {  // 魔数、版本或固定消息体大小不匹配时拒绝此连接
        return -2;                      // -2 表示收到不兼容或损坏的协议包
    }

    result = qq_server_recv_all(fd, body, sizeof(*body)); // 协议头合法后继续读取完整固定消息体
    if(result <= 0) {                   // 消息体中途断开也视为连接结束或网络错误
        return result;                  // 将具体结果交回客户端线程
    }

    body->sender[sizeof(body->sender) - 1] = '\0'; // 强制昵称以 \0 结束，防止恶意数据越界读取
    body->receiver[sizeof(body->receiver) - 1] = '\0'; //new+ 强制私聊接收者以字符串结束符结尾，避免越界读取
    body->text[sizeof(body->text) - 1] = '\0'; // 强制消息正文以 \0 结束
    return 1;                           // 一个完整且格式正确的数据包已经接收
}

/* ===== 清理昵称中的控制字符 ===== */
static void qq_server_sanitize_name(char * name)
{
    size_t index;                        // 遍历昵称字符使用的索引

    for(index = 0; name[index] != '\0'; ++index) { // 检查昵称内的每一个字符
        unsigned char character = (unsigned char)name[index]; // 避免有符号 char 参与范围判断
        if(character < 32 || character == 127) { // 换行、制表符等控制字符会破坏日志和界面
            name[index] = ' ';           // 将控制字符替换为空格，同时保留昵称其余内容
        }
    }
}

/* ===== 为单个客户端持续处理聊天消息 ===== */
static void * qq_server_client_thread(void * argument)
{
    qq_server_thread_arg_t * thread_argument = argument; // 取出主线程传入的客户端信息
    int slot_index = thread_argument->slot_index; // 保存槽位索引，线程退出时用于精确移除连接
    qq_socket_t fd = thread_argument->fd; // 保存当前客户端连接描述符或句柄
    char address[INET_ADDRSTRLEN];       // 保存客户端 IP，释放参数后仍可用于日志
    uint16_t port = thread_argument->port; // 保存客户端源端口
    char client_name[QQ_CHAT_NAME_MAX] = {0}; // 保存通过 JOIN 验证的昵称
    int joined = 0;                     // 标记当前连接是否已成功加入聊天室
    int leave_announced = 0;            // 防止主动 QUIT 后再次广播异常离线提示
    char leave_text[QQ_CHAT_NAME_MAX + 40] = {0}; //new+ 保存统一移除流程需要广播的正常退出或异常断线提示

    snprintf(address, sizeof(address), "%s", thread_argument->address); // 复制客户端 IPv4 地址
    free(thread_argument);              // 启动参数不再需要，尽早释放堆内存

    for(;;) {                            // 一个线程持续处理一个客户端，直到其退出或断线
        qq_chat_message_body_t body;    // 保存本轮接收的固定消息体
        uint32_t type = 0;              // 保存本轮消息类型
        int receive_result = qq_server_receive_packet(fd, &type, &body); // 阻塞等待一条完整协议消息

        if(receive_result <= 0) {       // 客户端断线、网络错误或协议校验失败时结束循环
            if(receive_result == -2) {  // -2 专门表示协议头不兼容或已经损坏
                qq_server_send_packet(fd, QQ_MSG_ERROR, "SYSTEM", "Invalid chat protocol"); // 尽量向客户端说明错误原因
                qq_server_log("ERROR", "%s:%u sent an invalid protocol packet", address, port); // 记录非法协议来源
            }
            break;                      // 进入线程末尾的统一下线和资源清理流程
        }

        if(type == QQ_MSG_JOIN) {       // JOIN 是客户端连接后必须发送的第一条业务消息
            int register_result;        // 保存昵称登记与查重结果
            char system_text[QQ_CHAT_NAME_MAX + 40]; // 保存带昵称的加入提示

            if(joined) {                // 同一个连接不允许重复发送 JOIN
                qq_server_send_packet(fd, QQ_MSG_ERROR, "SYSTEM", "Client has already joined"); // 回复重复加入错误
                break;                  // 关闭行为异常的连接
            }

            qq_server_sanitize_name(body.sender); // 清除昵称中的换行和其他控制字符
            if(body.sender[0] == '\0') { // 空昵称无法区分客户端，也会影响消息过滤
                qq_server_send_packet(fd, QQ_MSG_ERROR, "SYSTEM", "QQ_NAME cannot be empty"); // 告知客户端必须设置昵称
                break;                  // 拒绝空昵称连接
            }

            snprintf(system_text, sizeof(system_text), "%s joined the chat room", body.sender); //new+ 提前构造上线提示并交给原子登录事务广播
            register_result = qq_server_register_sync_and_announce(slot_index, fd, body.sender, system_text); //new+ 原子同步在线列表、登记昵称并通知其他用户添加名片
            if(register_result == -2) { // 当前已有同名客户端在线
                qq_server_send_packet(fd, QQ_MSG_ERROR, "SYSTEM", "This QQ_NAME is already online"); // 返回明确的重名提示
                qq_server_log("WARN", "Rejected duplicate name '%s' from %s:%u", body.sender, address, port); // 记录拒绝原因
                break;                  // 当前客户端必须换名后重新连接
            }
            if(register_result == -3) {                              //new+ 在线用户列表没有完整发送到新客户端
                qq_server_log("ERROR", "Failed to synchronize online users to %s:%u", address, port); //new+ 记录失败原因便于排查网络中断
                break;                                               //new+ 不登记不完整客户端，进入统一连接清理流程
            }
            if(register_result != 0) {  // 槽位失效属于服务器内部状态异常
                break;                  // 不再继续使用已经无效的连接
            }

            snprintf(client_name, sizeof(client_name), "%s", body.sender); // 在线程内保存经过验证的昵称
            joined = 1;                 // 允许该连接后续发送 DATA 和 QUIT
            qq_server_log("JOIN", "%s (%s:%u) joined the chat room", client_name, address, port); // 记录用户上线
            continue;                   // 等待该客户端的下一条消息
        }

        if(!joined) {                   // 未完成 JOIN 的连接不能直接发送其他消息
            qq_server_send_packet(fd, QQ_MSG_ERROR, "SYSTEM", "JOIN message required first"); // 返回协议使用顺序错误
            break;                      // 关闭未按约定握手的客户端
        }

        if(type == QQ_MSG_DATA) {       // DATA 表示需要转发给其他客户端的普通聊天正文
            if(body.text[0] == '\0') { // 空正文没有显示意义
                continue;               // 忽略空消息但保持当前连接
            }

            qq_server_log("CHAT", "%s: %s", client_name, body.text); // 把发送者和正文写入终端及日志文件
            qq_server_broadcast(QQ_MSG_DATA, client_name, body.text, fd); // 保留服务器登记的昵称并广播给其他客户端
            continue;                   // 当前消息处理完成，继续接收下一条
        }

        if(type == QQ_MSG_PRIVATE) {    //new+ PRIVATE 使用 receiver 字段指定唯一接收者并由服务器认证真实发送者
            int private_result;         //new+ 保存定向转发、业务失败或来源连接失败三种结果
            char target_name[QQ_CHAT_NAME_MAX]; //new+ 独立保存并清理目标昵称，避免直接修改接收缓冲区

            snprintf(target_name, sizeof(target_name), "%s", body.receiver); //new+ 协议版本三统一从 receiver 字段读取私聊目标
            qq_server_sanitize_name(target_name); //new+ 清除目标昵称中的控制字符，避免日志内容被破坏
            if(body.text[0] == '\0') { //new+ 空正文没有显示和转发意义
                continue;               //new+ 忽略空私聊消息但保持当前连接
            }

            private_result = qq_server_route_private(slot_index, fd, target_name, body.text); //new+ 使用连接槽位取得可信昵称并完成目标转发和发送方确认
            if(private_result < 0) {    //new+ 负值表示发送方连接已经无法继续可靠通信
                break;                  //new+ 退出当前客户端线程并交给统一清理流程关闭连接
            }
            if(private_result == 0) {   //new+ 零表示目标收包且发送方已收到服务器确认包
                qq_server_log("PRIVATE", "%s -> %s: %s", client_name, target_name, body.text); //new+ 在服务端日志中记录成功私聊的双方和正文
            }
            else {                      //new+ 一表示目标无效或离线，错误包已经由路由函数回复
                qq_server_log("PRIVATE", "%s -> %s failed", client_name, target_name); //new+ 仅记录业务失败且不重复发送错误包
            }
            continue;                   //new+ 私聊处理完成后继续接收当前客户端的下一条消息
        }

        if(type == QQ_MSG_QUIT) {       // QUIT 表示客户端主动点击返回并正常退出聊天室
            char system_text[QQ_CHAT_NAME_MAX + 40]; // 保存带昵称的离开系统提示

            snprintf(system_text, sizeof(system_text), "%s left the chat room", client_name); // 拼接可直接显示的离开消息
            qq_server_log("QUIT", "%s (%s:%u) left the chat room", client_name, address, port); // 记录正常离线
            snprintf(leave_text, sizeof(leave_text), "%s", system_text); //new+ 保存提示并由统一移除流程在摘除槽位后广播 USER_LEAVE
            leave_announced = 1;        // 标记下线消息已经发出，避免循环结束后重复广播
            break;                      // 结束该客户端线程并关闭连接
        }

        qq_server_send_packet(fd, QQ_MSG_ERROR, "SYSTEM", "Unsupported client message type"); // 客户端不应主动发送 SYSTEM 或 ERROR
        qq_server_log("WARN", "%s sent unsupported message type %u", client_name, type); // 记录不支持的消息类型
        break;                          // 为防止协议状态混乱，发送错误后断开此连接
    }

    if(joined && !leave_announced && qq_server_running) { // 服务器正常运行时，意外断线才需要向其他用户广播
        char system_text[QQ_CHAT_NAME_MAX + 40]; // 保存异常离线提示

        snprintf(system_text, sizeof(system_text), "%s disconnected", client_name); // 拼接包含昵称的断线提示
        qq_server_log("DROP", "%s (%s:%u) disconnected", client_name, address, port); // 记录异常离线
        snprintf(leave_text, sizeof(leave_text), "%s", system_text); //new+ 保存异常断线提示并在槽位移除后统一广播 USER_LEAVE
    }
    else if(joined && !leave_announced) { // 服务器主动关闭连接时只记录停止信息，不再向其他客户端广播
        qq_server_log("CLOSE", "%s (%s:%u) closed because the server is stopping", client_name, address, port); // 记录服务停止导致的下线
    }
    else if(!joined) {                  // 尚未完成 JOIN 就断开的连接只记录地址
        qq_server_log("CLOSE", "%s:%u disconnected before JOIN", address, port); // 记录未完成握手的连接
    }

    qq_server_remove_client(slot_index, fd, joined, leave_text); //new+ 先从在线数组摘除用户，再向其他客户端广播一次结构化下线消息
    return NULL;                         // 结束已经设置为 detached 的客户端线程
}

/* ===== 解析服务器监听端口 ===== */
static int qq_server_parse_port(int argc, char ** argv, uint16_t * port)
{
    const char * port_text = NULL;       // 保存命令行或环境变量提供的端口字符串
    char * end = NULL;                   // 保存 strtol 停止解析的位置
    long value;                          // 保存转换后的端口整数

    if(argc > 2) {                       // 程序最多接受一个可选端口参数
        return -1;                       // 参数过多时让 main 打印使用方法
    }
    if(argc == 2) {                      // 命令行参数具有最高优先级
        port_text = argv[1];             // 使用 ./QQ_Server 后面的端口
    }
    else {
        port_text = getenv("QQ_SERVER_PORT"); // 未传参数时读取与客户端相同的端口环境变量
    }

    if(port_text == NULL || port_text[0] == '\0') { // 没有任何端口配置时
        *port = QQ_CHAT_DEFAULT_PORT;    // 使用 qq_protocol.h 中定义的默认 8888 端口
        return 0;                        // 默认端口有效
    }

    errno = 0;                           // 清除旧错误，便于准确判断 strtol 结果
    value = strtol(port_text, &end, 10); // 按十进制解析端口字符串
    if(errno != 0 || end == port_text || *end != '\0' || value < 1 || value > 65535) { // 检查格式和 TCP 端口范围
        return -1;                       // 无效端口交给 main 输出错误并退出
    }

    *port = (uint16_t)value;             // 将已经验证的端口写入输出参数
    return 0;                            // 端口解析成功
}

/* ===== 初始化服务器日志文件 ===== */
static void qq_server_open_log(void)
{
    const char * log_path = getenv("QQ_SERVER_LOG"); // 允许通过环境变量修改日志文件位置

    if(log_path == NULL || log_path[0] == '\0') { // 没有自定义日志路径时
        log_path = QQ_SERVER_LOG_DEFAULT; // 使用当前目录下的 QQ_Server.log
    }

    qq_log_file = fopen(log_path, "a");  // 以追加模式打开，保留以前的聊天记录
    if(qq_log_file == NULL) {             // 文件无法打开时服务器仍可以继续提供聊天功能
        fprintf(stderr, "Cannot open log file '%s': %s\n", log_path, strerror(errno)); // 仅在终端提示日志错误
    }
}

/* ===== 关闭所有在线客户端 ===== */
static void qq_server_shutdown_clients(void)
{
    int index;                            // 遍历客户端槽位数组使用的索引

    pthread_mutex_lock(&qq_broadcast_mutex); // 等待当前广播结束，避免与 shutdown 同时操作
    pthread_mutex_lock(&qq_clients_mutex); // 固定客户端数组状态
    for(index = 0; index < QQ_SERVER_MAX_CLIENTS; ++index) { // 遍历全部在线连接
        if(qq_clients[index].active && qq_clients[index].fd != QQ_SERVER_INVALID_SOCKET) { // 找到有效客户端
            shutdown(qq_clients[index].fd, QQ_SERVER_SHUT_RDWR); // 唤醒客户端线程的阻塞 recv，使其自行完成清理
        }
    }
    pthread_mutex_unlock(&qq_clients_mutex); // 在线连接均已收到关闭通知
    pthread_mutex_unlock(&qq_broadcast_mutex); // 释放发送锁
}

/* ===== 等待所有客户端线程完成资源清理 ===== */
static void qq_server_wait_for_clients(void)
{
    pthread_mutex_lock(&qq_clients_mutex); // 读取活动连接计数前锁定共享状态
    while(qq_active_client_count > 0) {    // 只要还有客户端线程未调用 remove_client 就继续等待
        pthread_cond_wait(&qq_clients_empty_cond, &qq_clients_mutex); // 休眠并在计数变化时重新检查条件
    }
    pthread_mutex_unlock(&qq_clients_mutex); // 所有客户端均退出后释放共享状态锁
}

/* ===== QQ 聊天室服务器程序入口 ===== */
int main(int argc, char ** argv)
{
    uint16_t server_port;                 // 保存最终使用的监听端口
    struct sockaddr_in server_address;    // 保存服务器 IPv4 监听地址
#ifndef _WIN32                           // Linux 使用 sigaction 注册可靠的退出信号处理
    struct sigaction signal_action;       // 配置 SIGINT 和 SIGTERM 的处理方式
#else                                   // Windows 必须先初始化 WinSock 库
    WSADATA winsock_data;                // 保存 WSAStartup 返回的 WinSock 实现信息
#endif                                  // 结束操作系统初始化变量选择
    int reuse_address = 1;                // SO_REUSEADDR 选项的开启值
    int index;                            // 初始化客户端槽位使用的索引

    if(qq_server_parse_port(argc, argv, &server_port) != 0) { // 读取并验证可选端口
        fprintf(stderr, "Usage: %s [1-65535]\n", argv[0]); // 显示正确启动格式
        return EXIT_FAILURE;              // 参数错误时不创建任何网络资源
    }

    for(index = 0; index < QQ_SERVER_MAX_CLIENTS; ++index) { // 初始化全部客户端槽位
        qq_clients[index].fd = QQ_SERVER_INVALID_SOCKET; // 使用平台对应常量表示没有有效套接字
        qq_clients[index].active = 0;      // 所有槽位初始均可使用
        qq_clients[index].joined = 0;      // 初始没有客户端完成 JOIN
        qq_clients[index].name[0] = '\0'; // 初始昵称为空
    }

    qq_server_open_log();                  // 打开聊天记录文件，失败也不会中止网络服务
#ifdef _WIN32                           // Windows 网络函数必须先初始化 WinSock 2.2
    if(WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) { // 请求加载 WinSock 2.2 运行库
        qq_server_log("FATAL", "WSAStartup failed"); // WinSock 初始化失败时无法提供 TCP 服务
        if(qq_log_file != NULL) fclose(qq_log_file); // 关闭已经打开的日志文件
        return EXIT_FAILURE;              // 返回失败状态
    }
    signal(SIGINT, qq_server_signal_handler); // Windows 本机测试时注册 Ctrl+C 处理
    signal(SIGTERM, qq_server_signal_handler); // 注册终止信号处理
#else                                   // Linux 开发板使用 POSIX 信号配置
    signal(SIGPIPE, SIG_IGN);              // 忽略坏连接写入产生的 SIGPIPE，改由 send 返回错误

    memset(&signal_action, 0, sizeof(signal_action)); // 清空 sigaction 结构体
    signal_action.sa_handler = qq_server_signal_handler; // Ctrl+C 或终止信号只修改运行标志
    sigemptyset(&signal_action.sa_mask);   // 信号处理期间不额外屏蔽其他信号
    signal_action.sa_flags = 0;            // 不使用 SA_RESTART，让信号可以中断阻塞的 accept
    sigaction(SIGINT, &signal_action, NULL); // 注册 Ctrl+C 退出处理
    sigaction(SIGTERM, &signal_action, NULL); // 注册系统终止信号处理
#endif                                  // 结束平台信号与网络初始化

    qq_listen_fd = socket(AF_INET, SOCK_STREAM, 0); // 创建 IPv4 TCP 监听套接字
    if(qq_listen_fd == QQ_SERVER_INVALID_SOCKET) { // socket 创建失败时无法启动服务
        qq_server_log("FATAL", "socket failed, error: %d", QQ_SERVER_SOCKET_ERROR()); // 输出具体系统错误码
        if(qq_log_file != NULL) fclose(qq_log_file); // 关闭已经打开的日志文件
#ifdef _WIN32
        WSACleanup();                       // Windows 失败路径释放 WinSock 运行库
#endif
        return EXIT_FAILURE;               // 返回失败状态
    }

    if(setsockopt(qq_listen_fd, SOL_SOCKET, SO_REUSEADDR,
                  (const char *)&reuse_address, sizeof(reuse_address)) != 0) { // 允许服务器重启后快速重新绑定相同端口
        qq_server_log("WARN", "SO_REUSEADDR failed, error: %d", QQ_SERVER_SOCKET_ERROR()); // 选项失败不影响继续尝试 bind
    }

    memset(&server_address, 0, sizeof(server_address)); // 清空服务器地址结构
    server_address.sin_family = AF_INET;   // 使用 IPv4 地址族
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // 监听开发板所有网络接口
    server_address.sin_port = htons(server_port); // 将监听端口转换为网络字节序

    if(bind(qq_listen_fd, (struct sockaddr *)&server_address,
            sizeof(server_address)) != 0) { // 把监听套接字绑定到 0.0.0.0 和指定端口
        qq_server_log("FATAL", "bind 0.0.0.0:%u failed, error: %d", server_port, QQ_SERVER_SOCKET_ERROR()); // 提示端口占用等错误
        qq_server_close_socket(qq_listen_fd); // 释放监听套接字
        if(qq_log_file != NULL) fclose(qq_log_file); // 关闭日志文件
#ifdef _WIN32
        WSACleanup();                       // Windows 失败路径释放 WinSock 运行库
#endif
        return EXIT_FAILURE;               // 绑定失败时结束程序
    }

    if(listen(qq_listen_fd, QQ_SERVER_LISTEN_BACKLOG) != 0) { // 将套接字切换为监听状态
        qq_server_log("FATAL", "listen failed, error: %d", QQ_SERVER_SOCKET_ERROR()); // 输出监听失败原因
        qq_server_close_socket(qq_listen_fd); // 释放监听套接字
        if(qq_log_file != NULL) fclose(qq_log_file); // 关闭日志文件
#ifdef _WIN32
        WSACleanup();                       // Windows 失败路径释放 WinSock 运行库
#endif
        return EXIT_FAILURE;               // 监听失败时结束程序
    }

    qq_server_log("START", "QQ_Server listening on 0.0.0.0:%u, max clients: %d",
                  server_port, QQ_SERVER_MAX_CLIENTS); // 显示服务器已启动及关键配置

    while(qq_server_running) {             // 主线程持续 accept 新客户端，具体通信交给独立线程
        struct sockaddr_in client_address; // 保存本次连接的客户端地址
        qq_socklen_t client_address_length = (qq_socklen_t)sizeof(client_address); // accept 需要的地址结构长度
        qq_socket_t client_fd = accept(qq_listen_fd, (struct sockaddr *)&client_address,
                               &client_address_length); // 阻塞等待一个新的 TCP 客户端连接
        qq_server_thread_arg_t * thread_argument; // 保存准备交给客户端线程的参数
        pthread_t thread;                   // 保存新客户端线程标识符
#ifdef _WIN32
        DWORD send_timeout = QQ_SERVER_SEND_TIMEOUT_SEC * 1000U; // Windows SO_SNDTIMEO 使用毫秒整数
#else
        struct timeval send_timeout;        // Linux SO_SNDTIMEO 使用 timeval 结构
#endif
        int keep_alive = 1;                 // SO_KEEPALIVE 选项的开启值
        int slot_index;                     // 保存新连接分配到的客户端槽位

        if(client_fd == QQ_SERVER_INVALID_SOCKET) { // accept 失败时判断是退出信号还是临时错误
            if(QQ_SERVER_SOCKET_ERROR() == QQ_SERVER_INTERRUPTED) { // 信号打断 accept 时回到循环顶部检查运行标志
                continue;
            }
            if(qq_server_running) {         // 正常运行期间的 accept 错误需要记录
                qq_server_log("ERROR", "accept failed, error: %d", QQ_SERVER_SOCKET_ERROR()); // 输出错误原因
            }
            continue;                       // 临时 accept 错误不终止整个服务器
        }

#ifndef _WIN32
        send_timeout.tv_sec = QQ_SERVER_SEND_TIMEOUT_SEC; // 设置最大阻塞发送秒数
        send_timeout.tv_usec = 0;            // 不需要额外微秒
#endif
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO,
                   (const char *)&send_timeout, sizeof(send_timeout)); // 防止一个不读数据的客户端永久卡住群聊广播
        setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE,
                   (const char *)&keep_alive, sizeof(keep_alive)); // 允许内核检测长期失效的 TCP 连接

        slot_index = qq_server_add_client(client_fd); // 尝试为新连接分配在线槽位
        if(slot_index < 0) {                 // 没有空闲槽位表示聊天室人数已满
            qq_server_send_packet(client_fd, QQ_MSG_ERROR, "SYSTEM", "Chat room is full"); // 按客户端协议发送满员提示
            qq_server_log("FULL", "Rejected a connection because the room is full"); // 记录拒绝原因
            shutdown(client_fd, QQ_SERVER_SHUT_RDWR); // 关闭新连接读写方向
            qq_server_close_socket(client_fd); // 释放未加入数组的连接
            continue;                        // 等待下一次新连接
        }

        thread_argument = malloc(sizeof(*thread_argument)); // 为线程参数分配独立内存，不能传 accept 局部变量地址
        if(thread_argument == NULL) {         // 内存不足时无法安全启动客户端线程
            qq_server_send_packet(client_fd, QQ_MSG_ERROR, "SYSTEM", "Server has insufficient memory"); // 尽量通知客户端
            qq_server_remove_client(slot_index, client_fd, 0, NULL); //new+ 未完成 JOIN 的连接只回收槽位，不发布用户离线通知
            continue;                        // 服务器仍继续接受其他连接
        }

        thread_argument->slot_index = slot_index; // 保存客户端数组索引
        thread_argument->fd = client_fd;      // 保存连接描述符
        thread_argument->port = ntohs(client_address.sin_port); // 保存转换为本机字节序的客户端端口
        if(inet_ntop(AF_INET, &client_address.sin_addr, thread_argument->address,
                     sizeof(thread_argument->address)) == NULL) { // 将客户端 IPv4 地址转换为文本
            snprintf(thread_argument->address, sizeof(thread_argument->address), "unknown"); // 转换失败时使用占位文字
        }

        if(pthread_create(&thread, NULL, qq_server_client_thread, thread_argument) != 0) { // 为新连接创建独立接收线程
            qq_server_log("ERROR", "pthread_create failed for %s:%u",
                          thread_argument->address, thread_argument->port); // 记录线程创建失败的连接
            free(thread_argument);          // 线程未启动，主线程负责释放参数内存
            qq_server_remove_client(slot_index, client_fd, 0, NULL); //new+ 线程创建失败的连接尚未上线，只回收资源
            continue;                       // 不影响服务器继续接收新连接
        }

        pthread_detach(thread);              // 线程退出后由系统自动回收，不要求主线程 pthread_join
    }

    qq_server_log("STOP", "QQ_Server is shutting down"); // 记录服务器开始停止
    if(qq_listen_fd != QQ_SERVER_INVALID_SOCKET) { // 如果监听套接字尚未由信号处理函数关闭
        qq_server_close_socket((qq_socket_t)qq_listen_fd); // 关闭监听套接字，不再接受新连接
        qq_listen_fd = QQ_SERVER_INVALID_SOCKET; // 清除监听描述符状态
    }
    qq_server_shutdown_clients();            // 唤醒并结束所有仍在线的客户端线程
    qq_server_wait_for_clients();             // 等待后台线程完成日志、槽位和套接字清理

    pthread_mutex_lock(&qq_log_mutex);       // 关闭日志前阻止其他线程同时写入
    if(qq_log_file != NULL) {                // 日志文件曾成功打开时才关闭
        fclose(qq_log_file);                 // 刷新并释放日志文件资源
        qq_log_file = NULL;                  // 清空日志指针
    }
    pthread_mutex_unlock(&qq_log_mutex);     // 日志资源清理完成

#ifdef _WIN32
    WSACleanup();                            // Windows 本机测试结束后释放 WinSock 运行库
#endif

    return EXIT_SUCCESS;                     // 服务器正常结束
}
