#ifndef QQ_PROTOCOL_H                    // 防止 QQ 聊天协议头文件被重复包含
#define QQ_PROTOCOL_H                    // 定义头文件保护宏

#include <stdint.h>                      // 提供长度固定的 uint32_t 无符号整数类型

#define QQ_CHAT_MAGIC 0x51514348U        // 协议魔数，ASCII 含义接近 QQCH，用于识别合法聊天数据包
#define QQ_CHAT_VERSION 3U               //new+ 私聊新增接收者字段，客户端与服务端同步升级为协议版本三
#define QQ_CHAT_NAME_MAX 32              // 发送者昵称缓冲区总长度，包含字符串结尾的 \0
#define QQ_CHAT_TEXT_MAX 512             // 单条消息文本缓冲区总长度，包含字符串结尾的 \0
#define QQ_CHAT_DEFAULT_PORT 8888        // 未设置 QQ_SERVER_PORT 时使用的默认 TCP 端口

typedef enum {                            // 定义聊天数据包可使用的消息类型
    QQ_MSG_JOIN = 1,                     // 客户端连接成功后发送的加入聊天室通知
    QQ_MSG_DATA = 2,                     // 客户端之间传递的普通文字聊天消息
    QQ_MSG_QUIT = 3,                     // 客户端主动退出聊天室时发送的离开通知
    QQ_MSG_SYSTEM = 4,                   // 由服务端广播的普通系统提示消息
    QQ_MSG_ERROR = 5,                    // 由服务端返回的错误提示消息
    QQ_MSG_USER_LIST_BEGIN = 6,          //new+ 服务端开始向刚登录的客户端同步完整在线用户列表
    QQ_MSG_USER_LIST_ITEM = 7,           //new+ 单个在线用户条目，用户昵称保存在消息体 sender 字段
    QQ_MSG_USER_LIST_END = 8,            //new+ 服务端完成本轮完整在线用户列表同步
    QQ_MSG_USER_JOIN = 9,                //new+ 已登录客户端收到其他用户上线的增量通知
    QQ_MSG_USER_LEAVE = 10,              //new+ 已登录客户端收到其他用户下线的增量通知
    QQ_MSG_PRIVATE = 11                  //new+ 客户端和服务端使用接收者昵称定向传递真正的私聊消息
} qq_chat_message_type_t;                // 消息类型枚举的类型名称

typedef struct {                          // 每个网络数据包开头固定存在的协议头
    uint32_t magic;                       // 网络字节序的协议魔数，用于过滤无效数据
    uint32_t version;                     // 网络字节序的协议版本号
    uint32_t type;                        // 网络字节序的消息类型，对应 qq_chat_message_type_t
    uint32_t body_length;                 // 网络字节序的消息体长度，便于校验包结构
} qq_chat_message_header_t;               // QQ 聊天消息头结构体类型

typedef struct {                          // 紧跟协议头发送的固定长度消息体
    char sender[QQ_CHAT_NAME_MAX];        // 保存发送者昵称的 C 字符串缓冲区
    char receiver[QQ_CHAT_NAME_MAX];      //new+ 保存私聊接收者昵称，群聊和系统消息保持为空字符串
    char text[QQ_CHAT_TEXT_MAX];          // 保存聊天正文或系统提示的 C 字符串缓冲区
} qq_chat_message_body_t;                 // QQ 聊天消息体结构体类型

typedef char qq_protocol_header_size_must_be_16[(sizeof(qq_chat_message_header_t) == 16) ? 1 : -1]; //new+ 编译期确认固定协议头仍为十六字节
typedef char qq_protocol_body_size_must_be_576[(sizeof(qq_chat_message_body_t) == 576) ? 1 : -1]; //new+ 编译期确认新增接收者后的消息体为五百七十六字节

#endif                                    // 结束 QQ_PROTOCOL_H 头文件保护
