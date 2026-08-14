#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "qq_protocol.h"                 // 引入 QQ 聊天室客户端与服务端共同使用的消息协议定义
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "lv_myfont_30.h"
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include "game_2048.h"
#include <signal.h>
#ifdef _WIN32                           // Windows 环境下使用 WinSock 提供 QQ 聊天室网络接口
#include <winsock2.h>                   // 提供 Windows 的 socket、send、recv 和 closesocket 等函数
#include <ws2tcpip.h>                   // 提供 Windows 的 inet_pton 及现代 TCP/IP 地址接口
#else                                   // Linux 开发板使用 POSIX 网络接口
#include <arpa/inet.h>                  // 提供 htonl、ntohl、htons 和 inet_pton 等字节序与地址转换函数
#include <netinet/in.h>                 // 提供 sockaddr_in、AF_INET 等 IPv4 数据结构与常量
#include <sys/select.h>                 // 提供网络 I/O 相关的系统类型，便于后续扩展复用
#include <sys/socket.h>                 // 提供 socket、connect、send、recv、shutdown 等 TCP 接口
#endif                                  // 结束 Windows 与 Linux 网络头文件选择

#ifndef MSG_NOSIGNAL                    // 某些平台没有“发送失败时不产生信号”的标志
#define MSG_NOSIGNAL 0                  // 缺少该标志时定义为 0，保持 send 调用可以跨平台编译
#endif                                  // 结束 MSG_NOSIGNAL 兼容定义
#ifndef MSG_DONTWAIT                    // 某些平台没有单次非阻塞接收标志
#define MSG_DONTWAIT 0                  // 缺少该标志时退化为普通 recv 参数，保证源码能够编译
#endif                                  // 结束 MSG_DONTWAIT 兼容定义

#ifdef _WIN32                                                                  //new+ 为 Windows 虚拟屏选择 WinSock 套接字类型和常量
typedef SOCKET qq_chat_socket_t;                                               //new+ Win64 的 SOCKET 可能宽于 int，必须保留完整句柄
typedef int qq_chat_io_result_t;                                               //new+ WinSock 的 send 和 recv 返回 int
#define QQ_CHAT_INVALID_SOCKET INVALID_SOCKET                                  //new+ 使用 WinSock 规定的无效套接字值
#define QQ_CHAT_SOCKET_ERROR SOCKET_ERROR                                      //new+ 使用 WinSock 规定的网络调用失败返回值
#define QQ_CHAT_SHUT_RDWR SD_BOTH                                              //new+ Windows 使用 SD_BOTH 关闭套接字双向通信
#define qq_chat_close_socket closesocket                                       //new+ Windows 必须使用 closesocket 释放套接字
#define QQ_CHAT_LAST_SOCKET_ERROR() WSAGetLastError()                           //new+ Windows 从 WinSock 获取最近一次网络错误码
#define QQ_CHAT_INTERRUPTED WSAEINTR                                            //new+ Windows 使用 WSAEINTR 表示网络调用被中断
#define QQ_CHAT_WOULD_BLOCK(error_code) ((error_code) == WSAEWOULDBLOCK)         //new+ Windows 非阻塞套接字暂无数据时返回 WSAEWOULDBLOCK
#else                                                                           //new+ 开发板继续使用原来的 POSIX 套接字接口
typedef int qq_chat_socket_t;                                                  //new+ Linux 套接字由普通文件描述符保存
typedef ssize_t qq_chat_io_result_t;                                           //new+ Linux 的 send 和 recv 返回 ssize_t
#define QQ_CHAT_INVALID_SOCKET (-1)                                            //new+ Linux 使用负一表示无效套接字
#define QQ_CHAT_SOCKET_ERROR (-1)                                              //new+ Linux 网络调用失败时返回负一
#define QQ_CHAT_SHUT_RDWR SHUT_RDWR                                            //new+ Linux 使用 SHUT_RDWR 关闭套接字双向通信
#define qq_chat_close_socket close                                             //new+ Linux 继续使用 close 释放套接字描述符
#define QQ_CHAT_LAST_SOCKET_ERROR() errno                                      //new+ Linux 从 errno 获取最近一次网络错误码
#define QQ_CHAT_INTERRUPTED EINTR                                              //new+ Linux 使用 EINTR 表示系统调用被信号中断
#define QQ_CHAT_WOULD_BLOCK(error_code) ((error_code) == EAGAIN || (error_code) == EWOULDBLOCK) //new+ Linux 非阻塞接收暂无数据时识别两种等价错误码
#endif                                                                          //new+ 结束 QQ 客户端跨平台套接字定义






typedef  char *  ElemType;			//数据元素中的 数据的类型
								//以便于程序之后存放不同类型的数据，方便移植

typedef  struct  dnode		//数据元素的类型
{
    ElemType  data;			//数据域	---> 保存数据
    struct dnode *  next;	//指针域   ---> 保存关系（保存逻辑关系上的下一个）
    struct dnode *  prev;//指针域   ---> 保存关系（保存逻辑关系上的上一个）
} DNode;

typedef  struct  head		//数据元素的类型
{
    struct dnode *  first;	//指针域   ---> 保存关系（保存逻辑关系上的下一个）
    struct dnode *  last;//指针域   ---> 保存关系（保存逻辑关系上的上一个）
    int num;
} Head;




void creat_link(Head *h,char*file_name);
void delete_link(Head *h, DNode *pn);
void find_pic(char*path,Head *h);
void find_music(char *path, Head *h);
void find_video(char *path, Head *h);
static void pic_next(lv_event_t * e);
static void pic_last(lv_event_t * e);
static void album_update_image(void);
static void album_back_cb(lv_event_t * e);
void Calendar_Interface(void);
void Album_Interface(void);
static void Album_cb(lv_event_t * e);
void show_bmp(void);
static void pic_next(lv_event_t * e);
static void pic_last(lv_event_t * e);
static void Video_cb(lv_event_t * e);
void Video_Interface(void);
static void video_prev_cb(lv_event_t * e);
static void video_play_pause_cb(lv_event_t * e);
static void video_next_cb(lv_event_t * e);
static void video_vol_cb(lv_event_t * e);
static void video_seek_cb(lv_event_t * e);
static void video_back_to_main_cb(lv_event_t * e);

/* 音乐播放器 */
static void Music_cb(lv_event_t * e);
void Music_Interface(void);
static void music_play_pause_cb(lv_event_t * e);
static void music_next_cb(lv_event_t * e);
static void music_prev_cb(lv_event_t * e);
static void music_back_to_main_cb(lv_event_t * e);
static void music_kill(void);
static void music_start_play(void);
static void music_update_track_info(void);
static void music_style_button(lv_obj_t * button);


/* 视频播放器 */
static void Video_cb(lv_event_t * e);
void Video_Interface(void);
static void video_prev_cb(lv_event_t * e);
static void video_play_pause_cb(lv_event_t * e);
static void video_next_cb(lv_event_t * e);
static void video_vol_cb(lv_event_t * e);



/* 记事本 */
static void Note_cb(lv_event_t * e);
void Note_Interface(void);
static void note_new_cb(lv_event_t * e);
static void note_edit_cb(lv_event_t * e);
static void note_save_cb(lv_event_t * e);
static void note_back_list_cb(lv_event_t * e);
static void msgbox_close_cb(lv_timer_t * timer);
static void note_delete_cb(lv_event_t * e);
static void note_delete_confirm_cb(lv_event_t * ev);
static void note_delete_cancel_cb(lv_event_t * ev);
static void note_keyboard_cb(lv_event_t * e);
static void note_refresh_list(void);
static void note_close_cb(lv_event_t * e);
static void note_style_button(lv_obj_t * button);
static void note_style_textarea(lv_obj_t * textarea);
void find_notes(char *path, Head *h);
static void Games_cb(lv_event_t * e);
static void show_main_win(void);

/* QQ 聊天室 */
static void QQ_cb(lv_event_t * e);                                                     // 主界面 QQ Chat 按钮的点击回调
void QQ_Chat_Interface(void);                                                         // 创建并显示完整的 QQ 聊天室界面
static void qq_chat_back_cb(lv_event_t * e);                                          // 退出聊天室并返回主界面的回调
static void qq_chat_input_cb(lv_event_t * e);                                         // 处理消息输入框与软键盘事件
static void qq_chat_send_cb(lv_event_t * e);                                          // 读取输入框内容并发送聊天消息
static int qq_chat_client_connect(void);                                               // 建立客户端到 TCP 聊天服务器的连接
static void qq_chat_client_disconnect(int send_quit);                                 // 关闭 TCP 连接，可选择先发送离开通知
static int qq_chat_send_packet(uint32_t type, const char * text);                     // 按 QQ 协议封装并发送一条完整数据包
static void qq_chat_receive_timer_cb(lv_timer_t * timer);                             // LVGL 定时器回调，非阻塞接收服务器消息
static void qq_chat_add_message(const char * sender, const char * text, int is_self); // 在聊天记录区域添加消息气泡或系统提示
static void qq_chat_set_keyboard_visible(int visible);                                // 显示或隐藏聊天室软键盘并调整布局
static void qq_contact_card_cb(lv_event_t * e);                                       //new+ 处理在线联系人名片点击并切换到对应私聊会话
static void qq_group_card_cb(lv_event_t * e);                                         //new+ 处理群聊名片点击并切回公共群聊会话
static void qq_contact_add(const char * name);                                        //new+ 为新上线用户创建联系人链表节点和左侧名片
static void qq_contact_remove(const char * name);                                     //new+ 删除已下线用户对应的联系人节点和左侧名片
static void qq_contact_clear(void);                                                    //new+ 清空本轮连接保存的全部在线联系人
static int qq_chat_send_private_packet(const char * target, const char * text);        //new+ 将目标昵称和正文封装为私聊协议包
static void qq_chat_switch_session(const char * peer);                                 //new+ 切换群聊或指定联系人的私聊会话并重绘记录
static void qq_chat_update_contact_status(const char * peer);                          //new+ 刷新联系人在线状态和对应私聊未读数量
static void qq_chat_clear_sessions(void);                                              //new+ 退出 QQ 时释放全部独立会话消息缓存
static void QQ_login(void);                                                          //new+ 声明 QQ 登录模态沙盒创建函数
static void qq_login_textarea_cb(lv_event_t * e);                                     //new+ 处理 QQ 登录输入框聚焦并绑定软键盘
static void qq_login_keyboard_cb(lv_event_t * e);                                     //new+ 处理 QQ 登录软键盘的确认和收起事件
static void qq_login_submit_cb(lv_event_t * e);                                       //new+ 校验并保存 QQ 登录信息后进入聊天室
static void qq_login_cancel_cb(lv_event_t * e);                                       //new+ 取消 QQ 登录并关闭模态沙盒
static void qq_login_close(void);                                                     //new+ 统一释放 QQ 登录沙盒及其子控件引用
static void qq_login_style_textarea(lv_obj_t * textarea);                             //new+ 统一设置 QQ 登录输入框的主题样式


static void File_cb(lv_event_t * e);                                      //主界面 File System 按钮的点击回调
void File_System_Interface(void);
static void File_login(void); 





/* 全局变量的实现 */
static int login_ok = 0;
static lv_obj_t * Main_win = NULL;
static   lv_obj_t * label = NULL;
static   lv_obj_t * obj = NULL;
static lv_obj_t * album_content = NULL;
Head *h = NULL;
static DNode *p = NULL;
static lv_obj_t * g_bmp = NULL;
static lv_obj_t * album_filename_label = NULL;
static lv_obj_t * album_counter_label = NULL;
static int album_current_index = 0;
static lv_obj_t * g_ta_user = NULL;
static lv_obj_t * g_ta_pass = NULL;

/* File System 全局变量 */
static lv_obj_t * file_login_win = NULL;                            // File System 登录界面
static lv_obj_t * file_login_panel = NULL;                                         //new+ 保存 QQ 登录沙盒面板以便键盘弹出时调整位置
static lv_obj_t * file_login_ip_ta = NULL;                                         //new+ 保存服务器 IP 输入框对象
static lv_obj_t * file_login_port_ta = NULL;                                       //new+ 保存服务器端口输入框对象
static lv_obj_t * file_login_name_ta = NULL;                                       //new+ 保存聊天昵称输入框对象
static lv_obj_t * file_login_keyboard = NULL;                                      //new+ 保存 QQ 登录专用软键盘对象
static lv_obj_t * file_login_error_label = NULL;                                   //new+ 保存 QQ 登录校验错误提示标签
static char file_IP_storage[16] = {0};                                             //new+ 为最长 15 字符的 IPv4 地址提供稳定存储空间
static char * file_IP = file_IP_storage;                                             //new+ 让用户定义的 IP 指针始终指向有效可写缓冲区
static int file_Port = 0;                                  // QQ 连接的服务器的端口
static char file_name[64];                                 // QQ 登录时的名字
static lv_obj_t * file_win = NULL;                 // QQ 聊天室最外层窗口对象，退出时统一异步删除
static lv_obj_t * file_message_list = NULL;        // 存放系统提示和双方消息气泡的可滚动列表
static lv_obj_t * file_input = NULL;               // 用户输入待发送文字的多行文本框
static lv_obj_t * file_title_label = NULL;         // 右侧标题栏中的当前聊天室名称标签
static lv_obj_t * file_status_label = NULL;        // 左上角显示 OFFLINE、CONNECTING 或 ONLINE 的状态标签
static lv_obj_t * file_input_area = NULL;          // 底部输入框和发送按钮所在的容器
static lv_obj_t * file_keyboard = NULL;            // 点击消息输入框后显示的 LVGL 软键盘


/* QQ 聊天室全局变量 */
static lv_obj_t * qq_login_win = NULL;                // QQ 登录界面
static lv_obj_t * qq_login_panel = NULL;                                         //new+ 保存 QQ 登录沙盒面板以便键盘弹出时调整位置
static lv_obj_t * qq_login_ip_ta = NULL;                                         //new+ 保存服务器 IP 输入框对象
static lv_obj_t * qq_login_port_ta = NULL;                                       //new+ 保存服务器端口输入框对象
static lv_obj_t * qq_login_name_ta = NULL;                                       //new+ 保存聊天昵称输入框对象
static lv_obj_t * qq_login_keyboard = NULL;                                      //new+ 保存 QQ 登录专用软键盘对象
static lv_obj_t * qq_login_error_label = NULL;                                   //new+ 保存 QQ 登录校验错误提示标签
static char qq_IP_storage[16] = {0};                                             //new+ 为最长 15 字符的 IPv4 地址提供稳定存储空间
static char * qq_IP = qq_IP_storage;                                             //new+ 让用户定义的 IP 指针始终指向有效可写缓冲区
static int qq_Port = 0;                                  // QQ 连接的服务器的端口
static char qq_name[64];                                 // QQ 登录时的名字
static lv_obj_t * qq_chat_win = NULL;                 // QQ 聊天室最外层窗口对象，退出时统一异步删除
static lv_obj_t * qq_chat_message_list = NULL;        // 存放系统提示和双方消息气泡的可滚动列表
static lv_obj_t * qq_chat_input = NULL;               // 用户输入待发送文字的多行文本框
static lv_obj_t * qq_chat_title_label = NULL;         // 右侧标题栏中的当前聊天室名称标签
static lv_obj_t * qq_chat_status_label = NULL;        // 左上角显示 OFFLINE、CONNECTING 或 ONLINE 的状态标签
static lv_obj_t * qq_chat_input_area = NULL;          // 底部输入框和发送按钮所在的容器
static lv_obj_t * qq_chat_keyboard = NULL;            // 点击消息输入框后显示的 LVGL 软键盘
static lv_obj_t * qq_group_card = NULL;                                         //new+ 保存左侧群聊名片以便点击切换和更新选中样式
static lv_obj_t * qq_chat_subtitle_label = NULL;                                //new+ 保存右侧标题栏的群聊或私聊类型标签
static lv_obj_t * qq_chat_header_icon_label = NULL;                             //new+ 保存右侧标题栏头像图标以便随会话切换
static lv_timer_t * qq_chat_receive_timer = NULL;     // 每 100 毫秒检查一次服务器数据的 LVGL 定时器
static qq_chat_socket_t qq_chat_socket_fd = QQ_CHAT_INVALID_SOCKET; //new+ 使用平台正确的类型保存 TCP 套接字，避免 Win64 句柄被截断
static int qq_chat_connected = 0;                     // TCP 连接状态标志，1 表示已经连接到服务器
static unsigned char qq_chat_receive_buffer[sizeof(qq_chat_message_header_t) + sizeof(qq_chat_message_body_t)]; // 缓存一个完整协议包的接收缓冲区
static size_t qq_chat_receive_length = 0;             // 当前接收缓冲区内已经累计的有效字节数
static char qq_chat_user_name[QQ_CHAT_NAME_MAX] = "LVGL User"; // 本机聊天昵称，可由环境变量 QQ_NAME 覆盖
typedef struct qq_contact_node {                                      //new+ 定义独立于图片、音乐和视频链表的 QQ 在线联系人节点
    char name[QQ_CHAT_NAME_MAX];                                      //new+ 保存服务器确认过的在线用户昵称
    lv_obj_t * card;                                                  //new+ 保存该联系人在左侧列表中的可点击名片对象
    lv_obj_t * name_label;                                            //new+ 保存名片中的昵称标签，便于后续扩展状态显示
    lv_obj_t * status_label;                                          //new+ 保存在线状态标签以便同时显示该私聊的未读数量
    struct qq_contact_node * next;                                    //new+ 指向下一个在线联系人节点，组成单向链表
} qq_contact_node_t;                                                  //new+ 声明 QQ 联系人节点类型，不修改项目原有媒体链表
static qq_contact_node_t * qq_contact_head = NULL;                    //new+ 指向当前在线联系人链表首节点
static lv_obj_t * qq_private_contact_list = NULL;                     //new+ 保存群聊名片下方用于容纳动态私聊名片的滚动列表
typedef struct qq_message_node {                                      //new+ 定义只属于 QQ 会话缓存的单条文字消息节点
    char sender[QQ_CHAT_NAME_MAX];                                    //new+ 保存消息气泡上方显示的发送者昵称
    char text[QQ_CHAT_TEXT_MAX];                                      //new+ 保存消息正文且沿用现有协议长度上限
    int is_self;                                                      //new+ 标记该消息是否由当前客户端发送以决定气泡方向
    int is_system;                                                    //new+ 标记该消息是否使用居中的系统提示样式
    struct qq_message_node * next;                                    //new+ 指向同一会话中的下一条消息记录
} qq_message_node_t;                                                  //new+ 声明 QQ 消息缓存节点类型且不影响媒体链表
typedef struct qq_session_node {                                      //new+ 定义群聊或单个联系人私聊的独立会话节点
    char peer[QQ_CHAT_NAME_MAX];                                      //new+ 空昵称代表群聊，非空昵称代表对应联系人私聊
    qq_message_node_t * head;                                         //new+ 指向本会话最早保留的消息记录
    qq_message_node_t * tail;                                         //new+ 指向本会话最后一条消息以便快速追加
    unsigned int message_count;                                       //new+ 记录当前缓存数量并限制嵌入式设备内存占用
    unsigned int unread_count;                                        //new+ 记录会话未处于当前页面时收到的新消息数量
    struct qq_session_node * next;                                    //new+ 指向下一个 QQ 会话节点
} qq_session_node_t;                                                  //new+ 声明独立 QQ 会话链表类型
static qq_session_node_t * qq_session_head = NULL;                    //new+ 指向 QQ 会话缓存链表首节点
static qq_session_node_t * qq_active_session = NULL;                  //new+ 指向当前显示会话且不依赖联系人节点生命周期
#define QQ_CHAT_SESSION_MESSAGE_LIMIT 100U                            //new+ 每个会话最多缓存一百条消息以避免内存无限增长


/* 音乐播放器全局变量 */
static lv_obj_t * music_win = NULL;
static lv_obj_t * music_label = NULL;
static lv_obj_t * music_slider = NULL;
static lv_obj_t * play_btn = NULL;
static lv_obj_t * prev_btn = NULL;
static lv_obj_t * next_btn = NULL;
static lv_obj_t * music_status_label = NULL;
static lv_obj_t * music_counter_label = NULL;
static DNode * music_current = NULL;
static int music_is_playing = 0;
static pid_t music_pid = 0;
static lv_timer_t * login_timer = NULL;
static Head *h_music = NULL;


/* 记事本全局变量 */
static lv_obj_t * Note_win = NULL;
static lv_obj_t * notes_list = NULL;
static lv_obj_t * note_editor_win = NULL;
static lv_obj_t * note_title_ta = NULL;
static lv_obj_t * note_content_ta = NULL;
static lv_obj_t * note_kb = NULL;
static lv_obj_t * note_filename_ta = NULL;
static lv_obj_t * note_count_label = NULL;
static char note_current_file[256] = {0};
static int note_is_modified = 0;
Head *h_notes = NULL;

/* 视频播放器全局变量 */
static lv_obj_t * video_win = NULL;
static lv_obj_t * video_control_bar = NULL;
static lv_obj_t * video_play_btn = NULL;
static lv_obj_t * video_play_label = NULL;
static lv_obj_t * video_slider = NULL;
static lv_obj_t * video_vol_slider = NULL;
static lv_obj_t * video_time_label = NULL;
static lv_obj_t * video_name_label = NULL;
static lv_timer_t * video_hide_timer = NULL;
static lv_timer_t * video_progress_timer = NULL;
static Head * h_video = NULL;
static DNode * video_current = NULL;
static pid_t video_pid = 0;
static int video_fifo_fd = -1;
static int video_output_fd = -1;
static int video_is_playing = 0;
static int video_is_paused = 0;
static int video_percent = 0;
static double video_position = 0.0;
static double video_duration = 0.0;
static char video_output_buffer[2048];
static size_t video_output_length = 0;
static const char video_fifo_path[] = "/tmp/lvgl_video.fifo";
static int flag_dog;
pid_t pid;





/* 回调函数前向声明 */
static void Calendar_cb(lv_event_t * e);
static void event_handler(lv_event_t * e);
static void back_to_main_cb(lv_event_t * e);
static void calendar_back_to_main_cb(lv_event_t * e);


/************获取触摸点的坐标**************/
static void my_active_cb(lv_event_t * e)
{
    lv_point_t point;
    //获取触摸点的坐标
    lv_indev_get_point(lv_indev_active(), &point);

    printf("-----(%d,%d)-----\n",point.x,point.y);
}

/* ===== 注册触摸坐标调试回调 ===== */
void get_point()
{
    lv_obj_add_event_cb(lv_screen_active(), my_active_cb ,LV_EVENT_CLICKED,NULL);
}
/***************************************/



/* ===== 创建系统主界面 ===== */
static void Main_Interface( void )  //主界面
{
    // get_point();
    Main_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(Main_win, 1024, 600);
    lv_obj_set_pos(Main_win, 0, 0);
    lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_SCROLLABLE);

    static lv_style_t main_style;
    static lv_style_t grid_style;
    static lv_style_t title_style;
    static lv_style_t button_style;
    static lv_style_t button_pressed_style;
    static lv_style_t button_focused_style;
    static lv_style_t placeholder_style;
    static lv_style_t label_style;
    static lv_style_t placeholder_label_style;
    static lv_style_t icon_style;
    static lv_style_t status_style;

    lv_style_init(&main_style);
    lv_style_set_bg_color(&main_style, lv_color_hex(0x05070B));
    lv_style_set_bg_opa(&main_style, LV_OPA_COVER);
    lv_style_set_border_width(&main_style, 0);
    lv_style_set_radius(&main_style, 0);
    lv_style_set_pad_all(&main_style, 0);
    lv_obj_add_style(Main_win, &main_style, LV_STATE_DEFAULT);

    /* Header */
    lv_obj_t * title_mark = lv_obj_create(Main_win);
    lv_obj_set_size(title_mark, 5, 38);
    lv_obj_set_pos(title_mark, 48, 30);
    lv_obj_set_style_bg_color(title_mark, lv_color_hex(0x1687FF), 0);
    lv_obj_set_style_bg_opa(title_mark, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(title_mark, 0, 0);
    lv_obj_set_style_radius(title_mark, 2, 0);
    lv_obj_remove_flag(title_mark, LV_OBJ_FLAG_SCROLLABLE);

    lv_style_init(&title_style);
    lv_style_set_text_color(&title_style, lv_color_hex(0xFFFFFF));
    lv_style_set_text_font(&title_style, &lv_font_montserrat_32);

    lv_obj_t * title = lv_label_create(Main_win);
    lv_label_set_text(title, "HOME");
    lv_obj_add_style(title, &title_style, LV_STATE_DEFAULT);
    lv_obj_set_pos(title, 68, 30);

    lv_obj_t * header_rule = lv_obj_create(Main_win);
    lv_obj_set_size(header_rule, 928, 1);
    lv_obj_set_pos(header_rule, 48, 91);
    lv_obj_set_style_bg_color(header_rule, lv_color_hex(0x202833), 0);
    lv_obj_set_style_bg_opa(header_rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_rule, 0, 0);
    lv_obj_set_style_radius(header_rule, 0, 0);
    lv_obj_remove_flag(header_rule, LV_OBJ_FLAG_SCROLLABLE);

    static int32_t col_dsc[] = {290, 290, 290, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {126, 126, 126, LV_GRID_TEMPLATE_LAST};

    /* 3 x 3 application grid */
    lv_obj_t * cont = lv_obj_create(Main_win);
    lv_obj_set_style_grid_column_dsc_array(cont, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, 0);
    lv_obj_set_size(cont, 910, 414);
    lv_obj_set_pos(cont, 57, 126);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_style_init(&grid_style);
    lv_style_set_bg_opa(&grid_style, LV_OPA_TRANSP);
    lv_style_set_border_width(&grid_style, 0);
    lv_style_set_radius(&grid_style, 0);
    lv_style_set_pad_all(&grid_style, 0);
    lv_style_set_pad_row(&grid_style, 18);
    lv_style_set_pad_column(&grid_style, 20);
    lv_obj_add_style(cont, &grid_style, LV_STATE_DEFAULT);

    lv_style_init(&button_style);
    lv_style_set_bg_color(&button_style, lv_color_hex(0x086CD9));
    lv_style_set_bg_opa(&button_style, LV_OPA_COVER);
    lv_style_set_border_color(&button_style, lv_color_hex(0x2B8CFF));
    lv_style_set_border_width(&button_style, 1);
    lv_style_set_radius(&button_style, 8);
    lv_style_set_shadow_color(&button_style, lv_color_hex(0x006EE6));
    lv_style_set_shadow_opa(&button_style, LV_OPA_20);
    lv_style_set_shadow_width(&button_style, 12);
    lv_style_set_shadow_spread(&button_style, 0);
    lv_style_set_pad_all(&button_style, 0);

    lv_style_init(&button_pressed_style);
    lv_style_set_bg_color(&button_pressed_style, lv_color_hex(0x0454AD));
    lv_style_set_border_color(&button_pressed_style, lv_color_hex(0x72B8FF));
    lv_style_set_shadow_width(&button_pressed_style, 4);

    lv_style_init(&button_focused_style);
    lv_style_set_outline_color(&button_focused_style, lv_color_hex(0x9ACBFF));
    lv_style_set_outline_width(&button_focused_style, 2);
    lv_style_set_outline_pad(&button_focused_style, 3);

    lv_style_init(&placeholder_style);
    lv_style_set_bg_color(&placeholder_style, lv_color_hex(0x0B2D50));
    lv_style_set_border_color(&placeholder_style, lv_color_hex(0x155A95));
    lv_style_set_shadow_opa(&placeholder_style, LV_OPA_TRANSP);

    lv_style_init(&label_style);
    lv_style_set_text_color(&label_style, lv_color_hex(0xFFFFFF));
    lv_style_set_text_font(&label_style, &lv_font_montserrat_26);

    lv_style_init(&placeholder_label_style);
    lv_style_set_text_color(&placeholder_label_style, lv_color_hex(0xFFFFFF));
    lv_style_set_text_font(&placeholder_label_style, &lv_font_montserrat_24);

    lv_style_init(&icon_style);
    lv_style_set_text_color(&icon_style, lv_color_hex(0xFFFFFF));
    lv_style_set_text_font(&icon_style, &lv_font_montserrat_32);

    lv_style_init(&status_style);
    lv_style_set_text_color(&status_style, lv_color_hex(0x6F9DC5));
    lv_style_set_text_font(&status_style, &lv_font_montserrat_12);

    static const char * button_names[9] = {
        "Calendar", "Album", "Video",
        "Music", "Note", "QQ Chat",                 // 第 6 个主界面按钮用于进入 QQ 聊天室
        "File System", "Reserved 2", "Reserved 3"
    };
    static const char * button_icons[9] = {
        LV_SYMBOL_LIST, LV_SYMBOL_IMAGE, LV_SYMBOL_VIDEO,
        LV_SYMBOL_AUDIO, LV_SYMBOL_EDIT, LV_SYMBOL_ENVELOPE, // QQ Chat 按钮使用信封图标表示消息功能
        LV_SYMBOL_DIRECTORY, LV_SYMBOL_SETTINGS, LV_SYMBOL_BARS
    };

    uint32_t i;
    for(i = 0; i < 9; i++) {
        uint8_t col = i % 3;
        uint8_t row = i / 3;

        obj = lv_button_create(cont);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, col, 1,
                             LV_GRID_ALIGN_STRETCH, row, 1);
        lv_obj_add_style(obj, &button_style, LV_STATE_DEFAULT);
        lv_obj_add_style(obj, &button_pressed_style, LV_STATE_PRESSED);
        lv_obj_add_style(obj, &button_focused_style, LV_STATE_FOCUSED);

        label = lv_label_create(obj);
        lv_label_set_text(label, button_names[i]);
        lv_obj_add_style(label, i < 7 ? &label_style : &placeholder_label_style,
                         LV_STATE_DEFAULT);

        lv_obj_t * icon = lv_label_create(obj);
        lv_label_set_text(icon, button_icons[i]);
        lv_obj_add_style(icon, &icon_style, LV_STATE_DEFAULT);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, i < 7 ? 16 : 12);

        if(i < 7)
            lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -16);
        else
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 9);

        switch(i) {
            case 0:
                lv_obj_add_event_cb(obj, Calendar_cb, LV_EVENT_CLICKED, NULL);
                break;
            case 1:
                lv_obj_add_event_cb(obj, Album_cb, LV_EVENT_CLICKED, NULL);
                break;
            case 2:
                lv_obj_add_event_cb(obj, Video_cb, LV_EVENT_CLICKED, NULL);
                break;
            case 3:
                lv_obj_add_event_cb(obj, Music_cb, LV_EVENT_CLICKED, NULL);
                break;
            case 4:
                lv_obj_add_event_cb(obj, Note_cb, LV_EVENT_CLICKED, NULL);
                break;
            case 5:                                     // 网格索引 5 对应 QQ Chat 主界面按钮
                lv_obj_add_event_cb(obj, QQ_cb, LV_EVENT_CLICKED, NULL); // 点击时调用 QQ 聊天室入口回调
                break;                                  // QQ 按钮绑定完成后退出 switch
            case 6:
                lv_obj_add_event_cb(obj, File_cb, LV_EVENT_CLICKED, NULL);
                break;
            default: {
                lv_obj_add_style(obj, &placeholder_style, LV_STATE_DEFAULT);
                lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);

                lv_obj_t * status = lv_label_create(obj);
                lv_label_set_text(status, "COMING SOON");
                lv_obj_add_style(status, &status_style, LV_STATE_DEFAULT);
                lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -16);
                break;
            }
        }
    }

    lv_obj_add_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
}

/* ===== File System 入口按钮回调 ===== */
static void File_cb(lv_event_t * e )
{
    LV_UNUSED(e);
    File_login(); 
}

void File_System_Interface(void)
{
    file_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(qq_login_win, 1024, 600);                                          //new+ 遮罩尺寸与开发板屏幕分辨率完全一致
    lv_obj_set_pos(qq_login_win, 0, 0);                                                //new+ 遮罩从左上角覆盖整个下层主界面
    lv_obj_set_style_bg_color(qq_login_win, lv_color_hex(0x343A42), 0);               //new+ 使用中性灰色压暗下层界面
    lv_obj_set_style_bg_opa(qq_login_win, LV_OPA_70, 0);                               //new+ 保留主界面轮廓同时降低其视觉亮度
    lv_obj_set_style_border_width(qq_login_win, 0, 0);                                 //new+ 全屏遮罩不绘制边框
    lv_obj_set_style_radius(qq_login_win, 0, 0);                                       //new+ 全屏遮罩四角贴合屏幕不使用圆角
    lv_obj_set_style_pad_all(qq_login_win, 0, 0);                                      //new+ 清除遮罩内边距以便使用精确坐标布局
    lv_obj_remove_flag(qq_login_win, LV_OBJ_FLAG_SCROLLABLE);                          //new+ 禁止模态遮罩被手势滚动
    lv_obj_add_flag(qq_login_win, LV_OBJ_FLAG_CLICKABLE);                              //new+ 拦截触摸事件防止用户操作下层主界面



}

void File_login(void)
{
    if(file_win != NULL) {                                                         //new+ 已存在登录沙盒时避免重复创建多个遮罩
        lv_obj_move_foreground(file_win);                                           //new+ 将现有登录遮罩重新置于最上层
        return;                                                                         //new+ 复用已有沙盒并结束本次重复入口调用
    }                                                                                   //new+ 结束重复创建保护判断
    file_win = lv_obj_create(lv_screen_active());                                  //new+ 在活动屏幕创建覆盖全屏的模态遮罩
    lv_obj_set_size(file_win, 1024, 600);                                          //new+ 遮罩尺寸与开发板屏幕分辨率完全一致
    lv_obj_set_pos(file_win, 0, 0);                                                //new+ 遮罩从左上角覆盖整个下层主界面
    lv_obj_set_style_bg_color(file_win, lv_color_hex(0x343A42), 0);               //new+ 使用中性灰色压暗下层界面
    lv_obj_set_style_bg_opa(file_win, LV_OPA_70, 0);                               //new+ 保留主界面轮廓同时降低其视觉亮度
    lv_obj_set_style_border_width(file_win, 0, 0);                                 //new+ 全屏遮罩不绘制边框
    lv_obj_set_style_radius(file_win, 0, 0);                                       //new+ 全屏遮罩四角贴合屏幕不使用圆角
    lv_obj_set_style_pad_all(file_win, 0, 0);                                      //new+ 清除遮罩内边距以便使用精确坐标布局
    lv_obj_remove_flag(file_win, LV_OBJ_FLAG_SCROLLABLE);                          //new+ 禁止模态遮罩被手势滚动
    lv_obj_add_flag(file_win, LV_OBJ_FLAG_CLICKABLE);                              //new+ 拦截触摸事件防止用户操作下层主界面
    file_login_panel = lv_obj_create(file_win);                                      //new+ 在灰色遮罩中央创建连接信息沙盒
    lv_obj_set_size(file_login_panel, 520, 320);                                         //new+ 沙盒采用约五百乘三百的紧凑尺寸
    lv_obj_center(file_login_panel);                                                     //new+ 未弹出键盘时将沙盒放在屏幕中央
    lv_obj_set_style_bg_color(file_login_panel, lv_color_hex(0x0C1118), 0);             //new+ 沙盒背景沿用系统深色主题
    lv_obj_set_style_bg_opa(file_login_panel, LV_OPA_COVER, 0);                          //new+ 沙盒完全不透明以清晰分隔表单内容
    lv_obj_set_style_border_color(file_login_panel, lv_color_hex(0x2B8CFF), 0);         //new+ 使用主题蓝色描边强调当前模态操作
    lv_obj_set_style_border_width(file_login_panel, 1, 0);                               //new+ 沙盒绘制一像素主题边框
    lv_obj_set_style_radius(file_login_panel, 8, 0);                                     //new+ 沙盒使用项目允许的八像素圆角
    lv_obj_set_style_shadow_color(file_login_panel, lv_color_hex(0x000000), 0);         //new+ 沙盒阴影使用纯黑色增强层级感
    lv_obj_set_style_shadow_opa(file_login_panel, LV_OPA_50, 0);                         //new+ 使用适度阴影透明度避免视觉过重
    lv_obj_set_style_shadow_width(file_login_panel, 24, 0);                              //new+ 扩展阴影范围使沙盒从压暗背景中浮起
    lv_obj_set_style_pad_all(file_login_panel, 0, 0);                                    //new+ 清除沙盒默认内边距以精确排列字段
    lv_obj_remove_flag(file_login_panel, LV_OBJ_FLAG_SCROLLABLE);                        //new+ 沙盒内容固定且不允许滚动
    lv_obj_t * title_mark = lv_obj_create(file_login_panel);                             //new+ 创建标题左侧的主题蓝色标记
    lv_obj_set_size(title_mark, 4, 28);                                                //new+ 标题标记使用窄而清晰的固定尺寸
    lv_obj_set_pos(title_mark, 24, 18);                                                //new+ 将标题标记放在沙盒左上角
    lv_obj_set_style_bg_color(title_mark, lv_color_hex(0x1687FF), 0);                 //new+ 标题标记使用系统主题蓝色
    lv_obj_set_style_bg_opa(title_mark, LV_OPA_COVER, 0);                              //new+ 标题标记保持完全不透明
    lv_obj_set_style_border_width(title_mark, 0, 0);                                   //new+ 标题标记不绘制多余边框
    lv_obj_set_style_radius(title_mark, 2, 0);                                         //new+ 为标题标记添加轻微圆角
    lv_obj_remove_flag(title_mark, LV_OBJ_FLAG_SCROLLABLE);                            //new+ 标题标记仅作装饰且不响应滚动
    lv_obj_t * title = lv_label_create(qq_login_panel);                                //new+ 创建 QQ 连接沙盒主标题
    lv_label_set_text(title, "File System");                                          //new+ 明确说明当前操作用于配置 QQ 连接
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);                     //new+ 主标题使用高对比度白色
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);                      //new+ 主标题使用二十号字体匹配紧凑弹窗
    lv_obj_set_pos(title, 40, 20);                                                     //new+ 主标题与蓝色标记水平对齐
    lv_obj_t * subtitle = lv_label_create(qq_login_panel);                             //new+ 创建连接信息填写说明副标题
    lv_label_set_text(subtitle, "Enter server details to continue");                 //new+ 提示完成表单后才能进入聊天室
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x718196), 0);                  //new+ 副标题使用弱化灰蓝色减少视觉竞争
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);                   //new+ 副标题采用十二号辅助文字
    lv_obj_set_pos(subtitle, 40, 47);                                                  //new+ 将副标题放在主标题正下方
    lv_obj_t * ip_label = lv_label_create(qq_login_panel);                             //new+ 创建服务器 IP 字段标签
    lv_label_set_text(ip_label, "SERVER IP");                                          //new+ 标识第一个输入框用于服务器 IPv4 地址
    lv_obj_set_style_text_color(ip_label, lv_color_hex(0x91A1B3), 0);                 //new+ IP 字段标签使用清晰的灰蓝色
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_12, 0);                   //new+ IP 字段标签采用十二号字体
    lv_obj_set_pos(ip_label, 24, 92);                                                  //new+ IP 标签与对应输入框垂直居中
    qq_login_ip_ta = lv_textarea_create(qq_login_panel);                               //new+ 创建服务器 IPv4 地址输入框
    lv_obj_set_size(qq_login_ip_ta, 374, 44);                                          //new+ IP 输入框为最长 IPv4 文本提供稳定空间
    lv_obj_set_pos(qq_login_ip_ta, 122, 78);                                           //new+ 将 IP 输入框放在第一行标签右侧
    lv_textarea_set_placeholder_text(qq_login_ip_ta, "192.168.1.10");                //new+ 用示例地址提示用户输入格式
    lv_textarea_set_accepted_chars(qq_login_ip_ta, "0123456789.");                   //new+ IP 输入仅接受数字和英文句点
    lv_textarea_set_max_length(qq_login_ip_ta, 15);                                    //new+ IPv4 地址最多允许十五个字符
    qq_login_style_textarea(qq_login_ip_ta);                                           //new+ 为 IP 输入框应用统一主题样式
    lv_obj_add_event_cb(qq_login_ip_ta, qq_login_textarea_cb, LV_EVENT_ALL, NULL);    //new+ 注册 IP 输入框事件并由回调读取全局专用键盘
    lv_obj_t * port_label = lv_label_create(qq_login_panel);                           //new+ 创建服务器端口字段标签
    lv_label_set_text(port_label, "PORT");                                             //new+ 标识第二个输入框用于 TCP 端口
    lv_obj_set_style_text_color(port_label, lv_color_hex(0x91A1B3), 0);               //new+ 端口字段标签使用灰蓝色
    lv_obj_set_style_text_font(port_label, &lv_font_montserrat_12, 0);                 //new+ 端口字段标签采用十二号字体
    lv_obj_set_pos(port_label, 24, 148);                                                //new+ 端口标签与第二行输入框垂直居中
    qq_login_port_ta = lv_textarea_create(qq_login_panel);                             //new+ 创建服务器 TCP 端口输入框
    lv_obj_set_size(qq_login_port_ta, 374, 44);                                        //new+ 端口输入框与其他字段保持统一尺寸
    lv_obj_set_pos(qq_login_port_ta, 122, 134);                                         //new+ 将端口输入框放在第二行标签右侧
    lv_textarea_set_placeholder_text(qq_login_port_ta, "8888");                      //new+ 使用项目默认端口作为格式示例
    lv_textarea_set_accepted_chars(qq_login_port_ta, "0123456789");                  //new+ 端口输入仅允许十进制数字
    lv_textarea_set_max_length(qq_login_port_ta, 5);                                    //new+ TCP 端口文本最多保留五位数字
    qq_login_style_textarea(qq_login_port_ta);                                         //new+ 为端口输入框应用统一主题样式
    lv_obj_add_event_cb(qq_login_port_ta, qq_login_textarea_cb, LV_EVENT_ALL, NULL);  //new+ 注册端口输入框事件并由回调读取全局专用键盘
    lv_obj_t * name_label = lv_label_create(qq_login_panel);                           //new+ 创建聊天昵称字段标签
    lv_label_set_text(name_label, "NAME");                                             //new+ 标识第三个输入框用于聊天室显示名称
    lv_obj_set_style_text_color(name_label, lv_color_hex(0x91A1B3), 0);               //new+ 昵称字段标签使用灰蓝色
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_12, 0);                 //new+ 昵称字段标签采用十二号字体
    lv_obj_set_pos(name_label, 24, 204);                                                //new+ 昵称标签与第三行输入框垂直居中
    qq_login_name_ta = lv_textarea_create(qq_login_panel);                             //new+ 创建聊天室昵称输入框
    lv_obj_set_size(qq_login_name_ta, 374, 44);                                        //new+ 昵称输入框与前两项保持对齐
    lv_obj_set_pos(qq_login_name_ta, 122, 190);                                         //new+ 将昵称输入框放在第三行标签右侧
    lv_textarea_set_placeholder_text(qq_login_name_ta, "Your display name");          //new+ 提示用户填写聊天中显示的昵称
    lv_textarea_set_max_length(qq_login_name_ta, QQ_CHAT_NAME_MAX - 1);                //new+ 昵称长度与聊天协议固定字段保持一致
    qq_login_style_textarea(qq_login_name_ta);                                         //new+ 为昵称输入框应用统一主题样式
    lv_obj_add_event_cb(qq_login_name_ta, qq_login_textarea_cb, LV_EVENT_ALL, NULL);  //new+ 注册昵称输入框事件并由回调读取全局专用键盘
    qq_login_error_label = lv_label_create(qq_login_panel);                            //new+ 创建表单校验错误提示标签
    lv_label_set_text(qq_login_error_label, "");                                       //new+ 初次打开时不显示任何错误内容
    lv_obj_set_width(qq_login_error_label, 260);                                        //new+ 限定提示宽度避免覆盖右侧操作按钮
    lv_label_set_long_mode(qq_login_error_label, LV_LABEL_LONG_DOT);                   //new+ 极端长提示自动省略以保持布局稳定
    lv_obj_set_style_text_color(qq_login_error_label, lv_color_hex(0xF26B6B), 0);      //new+ 错误内容使用醒目的柔和红色
    lv_obj_set_style_text_font(qq_login_error_label, &lv_font_montserrat_12, 0);       //new+ 错误提示使用十二号辅助字体
    lv_obj_set_pos(qq_login_error_label, 24, 278);                                      //new+ 将错误提示固定在沙盒底部左侧
    lv_obj_t * cancel_btn = lv_button_create(qq_login_panel);                          //new+ 创建返回主界面的取消按钮
    lv_obj_set_size(cancel_btn, 88, 40);                                                //new+ 取消按钮提供足够的触控尺寸
    lv_obj_set_pos(cancel_btn, 306, 266);                                               //new+ 将取消按钮放在沙盒底部右侧区域
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x1A2634), 0);                 //new+ 取消按钮使用次要深灰蓝背景
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x25384D), LV_STATE_PRESSED);   //new+ 按下时提亮取消按钮提供反馈
    lv_obj_set_style_border_color(cancel_btn, lv_color_hex(0x405267), 0);             //new+ 取消按钮使用低调灰蓝色边框
    lv_obj_set_style_border_width(cancel_btn, 1, 0);                                   //new+ 取消按钮绘制一像素边框
    lv_obj_set_style_radius(cancel_btn, 6, 0);                                         //new+ 取消按钮使用六像素圆角
    lv_obj_set_style_shadow_width(cancel_btn, 0, 0);                                   //new+ 移除取消按钮默认阴影保持界面克制
    lv_obj_t * cancel_label = lv_label_create(cancel_btn);                             //new+ 创建取消按钮文字标签
    lv_label_set_text(cancel_label, "CANCEL");                                         //new+ 使用明确命令文字表示退出配置
    lv_obj_set_style_text_color(cancel_label, lv_color_hex(0xD7E0EA), 0);             //new+ 取消文字使用柔和白色
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_12, 0);               //new+ 取消按钮文字使用十二号字体
    lv_obj_center(cancel_label);                                                       //new+ 将取消文字居中到按钮内部
    lv_obj_add_event_cb(cancel_btn, qq_login_cancel_cb, LV_EVENT_CLICKED, NULL);       //new+ 点击取消时关闭沙盒但不进入聊天室
    lv_obj_t * connect_btn = lv_button_create(qq_login_panel);                         //new+ 创建提交连接信息的主要按钮
    lv_obj_set_size(connect_btn, 96, 40);                                               //new+ 连接按钮尺寸略大于次要操作按钮
    lv_obj_set_pos(connect_btn, 400, 266);                                              //new+ 将连接按钮固定在沙盒右下角
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x086CD9), 0);                //new+ 连接按钮使用项目主题蓝色
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x0454AD), LV_STATE_PRESSED);  //new+ 按下时切换为深蓝色反馈
    lv_obj_set_style_border_color(connect_btn, lv_color_hex(0x2B8CFF), 0);            //new+ 连接按钮使用亮蓝色描边
    lv_obj_set_style_border_width(connect_btn, 1, 0);                                  //new+ 连接按钮绘制一像素边框
    lv_obj_set_style_radius(connect_btn, 6, 0);                                        //new+ 连接按钮使用六像素圆角
    lv_obj_set_style_shadow_width(connect_btn, 0, 0);                                  //new+ 移除连接按钮默认阴影保持扁平风格
    lv_obj_t * connect_label = lv_label_create(connect_btn);                           //new+ 创建连接按钮文字和箭头标签
    lv_label_set_text(connect_label, "ENTER  " LV_SYMBOL_RIGHT);                       //new+ 使用进入文字和右箭头强调下一步动作
    lv_obj_set_style_text_color(connect_label, lv_color_hex(0xFFFFFF), 0);             //new+ 主要按钮文字使用白色形成清晰对比
    lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_12, 0);              //new+ 连接按钮文字使用十二号字体
    lv_obj_center(connect_label);                                                      //new+ 将连接按钮内容水平垂直居中
    lv_obj_add_event_cb(connect_btn, qq_login_submit_cb, LV_EVENT_CLICKED, NULL);      //new+ 点击后校验三项信息并决定是否进入聊天室
    qq_login_keyboard = lv_keyboard_create(qq_login_win);                              //new+ 为 QQ 登录沙盒创建独立 LVGL 软键盘
    lv_obj_set_size(qq_login_keyboard, 760, 230);                                      //new+ 键盘使用适合一零二四乘六百屏幕的稳定尺寸
    lv_obj_align(qq_login_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);                        //new+ 将键盘贴合屏幕底部并水平居中
    lv_obj_set_style_bg_color(qq_login_keyboard, lv_color_hex(0x080C12), 0);          //new+ 键盘背景适配现有深色主题
    lv_obj_set_style_bg_opa(qq_login_keyboard, LV_OPA_COVER, 0);                       //new+ 键盘背景完全不透明避免下层内容干扰
    lv_obj_set_style_border_color(qq_login_keyboard, lv_color_hex(0x202C3A), 0);      //new+ 键盘外框使用灰蓝色分隔线
    lv_obj_set_style_border_width(qq_login_keyboard, 1, 0);                            //new+ 键盘整体绘制一像素边框
    lv_obj_set_style_radius(qq_login_keyboard, 0, 0);                                  //new+ 底部贴边键盘不使用外部圆角
    lv_obj_set_style_pad_all(qq_login_keyboard, 8, 0);                                 //new+ 键帽与键盘外框之间保留八像素间距
    lv_obj_set_style_pad_row(qq_login_keyboard, 6, 0);                                 //new+ 键盘各行之间保留六像素间距
    lv_obj_set_style_pad_column(qq_login_keyboard, 6, 0);                              //new+ 同一行键帽之间保留六像素间距
    lv_obj_set_style_bg_color(qq_login_keyboard, lv_color_hex(0x152131), LV_PART_ITEMS); //new+ 普通键帽使用深灰蓝色背景
    lv_obj_set_style_bg_color(qq_login_keyboard, lv_color_hex(0x075DBD), LV_PART_ITEMS | LV_STATE_PRESSED); //new+ 键帽按下时使用主题蓝色
    lv_obj_set_style_text_color(qq_login_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS); //new+ 键盘字符统一使用白色
    lv_obj_set_style_text_font(qq_login_keyboard, &lv_font_montserrat_16, LV_PART_ITEMS); //new+ 键盘字符使用易读的十六号字体
    lv_obj_set_style_border_width(qq_login_keyboard, 0, LV_PART_ITEMS);                 //new+ 单个键帽不额外绘制边框
    lv_obj_set_style_radius(qq_login_keyboard, 6, LV_PART_ITEMS);                      //new+ 单个键帽使用六像素小圆角
    lv_obj_add_event_cb(qq_login_keyboard, qq_login_keyboard_cb, LV_EVENT_ALL, NULL);  //new+ 监听键盘确认和左下角收起事件
    lv_obj_add_flag(qq_login_keyboard, LV_OBJ_FLAG_HIDDEN);                            //new+ 初次显示沙盒时先隐藏软键盘
}






/* ===== QQ 聊天室入口按钮回调 ===== */
static void QQ_cb(lv_event_t * e)                         // 用户点击主界面的 QQ Chat 按钮时进入此函数
{
    LV_UNUSED(e);                                        // 当前不需要读取具体事件对象，消除未使用参数警告
    QQ_login();                                          //new+ 先显示模态配置沙盒，信息完整前不创建聊天室
}

/* ===== 完整发送聊天室数据 ===== */
static int qq_chat_send_all(const void * data, size_t length) // 循环调用 send，保证指定长度的数据尽量全部写入 TCP
{
    const unsigned char * cursor = data;                 // 游标指向下一段尚未发送的数据起始位置

    while(length > 0) {                                  // TCP 单次 send 可能只发送部分数据，因此持续发送剩余部分
        qq_chat_io_result_t sent = send(qq_chat_socket_fd, (const char *)cursor, (int)length, MSG_NOSIGNAL); //new+ 使用平台对应返回类型发送数据并兼容 WinSock 的整数长度参数
        if(sent > 0) {                                   // 返回正数表示本次成功写入了 sent 个字节
            cursor += sent;                              // 将游标向后移动到尚未发送的数据位置
            length -= (size_t)sent;                      // 从剩余长度中扣除本次已发送的字节数
            continue;                                    // 继续尝试发送剩余字节
        }
        if(sent == QQ_CHAT_SOCKET_ERROR) {                                      //new+ 仅在当前平台明确报告发送失败时读取并处理网络错误码
            int socket_error = QQ_CHAT_LAST_SOCKET_ERROR();                     //new+ 立即保存错误码，避免后续系统调用覆盖失败原因
            if(socket_error == QQ_CHAT_INTERRUPTED) continue;                   //new+ 网络调用被中断时重试尚未发送的数据
#ifdef _WIN32                                                                  //new+ Windows 非阻塞聊天套接字还可能暂时没有可写空间
            if(QQ_CHAT_WOULD_BLOCK(socket_error)) {                             //new+ 将 WSAEWOULDBLOCK 视为可等待恢复的临时状态
                fd_set write_set;                                              //new+ 创建只监视当前聊天套接字的可写集合
                struct timeval write_timeout;                                  //new+ 限制等待时间，避免服务器异常时永久卡住界面
                FD_ZERO(&write_set);                                            //new+ 清空集合后再加入当前套接字
                FD_SET(qq_chat_socket_fd, &write_set);                          //new+ 要求 select 监视聊天连接何时恢复可写
                write_timeout.tv_sec = 3;                                      //new+ 单次发送最多等待三秒恢复可写
                write_timeout.tv_usec = 0;                                     //new+ 不追加额外微秒等待时间
                if(select(0, NULL, &write_set, NULL, &write_timeout) > 0) continue; //new+ 套接字恢复可写后继续发送剩余字节
            }                                                                  //new+ 结束 Windows 临时不可写错误处理
#endif                                                                         //new+ Linux 套接字仍使用原来的阻塞发送方式
        }                                                                      //new+ 结束发送失败错误码分支
        return -1;                                       // 其他错误无法在此恢复，通知上层发送失败
    }
    return 0;                                            // 所有指定数据均已发送完成
}

/* ===== 发送聊天室协议消息 ===== */
static int qq_chat_send_packet(uint32_t type, const char * text) // 将消息类型、昵称和正文封装为固定协议包后发送
{
    if(!qq_chat_connected || qq_chat_socket_fd == QQ_CHAT_INVALID_SOCKET) return -1; //new+ 使用平台无效值判断当前套接字是否可发送

    qq_chat_message_header_t header;                     // 在栈上创建固定长度的协议头
    header.magic = htonl(QQ_CHAT_MAGIC);                 // 将协议魔数转换成网络字节序后写入包头
    header.version = htonl(QQ_CHAT_VERSION);             // 将协议版本转换成网络字节序后写入包头
    header.type = htonl(type);                           // 将 JOIN、DATA 或 QUIT 等消息类型转换为网络字节序
    header.body_length = htonl(sizeof(qq_chat_message_body_t)); // 告诉接收方后续消息体具有固定大小

    qq_chat_message_body_t body = {0};                   // 创建并清零消息体，确保未使用区域不会带入随机数据
    snprintf(body.sender, sizeof(body.sender), "%s", qq_chat_user_name); // 安全复制当前客户端昵称并保证字符串结尾有效
    if(text != NULL) snprintf(body.text, sizeof(body.text), "%s", text); // 文本非空时安全复制消息正文，超长内容自动截断

    if(qq_chat_send_all(&header, sizeof(header)) != 0 || // 先发送固定长度协议头，失败时不再继续使用该消息
       qq_chat_send_all(&body, sizeof(body)) != 0) {     // 按“协议头在前、消息体在后”的顺序完整发送
        return -1;                                       // 任一部分发送失败都将整条消息判定为失败
    }
    return 0;                                            // 协议头和消息体均发送成功
}

/* ===== 发送 QQ 私聊协议消息 ===== */                                          //new+ 标记私聊数据包封装函数区域
static int qq_chat_send_private_packet(const char * target, const char * text)    //new+ 将真实发送者、目标昵称和正文封装为协议版本三私聊包
{                                                                                 //new+ 开始构造固定长度的私聊协议包
    qq_chat_message_header_t header;                                               //new+ 在栈上创建与群聊相同的固定协议头
    qq_chat_message_body_t body = {0};                                             //new+ 清零消息体以避免未使用字节携带随机数据
    if(!qq_chat_connected || qq_chat_socket_fd == QQ_CHAT_INVALID_SOCKET ||        //new+ 连接无效时不能发送私聊数据
       target == NULL || target[0] == '\0' || text == NULL || text[0] == '\0') {   //new+ 目标昵称或正文为空时拒绝生成无效私聊包
        return -1;                                                                 //new+ 将参数或连接错误返回给发送回调统一处理
    }                                                                              //new+ 结束私聊发送前置条件检查
    header.magic = htonl(QQ_CHAT_MAGIC);                                           //new+ 将协议魔数转换为网络字节序
    header.version = htonl(QQ_CHAT_VERSION);                                       //new+ 使用当前客户端和服务端共同支持的协议版本
    header.type = htonl(QQ_MSG_PRIVATE);                                           //new+ 明确标记该数据包需要服务端执行定向转发
    header.body_length = htonl(sizeof(qq_chat_message_body_t));                    //new+ 私聊继续使用原有固定消息体大小
    snprintf(body.sender, sizeof(body.sender), "%s", qq_chat_user_name);          //new+ sender 始终填写本机昵称且服务器仍会用连接身份重新认证
    snprintf(body.receiver, sizeof(body.receiver), "%s", target);                 //new+ receiver 专门填写当前联系人昵称供服务端执行定向转发
    snprintf(body.text, sizeof(body.text), "%s", text);                            //new+ 安全复制私聊正文并自动截断超长内容
    if(qq_chat_send_all(&header, sizeof(header)) != 0 ||                           //new+ 先完整发送私聊协议头
       qq_chat_send_all(&body, sizeof(body)) != 0) {                              //new+ 再完整发送目标昵称和消息正文
        return -1;                                                                 //new+ 任一段发送失败都将整条私聊判定为失败
    }                                                                              //new+ 结束私聊网络发送失败处理
    return 0;                                                                      //new+ 协议头和消息体均成功写入 TCP 连接
}                                                                                 //new+ 结束 QQ 私聊协议消息发送函数

/* ===== 查找 QQ 会话缓存 ===== */                                                //new+ 标记群聊和私聊会话查找函数区域
static qq_session_node_t * qq_chat_find_session(const char * peer)                 //new+ 按联系人昵称查找会话，空昵称统一代表群聊
{                                                                                 //new+ 开始遍历独立的 QQ 会话链表
    const char * key = peer != NULL ? peer : "";                                 //new+ 将空指针规范为空字符串以便统一比较群聊键
    qq_session_node_t * session = qq_session_head;                                 //new+ 从会话链表首节点开始顺序查找
    while(session != NULL) {                                                       //new+ 逐个检查当前保留的群聊和私聊会话
        if(strcmp(session->peer, key) == 0) return session;                        //new+ 会话键完全相同时返回对应缓存节点
        session = session->next;                                                   //new+ 当前会话不匹配时继续检查下一节点
    }                                                                              //new+ 结束 QQ 会话链表遍历
    return NULL;                                                                   //new+ 未找到时由调用者决定是否创建新会话
}                                                                                 //new+ 结束 QQ 会话缓存查找函数

/* ===== 获取或创建 QQ 会话缓存 ===== */                                          //new+ 标记会话按需创建函数区域
static qq_session_node_t * qq_chat_get_session(const char * peer, int create)      //new+ 返回已有会话或按 create 参数创建新会话
{                                                                                 //new+ 开始获取规范化的 QQ 会话节点
    const char * key = peer != NULL ? peer : "";                                 //new+ 空指针继续按公共群聊会话处理
    qq_session_node_t * session = qq_chat_find_session(key);                       //new+ 首先复用已有会话以保留独立历史记录
    qq_session_node_t ** tail;                                                     //new+ 保存链表尾部位置以追加新建会话
    if(session != NULL || !create) return session;                                 //new+ 已找到或禁止创建时直接返回当前结果
    session = calloc(1, sizeof(*session));                                         //new+ 为新会话分配并清零独立缓存节点
    if(session == NULL) return NULL;                                                //new+ 内存不足时不访问空节点并向上层报告失败
    snprintf(session->peer, sizeof(session->peer), "%s", key);                   //new+ 保存群聊空键或私聊联系人昵称
    tail = &qq_session_head;                                                       //new+ 从会话链表首指针开始寻找尾部位置
    while(*tail != NULL) tail = &(*tail)->next;                                    //new+ 保持会话按照首次打开顺序追加
    *tail = session;                                                               //new+ 将新会话节点接入 QQ 会话链表尾部
    return session;                                                                //new+ 返回可立即写入消息的新会话节点
}                                                                                 //new+ 结束 QQ 会话缓存获取函数

/* ===== 释放全部 QQ 会话缓存 ===== */                                            //new+ 标记退出聊天室时的消息缓存清理区域
static void qq_chat_clear_sessions(void)                                           //new+ 逐个释放 QQ 会话及其中保存的文字消息
{                                                                                 //new+ 开始清理独立于媒体链表的 QQ 会话资源
    qq_session_node_t * session = qq_session_head;                                 //new+ 保存当前需要释放的会话节点
    qq_session_head = NULL;                                                        //new+ 先断开全局入口避免清理时被旧逻辑重新访问
    qq_active_session = NULL;                                                      //new+ 当前会话同时失效以避免悬空指针
    while(session != NULL) {                                                       //new+ 顺序释放全部群聊和私聊会话
        qq_session_node_t * next_session = session->next;                          //new+ 在释放当前会话前保存下一个节点
        qq_message_node_t * message = session->head;                               //new+ 从当前会话最早消息开始释放记录
        while(message != NULL) {                                                   //new+ 逐条清除当前会话的文字消息缓存
            qq_message_node_t * next_message = message->next;                      //new+ 在释放当前消息前保存下一条记录
            free(message);                                                         //new+ 释放当前 QQ 消息节点占用的堆内存
            message = next_message;                                                //new+ 继续处理同一会话的下一条消息
        }                                                                          //new+ 当前会话的全部消息已经释放完成
        free(session);                                                             //new+ 释放已经清空消息的会话节点
        session = next_session;                                                    //new+ 继续清理 QQ 会话链表下一节点
    }                                                                              //new+ 全部 QQ 会话缓存均已释放
}                                                                                 //new+ 结束 QQ 会话缓存清理函数

/* ===== 更新聊天室连接状态 ===== */
static void qq_chat_update_connection_status(const char * text, lv_color_t color) // 同时更新左上角的连接状态文字和颜色
{
    if(qq_chat_status_label == NULL) return;             // 界面尚未创建或已退出时不再访问标签对象
    lv_label_set_text(qq_chat_status_label, text);       // 设置 OFFLINE、CONNECTING 或 ONLINE 状态文字
    lv_obj_set_style_text_color(qq_chat_status_label, color, 0); // 用红、黄、绿颜色直观区分当前连接状态
}

/* ===== 查找 QQ 在线联系人 ===== */
static qq_contact_node_t * qq_contact_find(const char * name)      //new+ 按昵称查找已有节点，避免同一用户被重复创建名片
{
    qq_contact_node_t * node = qq_contact_head;                    //new+ 从在线联系人链表首节点开始顺序查找

    if(name == NULL || name[0] == '\0') return NULL;                //new+ 空昵称不可能对应合法联系人，直接返回未找到
    while(node != NULL) {                                          //new+ 逐个检查当前连接保存的所有在线联系人
        if(strcmp(node->name, name) == 0) return node;              //new+ 昵称完全相同时返回对应联系人节点
        node = node->next;                                         //new+ 当前节点不匹配时继续检查下一个节点
    }
    return NULL;                                                   //new+ 遍历结束仍未匹配表示联系人尚未加入列表
}

/* ===== 点击 QQ 在线联系人名片 ===== */
static void qq_contact_card_cb(lv_event_t * e)                     //new+ 接收联系人名片点击事件并切换到该联系人的专属私聊
{
    qq_contact_node_t * expected = lv_event_get_user_data(e);      //new+ 读取创建名片时绑定的节点地址但暂不解引用
    qq_contact_node_t * node = qq_contact_head;                    //new+ 从当前有效链表查找该地址，避免先读取已释放节点内容

    while(node != NULL && node != expected) node = node->next;      //new+ 仅沿仍有效的联系人节点比较地址，不访问潜在悬空 user_data
    if(node == NULL) return;                                        //new+ 节点已下线或引用失效时忽略本次点击
    qq_chat_switch_session(node->name);                             //new+ 使用联系人昵称打开其独立消息历史和定向发送窗口
}

/* ===== 点击 QQ 群聊名片 ===== */                                  //new+ 标记从私聊返回公共群聊的名片事件区域
static void qq_group_card_cb(lv_event_t * e)                        //new+ 点击固定群聊名片时恢复公共聊天窗口
{                                                                  //new+ 开始处理群聊会话切换事件
    LV_UNUSED(e);                                                   //new+ 群聊会话固定为空键，无需读取事件对象内容
    qq_chat_switch_session("");                                    //new+ 空联系人昵称代表公共群聊并恢复其独立历史
}                                                                  //new+ 结束 QQ 群聊名片点击回调

/* ===== 添加 QQ 在线联系人名片 ===== */
static void qq_contact_add(const char * name)                       //new+ 收到完整列表条目或上线通知后同步创建链表节点与名片
{
    qq_contact_node_t * node;                                      //new+ 保存即将分配并加入链表的联系人节点
    qq_contact_node_t ** tail;                                     //new+ 保存链表尾部 next 指针位置以维持服务器同步顺序
    lv_obj_t * avatar;                                             //new+ 保存名片左侧的联系人头像底板
    lv_obj_t * avatar_icon;                                        //new+ 保存头像中央的联系人图标
    lv_obj_t * status_label;                                       //new+ 保存名片中的在线状态副标题

    if(name == NULL || name[0] == '\0' ||                          //new+ 拒绝空昵称，防止创建无法识别的空白名片
       strcmp(name, qq_chat_user_name) == 0 ||                      //new+ 左侧私聊列表不显示当前客户端自己
       qq_private_contact_list == NULL ||                           //new+ 聊天界面尚未创建联系人容器时不操作 UI
       !lv_obj_is_valid(qq_private_contact_list) ||                  //new+ 返回主界面后的失效容器不能继续创建子对象
       qq_contact_find(name) != NULL) return;                       //new+ 已存在同名联系人时不重复分配节点和名片

    node = calloc(1, sizeof(*node));                                //new+ 为 QQ 联系人分配并清零独立链表节点
    if(node == NULL) {                                              //new+ 内存不足时保留网络连接并在聊天区显示错误
        qq_chat_add_message("SYSTEM", "Unable to add online contact", 0); //new+ 告知用户本次在线名片创建失败
        return;                                                     //new+ 分配失败后不再访问空节点
    }
    snprintf(node->name, sizeof(node->name), "%s", name);          //new+ 安全复制服务器提供的昵称并自动限制最大长度

    node->card = lv_button_create(qq_private_contact_list);         //new+ 在群聊名片下方的滚动列表创建联系人名片
    lv_obj_set_size(node->card, 244, 68);                           //new+ 设置稳定触控尺寸，动态增删不会改变其他名片大小
    lv_obj_set_flex_grow(node->card, 0);                            //new+ 禁止 Flex 自动拉伸名片高度
    lv_obj_set_style_bg_color(node->card, lv_color_hex(0x0D151F), 0); //new+ 默认使用适配黑色主题的深灰蓝背景
    lv_obj_set_style_bg_color(node->card, lv_color_hex(0x123E67), LV_STATE_PRESSED); //new+ 按下名片时用蓝色提供触控反馈
    lv_obj_set_style_bg_opa(node->card, LV_OPA_COVER, 0);           //new+ 名片背景完全不透明以保证文字清晰
    lv_obj_set_style_border_color(node->card, lv_color_hex(0x26384A), 0); //new+ 使用低对比灰蓝色描边区分相邻联系人
    lv_obj_set_style_border_width(node->card, 1, 0);                //new+ 绘制一像素名片边框
    lv_obj_set_style_radius(node->card, 6, 0);                      //new+ 名片圆角与现有 QQ 群聊卡保持一致
    lv_obj_set_style_shadow_width(node->card, 0, 0);                //new+ 去除默认按钮阴影保持扁平风格
    lv_obj_set_style_pad_all(node->card, 0, 0);                     //new+ 清除默认内边距后使用稳定坐标放置内容
    lv_obj_remove_flag(node->card, LV_OBJ_FLAG_SCROLLABLE);         //new+ 名片本身不滚动，只允许外层联系人列表滚动
    lv_obj_add_event_cb(node->card, qq_contact_card_cb, LV_EVENT_CLICKED, node); //new+ 将联系人节点绑定到该名片点击事件

    avatar = lv_obj_create(node->card);                             //new+ 为联系人创建独立头像底板
    lv_obj_set_size(avatar, 42, 42);                                //new+ 头像保持固定方形尺寸避免列表布局跳动
    lv_obj_set_pos(avatar, 10, 12);                                 //new+ 将头像放在名片左侧并垂直居中
    lv_obj_set_style_bg_color(avatar, lv_color_hex(0x086CD9), 0);  //new+ 使用项目主题蓝色区分在线联系人
    lv_obj_set_style_bg_opa(avatar, LV_OPA_COVER, 0);               //new+ 头像底板完全不透明
    lv_obj_set_style_border_width(avatar, 0, 0);                    //new+ 头像不额外绘制边框
    lv_obj_set_style_radius(avatar, 6, 0);                          //new+ 头像采用与卡片一致的小圆角
    lv_obj_set_style_pad_all(avatar, 0, 0);                         //new+ 清除头像内边距便于图标精确居中
    lv_obj_remove_flag(avatar, LV_OBJ_FLAG_SCROLLABLE);             //new+ 头像只用于显示，不响应滚动
    lv_obj_remove_flag(avatar, LV_OBJ_FLAG_CLICKABLE);              //new+ 头像不截获点击事件，整个联系人名片都可稳定触发父按钮回调

    avatar_icon = lv_label_create(avatar);                          //new+ 在头像底板中创建联系人图标标签
    lv_label_set_text(avatar_icon, LV_SYMBOL_ENVELOPE);             //new+ 使用信封图标表示该用户可作为私聊对象
    lv_obj_set_style_text_color(avatar_icon, lv_color_hex(0xFFFFFF), 0); //new+ 图标使用白色以适配蓝色头像背景
    lv_obj_set_style_text_font(avatar_icon, &lv_font_montserrat_18, 0); //new+ 设置清晰且不会溢出的图标字号
    lv_obj_center(avatar_icon);                                     //new+ 将图标水平垂直居中到头像中

    node->name_label = lv_label_create(node->card);                 //new+ 创建并保存联系人昵称标签对象
    lv_label_set_text(node->name_label, node->name);                //new+ 在名片主行显示服务器确认的在线昵称
    lv_label_set_long_mode(node->name_label, LV_LABEL_LONG_DOT);    //new+ 昵称过长时使用省略号避免越出名片
    lv_obj_set_size(node->name_label, 168, 22);                     //new+ 为最长昵称提供固定显示范围
    lv_obj_set_pos(node->name_label, 64, 12);                       //new+ 将昵称放在头像右侧上半区域
    lv_obj_set_style_text_color(node->name_label, lv_color_hex(0xFFFFFF), 0); //new+ 昵称使用白字匹配项目主题
    lv_obj_set_style_text_font(node->name_label, &lv_font_montserrat_14, 0); //new+ 昵称使用适合紧凑列表的十四号字体

    status_label = lv_label_create(node->card);                     //new+ 创建联系人在线状态副标题
    node->status_label = status_label;                              //new+ 保存状态标签引用以便显示未读消息数量
    lv_label_set_text(status_label, "ONLINE");                     //new+ 当前名片只代表服务器确认的在线用户
    lv_obj_set_pos(status_label, 64, 39);                           //new+ 将状态放在昵称正下方
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x35D07F), 0); //new+ 在线状态使用绿色便于快速识别
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10, 0); //new+ 状态文字降低字号避免抢占昵称层级

    tail = &qq_contact_head;                                        //new+ 从链表首指针开始寻找最后一个 next 位置
    while(*tail != NULL) tail = &(*tail)->next;                     //new+ 保持在线列表与服务器逐项同步的顺序一致
    *tail = node;                                                   //new+ 将新联系人节点接到链表尾部
    qq_chat_update_contact_status(node->name);                      //new+ 联系人重新上线时立即恢复其旧私聊的未读数量显示
}

/* ===== 删除 QQ 离线联系人名片 ===== */
static void qq_contact_remove(const char * name)                    //new+ 收到用户下线通知后同步删除节点和左侧名片
{
    qq_contact_node_t ** link = &qq_contact_head;                   //new+ 保存指向当前节点的上一级指针以便安全摘链
    int return_to_group = qq_active_session != NULL &&              //new+ 记录下线用户是否正是当前私聊对象
                          strcmp(qq_active_session->peer, name != NULL ? name : "") == 0; //new+ 在删除联系人节点前完成稳定会话昵称比较

    if(name == NULL || name[0] == '\0') return;                     //new+ 空昵称无法确定目标联系人，不执行误删除
    while(*link != NULL) {                                         //new+ 顺序查找与下线昵称完全一致的节点
        qq_contact_node_t * node = *link;                           //new+ 保存当前候选节点便于检查和释放
        if(strcmp(node->name, name) == 0) {                         //new+ 找到服务器通知的下线用户
            *link = node->next;                                    //new+ 先从联系人链表摘除节点，防止回调再引用它
            if(node->card != NULL && lv_obj_is_valid(node->card)) { //new+ 名片仍属于有效界面时才请求 LVGL 删除
                lv_obj_delete(node->card);                          //new+ 立即删除名片及其头像、昵称和状态子对象
            }
            free(node);                                            //new+ 释放对应联系人链表节点内存
            if(return_to_group) qq_chat_switch_session("");        //new+ 当前私聊对象离线后自动返回公共群聊避免继续误发
            return;                                                //new+ 昵称唯一，删除成功后无需继续遍历
        }
        link = &node->next;                                        //new+ 当前昵称不匹配时检查下一个联系人
    }
}

/* ===== 清空 QQ 在线联系人 ===== */
static void qq_contact_clear(void)                                 //new+ 断线、重连同步或退出界面时统一释放全部联系人
{
    qq_contact_node_t * node = qq_contact_head;                     //new+ 保存当前需要释放的联系人节点

    qq_contact_head = NULL;                                        //new+ 先断开全局链表入口，避免清理过程中被旧回调查询到
    while(node != NULL) {                                          //new+ 逐个释放当前连接保存的全部在线联系人
        qq_contact_node_t * next = node->next;                      //new+ 在释放当前节点前保存下一个节点地址
        if(node->card != NULL && lv_obj_is_valid(node->card)) {     //new+ 联系人界面仍有效时同步删除对应名片
            lv_obj_delete(node->card);                              //new+ 删除名片及其全部子控件，列表自动重新排版
        }
        free(node);                                                 //new+ 释放当前联系人节点占用的堆内存
        node = next;                                                //new+ 继续清理已保存的下一个节点
    }
}

/* ===== 连接 TCP 聊天室服务端 ===== */
static int qq_chat_client_connect(void)                  // 读取运行参数并建立 IPv4 TCP 客户端连接
{
    if(qq_chat_connected) return 0;                      // 已连接时直接返回成功，避免重复创建套接字

    const char * server_ip = qq_IP;                      //new+ 使用登录沙盒中已校验并保存的服务器 IPv4 地址
    int server_port = qq_Port;                           //new+ 使用登录沙盒中已校验并保存的 TCP 端口
    snprintf(qq_chat_user_name, sizeof(qq_chat_user_name), "%.*s", QQ_CHAT_NAME_MAX - 1, qq_name); //new+ 按协议字段上限复制登录昵称并确保字符串安全结束

    qq_chat_update_connection_status("CONNECTING", lv_color_hex(0xE5B94C)); // 连接过程中以黄色显示 CONNECTING
    qq_chat_socket_fd = socket(AF_INET, SOCK_STREAM, 0); // 创建一个 IPv4、面向字节流的 TCP 套接字
    if(qq_chat_socket_fd == QQ_CHAT_INVALID_SOCKET) {    //new+ 使用平台定义的无效套接字值判断创建失败
        qq_chat_update_connection_status("OFFLINE", lv_color_hex(0xD95C5C)); // 恢复红色离线状态
        return -1;                                       // 向调用者报告连接失败
    }

    struct sockaddr_in server_address;                   // 保存目标服务器 IPv4 地址与端口
    memset(&server_address, 0, sizeof(server_address));  // 先清零结构体，避免未初始化字段影响 connect
    server_address.sin_family = AF_INET;                 // 指定地址族为 IPv4
    server_address.sin_port = htons((uint16_t)server_port); // 将主机端口转换为网络字节序
    if(inet_pton(AF_INET, server_ip, &server_address.sin_addr) != 1 || // 将点分十进制 IPv4 文本写入服务端地址结构
       connect(qq_chat_socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) != 0) { // 转换 IP 或建立 TCP 连接失败时进入清理流程
        qq_chat_close_socket(qq_chat_socket_fd);         //new+ 使用当前平台正确的函数释放连接失败的套接字
        qq_chat_socket_fd = QQ_CHAT_INVALID_SOCKET;      //new+ 将连接失败后的句柄恢复为平台无效值
        qq_chat_update_connection_status("OFFLINE", lv_color_hex(0xD95C5C)); // 界面恢复红色离线状态
        return -1;                                       // 向调用者报告失败，界面会显示连接提示
    }

#ifdef _WIN32                                                                  //new+ Windows 没有可用于单次 recv 的 MSG_DONTWAIT 标志
    u_long nonblocking_mode = 1;                                               //new+ 数值一要求 WinSock 将聊天套接字切换为非阻塞模式
    if(ioctlsocket(qq_chat_socket_fd, FIONBIO, &nonblocking_mode) == SOCKET_ERROR) { //new+ 非阻塞配置失败时不能让 LVGL 定时器执行阻塞接收
        qq_chat_close_socket(qq_chat_socket_fd);                               //new+ 释放无法安全用于虚拟屏定时接收的套接字
        qq_chat_socket_fd = QQ_CHAT_INVALID_SOCKET;                            //new+ 清除已经关闭的 Windows 套接字句柄
        qq_chat_update_connection_status("OFFLINE", lv_color_hex(0xD95C5C));  //new+ 在界面中显示非阻塞配置失败后的离线状态
        return -1;                                                             //new+ 阻止使用可能卡住虚拟屏界面的连接
    }                                                                          //new+ 结束 Windows 非阻塞模式配置失败处理
#endif                                                                         //new+ Linux 继续通过 MSG_DONTWAIT 执行单次非阻塞接收

    qq_chat_connected = 1;                              // 标记 TCP 三次握手已经成功完成
    qq_chat_receive_length = 0;                         // 新连接开始前清空上一次可能残留的收包长度
    qq_chat_update_connection_status("ONLINE", lv_color_hex(0x35D07F)); // 使用绿色显示在线状态

    if(qq_chat_send_packet(QQ_MSG_JOIN, "joined the chat room") != 0) { // 首先向服务端报告当前用户加入聊天室
        qq_chat_client_disconnect(0);                    // 加入消息发送失败时直接清理连接且不再发 QUIT
        return -1;                                       // 将初始化失败传回界面创建函数
    }

    char connection_text[96];                           // 临时保存连接成功提示文字
    snprintf(connection_text, sizeof(connection_text), "Connected to %s:%d", server_ip, server_port); // 拼接实际连接的服务器地址和端口
    qq_chat_add_message("SYSTEM", connection_text, 0); // 将连接成功信息作为居中的系统消息显示
    return 0;                                           // TCP 连接及 JOIN 消息均处理成功
}

/* ===== 断开 TCP 聊天室服务端 ===== */
static void qq_chat_client_disconnect(int send_quit)    // 结束网络会话并重置全部连接状态
{
    if(qq_chat_socket_fd != QQ_CHAT_INVALID_SOCKET) {   //new+ 仅当平台套接字句柄有效时执行网络关闭操作
        if(send_quit && qq_chat_connected) {            // 用户主动退出且连接正常时通知服务器
            qq_chat_send_packet(QQ_MSG_QUIT, "left the chat room"); // 发送离开聊天室协议消息
        }
        shutdown(qq_chat_socket_fd, QQ_CHAT_SHUT_RDWR); //new+ 使用当前平台的常量关闭套接字双向通信
        qq_chat_close_socket(qq_chat_socket_fd);        //new+ 使用当前平台规定的函数释放套接字资源
    }

    qq_chat_socket_fd = QQ_CHAT_INVALID_SOCKET;         //new+ 使用平台无效值清除已关闭的套接字句柄
    qq_chat_connected = 0;                             // 将逻辑连接状态复位为未连接
    qq_chat_receive_length = 0;                        // 丢弃未接收完整的旧数据包片段
    qq_contact_clear();                                 //new+ 连接断开后清除已失效的在线联系人和对应名片
    if(qq_active_session != NULL && qq_active_session->peer[0] != '\0' && //new+ 真正断线时不能继续停留在不可发送的私聊窗口
       qq_chat_win != NULL && lv_obj_is_valid(qq_chat_win)) {      //new+ 仅在聊天室界面仍存在时执行会话回退
        qq_chat_switch_session("");                               //new+ 断开服务器后自动返回公共群聊并保留私聊历史缓存
    }                                                              //new+ 结束断线后的私聊回退处理
    qq_chat_update_connection_status("OFFLINE", lv_color_hex(0xD95C5C)); // 将界面状态改回红色离线
}

/* ===== 添加一条聊天消息到界面 ===== */
static void qq_chat_draw_message(const char * sender, const char * text,            //new+ 接收发送者和正文创建一条可见消息
                                 int is_self, int is_system)                        //new+ 显式接收气泡方向和系统消息类型，避免昵称伪装系统提示
{
    if(qq_chat_message_list == NULL || text == NULL || text[0] == '\0') return; // 列表不存在或正文为空时不创建无效控件

    lv_obj_t * row = lv_obj_create(qq_chat_message_list); // 每条消息建立独立行，便于控制整体左右对齐
    lv_obj_set_width(row, LV_PCT(100));                  // 消息行占满聊天列表的可用宽度
    lv_obj_set_height(row, LV_SIZE_CONTENT);             // 行高根据发送者标签和消息正文自动计算
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);      // 消息行本身透明，只显示内部气泡
    lv_obj_set_style_border_width(row, 0, 0);            // 去掉消息行默认边框
    lv_obj_set_style_radius(row, 0, 0);                  // 消息行无需圆角，气泡单独设置圆角
    lv_obj_set_style_pad_all(row, 0, 0);                 // 清除默认内边距，准确控制气泡宽度
    lv_obj_set_style_pad_row(row, 4, 0);                 // 发送者名称与消息气泡之间保留 4 像素间隔
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);     // 单条消息行不能独立滚动，由外层列表统一滚动
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);      // 发送者名称和气泡按垂直方向从上到下排列

    if(is_system) {                                      // 系统消息不使用聊天气泡，而是居中显示灰色文字
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER); // 将系统提示在消息行中央对齐
        lv_obj_t * system_label = lv_label_create(row);  // 创建显示连接、离线或错误信息的系统标签
        lv_label_set_text(system_label, text);           // 设置需要展示的系统提示正文
        lv_label_set_long_mode(system_label, LV_LABEL_LONG_WRAP); // 超过宽度的系统提示自动换行
        lv_obj_set_width(system_label, 650);             // 限制提示宽度，避免文字紧贴列表边缘
        lv_obj_set_style_text_align(system_label, LV_TEXT_ALIGN_CENTER, 0); // 多行系统提示也保持文字居中
        lv_obj_set_style_text_color(system_label, lv_color_hex(0x6F8193), 0); // 使用低对比灰蓝色区别于普通聊天消息
        lv_obj_set_style_text_font(system_label, &lv_font_montserrat_12, 0); // 系统提示使用较小字号减少视觉干扰
    }
    else {                                               // 普通消息显示发送者名称和有背景色的消息气泡
        lv_obj_set_flex_align(row,                       // 设置普通消息行在纵向主轴和横向交叉轴上的对齐方式
                              LV_FLEX_ALIGN_START,       // 消息行内部内容从纵向起点开始排列
                              is_self ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, // 自己的消息横向靠右，其他人的消息横向靠左
                              LV_FLEX_ALIGN_START);       // 自己的消息靠右，其他客户端消息靠左

        lv_obj_t * sender_label = lv_label_create(row);  // 在气泡上方创建发送者昵称标签
        lv_label_set_text(sender_label, sender);         // 显示消息协议中携带的发送者昵称
        lv_obj_set_style_text_color(sender_label, lv_color_hex(0x718196), 0); // 昵称使用灰蓝色，正文仍保持白色
        lv_obj_set_style_text_font(sender_label, &lv_font_montserrat_12, 0); // 昵称使用 12 号字体以建立文字层级

        lv_obj_t * bubble = lv_obj_create(row);          // 创建包裹聊天正文的消息气泡容器
        lv_obj_set_width(bubble, 460);                   // 固定最大展示宽度，让长消息在气泡内部换行
        lv_obj_set_height(bubble, LV_SIZE_CONTENT);      // 气泡高度跟随换行后的正文自动增长
        lv_obj_set_style_bg_color(bubble,                // 根据消息发送者设置气泡默认背景颜色
                                  is_self ? lv_color_hex(0x086CD9) : lv_color_hex(0x111B27), 0); // 自己使用亮蓝色，其他人使用深灰蓝色
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0); // 气泡背景完全不透明，保证白字清晰可读
        lv_obj_set_style_border_color(bubble,            // 根据消息发送者设置气泡边框颜色
                                      is_self ? lv_color_hex(0x2B8CFF) : lv_color_hex(0x223244), 0); // 边框颜色随消息来源匹配气泡主题
        lv_obj_set_style_border_width(bubble, 1, 0);     // 使用 1 像素边框增强气泡轮廓
        lv_obj_set_style_radius(bubble, 6, 0);           // 采用较小圆角以适配项目统一 UI 风格
        lv_obj_set_style_pad_all(bubble, 12, 0);         // 正文与气泡边缘保持 12 像素内边距
        lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE); // 气泡自身不滚动，由聊天列表统一管理滚动

        lv_obj_t * message_label = lv_label_create(bubble); // 在气泡内部创建真正显示正文的标签
        lv_label_set_text(message_label, text);          // 写入收到或刚发送的聊天文本
        lv_label_set_long_mode(message_label, LV_LABEL_LONG_WRAP); // 长消息到达固定宽度后自动换行
        lv_obj_set_width(message_label, 434);            // 460 像素气泡扣除边框和内边距后的正文宽度
        lv_obj_set_style_text_color(message_label, lv_color_hex(0xFFFFFF), 0); // 所有聊天正文统一使用白色
        lv_obj_set_style_text_font(message_label, &lv_font_montserrat_14, 0); // 正文使用清晰的 14 号字体
    }

    lv_obj_update_layout(qq_chat_message_list);          // 立即计算新消息行的尺寸和位置
    lv_obj_scroll_to_view(row, LV_ANIM_ON);              // 平滑滚动到底部，确保用户能看到最新消息
}

/* ===== 更新 QQ 联系人会话状态 ===== */                                      //new+ 标记联系人在线状态和未读数量更新区域
static void qq_chat_update_contact_status(const char * peer)                       //new+ 根据对应私聊会话的未读数更新左侧联系人副标题
{                                                                                 //new+ 开始刷新指定联系人的状态标签
    qq_contact_node_t * contact = qq_contact_find(peer);                           //new+ 仅在线联系人存在可见名片和状态标签
    qq_session_node_t * session = qq_chat_find_session(peer);                      //new+ 查找该联系人已有的独立私聊缓存
    char status_text[32];                                                          //new+ 保存 ONLINE 或带未读数量的短状态文字
    if(contact == NULL || contact->status_label == NULL ||                         //new+ 联系人已经离线或标签无效时无需刷新 UI
       !lv_obj_is_valid(contact->status_label)) return;                            //new+ 防止访问已删除联系人名片中的子对象
    if(session != NULL && session->unread_count > 0) {                             //new+ 当前私聊有未读消息时在在线状态后显示数量
        snprintf(status_text, sizeof(status_text), "ONLINE - %u NEW",             //new+ 生成适合侧栏宽度的未读状态文字
                 session->unread_count > 99U ? 99U : session->unread_count);       //new+ 显示上限为九十九以避免文本溢出名片
        lv_label_set_text(contact->status_label, status_text);                     //new+ 将未读数量同步到该联系人名片
        lv_obj_set_style_text_color(contact->status_label, lv_color_hex(0x4AA3FF), 0); //new+ 未读状态使用主题亮蓝色突出显示
    }                                                                              //new+ 结束有未读消息的联系人状态处理
    else {                                                                         //new+ 当前会话没有未读消息时恢复普通在线状态
        lv_label_set_text(contact->status_label, "ONLINE");                       //new+ 显示服务端确认的标准在线状态
        lv_obj_set_style_text_color(contact->status_label, lv_color_hex(0x35D07F), 0); //new+ 普通在线状态继续使用绿色
    }                                                                              //new+ 结束无未读消息的联系人状态处理
}                                                                                 //new+ 结束 QQ 联系人会话状态更新函数

/* ===== 缓存一条 QQ 会话消息 ===== */                                          //new+ 标记独立会话消息追加和限量处理区域
static void qq_chat_append_session_message(qq_session_node_t * session,            //new+ 指定需要保存消息的群聊或私聊会话
                                           const char * sender, const char * text, //new+ 接收需要显示的发送者昵称和正文
                                           int is_self, int is_system, int unread) //new+ 接收气泡方向、系统样式和未读计数标志
{                                                                                 //new+ 开始保存并按需显示单条 QQ 消息
    qq_message_node_t * message;                                                   //new+ 保存本次新分配的消息缓存节点
    if(session == NULL || text == NULL || text[0] == '\0') return;                 //new+ 会话或正文无效时不创建空消息记录
    message = calloc(1, sizeof(*message));                                         //new+ 为新消息分配并清零独立缓存节点
    if(message == NULL) {                                                          //new+ 消息缓存内存不足时仍尽量向当前用户显示提示
        if(session == qq_active_session) qq_chat_draw_message("SYSTEM", "Message history is full", 0, 1); //new+ 当前会话可见时显示明确的系统缓存失败提示
        return;                                                                     //new+ 分配失败后不再访问空消息节点
    }                                                                              //new+ 结束 QQ 消息节点分配失败处理
    snprintf(message->sender, sizeof(message->sender), "%s",                     //new+ 将系统名称或实际昵称保存到固定长度字段
             is_system ? "SYSTEM" : (sender != NULL ? sender : "UNKNOWN"));      //new+ 空普通昵称使用 UNKNOWN 防止标签读取空指针
    snprintf(message->text, sizeof(message->text), "%s", text);                  //new+ 安全复制正文并按协议长度自动截断
    message->is_self = is_self ? 1 : 0;                                            //new+ 将任意非零发送方向规范为一
    message->is_system = is_system ? 1 : 0;                                        //new+ 将任意非零系统标志规范为一
    if(session->tail != NULL) session->tail->next = message;                       //new+ 非空会话把新消息接到原尾节点之后
    else session->head = message;                                                  //new+ 空会话让首指针直接指向第一条消息
    session->tail = message;                                                       //new+ 更新尾指针以便下一条消息常量时间追加
    session->message_count++;                                                      //new+ 累加本会话当前缓存消息数量
    if(session->message_count > QQ_CHAT_SESSION_MESSAGE_LIMIT) {                   //new+ 超过上限时丢弃本会话最早的一条记录
        qq_message_node_t * oldest = session->head;                                //new+ 保存需要淘汰的最早消息节点
        session->head = oldest->next;                                              //new+ 将会话首指针移动到第二条消息
        if(session == qq_active_session && qq_chat_message_list != NULL &&          //new+ 当前可见会话还需同步删除屏幕上最早的气泡对象
           lv_obj_is_valid(qq_chat_message_list) &&                                 //new+ 确认消息列表仍属于当前有效聊天室界面
           lv_obj_get_child_count(qq_chat_message_list) > 0) {                     //new+ 只有列表确实包含消息行时才获取第一个子对象
            lv_obj_delete(lv_obj_get_child(qq_chat_message_list, 0));               //new+ 删除最早气泡使界面和一百条缓存上限保持一致
        }                                                                          //new+ 结束当前会话旧气泡同步淘汰处理
        free(oldest);                                                              //new+ 释放已从缓存中淘汰的最早消息
        session->message_count--;                                                  //new+ 将数量恢复到定义的一百条上限
    }                                                                              //new+ 结束 QQ 会话消息上限维护
    if(session == qq_active_session) {                                             //new+ 消息属于当前页面时立即创建可见气泡
        session->unread_count = 0;                                                 //new+ 当前页面已经展示的新消息不再计为未读
        qq_chat_draw_message(message->is_system ? "SYSTEM" : message->sender,      //new+ 使用缓存中的规范化发送者重绘消息
                             message->text, message->is_self, message->is_system); //new+ 按显式类型和方向显示系统文字或左右气泡
    }                                                                              //new+ 结束当前会话的即时消息绘制
    else if(unread && session->unread_count < 99U) {                               //new+ 后台会话未读数量在名片可显示范围内饱和累加
        session->unread_count++;                                                   //new+ 记录用户尚未查看且最多九十九条的新消息
    }                                                                              //new+ 结束后台会话未读数量更新
    if(session->peer[0] != '\0') qq_chat_update_contact_status(session->peer);     //new+ 私聊会话同步刷新联系人名片状态
}                                                                                 //new+ 结束 QQ 会话消息缓存追加函数

/* ===== 添加一条 QQ 群聊消息 ===== */                                          //new+ 保留原调用入口并统一把公共消息写入群聊缓存
static void qq_chat_add_message(const char * sender, const char * text, int is_self) //new+ 缓存并按当前会话状态显示群聊或系统消息
{                                                                                 //new+ 开始处理公共群聊消息
    int is_system = sender == NULL || strcmp(sender, "SYSTEM") == 0;              //new+ 根据原有调用约定识别系统提示样式
    qq_session_node_t * session = qq_chat_get_session("", 1);                    //new+ 获取空昵称代表的公共群聊会话缓存
    qq_chat_append_session_message(session, sender, text, is_self, is_system,      //new+ 保存群聊消息并在当前群聊时立即绘制
                                   !is_self && !is_system);                        //new+ 仅后台收到的其他用户普通消息计入未读
}                                                                                 //new+ 结束 QQ 群聊消息添加函数

/* ===== 更新 QQ 会话选中样式 ===== */                                          //new+ 标记群聊和联系人名片选中状态刷新区域
static void qq_chat_update_session_styles(void)                                   //new+ 根据当前会话统一刷新左侧所有可点击名片
{                                                                                 //new+ 开始更新群聊及联系人选中样式
    qq_contact_node_t * contact = qq_contact_head;                                 //new+ 从在线联系人链表首节点开始刷新
    int group_selected = qq_active_session == NULL || qq_active_session->peer[0] == '\0'; //new+ 空会话键代表群聊处于选中状态
    if(qq_group_card != NULL && lv_obj_is_valid(qq_group_card)) {                  //new+ 群聊名片仍有效时更新背景和边框颜色
        lv_obj_set_style_bg_color(qq_group_card,                                  //new+ 选中群聊使用蓝色背景，未选中恢复深灰蓝
                                  lv_color_hex(group_selected ? 0x123E67 : 0x0D151F), 0); //new+ 将群聊背景匹配当前会话状态
        lv_obj_set_style_border_color(qq_group_card,                              //new+ 选中群聊使用亮蓝描边，未选中使用低对比描边
                                      lv_color_hex(group_selected ? 0x1687FF : 0x26384A), 0); //new+ 完成群聊边框状态刷新
    }                                                                              //new+ 结束群聊名片选中样式更新
    while(contact != NULL) {                                                       //new+ 逐个刷新所有在线联系人名片
        int selected = qq_active_session != NULL &&                               //new+ 当前存在有效会话时才可能选中联系人
                       strcmp(qq_active_session->peer, contact->name) == 0;         //new+ 会话昵称与联系人昵称一致表示当前私聊
        if(contact->card != NULL && lv_obj_is_valid(contact->card)) {              //new+ 仅操作仍存在于当前界面的联系人名片
            lv_obj_set_style_bg_color(contact->card,                              //new+ 私聊选中时使用蓝色背景突出当前联系人
                                      lv_color_hex(selected ? 0x123E67 : 0x0D151F), 0); //new+ 未选中联系人恢复默认深灰蓝背景
            lv_obj_set_style_border_color(contact->card,                          //new+ 私聊选中时使用主题亮蓝色描边
                                          lv_color_hex(selected ? 0x1687FF : 0x26384A), 0); //new+ 未选中联系人恢复普通描边
        }                                                                          //new+ 结束当前联系人有效名片的样式更新
        contact = contact->next;                                                   //new+ 继续刷新下一个在线联系人
    }                                                                              //new+ 全部联系人选中样式已经刷新完成
}                                                                                 //new+ 结束 QQ 会话选中样式更新函数

/* ===== 切换 QQ 群聊或私聊会话 ===== */                                        //new+ 标记左侧名片驱动的会话切换区域
static void qq_chat_switch_session(const char * peer)                              //new+ 空昵称切到群聊，非空昵称切到对应联系人私聊
{                                                                                 //new+ 开始切换当前显示的独立 QQ 会话
    qq_session_node_t * session = qq_chat_get_session(peer, 1);                    //new+ 获取或按需创建目标会话缓存
    qq_message_node_t * message;                                                   //new+ 保存重绘历史记录时的当前消息节点
    int private_chat;                                                              //new+ 标记目标是否为联系人私聊会话
    if(session == NULL) return;                                                     //new+ 内存不足无法创建会话时保留当前界面
    if(session == qq_active_session) {                                              //new+ 重复点击当前名片时不清空正在编辑的消息草稿
        qq_chat_update_session_styles();                                            //new+ 仍刷新一次选中样式以修复外部状态变化
        return;                                                                     //new+ 当前会话无需删除和重绘相同历史记录
    }                                                                              //new+ 结束重复点击当前会话的快速返回处理
    qq_chat_set_keyboard_visible(0);                                               //new+ 切换会话前收起键盘并恢复完整消息区域
    if(qq_chat_input != NULL && lv_obj_is_valid(qq_chat_input)) {                   //new+ 输入框仍有效时清除上一会话尚未发送的草稿
        lv_textarea_set_text(qq_chat_input, "");                                  //new+ 防止为联系人甲编辑的文字被误发给联系人乙
        lv_obj_remove_state(qq_chat_input, LV_STATE_FOCUSED);                       //new+ 移除输入焦点避免键盘在切换后被焦点事件再次打开
    }                                                                              //new+ 结束切换会话时的输入框清理
    qq_active_session = session;                                                   //new+ 使用稳定会话节点保存当前页面而非联系人节点地址
    session->unread_count = 0;                                                     //new+ 打开会话后将其中所有消息标记为已读
    private_chat = session->peer[0] != '\0';                                      //new+ 非空会话键代表一对一私聊
    if(qq_chat_title_label != NULL && lv_obj_is_valid(qq_chat_title_label)) {      //new+ 标题标签存在时显示群聊名或联系人昵称
        lv_label_set_text(qq_chat_title_label, private_chat ? session->peer : "Chat Group"); //new+ 将右侧主标题与当前会话同步
    }                                                                              //new+ 结束 QQ 会话主标题更新
    if(qq_chat_subtitle_label != NULL && lv_obj_is_valid(qq_chat_subtitle_label)) { //new+ 副标题标签存在时显示当前通信类型或离线状态
        lv_label_set_text(qq_chat_subtitle_label,                                  //new+ 群聊、在线私聊和离线私聊使用不同说明
                          !private_chat ? "TCP GROUP CHAT" :                      //new+ 公共会话继续显示 TCP 群聊说明
                          (qq_contact_find(session->peer) != NULL ? "PRIVATE CHAT" : "OFFLINE")); //new+ 私聊根据联系人在线状态显示说明
    }                                                                              //new+ 结束 QQ 会话副标题更新
    if(qq_chat_header_icon_label != NULL && lv_obj_is_valid(qq_chat_header_icon_label)) { //new+ 头像图标存在时匹配群聊或私聊类型
        lv_label_set_text(qq_chat_header_icon_label, private_chat ? LV_SYMBOL_ENVELOPE : LV_SYMBOL_HOME); //new+ 私聊显示信封，群聊显示 HOME
    }                                                                              //new+ 结束 QQ 标题栏会话图标更新
    qq_chat_update_session_styles();                                               //new+ 更新左侧群聊及所有联系人名片的选中状态
    if(private_chat) qq_chat_update_contact_status(session->peer);                 //new+ 清零未读后立即恢复该联系人 ONLINE 状态
    if(qq_chat_message_list == NULL || !lv_obj_is_valid(qq_chat_message_list)) return; //new+ 消息区域尚未建立时只更新会话状态
    lv_obj_clean(qq_chat_message_list);                                            //new+ 删除上一会话的气泡但保留外层列表样式和缓存
    message = session->head;                                                       //new+ 从目标会话最早保留的消息开始重绘
    while(message != NULL) {                                                       //new+ 按原始接收顺序恢复当前会话全部消息
        qq_chat_draw_message(message->is_system ? "SYSTEM" : message->sender,      //new+ 系统记录恢复居中样式，普通记录显示昵称
                             message->text, message->is_self, message->is_system); //new+ 使用缓存中的显式类型和方向恢复消息布局
        message = message->next;                                                   //new+ 继续重绘目标会话下一条消息
    }                                                                              //new+ 当前会话全部缓存消息已经恢复到界面
}                                                                                 //new+ 结束 QQ 会话切换函数

/* ===== 处理接收到的聊天室数据包 ===== */
static int qq_chat_process_received_packet(void)         // 解析缓冲区内已经接收完整的一条固定长度协议包
{
    qq_chat_message_header_t wire_header;                // 临时保存仍处于网络字节序的协议头
    memcpy(&wire_header, qq_chat_receive_buffer, sizeof(wire_header)); // 从原始字节缓冲区安全复制协议头

    uint32_t magic = ntohl(wire_header.magic);           // 将协议魔数从网络字节序转换为本机字节序
    uint32_t version = ntohl(wire_header.version);       // 将协议版本从网络字节序转换为本机字节序
    uint32_t type = ntohl(wire_header.type);             // 将消息类型从网络字节序转换为本机字节序
    uint32_t body_length = ntohl(wire_header.body_length); // 将消息体长度从网络字节序转换为本机字节序
    if(magic != QQ_CHAT_MAGIC || version != QQ_CHAT_VERSION || // 魔数或版本不一致说明收到的数据不属于当前协议
       body_length != sizeof(qq_chat_message_body_t)) {  // 校验魔数、版本和固定消息体大小，拒绝错误协议数据
        return -1;                                       // 包结构无效，接收回调将提示错误并断开连接
    }

    qq_chat_message_body_t body;                         // 创建解析后的消息体对象
    memcpy(&body,                                        // 从完整收包缓冲区中提取固定长度消息体
           qq_chat_receive_buffer + sizeof(qq_chat_message_header_t), // 源地址跳过前面的固定协议头
           sizeof(body));                                // 跳过协议头，将剩余固定字节复制为发送者和正文
    body.sender[sizeof(body.sender) - 1] = '\0';         // 强制昵称最后一字节为字符串结束符，避免越界读取
    body.receiver[sizeof(body.receiver) - 1] = '\0';     //new+ 强制私聊接收者字段结束，后续比较昵称时不会越界读取
    body.text[sizeof(body.text) - 1] = '\0';             // 强制正文最后一字节为字符串结束符，避免越界读取

    if(type == QQ_MSG_USER_LIST_BEGIN) {                 //new+ 服务端开始发送一份新的完整在线用户快照
        qq_contact_clear();                              //new+ 先清除旧连接或旧快照留下的联系人，避免显示重复名片
    }
    else if(type == QQ_MSG_USER_LIST_ITEM) {             //new+ 当前包的 sender 字段保存一个在线用户昵称
        qq_contact_add(body.sender);                     //new+ 排除自己和重复项后创建该在线用户的联系人名片
    }
    else if(type == QQ_MSG_USER_LIST_END) {              //new+ 完整在线用户列表已经同步结束
        if(qq_active_session != NULL && qq_active_session->peer[0] != '\0' && //new+ 当前仍停留在重连前的联系人私聊时检查其是否在线
           qq_contact_find(qq_active_session->peer) == NULL) {      //new+ 完整快照结束仍找不到目标说明该联系人已经离线
            qq_chat_switch_session("");                            //new+ 确认目标不在线后再切回群聊，避免列表重建期间误切换
        }                                                           //new+ 结束完整在线列表后的私聊目标校验
        else if(qq_active_session != NULL && qq_active_session->peer[0] != '\0') { //new+ 目标仍在线时让列表中新建的名片恢复当前私聊选中样式
            qq_chat_switch_session(qq_active_session->peer);         //new+ 重复切换同一会话只刷新样式，不清空输入草稿或重绘历史
        }                                                            //new+ 结束在线列表重建后的当前私聊样式恢复
    }
    else if(type == QQ_MSG_USER_JOIN) {                  //new+ 其他客户端在初始列表同步之后成功上线
        qq_contact_add(body.sender);                     //new+ 通过服务器提供的结构化昵称添加对应在线名片
        if(body.text[0] != '\0') qq_chat_add_message("SYSTEM", body.text, 0); //new+ 同时保留原有用户上线系统提示
    }
    else if(type == QQ_MSG_USER_LEAVE) {                 //new+ 其他客户端主动退出或异常断开连接
        qq_contact_remove(body.sender);                  //new+ 使用结构化昵称精确删除该用户的在线名片
        if(body.text[0] != '\0') qq_chat_add_message("SYSTEM", body.text, 0); //new+ 同时在聊天区显示用户离线原因
    }
    else if(type == QQ_MSG_DATA) {                       // 普通 DATA 类型代表其他客户端发送的文字消息
        if(strcmp(body.sender, qq_chat_user_name) != 0) { // 服务端若回送本机消息则忽略，防止界面重复显示
            qq_chat_add_message(body.sender, body.text, 0); // 将其他客户端消息以左侧深色气泡显示
        }
    }
    else if(type == QQ_MSG_PRIVATE) {                    //new+ 私聊包只归入发送者与接收者共同对应的一对一会话
        const char * peer = NULL;                        //new+ 保存除本机之外的另一方昵称作为私聊会话键
        int is_self = 0;                                 //new+ 标记该包是否为服务器回送给发送方的成功确认
        qq_session_node_t * session;                     //new+ 保存本条私聊需要写入的独立会话缓存
        if(strcmp(body.sender, qq_chat_user_name) == 0 && body.receiver[0] != '\0') { //new+ sender 是本机表示已成功发送给 receiver
            peer = body.receiver;                        //new+ 使用接收者昵称定位发送方看到的专属私聊窗口
            is_self = 1;                                 //new+ 服务器确认消息在界面右侧显示为自己的蓝色气泡
        }                                                //new+ 结束本机发送确认包识别
        else if(strcmp(body.receiver, qq_chat_user_name) == 0 && body.sender[0] != '\0') { //new+ receiver 是本机表示收到其他联系人私聊
            peer = body.sender;                          //new+ 使用真实发送者昵称定位接收方看到的专属私聊窗口
        }                                                //new+ 结束其他联系人发给本机的私聊识别
        if(peer != NULL && strcmp(peer, qq_chat_user_name) != 0) { //new+ 只处理确实与本机相关且另一方不是自己的私聊包
            if(qq_contact_find(peer) == NULL) qq_contact_add(peer); //new+ 异常时序下缺少名片时按服务器私聊包补建在线联系人
            session = qq_chat_get_session(peer, 1);      //new+ 获取或创建该联系人独享的消息历史缓存
            qq_chat_append_session_message(session, body.sender, body.text,        //new+ 保存服务端认证过的发送者和私聊正文
                                           is_self, 0, !is_self);                   //new+ 收到的后台私聊计未读，自己的确认包不计未读
        }                                                //new+ 结束与本机相关私聊包的缓存和显示处理
    }
    else if(type == QQ_MSG_ERROR && body.receiver[0] != '\0') { //new+ 带接收者的错误属于某次具体私聊发送结果
        qq_session_node_t * session = qq_chat_get_session(body.receiver, 1); //new+ 按失败目标找到对应私聊而不污染群聊历史
        qq_chat_append_session_message(session, "SYSTEM", body.text, 0, 1, 1); //new+ 在目标私聊中保存居中错误提示并在后台标记未读
    }
    else if(type == QQ_MSG_JOIN || type == QQ_MSG_QUIT || // 用户加入或离开的通知进入系统消息分支
            type == QQ_MSG_SYSTEM || type == QQ_MSG_ERROR) { //new+ 未指定私聊目标的错误仍作为公共系统提示处理
        qq_chat_add_message("SYSTEM", body.text, 0);    // 将服务器提示以居中的灰色文字显示
    }
    return 0;                                            // 数据包格式正确并已完成对应的界面处理
}

/* ===== 定时接收其他客户端的聊天消息 ===== */
static void qq_chat_receive_timer_cb(lv_timer_t * timer) // 每次定时器触发时尝试以非阻塞方式读取服务器数据
{
    LV_UNUSED(timer);                                    // 当前不使用定时器对象本身，只利用其周期触发机制
    if(!qq_chat_connected || qq_chat_socket_fd == QQ_CHAT_INVALID_SOCKET) return; //new+ 离线或句柄无效时不调用 recv

    const size_t packet_size = sizeof(qq_chat_receive_buffer); // 一个完整协议包等于固定协议头与固定消息体之和
    while(qq_chat_receive_length < packet_size) {        // 持续读取直到拼成完整包或当前暂时没有更多数据
        qq_chat_io_result_t received = recv(qq_chat_socket_fd, //new+ 使用平台对应返回类型读取当前协议包尚未接收的部分
                                (char *)qq_chat_receive_buffer + qq_chat_receive_length, // 将新数据追加到已收数据后方
                                (int)(packet_size - qq_chat_receive_length), //new+ 将剩余长度转换为 WinSock 接受的整数参数
                                MSG_DONTWAIT);            // 从已接收位置继续写入，并避免等待网络时阻塞 LVGL 界面
        if(received > 0) {                               // 正数表示本次成功收到了部分或全部数据
            qq_chat_receive_length += (size_t)received;  // 累计当前协议包已经接收的字节数
            if(qq_chat_receive_length == packet_size) {  // 只有完整收到固定长度数据包后才进行协议解析
                if(qq_chat_process_received_packet() != 0) { // 校验并处理消息，非 0 表示协议包无效
                    qq_chat_add_message("SYSTEM", "Invalid packet received", 0); // 在界面显示数据格式错误提示
                    qq_chat_client_disconnect(0);        // 无效包可能导致数据边界错乱，因此直接关闭当前连接
                    return;                              // 停止本轮接收，等待用户下次发送时重新连接
                }
                qq_chat_receive_length = 0;              // 当前完整包处理完毕，缓冲区从头接收下一条消息
            }
            continue;                                    // 如果套接字仍有数据则继续读取，提高一次定时回调的处理效率
        }
        if(received == 0) {                              // recv 返回 0 表示服务端已经正常关闭 TCP 连接
            qq_chat_add_message("SYSTEM", "Server disconnected", 0); // 告知用户服务器连接已断开
            qq_chat_client_disconnect(0);                // 清理套接字和本地连接状态，不再发送 QUIT
            return;                                      // 停止继续读取已经关闭的连接
        }
        int socket_error = QQ_CHAT_LAST_SOCKET_ERROR();  //new+ 立即保存当前平台的接收错误码，避免后续调用覆盖它
        if(socket_error == QQ_CHAT_INTERRUPTED) continue; //new+ 接收被中断时继续尝试读取当前协议包
        if(QQ_CHAT_WOULD_BLOCK(socket_error)) break;     //new+ 非阻塞套接字暂无数据时等待下一次 LVGL 定时器

        qq_chat_add_message("SYSTEM", "Receive failed", 0); // 其余网络错误在聊天区域显示失败提示
        qq_chat_client_disconnect(0);                    // 发生不可恢复的接收错误后关闭连接并清空缓存
        return;                                          // 结束本轮回调，防止继续使用异常套接字
    }
}

/* ===== 显示或隐藏聊天室软键盘 ===== */
static void qq_chat_set_keyboard_visible(int visible)    // 根据 visible 参数切换键盘以及消息区、输入区的布局
{
    if(qq_chat_keyboard == NULL || qq_chat_input_area == NULL || // 软键盘或输入区无效时不能切换布局
       qq_chat_message_list == NULL) return;             // 任一关键控件尚未创建或已经删除时不执行界面操作

    if(visible) {                                        // 非 0 表示用户准备输入，需要弹出软键盘
        lv_obj_set_height(qq_chat_message_list, 134);    // 缩短消息列表，为下半屏键盘预留垂直空间
        lv_obj_set_y(qq_chat_input_area, 218);           // 将输入区域上移到键盘正上方，输入时仍可见
        lv_keyboard_set_textarea(qq_chat_keyboard, qq_chat_input); // 将键盘输入目标绑定到消息文本框
        lv_obj_remove_flag(qq_chat_keyboard, LV_OBJ_FLAG_HIDDEN); // 清除隐藏标志，使键盘参与绘制
        lv_obj_move_foreground(qq_chat_keyboard);        // 把键盘移至同级对象最上层，避免被其他控件遮挡
    }
    else {                                               // 0 表示取消输入或需要收起软键盘
        lv_keyboard_set_textarea(qq_chat_keyboard, NULL); // 先解除键盘与文本框的绑定，避免隐藏后继续输入
        lv_obj_add_flag(qq_chat_keyboard, LV_OBJ_FLAG_HIDDEN); // 添加隐藏标志，不再绘制也不接收点击
        lv_obj_set_height(qq_chat_message_list, 380);    // 恢复聊天记录区域的正常完整高度
        lv_obj_set_y(qq_chat_input_area, 464);           // 将底部输入区域恢复到窗口底部
    }
}

/* ===== QQ 聊天室返回按钮回调 ===== */
static void qq_chat_back_cb(lv_event_t * e)              // 点击左上角返回按钮时退出并彻底清理聊天室资源
{
    LV_UNUSED(e);                                        // 返回流程不需要读取具体事件信息

    qq_chat_client_disconnect(1);                        // 主动退出时先发送 QUIT，再关闭 TCP 套接字
    if(qq_chat_receive_timer != NULL) {                  // 确认收包定时器存在后再删除
        lv_timer_delete(qq_chat_receive_timer);          // 停止每 100 毫秒触发的网络接收回调
        qq_chat_receive_timer = NULL;                    // 清空指针，防止下次进入或退出时重复删除
    }

    lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_HIDDEN);    // 重新显示进入聊天室之前隐藏的主界面
    lv_obj_delete_async(qq_chat_win);                    // 延迟到 LVGL 安全时机删除聊天室窗口及其全部子控件
    qq_chat_win = NULL;                                  // 清空窗口引用，避免访问已经安排删除的对象
    qq_chat_message_list = NULL;                         // 清空聊天记录列表引用
    qq_chat_input = NULL;                                // 清空消息输入框引用
    qq_chat_title_label = NULL;                          // 清空会话标题标签引用
    qq_chat_status_label = NULL;                         // 清空连接状态标签引用
    qq_chat_input_area = NULL;                           // 清空底部输入区域引用
    qq_chat_keyboard = NULL;                             // 清空软键盘引用
    qq_private_contact_list = NULL;                      //new+ 父窗口删除后清空动态联系人列表引用，防止再次访问
    qq_group_card = NULL;                                //new+ 清空已经随聊天室窗口删除的群聊名片引用
    qq_chat_subtitle_label = NULL;                       //new+ 清空已经删除的会话类型副标题引用
    qq_chat_header_icon_label = NULL;                    //new+ 清空已经删除的标题栏图标引用
    qq_chat_clear_sessions();                            //new+ 释放群聊和所有私聊的独立消息缓存，避免再次登录串历史
    memset(qq_name, 0, sizeof(qq_name));                 // 清空 QQ 名字
    memset(qq_IP_storage, 0, sizeof(qq_IP_storage));     //new+ 清空 QQ 连接 IP 地址的固定缓冲区
    qq_IP = qq_IP_storage;                               //new+ 退出后仍让 IP 指针保持指向有效缓冲区
    qq_Port = 0;                                         // 清空 QQ 连接端口
}

/* ===== QQ 聊天室输入框回调 ===== */
static void qq_chat_input_cb(lv_event_t * e)             // 统一响应文本框聚焦和软键盘确认、取消事件
{
    lv_event_code_t code = lv_event_get_code(e);         // 读取本次 LVGL 回调对应的事件类型
    if(code == LV_EVENT_FOCUSED || code == LV_EVENT_PRESSED || code == LV_EVENT_CLICKED) { // 聚焦、按下或点击输入控件都应打开键盘
        qq_chat_set_keyboard_visible(1);                 // 调整布局并将软键盘置于界面最上层
    }
    else if(code == LV_EVENT_DEFOCUSED) {                //new+ 点击键盘外部的可聚焦对象时输入框会失焦
        qq_chat_set_keyboard_visible(0);                 //new+ 输入框失焦后收起键盘并保留尚未发送的文字
    }                                                    //new+ 结束点击键盘外部区域的收起处理
    else if(code == LV_EVENT_READY) {                    // 软键盘确认键会产生 READY 事件
        qq_chat_send_cb(e);                              // 将确认操作等价为点击 SEND 按钮并发送当前正文
    }
    else if(code == LV_EVENT_CANCEL) {                   // 软键盘取消或关闭键会产生 CANCEL 事件
        qq_chat_set_keyboard_visible(0);                 // 收起键盘并恢复消息列表和输入区原始布局
    }
}

/* ===== QQ 聊天室发送消息回调 ===== */
static void qq_chat_send_cb(lv_event_t * e)              // 发送按钮与键盘确认键共同使用的发送处理函数
{
    qq_session_node_t * session;                         //new+ 保存点击发送时的当前群聊或私聊会话
    int private_chat;                                    //new+ 标记当前消息是否需要服务器定向转发
    LV_UNUSED(e);                                        // 发送逻辑不依赖触发来源，因此忽略事件对象
    if(qq_chat_input == NULL) return;                    // 输入框不存在时直接返回，避免访问无效 LVGL 对象

    const char * text = lv_textarea_get_text(qq_chat_input); // 获取当前文本框内部保存的消息字符串
    if(text == NULL || text[0] == '\0') return;          // 空指针或空字符串不发送，也不创建空气泡
    session = qq_active_session;                         //new+ 锁定用户点击发送瞬间选中的会话目标
    if(session == NULL) {                                //new+ 防御界面初始化异常导致当前会话尚未建立的情况
        qq_chat_switch_session("");                     //new+ 自动建立并切回公共群聊作为可靠默认发送目标
        session = qq_active_session;                     //new+ 重新读取刚创建的公共群聊会话
    }                                                    //new+ 结束缺失当前会话时的恢复处理
    if(session == NULL) return;                          //new+ 内存不足仍无法创建会话时保留输入正文并停止发送
    private_chat = session->peer[0] != '\0';            //new+ 非空会话键表示当前正在与指定联系人私聊

    if(!qq_chat_connected && qq_chat_client_connect() != 0) { // 若之前离线，则在发送前自动尝试重新连接服务器
        qq_chat_append_session_message(qq_active_session != NULL ? qq_active_session : session, //new+ 将连接失败写入当前仍可见的会话
                                       "SYSTEM", "Unable to connect to chat server", 0, 1, 0); //new+ 使用居中系统提示且不产生额外未读数
        return;                                          // 保留输入框正文，便于服务器恢复后再次发送
    }

    if((private_chat && qq_chat_send_private_packet(session->peer, text) != 0) || //new+ 私聊将当前联系人写入 receiver 字段执行定向发送
       (!private_chat && qq_chat_send_packet(QQ_MSG_DATA, text) != 0)) { //new+ 群聊继续使用原有 DATA 广播协议
        qq_chat_append_session_message(session, "SYSTEM", "Message send failed", 0, 1, 0); //new+ 将失败提示保存在本次实际发送的会话中
        qq_chat_client_disconnect(0);                    // 发送错误说明连接不可再信任，清理套接字等待重连
        return;                                          // 不清空正文，使用户可以再次尝试发送
    }

    if(!private_chat) qq_chat_add_message(qq_chat_user_name, text, 1); //new+ 群聊没有服务器回送，因此成功写入后立即显示自己的气泡
    lv_textarea_set_text(qq_chat_input, "");            // 清空已成功发送的内容，准备输入下一条消息
    /* 私聊等待服务器回送 QQ_MSG_PRIVATE 确认后再显示，避免把离线目标误标为已发送。 */ //new+ 说明私聊不在发送回调中本地回显的原因
}

/* ===== QQ 登录输入框样式 ===== */ //new+ 标记 QQ 登录输入框公共样式区域
static void qq_login_style_textarea(lv_obj_t * textarea)                              //new+ 为 IP、端口和昵称输入框应用统一主题
{                                                                                     //new+ 开始设置 QQ 登录输入框样式
    lv_textarea_set_one_line(textarea, true);                                          //new+ 登录信息均使用单行输入模式
    lv_obj_set_style_bg_color(textarea, lv_color_hex(0x111A25), 0);                   //new+ 输入框使用深灰蓝背景以适配现有主题
    lv_obj_set_style_bg_opa(textarea, LV_OPA_COVER, 0);                                //new+ 输入框背景保持完全不透明以保证文字清晰
    lv_obj_set_style_border_color(textarea, lv_color_hex(0x30445A), 0);               //new+ 默认边框使用低对比度灰蓝色
    lv_obj_set_style_border_color(textarea, lv_color_hex(0x1687FF), LV_STATE_FOCUSED); //new+ 聚焦时用主题蓝色突出当前输入框
    lv_obj_set_style_border_width(textarea, 1, 0);                                     //new+ 输入框默认绘制一像素边框
    lv_obj_set_style_border_width(textarea, 2, LV_STATE_FOCUSED);                      //new+ 聚焦时加粗边框以提供明确反馈
    lv_obj_set_style_radius(textarea, 6, 0);                                           //new+ 输入框使用与项目一致的小圆角
    lv_obj_set_style_pad_all(textarea, 10, 0);                                         //new+ 输入文字与边框之间保留舒适间距
    lv_obj_set_style_text_color(textarea, lv_color_hex(0xFFFFFF), 0);                  //new+ 用户输入内容使用白色显示
    lv_obj_set_style_text_color(textarea, lv_color_hex(0x718196), LV_PART_TEXTAREA_PLACEHOLDER); //new+ 占位提示使用弱化灰蓝色
    lv_obj_set_style_text_font(textarea, &lv_font_montserrat_14, 0);                   //new+ 输入内容使用清晰的十四号字体
    lv_obj_set_style_bg_color(textarea, lv_color_hex(0x1687FF), LV_PART_CURSOR);       //new+ 文本光标使用主题蓝色便于识别
}                                                                                     //new+ 结束 QQ 登录输入框样式设置

/* ===== QQ 登录输入框事件 ===== */ //new+ 标记 QQ 登录输入框与软键盘交互区域
static void qq_login_textarea_cb(lv_event_t * e)                                       //new+ 点击或聚焦任意登录输入框时显示软键盘
{                                                                                     //new+ 开始处理 QQ 登录输入框事件
    lv_event_code_t code = lv_event_get_code(e);                                       //new+ 读取本次输入框事件类型
    if(code != LV_EVENT_FOCUSED && code != LV_EVENT_CLICKED) return;                   //new+ 仅处理聚焦和点击事件以避免重复动作
    lv_obj_t * textarea = lv_event_get_target(e);                                      //new+ 获取当前需要接收键盘输入的文本框
    lv_obj_t * keyboard = qq_login_keyboard;                                           //new+ 使用已经创建完成的 QQ 登录专用键盘全局引用
    if(keyboard == NULL) return;                                                       //new+ 键盘尚未创建完成时忽略输入事件以避免空指针访问
    lv_keyboard_set_textarea(keyboard, textarea);                                      //new+ 将软键盘输出绑定到当前文本框
    lv_keyboard_set_mode(keyboard, textarea == qq_login_name_ta ? LV_KEYBOARD_MODE_TEXT_LOWER : LV_KEYBOARD_MODE_NUMBER); //new+ 昵称使用字母键盘而 IP 和端口使用数字键盘
    lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN);                                  //new+ 清除隐藏标志使软键盘显示
    lv_obj_align(qq_login_panel, LV_ALIGN_TOP_MID, 0, 18);                             //new+ 键盘出现时将登录沙盒明确放到顶部并保持完整可见
    lv_obj_move_foreground(keyboard);                                                   //new+ 将键盘置于遮罩最上层防止被沙盒遮挡
}                                                                                     //new+ 结束 QQ 登录输入框事件处理

/* ===== QQ 登录软键盘事件 ===== */ //new+ 标记 QQ 登录软键盘确认和收起处理区域
static void qq_login_keyboard_cb(lv_event_t * e)                                       //new+ 响应键盘确认键和左下角收起键
{                                                                                     //new+ 开始处理 QQ 登录软键盘事件
    lv_event_code_t code = lv_event_get_code(e);                                       //new+ 获取软键盘当前发送的事件类型
    if(code != LV_EVENT_READY && code != LV_EVENT_CANCEL) 
        return;                                                                          //new+ 普通字符输入继续交由 LVGL 默认键盘逻辑处理
    // else if(code == LV_EVENT_CANCEL)
    // {
    //     lv_keyboard_set_textarea(qq_login_keyboard, NULL);
    //     lv_obj_add_flag(qq_login_keyboard, LV_OBJ_FLAG_HIDDEN);
    //     return;
    // }
    lv_keyboard_set_textarea(qq_login_keyboard, NULL);                                 //new+ 解除键盘与当前登录输入框的绑定
    lv_obj_add_flag(qq_login_keyboard, LV_OBJ_FLAG_HIDDEN);                            //new+ 确认或点击小键盘图标后收起软键盘
    lv_obj_center(qq_login_panel);                                                      //new+ 键盘收起后将登录沙盒恢复到屏幕中央
}                                                                                     //new+ 结束 QQ 登录软键盘事件处理

/* ===== 关闭 QQ 登录沙盒 ===== */ //new+ 标记 QQ 登录模态对象统一清理区域
static void qq_login_close(void)                                                       //new+ 删除遮罩并清空所有登录控件引用
{                                                                                     //new+ 开始清理 QQ 登录沙盒资源
    if(qq_login_win != NULL) lv_obj_delete_async(qq_login_win);                        //new+ 在 LVGL 安全时机删除遮罩及其全部子控件
    qq_login_win = NULL;                                                               //new+ 清空 QQ 登录遮罩引用防止重复访问
    qq_login_panel = NULL;                                                             //new+ 清空 QQ 登录沙盒面板引用
    qq_login_ip_ta = NULL;                                                             //new+ 清空服务器 IP 输入框引用
    qq_login_port_ta = NULL;                                                           //new+ 清空服务器端口输入框引用
    qq_login_name_ta = NULL;                                                           //new+ 清空聊天昵称输入框引用
    qq_login_keyboard = NULL;                                                          //new+ 清空 QQ 登录软键盘引用
    qq_login_error_label = NULL;                                                       //new+ 清空 QQ 登录错误提示标签引用
}                                                                                     //new+ 结束 QQ 登录沙盒资源清理

/* ===== 取消 QQ 登录 ===== */ //new+ 标记 QQ 登录取消按钮事件区域
static void qq_login_cancel_cb(lv_event_t * e)                                        //new+ 用户取消配置时保留并恢复下层主界面
{                                                                                     //new+ 开始处理 QQ 登录取消操作
    LV_UNUSED(e);                                                                      //new+ 取消操作不需要读取具体事件对象
    qq_login_close();                                                                  //new+ 关闭模态遮罩且不创建聊天室
}                                                                                     //new+ 结束 QQ 登录取消操作

/* ===== 提交 QQ 登录信息 ===== */ //new+ 标记 QQ 登录校验和保存区域
static void qq_login_submit_cb(lv_event_t * e)                                        //new+ 仅在三项信息合法时关闭沙盒并进入聊天室
{                                                                                     //new+ 开始校验 QQ 登录信息
    LV_UNUSED(e);                                                                      //new+ 提交逻辑直接读取三个全局文本框而不依赖事件对象
    const char * ip_text = lv_textarea_get_text(qq_login_ip_ta);                       //new+ 读取用户输入的服务器 IPv4 地址
    const char * port_text = lv_textarea_get_text(qq_login_port_ta);                   //new+ 读取用户输入的服务器端口文本
    const char * name_text = lv_textarea_get_text(qq_login_name_ta);                   //new+ 读取用户输入的聊天昵称
    struct in_addr parsed_address;                                                      //new+ 准备系统 IPv4 解析结果以严格验证地址格式
    char * port_end = NULL;                                                            //new+ 保存端口数字解析结束位置用于拒绝非数字字符
    long port_value = strtol(port_text, &port_end, 10);                                //new+ 将端口文本转换为长整型并保留完整性检查信息
    if(ip_text[0] == '\0' || port_text[0] == '\0' || name_text[0] == '\0') {          //new+ 任意必填项为空时保持沙盒并提示用户
        lv_label_set_text(qq_login_error_label, "IP, port and name are required");    //new+ 显示三项均为必填内容的错误提示
        return;                                                                         //new+ 阻止创建聊天室并继续等待用户补全信息
    }                                                                                   //new+ 结束必填项为空的判断
    if(inet_pton(AF_INET, ip_text, &parsed_address) != 1) {                             //new+ 使用系统解析器验证点分十进制 IPv4 地址
        lv_label_set_text(qq_login_error_label, "Enter a valid IPv4 address");        //new+ 提示用户修正服务器 IP 地址格式
        return;                                                                         //new+ IP 不合法时留在模态沙盒继续输入
    }                                                                                   //new+ 结束 IPv4 地址格式判断
    if(port_end == port_text || *port_end != '\0' || port_value < 1 || port_value > 65535) { //new+ 端口必须为一到六万五千五百三十五之间的纯数字
        lv_label_set_text(qq_login_error_label, "Port must be between 1 and 65535");   //new+ 显示合法端口范围提示
        return;                                                                         //new+ 端口不合法时阻止进入聊天室
    }                                                                                   //new+ 结束端口范围与格式判断
    snprintf(qq_IP_storage, sizeof(qq_IP_storage), "%s", ip_text);                    //new+ 将已校验 IP 复制到弹窗销毁后仍有效的固定缓冲区
    qq_IP = qq_IP_storage;                                                             //new+ 保持用户定义的 qq_IP 指针指向有效地址文本
    qq_Port = (int)port_value;                                                         //new+ 将已校验端口保存到用户定义的整型变量
    snprintf(qq_name, sizeof(qq_name), "%s", name_text);                              //new+ 将昵称保存到用户定义的固定字符数组
    snprintf(qq_chat_user_name, sizeof(qq_chat_user_name), "%.*s", QQ_CHAT_NAME_MAX - 1, qq_name); //new+ 按协议字段上限同步聊天室实际使用的发送者昵称
    qq_login_close();                                                                  //new+ 信息保存完成后关闭并释放登录模态沙盒
    lv_obj_add_flag(Main_win, LV_OBJ_FLAG_HIDDEN);                                     //new+ 仅在确认成功后隐藏主界面并切换到聊天室
    QQ_Chat_Interface();                                                               //new+ 三项信息完整合法后创建聊天室并尝试连接服务器
}                                                                                     //new+ 结束 QQ 登录信息提交处理


/* ===== QQ 登录界面 ======*/
static void QQ_login(void)                                                            //new+ 创建压暗主界面的 QQ 连接信息模态沙盒
{                                                                                     //new+ 开始创建 QQ 登录界面
    if(qq_login_win != NULL) {                                                         //new+ 已存在登录沙盒时避免重复创建多个遮罩
        lv_obj_move_foreground(qq_login_win);                                           //new+ 将现有登录遮罩重新置于最上层
        return;                                                                         //new+ 复用已有沙盒并结束本次重复入口调用
    }                                                                                   //new+ 结束重复创建保护判断
    qq_login_win = lv_obj_create(lv_screen_active());                                  //new+ 在活动屏幕创建覆盖全屏的模态遮罩
    lv_obj_set_size(qq_login_win, 1024, 600);                                          //new+ 遮罩尺寸与开发板屏幕分辨率完全一致
    lv_obj_set_pos(qq_login_win, 0, 0);                                                //new+ 遮罩从左上角覆盖整个下层主界面
    lv_obj_set_style_bg_color(qq_login_win, lv_color_hex(0x343A42), 0);               //new+ 使用中性灰色压暗下层界面
    lv_obj_set_style_bg_opa(qq_login_win, LV_OPA_70, 0);                               //new+ 保留主界面轮廓同时降低其视觉亮度
    lv_obj_set_style_border_width(qq_login_win, 0, 0);                                 //new+ 全屏遮罩不绘制边框
    lv_obj_set_style_radius(qq_login_win, 0, 0);                                       //new+ 全屏遮罩四角贴合屏幕不使用圆角
    lv_obj_set_style_pad_all(qq_login_win, 0, 0);                                      //new+ 清除遮罩内边距以便使用精确坐标布局
    lv_obj_remove_flag(qq_login_win, LV_OBJ_FLAG_SCROLLABLE);                          //new+ 禁止模态遮罩被手势滚动
    lv_obj_add_flag(qq_login_win, LV_OBJ_FLAG_CLICKABLE);                              //new+ 拦截触摸事件防止用户操作下层主界面
    qq_login_panel = lv_obj_create(qq_login_win);                                      //new+ 在灰色遮罩中央创建连接信息沙盒
    lv_obj_set_size(qq_login_panel, 520, 320);                                         //new+ 沙盒采用约五百乘三百的紧凑尺寸
    lv_obj_center(qq_login_panel);                                                     //new+ 未弹出键盘时将沙盒放在屏幕中央
    lv_obj_set_style_bg_color(qq_login_panel, lv_color_hex(0x0C1118), 0);             //new+ 沙盒背景沿用系统深色主题
    lv_obj_set_style_bg_opa(qq_login_panel, LV_OPA_COVER, 0);                          //new+ 沙盒完全不透明以清晰分隔表单内容
    lv_obj_set_style_border_color(qq_login_panel, lv_color_hex(0x2B8CFF), 0);         //new+ 使用主题蓝色描边强调当前模态操作
    lv_obj_set_style_border_width(qq_login_panel, 1, 0);                               //new+ 沙盒绘制一像素主题边框
    lv_obj_set_style_radius(qq_login_panel, 8, 0);                                     //new+ 沙盒使用项目允许的八像素圆角
    lv_obj_set_style_shadow_color(qq_login_panel, lv_color_hex(0x000000), 0);         //new+ 沙盒阴影使用纯黑色增强层级感
    lv_obj_set_style_shadow_opa(qq_login_panel, LV_OPA_50, 0);                         //new+ 使用适度阴影透明度避免视觉过重
    lv_obj_set_style_shadow_width(qq_login_panel, 24, 0);                              //new+ 扩展阴影范围使沙盒从压暗背景中浮起
    lv_obj_set_style_pad_all(qq_login_panel, 0, 0);                                    //new+ 清除沙盒默认内边距以精确排列字段
    lv_obj_remove_flag(qq_login_panel, LV_OBJ_FLAG_SCROLLABLE);                        //new+ 沙盒内容固定且不允许滚动
    lv_obj_t * title_mark = lv_obj_create(qq_login_panel);                             //new+ 创建标题左侧的主题蓝色标记
    lv_obj_set_size(title_mark, 4, 28);                                                //new+ 标题标记使用窄而清晰的固定尺寸
    lv_obj_set_pos(title_mark, 24, 18);                                                //new+ 将标题标记放在沙盒左上角
    lv_obj_set_style_bg_color(title_mark, lv_color_hex(0x1687FF), 0);                 //new+ 标题标记使用系统主题蓝色
    lv_obj_set_style_bg_opa(title_mark, LV_OPA_COVER, 0);                              //new+ 标题标记保持完全不透明
    lv_obj_set_style_border_width(title_mark, 0, 0);                                   //new+ 标题标记不绘制多余边框
    lv_obj_set_style_radius(title_mark, 2, 0);                                         //new+ 为标题标记添加轻微圆角
    lv_obj_remove_flag(title_mark, LV_OBJ_FLAG_SCROLLABLE);                            //new+ 标题标记仅作装饰且不响应滚动
    lv_obj_t * title = lv_label_create(qq_login_panel);                                //new+ 创建 QQ 连接沙盒主标题
    lv_label_set_text(title, "QQ CONNECTION");                                        //new+ 明确说明当前操作用于配置 QQ 连接
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);                     //new+ 主标题使用高对比度白色
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);                      //new+ 主标题使用二十号字体匹配紧凑弹窗
    lv_obj_set_pos(title, 40, 20);                                                     //new+ 主标题与蓝色标记水平对齐
    lv_obj_t * subtitle = lv_label_create(qq_login_panel);                             //new+ 创建连接信息填写说明副标题
    lv_label_set_text(subtitle, "Enter server details to continue");                 //new+ 提示完成表单后才能进入聊天室
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x718196), 0);                  //new+ 副标题使用弱化灰蓝色减少视觉竞争
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);                   //new+ 副标题采用十二号辅助文字
    lv_obj_set_pos(subtitle, 40, 47);                                                  //new+ 将副标题放在主标题正下方
    lv_obj_t * ip_label = lv_label_create(qq_login_panel);                             //new+ 创建服务器 IP 字段标签
    lv_label_set_text(ip_label, "SERVER IP");                                          //new+ 标识第一个输入框用于服务器 IPv4 地址
    lv_obj_set_style_text_color(ip_label, lv_color_hex(0x91A1B3), 0);                 //new+ IP 字段标签使用清晰的灰蓝色
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_12, 0);                   //new+ IP 字段标签采用十二号字体
    lv_obj_set_pos(ip_label, 24, 92);                                                  //new+ IP 标签与对应输入框垂直居中
    qq_login_ip_ta = lv_textarea_create(qq_login_panel);                               //new+ 创建服务器 IPv4 地址输入框
    lv_obj_set_size(qq_login_ip_ta, 374, 44);                                          //new+ IP 输入框为最长 IPv4 文本提供稳定空间
    lv_obj_set_pos(qq_login_ip_ta, 122, 78);                                           //new+ 将 IP 输入框放在第一行标签右侧
    lv_textarea_set_placeholder_text(qq_login_ip_ta, "192.168.1.10");                //new+ 用示例地址提示用户输入格式
    lv_textarea_set_accepted_chars(qq_login_ip_ta, "0123456789.");                   //new+ IP 输入仅接受数字和英文句点
    lv_textarea_set_max_length(qq_login_ip_ta, 15);                                    //new+ IPv4 地址最多允许十五个字符
    qq_login_style_textarea(qq_login_ip_ta);                                           //new+ 为 IP 输入框应用统一主题样式
    lv_obj_add_event_cb(qq_login_ip_ta, qq_login_textarea_cb, LV_EVENT_ALL, NULL);    //new+ 注册 IP 输入框事件并由回调读取全局专用键盘
    lv_obj_t * port_label = lv_label_create(qq_login_panel);                           //new+ 创建服务器端口字段标签
    lv_label_set_text(port_label, "PORT");                                             //new+ 标识第二个输入框用于 TCP 端口
    lv_obj_set_style_text_color(port_label, lv_color_hex(0x91A1B3), 0);               //new+ 端口字段标签使用灰蓝色
    lv_obj_set_style_text_font(port_label, &lv_font_montserrat_12, 0);                 //new+ 端口字段标签采用十二号字体
    lv_obj_set_pos(port_label, 24, 148);                                                //new+ 端口标签与第二行输入框垂直居中
    qq_login_port_ta = lv_textarea_create(qq_login_panel);                             //new+ 创建服务器 TCP 端口输入框
    lv_obj_set_size(qq_login_port_ta, 374, 44);                                        //new+ 端口输入框与其他字段保持统一尺寸
    lv_obj_set_pos(qq_login_port_ta, 122, 134);                                         //new+ 将端口输入框放在第二行标签右侧
    lv_textarea_set_placeholder_text(qq_login_port_ta, "8888");                      //new+ 使用项目默认端口作为格式示例
    lv_textarea_set_accepted_chars(qq_login_port_ta, "0123456789");                  //new+ 端口输入仅允许十进制数字
    lv_textarea_set_max_length(qq_login_port_ta, 5);                                    //new+ TCP 端口文本最多保留五位数字
    qq_login_style_textarea(qq_login_port_ta);                                         //new+ 为端口输入框应用统一主题样式
    lv_obj_add_event_cb(qq_login_port_ta, qq_login_textarea_cb, LV_EVENT_ALL, NULL);  //new+ 注册端口输入框事件并由回调读取全局专用键盘
    lv_obj_t * name_label = lv_label_create(qq_login_panel);                           //new+ 创建聊天昵称字段标签
    lv_label_set_text(name_label, "NAME");                                             //new+ 标识第三个输入框用于聊天室显示名称
    lv_obj_set_style_text_color(name_label, lv_color_hex(0x91A1B3), 0);               //new+ 昵称字段标签使用灰蓝色
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_12, 0);                 //new+ 昵称字段标签采用十二号字体
    lv_obj_set_pos(name_label, 24, 204);                                                //new+ 昵称标签与第三行输入框垂直居中
    qq_login_name_ta = lv_textarea_create(qq_login_panel);                             //new+ 创建聊天室昵称输入框
    lv_obj_set_size(qq_login_name_ta, 374, 44);                                        //new+ 昵称输入框与前两项保持对齐
    lv_obj_set_pos(qq_login_name_ta, 122, 190);                                         //new+ 将昵称输入框放在第三行标签右侧
    lv_textarea_set_placeholder_text(qq_login_name_ta, "Your display name");          //new+ 提示用户填写聊天中显示的昵称
    lv_textarea_set_max_length(qq_login_name_ta, QQ_CHAT_NAME_MAX - 1);                //new+ 昵称长度与聊天协议固定字段保持一致
    qq_login_style_textarea(qq_login_name_ta);                                         //new+ 为昵称输入框应用统一主题样式
    lv_obj_add_event_cb(qq_login_name_ta, qq_login_textarea_cb, LV_EVENT_ALL, NULL);  //new+ 注册昵称输入框事件并由回调读取全局专用键盘
    qq_login_error_label = lv_label_create(qq_login_panel);                            //new+ 创建表单校验错误提示标签
    lv_label_set_text(qq_login_error_label, "");                                       //new+ 初次打开时不显示任何错误内容
    lv_obj_set_width(qq_login_error_label, 260);                                        //new+ 限定提示宽度避免覆盖右侧操作按钮
    lv_label_set_long_mode(qq_login_error_label, LV_LABEL_LONG_DOT);                   //new+ 极端长提示自动省略以保持布局稳定
    lv_obj_set_style_text_color(qq_login_error_label, lv_color_hex(0xF26B6B), 0);      //new+ 错误内容使用醒目的柔和红色
    lv_obj_set_style_text_font(qq_login_error_label, &lv_font_montserrat_12, 0);       //new+ 错误提示使用十二号辅助字体
    lv_obj_set_pos(qq_login_error_label, 24, 278);                                      //new+ 将错误提示固定在沙盒底部左侧
    lv_obj_t * cancel_btn = lv_button_create(qq_login_panel);                          //new+ 创建返回主界面的取消按钮
    lv_obj_set_size(cancel_btn, 88, 40);                                                //new+ 取消按钮提供足够的触控尺寸
    lv_obj_set_pos(cancel_btn, 306, 266);                                               //new+ 将取消按钮放在沙盒底部右侧区域
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x1A2634), 0);                 //new+ 取消按钮使用次要深灰蓝背景
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x25384D), LV_STATE_PRESSED);   //new+ 按下时提亮取消按钮提供反馈
    lv_obj_set_style_border_color(cancel_btn, lv_color_hex(0x405267), 0);             //new+ 取消按钮使用低调灰蓝色边框
    lv_obj_set_style_border_width(cancel_btn, 1, 0);                                   //new+ 取消按钮绘制一像素边框
    lv_obj_set_style_radius(cancel_btn, 6, 0);                                         //new+ 取消按钮使用六像素圆角
    lv_obj_set_style_shadow_width(cancel_btn, 0, 0);                                   //new+ 移除取消按钮默认阴影保持界面克制
    lv_obj_t * cancel_label = lv_label_create(cancel_btn);                             //new+ 创建取消按钮文字标签
    lv_label_set_text(cancel_label, "CANCEL");                                         //new+ 使用明确命令文字表示退出配置
    lv_obj_set_style_text_color(cancel_label, lv_color_hex(0xD7E0EA), 0);             //new+ 取消文字使用柔和白色
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_12, 0);               //new+ 取消按钮文字使用十二号字体
    lv_obj_center(cancel_label);                                                       //new+ 将取消文字居中到按钮内部
    lv_obj_add_event_cb(cancel_btn, qq_login_cancel_cb, LV_EVENT_CLICKED, NULL);       //new+ 点击取消时关闭沙盒但不进入聊天室
    lv_obj_t * connect_btn = lv_button_create(qq_login_panel);                         //new+ 创建提交连接信息的主要按钮
    lv_obj_set_size(connect_btn, 96, 40);                                               //new+ 连接按钮尺寸略大于次要操作按钮
    lv_obj_set_pos(connect_btn, 400, 266);                                              //new+ 将连接按钮固定在沙盒右下角
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x086CD9), 0);                //new+ 连接按钮使用项目主题蓝色
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x0454AD), LV_STATE_PRESSED);  //new+ 按下时切换为深蓝色反馈
    lv_obj_set_style_border_color(connect_btn, lv_color_hex(0x2B8CFF), 0);            //new+ 连接按钮使用亮蓝色描边
    lv_obj_set_style_border_width(connect_btn, 1, 0);                                  //new+ 连接按钮绘制一像素边框
    lv_obj_set_style_radius(connect_btn, 6, 0);                                        //new+ 连接按钮使用六像素圆角
    lv_obj_set_style_shadow_width(connect_btn, 0, 0);                                  //new+ 移除连接按钮默认阴影保持扁平风格
    lv_obj_t * connect_label = lv_label_create(connect_btn);                           //new+ 创建连接按钮文字和箭头标签
    lv_label_set_text(connect_label, "ENTER  " LV_SYMBOL_RIGHT);                       //new+ 使用进入文字和右箭头强调下一步动作
    lv_obj_set_style_text_color(connect_label, lv_color_hex(0xFFFFFF), 0);             //new+ 主要按钮文字使用白色形成清晰对比
    lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_12, 0);              //new+ 连接按钮文字使用十二号字体
    lv_obj_center(connect_label);                                                      //new+ 将连接按钮内容水平垂直居中
    lv_obj_add_event_cb(connect_btn, qq_login_submit_cb, LV_EVENT_CLICKED, NULL);      //new+ 点击后校验三项信息并决定是否进入聊天室
    qq_login_keyboard = lv_keyboard_create(qq_login_win);                              //new+ 为 QQ 登录沙盒创建独立 LVGL 软键盘
    lv_obj_set_size(qq_login_keyboard, 760, 230);                                      //new+ 键盘使用适合一零二四乘六百屏幕的稳定尺寸
    lv_obj_align(qq_login_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);                        //new+ 将键盘贴合屏幕底部并水平居中
    lv_obj_set_style_bg_color(qq_login_keyboard, lv_color_hex(0x080C12), 0);          //new+ 键盘背景适配现有深色主题
    lv_obj_set_style_bg_opa(qq_login_keyboard, LV_OPA_COVER, 0);                       //new+ 键盘背景完全不透明避免下层内容干扰
    lv_obj_set_style_border_color(qq_login_keyboard, lv_color_hex(0x202C3A), 0);      //new+ 键盘外框使用灰蓝色分隔线
    lv_obj_set_style_border_width(qq_login_keyboard, 1, 0);                            //new+ 键盘整体绘制一像素边框
    lv_obj_set_style_radius(qq_login_keyboard, 0, 0);                                  //new+ 底部贴边键盘不使用外部圆角
    lv_obj_set_style_pad_all(qq_login_keyboard, 8, 0);                                 //new+ 键帽与键盘外框之间保留八像素间距
    lv_obj_set_style_pad_row(qq_login_keyboard, 6, 0);                                 //new+ 键盘各行之间保留六像素间距
    lv_obj_set_style_pad_column(qq_login_keyboard, 6, 0);                              //new+ 同一行键帽之间保留六像素间距
    lv_obj_set_style_bg_color(qq_login_keyboard, lv_color_hex(0x152131), LV_PART_ITEMS); //new+ 普通键帽使用深灰蓝色背景
    lv_obj_set_style_bg_color(qq_login_keyboard, lv_color_hex(0x075DBD), LV_PART_ITEMS | LV_STATE_PRESSED); //new+ 键帽按下时使用主题蓝色
    lv_obj_set_style_text_color(qq_login_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS); //new+ 键盘字符统一使用白色
    lv_obj_set_style_text_font(qq_login_keyboard, &lv_font_montserrat_16, LV_PART_ITEMS); //new+ 键盘字符使用易读的十六号字体
    lv_obj_set_style_border_width(qq_login_keyboard, 0, LV_PART_ITEMS);                 //new+ 单个键帽不额外绘制边框
    lv_obj_set_style_radius(qq_login_keyboard, 6, LV_PART_ITEMS);                      //new+ 单个键帽使用六像素小圆角
    lv_obj_add_event_cb(qq_login_keyboard, qq_login_keyboard_cb, LV_EVENT_ALL, NULL);  //new+ 监听键盘确认和左下角收起事件
    lv_obj_add_flag(qq_login_keyboard, LV_OBJ_FLAG_HIDDEN);                            //new+ 初次显示沙盒时先隐藏软键盘
}                                                                                     //new+ 完成 QQ 登录模态沙盒创建


/* ===== 创建 QQ 聊天室界面 ===== */
void QQ_Chat_Interface(void)                             // 创建单聊天室 UI，并启动收包定时器与 TCP 客户端连接
{
    qq_chat_win = lv_obj_create(lv_screen_active());     // 在当前活动屏幕上创建 QQ 聊天室最外层窗口
    lv_obj_set_size(qq_chat_win, 1024, 600);             // 窗口尺寸与开发板 1024x600 显示屏完全一致
    lv_obj_set_pos(qq_chat_win, 0, 0);                   // 将聊天室窗口放置在屏幕左上角并覆盖整个屏幕
    lv_obj_set_style_bg_color(qq_chat_win, lv_color_hex(0x05070B), 0); // 设置接近黑色的全局主题背景
    lv_obj_set_style_bg_opa(qq_chat_win, LV_OPA_COVER, 0); // 背景完全不透明，遮住已隐藏主界面的内容
    lv_obj_set_style_border_width(qq_chat_win, 0, 0);    // 去掉 LVGL 对象默认边框，形成全屏界面
    lv_obj_set_style_radius(qq_chat_win, 0, 0);          // 全屏窗口不使用圆角，确保四角覆盖完整
    lv_obj_set_style_pad_all(qq_chat_win, 0, 0);         // 清除默认内边距，让子区域按绝对坐标准确布局
    lv_obj_remove_flag(qq_chat_win, LV_OBJ_FLAG_SCROLLABLE); // 禁止整个窗口滚动，只允许消息列表独立滚动

    /* 左侧联系人区域 */
    lv_obj_t * sidebar = lv_obj_create(qq_chat_win);     // 在左侧创建返回入口、品牌和唯一聊天室所在的侧栏
    lv_obj_set_size(sidebar, 284, 600);                  // 侧栏宽 284 像素、高度占满整个屏幕
    lv_obj_set_pos(sidebar, 0, 0);                       // 将侧栏固定在窗口最左侧
    lv_obj_set_style_bg_color(sidebar, lv_color_hex(0x080C12), 0); // 使用比主背景略亮的深黑色区分功能区域
    lv_obj_set_style_bg_opa(sidebar, LV_OPA_COVER, 0);   // 侧栏背景完全不透明
    lv_obj_set_style_border_color(sidebar, lv_color_hex(0x202C3A), 0); // 预设灰蓝分隔线颜色以适配主题
    lv_obj_set_style_border_width(sidebar, 0, 0);        // 当前不显示侧栏边框，保持界面简洁
    lv_obj_set_style_border_side(sidebar, LV_BORDER_SIDE_RIGHT, 0); // 若启用边框则只在侧栏右侧显示
    lv_obj_set_style_radius(sidebar, 0, 0);              // 贴合屏幕边缘的侧栏不设置圆角
    lv_obj_set_style_pad_all(sidebar, 0, 0);             // 清除侧栏内边距，内部控件使用绝对位置
    lv_obj_remove_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE); // 当前只有一个聊天室，无需侧栏滚动

    lv_obj_t * back_btn = lv_button_create(sidebar);     // 在侧栏顶部创建返回主界面的图标按钮
    lv_obj_set_size(back_btn, 44, 44);                   // 返回按钮采用稳定的 44x44 像素触控尺寸
    lv_obj_set_pos(back_btn, 20, 18);                    // 按钮距左侧 20、距顶部 18 像素
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x086CD9), 0); // 默认状态使用主题蓝色背景
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x0454AD), LV_STATE_PRESSED); // 按下时变为深蓝色提供触觉反馈
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0x2B8CFF), 0); // 使用亮蓝色描边增强按钮识别度
    lv_obj_set_style_border_width(back_btn, 1, 0);       // 返回按钮边框宽度设为 1 像素
    lv_obj_set_style_radius(back_btn, 8, 0);             // 使用 8 像素圆角匹配项目按钮风格
    lv_obj_set_style_shadow_width(back_btn, 0, 0);       // 取消默认阴影，保持黑底界面清爽
    lv_obj_t * back_icon = lv_label_create(back_btn);    // 在返回按钮内部创建用于显示箭头的标签
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);        // 使用 LVGL 内置向左箭头符号表示返回
    lv_obj_set_style_text_color(back_icon, lv_color_hex(0xFFFFFF), 0); // 箭头使用白色，与蓝色按钮形成高对比
    lv_obj_set_style_text_font(back_icon, &lv_font_montserrat_20, 0); // 将箭头字体设为 20 号以便触屏识别
    lv_obj_center(back_icon);                            // 将返回箭头水平和垂直居中到按钮内部
    lv_obj_add_event_cb(back_btn, qq_chat_back_cb, LV_EVENT_CLICKED, NULL); // 点击返回按钮时断开连接并恢复主界面

    lv_obj_t * brand = lv_label_create(sidebar);         // 创建侧栏顶部的 QQ CHAT 品牌标题
    lv_label_set_text(brand, "QQ CHAT");                // 设置聊天室英文标题文字
    lv_obj_set_style_text_color(brand, lv_color_hex(0xFFFFFF), 0); // 标题使用纯白色适配深色背景
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_24, 0); // 标题使用 24 号字体形成清晰层级
    lv_obj_set_pos(brand, 80, 27);                       // 标题放在返回按钮右侧并与按钮视觉居中

    qq_chat_status_label = lv_label_create(sidebar);     // 创建用于动态显示服务器连接状态的标签
    lv_label_set_text(qq_chat_status_label, "OFFLINE"); // 界面刚创建时默认先显示离线状态
    lv_obj_set_style_text_color(qq_chat_status_label, lv_color_hex(0xD95C5C), 0); // 离线状态使用红色提示
    lv_obj_set_style_text_font(qq_chat_status_label, &lv_font_montserrat_12, 0); // 状态使用较小的 12 号字体
    lv_obj_set_pos(qq_chat_status_label, 201, 34);       // 将状态放在品牌标题右侧并垂直对齐

    lv_obj_t * contact_caption = lv_label_create(sidebar); // 创建唯一聊天室列表上方的分组说明文字
    lv_label_set_text(contact_caption, "CHAT ROOM OR CONTACT PERSON");      // 明确左侧区域展示聊天室和其他联系人
    lv_obj_set_style_text_color(contact_caption, lv_color_hex(0x718196), 0); // 使用灰蓝色降低分组标题视觉权重
    lv_obj_set_style_text_font(contact_caption, &lv_font_montserrat_12, 0); // 分组说明使用 12 号字体
    lv_obj_set_pos(contact_caption, 20, 92);              // 放置在顶部标题下方并与聊天室卡片左侧对齐

    lv_obj_t * contact = lv_button_create(sidebar);       // 创建当前界面唯一的 Class Group 聊天室卡片
    qq_group_card = contact;                              //new+ 保存群聊名片以便切换会话时更新选中状态
    lv_obj_set_size(contact, 244, 76);                    // 聊天室卡片占据侧栏内部宽度并提供足够触控高度
    lv_obj_set_pos(contact, 20, 116);                     // 卡片与侧栏左右各保留 20 像素空间
    lv_obj_set_style_bg_color(contact, lv_color_hex(0x0B2D50), 0); // 默认使用深蓝背景表示当前聊天室已选中
    lv_obj_set_style_bg_color(contact, lv_color_hex(0x123E67), LV_STATE_PRESSED); // 按下时稍微提亮背景提供反馈
    lv_obj_set_style_bg_opa(contact, LV_OPA_COVER, 0);    // 卡片背景完全不透明
    lv_obj_set_style_border_color(contact, lv_color_hex(0x1687FF), 0); // 使用主题亮蓝色强调唯一的活动会话
    lv_obj_set_style_border_width(contact, 1, 0);         // 卡片显示 1 像素选中边框
    lv_obj_set_style_radius(contact, 6, 0);               // 卡片使用 6 像素小圆角
    lv_obj_set_style_shadow_width(contact, 0, 0);         // 去掉按钮默认阴影以保持扁平风格
    lv_obj_add_event_cb(contact, qq_group_card_cb, LV_EVENT_CLICKED, NULL); //new+ 点击群聊名片时从联系人私聊切回公共会话

    lv_obj_t * avatar = lv_obj_create(contact);           // 在聊天室卡片左侧创建群聊头像底板
    lv_obj_set_size(avatar, 44, 44);                      // 群聊头像固定为 44x44 像素
    lv_obj_align(avatar, LV_ALIGN_LEFT_MID, -8, 0);       // 头像相对卡片左侧垂直居中
    lv_obj_set_style_bg_color(avatar, lv_color_hex(0x1687FF), 0); // 头像底板使用主题亮蓝色
    lv_obj_set_style_bg_opa(avatar, LV_OPA_COVER, 0);     // 头像背景完全不透明
    lv_obj_set_style_border_width(avatar, 0, 0);          // 头像不再额外绘制边框
    lv_obj_set_style_radius(avatar, 6, 0);                // 头像底板使用 6 像素小圆角
    lv_obj_set_style_pad_all(avatar, 0, 0);               // 清除头像容器内边距，便于图标精确居中
    lv_obj_remove_flag(avatar, LV_OBJ_FLAG_SCROLLABLE);   // 头像仅用于展示，不允许滚动
    lv_obj_remove_flag(avatar, LV_OBJ_FLAG_CLICKABLE);    //new+ 头像不截获点击，让整个群聊名片区域统一触发切换
    lv_obj_t * avatar_icon = lv_label_create(avatar);     // 在头像底板内部创建群聊图标标签
    lv_label_set_text(avatar_icon, LV_SYMBOL_HOME);       // 使用 HOME 图标代表班级公共聊天室
    lv_obj_set_style_text_color(avatar_icon, lv_color_hex(0xFFFFFF), 0); // 图标使用白色，与蓝色头像形成对比
    lv_obj_set_style_text_font(avatar_icon, &lv_font_montserrat_18, 0); // 设置图标为 18 号大小
    lv_obj_center(avatar_icon);                           // 将群聊图标在头像中完全居中
    lv_obj_remove_flag(avatar_icon, LV_OBJ_FLAG_CLICKABLE); //new+ 图标仅用于显示并将触控事件留给群聊名片

    lv_obj_t * contact_name = lv_label_create(contact);  // 创建聊天室卡片的主标题标签
    lv_label_set_text(contact_name, "Chat Group");      // 唯一聊天室名称固定为聊天群聊
    lv_obj_set_style_text_color(contact_name, lv_color_hex(0xFFFFFF), 0); // 会话名称使用白色突出显示
    lv_obj_set_style_text_font(contact_name, &lv_font_montserrat_16, 0); // 会话名称使用 16 号字体
    lv_obj_set_pos(contact_name, 48, -7);                 // 将名称放在头像右侧的上半区域
    lv_obj_remove_flag(contact_name, LV_OBJ_FLAG_CLICKABLE); //new+ 群聊名称不截获父名片的点击事件

    lv_obj_t * contact_message = lv_label_create(contact); // 创建聊天室卡片的功能副标题
    lv_label_set_text(contact_message, "TCP group chat"); // 说明该会话是基于 TCP 的简单群聊
    lv_obj_set_style_text_color(contact_message, lv_color_hex(0x8191A3), 0); // 副标题使用灰蓝色弱化显示
    lv_obj_set_style_text_font(contact_message, &lv_font_montserrat_12, 0); // 副标题使用 12 号字体
    lv_obj_set_pos(contact_message, 48, 18);              // 将副标题放在会话名称正下方
    lv_obj_remove_flag(contact_message, LV_OBJ_FLAG_CLICKABLE); //new+ 群聊副标题不截获父名片的点击事件

    qq_private_contact_list = lv_obj_create(sidebar);                    //new+ 在固定群聊名片下方创建动态在线联系人列表
    lv_obj_set_size(qq_private_contact_list, 244, 388);                   //new+ 使用侧栏剩余空间显示多个联系人并保留底部间距
    lv_obj_set_pos(qq_private_contact_list, 20, 202);                     //new+ 列表与群聊名片左侧对齐并从其下方开始
    lv_obj_set_style_bg_opa(qq_private_contact_list, LV_OPA_TRANSP, 0);   //new+ 使用透明背景延续侧栏整体黑色主题
    lv_obj_set_style_border_width(qq_private_contact_list, 0, 0);        //new+ 取消外层列表边框，避免形成多余嵌套卡片
    lv_obj_set_style_radius(qq_private_contact_list, 0, 0);              //new+ 列表属于页面区域，不使用装饰性圆角
    lv_obj_set_style_pad_all(qq_private_contact_list, 0, 0);             //new+ 动态名片与群聊名片保持相同的左右边缘
    lv_obj_set_style_pad_row(qq_private_contact_list, 8, 0);             //new+ 相邻联系人名片之间保留八像素间隔
    lv_obj_set_flex_flow(qq_private_contact_list, LV_FLEX_FLOW_COLUMN);  //new+ 新上线联系人按照垂直方向依次排列
    lv_obj_set_flex_align(qq_private_contact_list,                       //new+ 配置联系人列表的纵向 Flex 对齐规则
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START); //new+ 名片从顶部开始排列且保持固定宽高
    lv_obj_set_scroll_dir(qq_private_contact_list, LV_DIR_VER);          //new+ 在线用户较多时只允许联系人列表上下滚动
    lv_obj_set_scrollbar_mode(qq_private_contact_list, LV_SCROLLBAR_MODE_AUTO); //new+ 名片超出可见范围时自动显示滚动条

    /* 右侧会话标题栏 */
    lv_obj_t * chat_header = lv_obj_create(qq_chat_win); // 创建右侧会话标题栏，显示当前群聊信息
    lv_obj_set_size(chat_header, 740, 84);               // 右侧宽度为 1024 减去 284 像素侧栏
    lv_obj_set_pos(chat_header, 284, 0);                 // 标题栏从侧栏右边缘开始，位于屏幕顶部
    lv_obj_set_style_bg_color(chat_header, lv_color_hex(0x080C12), 0); // 标题栏与左侧栏使用一致的深色背景
    lv_obj_set_style_bg_opa(chat_header, LV_OPA_COVER, 0); // 标题栏背景完全不透明
    lv_obj_set_style_border_color(chat_header, lv_color_hex(0x202C3A), 0); // 定义标题栏底部分隔线颜色
    lv_obj_set_style_border_width(chat_header, 0, 0);    // 当前关闭分隔线宽度，保留颜色配置便于调整
    lv_obj_set_style_border_side(chat_header, LV_BORDER_SIDE_BOTTOM, 0); // 若启用边框则只绘制底边
    lv_obj_set_style_radius(chat_header, 0, 0);           // 标题栏与屏幕边缘贴合，不使用圆角
    lv_obj_set_style_pad_all(chat_header, 0, 0);          // 清除默认内边距，子控件按绝对坐标布局
    lv_obj_remove_flag(chat_header, LV_OBJ_FLAG_SCROLLABLE); // 标题栏内容固定，禁止滚动

    lv_obj_t * chat_avatar = lv_obj_create(chat_header); // 在右侧标题栏创建当前群聊的大号头像
    lv_obj_set_size(chat_avatar, 48, 48);                // 标题栏头像固定为 48x48 像素
    lv_obj_set_pos(chat_avatar, 28, 18);                 // 头像距标题栏左侧 28、顶部 18 像素
    lv_obj_set_style_bg_color(chat_avatar, lv_color_hex(0x1687FF), 0); // 使用亮蓝色头像底板延续主题强调色
    lv_obj_set_style_bg_opa(chat_avatar, LV_OPA_COVER, 0); // 头像背景完全不透明
    lv_obj_set_style_border_width(chat_avatar, 0, 0);    // 标题栏头像不绘制边框
    lv_obj_set_style_radius(chat_avatar, 6, 0);           // 头像使用 6 像素圆角
    lv_obj_set_style_pad_all(chat_avatar, 0, 0);          // 清除头像内部默认内边距
    lv_obj_remove_flag(chat_avatar, LV_OBJ_FLAG_SCROLLABLE); // 头像只负责展示，禁止滚动
    lv_obj_t * chat_avatar_icon = lv_label_create(chat_avatar); // 在标题栏头像内部创建 HOME 图标
    qq_chat_header_icon_label = chat_avatar_icon;          //new+ 保存图标引用以便群聊和私聊之间切换显示
    lv_label_set_text(chat_avatar_icon, LV_SYMBOL_HOME);  // 使用相同 HOME 图标保持左右会话标识一致
    lv_obj_set_style_text_color(chat_avatar_icon, lv_color_hex(0xFFFFFF), 0); // 图标使用白色以获得足够对比度
    lv_obj_set_style_text_font(chat_avatar_icon, &lv_font_montserrat_22, 0); // 标题栏头像图标使用更醒目的 22 号大小
    lv_obj_center(chat_avatar_icon);                      // 将图标水平和垂直居中到头像中

    qq_chat_title_label = lv_label_create(chat_header);  // 创建当前聊天室名称标签并保存全局引用
    lv_label_set_text(qq_chat_title_label, "Chat Group"); // 标题栏显示与左侧卡片一致的群聊名称
    lv_obj_set_style_text_color(qq_chat_title_label, lv_color_hex(0xFFFFFF), 0); // 群聊名称使用白色作为主标题
    lv_obj_set_style_text_font(qq_chat_title_label, &lv_font_montserrat_22, 0); // 主标题使用 22 号字体
    lv_obj_set_pos(qq_chat_title_label, 92, 19);          // 将主标题放在头像右侧上方

    lv_obj_t * member_label = lv_label_create(chat_header); // 创建标题栏内的群聊类型说明标签
    qq_chat_subtitle_label = member_label;                 //new+ 保存副标题引用以显示群聊、私聊或离线状态
    lv_label_set_text(member_label, "TCP GROUP CHAT");   // 明确当前会话使用 TCP 群聊通信方式
    lv_obj_set_style_text_color(member_label, lv_color_hex(0x718196), 0); // 类型说明使用灰蓝色作为辅助信息
    lv_obj_set_style_text_font(member_label, &lv_font_montserrat_12, 0); // 辅助说明使用 12 号字体
    lv_obj_set_pos(member_label, 92, 48);                 // 放置在群聊主标题正下方

    /* 聊天记录区域 */
    qq_chat_message_list = lv_obj_create(qq_chat_win);   // 创建可动态加入消息行的聊天记录列表
    lv_obj_set_size(qq_chat_message_list, 740, 380);     // 正常状态下占据右侧标题栏与输入区之间的空间
    lv_obj_set_pos(qq_chat_message_list, 284, 84);       // 列表从右侧标题栏下方开始显示
    lv_obj_set_style_bg_color(qq_chat_message_list, lv_color_hex(0x05070B), 0); // 与全局主背景保持同样的近黑色
    lv_obj_set_style_bg_opa(qq_chat_message_list, LV_OPA_COVER, 0); // 列表背景完全不透明
    lv_obj_set_style_border_width(qq_chat_message_list, 0, 0); // 去掉聊天记录列表默认边框
    lv_obj_set_style_radius(qq_chat_message_list, 0, 0); // 列表是页面区域，不使用圆角
    lv_obj_set_style_pad_all(qq_chat_message_list, 20, 0); // 消息气泡与列表四周保持 20 像素留白
    lv_obj_set_style_pad_row(qq_chat_message_list, 14, 0); // 相邻两条消息行之间保留 14 像素间隔
    lv_obj_set_flex_flow(qq_chat_message_list, LV_FLEX_FLOW_COLUMN); // 新消息按照垂直方向依次追加
    lv_obj_set_flex_align(qq_chat_message_list,          // 配置聊天列表中所有消息行的整体 Flex 对齐规则
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START); // 消息从列表顶部开始排列且不拉伸
    lv_obj_set_scroll_dir(qq_chat_message_list, LV_DIR_VER); // 只允许上下滚动浏览历史消息
    lv_obj_set_scrollbar_mode(qq_chat_message_list, LV_SCROLLBAR_MODE_AUTO); // 内容超出高度时自动显示滚动条

    /* 底部消息输入区域 */
    qq_chat_input_area = lv_obj_create(qq_chat_win);     // 创建底部输入框和发送按钮共用的容器
    lv_obj_set_size(qq_chat_input_area, 740, 136);       // 输入区域占满右侧宽度并固定为 136 像素高
    lv_obj_set_pos(qq_chat_input_area, 284, 464);        // 正常状态固定在屏幕底部，键盘出现时会整体上移
    lv_obj_set_style_bg_color(qq_chat_input_area, lv_color_hex(0x080C12), 0); // 使用与标题栏一致的深色背景
    lv_obj_set_style_bg_opa(qq_chat_input_area, LV_OPA_COVER, 0); // 输入区域背景完全不透明
    lv_obj_set_style_border_color(qq_chat_input_area, lv_color_hex(0x202C3A), 0); // 定义输入区域顶部的分隔线颜色
    lv_obj_set_style_border_width(qq_chat_input_area, 0, 0); // 当前不显示顶部边框，保持整体连贯
    lv_obj_set_style_border_side(qq_chat_input_area, LV_BORDER_SIDE_TOP, 0); // 若启用边框则只绘制顶部一侧
    lv_obj_set_style_radius(qq_chat_input_area, 0, 0);    // 底部页面区域不使用圆角
    lv_obj_set_style_pad_all(qq_chat_input_area, 0, 0);   // 清除默认内边距以便精确放置输入控件
    lv_obj_remove_flag(qq_chat_input_area, LV_OBJ_FLAG_SCROLLABLE); // 输入区域固定布局，不允许自身滚动

    lv_obj_t * input_hint = lv_label_create(qq_chat_input_area); // 在输入框上方创建 MESSAGE 字段提示
    lv_label_set_text(input_hint, "MESSAGE");             // 使用英文提示当前区域用于编辑聊天消息
    lv_obj_set_style_text_color(input_hint, lv_color_hex(0x6F8193), 0); // 提示文字使用灰蓝色避免抢占正文注意力
    lv_obj_set_style_text_font(input_hint, &lv_font_montserrat_12, 0); // 提示文字使用 12 号字体
    lv_obj_set_pos(input_hint, 20, 18);                   // 与下方消息输入框左边缘保持对齐

    qq_chat_input = lv_textarea_create(qq_chat_input_area); // 创建用户输入待发送消息的文本区域
    lv_obj_set_size(qq_chat_input, 584, 60);              // 输入框宽 584、高 60，右侧为发送按钮预留空间
    lv_obj_set_pos(qq_chat_input, 20, 42);                // 输入框距容器左侧 20、顶部 42 像素
    lv_textarea_set_one_line(qq_chat_input, false);       // 允许长消息在输入框中自动换为多行
    lv_textarea_set_max_length(qq_chat_input, QQ_CHAT_TEXT_MAX - 1); // 限制正文长度并为结尾 \0 保留一个字节
    lv_textarea_set_placeholder_text(qq_chat_input, "Type a message..."); // 输入为空时显示操作占位提示
    lv_obj_set_style_bg_color(qq_chat_input, lv_color_hex(0x0F1722), 0); // 输入框默认使用深灰蓝背景
    lv_obj_set_style_bg_opa(qq_chat_input, LV_OPA_COVER, 0); // 输入框背景完全不透明
    lv_obj_set_style_border_color(qq_chat_input, lv_color_hex(0x26384A), 0); // 默认边框使用低对比灰蓝色
    lv_obj_set_style_border_color(qq_chat_input, lv_color_hex(0x1687FF), LV_STATE_FOCUSED); // 获得焦点后边框变亮蓝色
    lv_obj_set_style_border_width(qq_chat_input, 1, 0);   // 输入框使用 1 像素边框
    lv_obj_set_style_radius(qq_chat_input, 6, 0);         // 输入框采用 6 像素小圆角
    lv_obj_set_style_pad_all(qq_chat_input, 12, 0);       // 输入文字与输入框边缘保持 12 像素内边距
    lv_obj_set_style_text_color(qq_chat_input, lv_color_hex(0xFFFFFF), 0); // 用户实际输入的内容使用白色
    lv_obj_set_style_text_color(qq_chat_input, lv_color_hex(0x718196), LV_PART_TEXTAREA_PLACEHOLDER); // 占位文字使用较暗灰蓝色
    lv_obj_set_style_text_font(qq_chat_input, &lv_font_montserrat_14, 0); // 输入正文使用 14 号字体
    lv_obj_add_event_cb(qq_chat_input, qq_chat_input_cb, LV_EVENT_ALL, NULL); // 监听全部事件以处理点击、聚焦、确认和取消

    lv_obj_t * send_btn = lv_button_create(qq_chat_input_area); // 在输入框右侧创建发送消息按钮
    lv_obj_set_size(send_btn, 96, 60);                   // 发送按钮与输入框等高，并提供 96 像素宽触控区域
    lv_obj_set_pos(send_btn, 624, 42);                   // 放置在输入框右侧并保持相同纵坐标
    lv_obj_set_style_bg_color(send_btn, lv_color_hex(0x086CD9), 0); // 默认背景使用主题蓝色
    lv_obj_set_style_bg_color(send_btn, lv_color_hex(0x0454AD), LV_STATE_PRESSED); // 按下时显示更深蓝色
    lv_obj_set_style_border_color(send_btn, lv_color_hex(0x2B8CFF), 0); // 使用亮蓝色描边强化主要操作
    lv_obj_set_style_border_width(send_btn, 1, 0);       // 发送按钮绘制 1 像素边框
    lv_obj_set_style_radius(send_btn, 6, 0);             // 发送按钮使用与输入框一致的 6 像素圆角
    lv_obj_set_style_shadow_width(send_btn, 0, 0);       // 关闭默认阴影，保持扁平主题
    lv_obj_t * send_label = lv_label_create(send_btn);   // 在发送按钮内部创建文字和箭头标签
    lv_label_set_text(send_label, "SEND  " LV_SYMBOL_RIGHT); // 使用 SEND 加右箭头明确发送动作
    lv_obj_set_style_text_color(send_label, lv_color_hex(0xFFFFFF), 0); // 按钮文字使用白色
    lv_obj_set_style_text_font(send_label, &lv_font_montserrat_14, 0); // 按钮文字使用 14 号字体
    lv_obj_center(send_label);                           // 将 SEND 标签水平和垂直居中
    lv_obj_add_event_cb(send_btn, qq_chat_send_cb, LV_EVENT_CLICKED, NULL); // 点击时读取输入框并发送 DATA 消息

    qq_chat_keyboard = lv_keyboard_create(qq_chat_win);  // 创建专门供消息输入框使用的屏幕软键盘
    lv_obj_set_size(qq_chat_keyboard, 740, 246);         // 键盘占满右侧宽度并覆盖屏幕下方 246 像素
    lv_obj_align(qq_chat_keyboard, LV_ALIGN_BOTTOM_RIGHT, 0, 0); // 覆盖键盘默认的底部居中对齐，使其实际位于右侧区域底部
    lv_obj_set_style_bg_color(qq_chat_keyboard, lv_color_hex(0x080C12), 0); // 键盘整体背景与侧栏和输入区一致
    lv_obj_set_style_bg_opa(qq_chat_keyboard, LV_OPA_COVER, 0); // 键盘背景完全不透明，避免下层控件透出
    lv_obj_set_style_border_color(qq_chat_keyboard, lv_color_hex(0x202C3A), 0); // 键盘外框使用灰蓝色
    lv_obj_set_style_border_width(qq_chat_keyboard, 1, 0); // 键盘整体显示 1 像素边框
    lv_obj_set_style_radius(qq_chat_keyboard, 0, 0);      // 键盘与屏幕底部贴合，因此不设置外部圆角
    lv_obj_set_style_pad_all(qq_chat_keyboard, 8, 0);     // 键盘按键与外边框之间保留 8 像素空间
    lv_obj_set_style_pad_row(qq_chat_keyboard, 6, 0);     // 键盘各行之间保留 6 像素间距
    lv_obj_set_style_pad_column(qq_chat_keyboard, 6, 0);  // 同一行按键之间保留 6 像素间距
    lv_obj_set_style_bg_color(qq_chat_keyboard, lv_color_hex(0x152131), LV_PART_ITEMS); // 普通按键使用深灰蓝色背景
    lv_obj_set_style_bg_color(qq_chat_keyboard, lv_color_hex(0x075DBD), LV_PART_ITEMS | LV_STATE_PRESSED); // 按键按下时变为主题蓝色
    lv_obj_set_style_text_color(qq_chat_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS); // 所有按键字符使用白色
    lv_obj_set_style_text_font(qq_chat_keyboard, &lv_font_montserrat_16, LV_PART_ITEMS); // 键盘字符使用易读的 16 号字体
    lv_obj_set_style_border_width(qq_chat_keyboard, 0, LV_PART_ITEMS); // 单个键帽不再额外绘制边框
    lv_obj_set_style_radius(qq_chat_keyboard, 6, LV_PART_ITEMS); // 单个键帽采用 6 像素小圆角
    /* LVGL 会把 READY 和 CANCEL 转发给输入框，不再监听键盘全部点击事件以免取消后被 CLICKED 重新打开。 */ //new+ 让键盘只使用框架内置事件转发
    lv_obj_add_flag(qq_chat_keyboard, LV_OBJ_FLAG_HIDDEN); // 初次进入聊天室时先隐藏键盘，点击输入框再显示

    qq_chat_switch_session("");                          //new+ 在网络提示到达前初始化公共群聊为当前可见会话
    qq_chat_receive_timer = lv_timer_create(qq_chat_receive_timer_cb, 100, NULL); // 每 100 毫秒非阻塞检查一次服务器消息
    if(qq_chat_client_connect() != 0) {                  // 界面完成后立即尝试连接配置的聊天服务器
        qq_chat_add_message("SYSTEM", "Set QQ_SERVER_IP and start the chat server", 0); // 连接失败时提示配置服务器地址并启动服务端
    }
}






/* ---- 2048 游戏入口 ---- */

// static void show_main_win(void)
// {
//     lv_obj_remove_flag(Main_win,LV_OBJ_FLAG_HIDDEN);
// }

// static void Games_cb(lv_event_t * e)
// {
//     lv_obj_t * btn = lv_event_get_target(e);
//     lv_obj_t * lb = lv_obj_get_child(btn, 0);
//     if(strcmp(lv_label_get_text(lb), "2048") == 0)
//     {
//         lv_obj_add_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
//         game_2048_on_exit = show_main_win;
//         game_2048_start();
//     }
// }

// static void show_main_win(void)
// {
//     lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
// }

// static void Games_cb(lv_event_t * e)
// {
//     lv_obj_t * btn = lv_event_get_target(e);
//     lv_obj_t * lb = lv_obj_get_child(btn, 0);
//     if(strcmp(lv_label_get_text(lb), "Games") == 0) {
//         lv_obj_add_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
//         game_2048_on_exit = show_main_win;
//         game_2048_start();
//     }
// }



/* ===== 视频播放器入口按钮回调 ===== */
static void Video_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * lb = lv_obj_get_child(btn, 0);
    const char * text = lv_label_get_text(lb);
    if(strcmp(text, "Video") == 0)
    {
        lv_obj_add_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
        Video_Interface();
    }
}

/* ===== 视频播放器辅助函数和回调 ===== */
static int video_hide_countdown = 0;

/* ===== 更新视频控制栏隐藏倒计时 ===== */
static void video_hide_tick_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    if(video_hide_countdown > 0) {
        video_hide_countdown--;
        if(video_hide_countdown == 0 && video_control_bar != NULL)
            lv_obj_add_flag(video_control_bar, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ===== 显示视频控制栏 ===== */
static void video_show_controls(void)
{
    if(video_control_bar == NULL) return;
    video_hide_countdown = 3;   /* 喂狗：重置3秒倒计时 */
    lv_obj_remove_flag(video_control_bar, LV_OBJ_FLAG_HIDDEN);
}

/* ===== 处理视频画面触摸事件 ===== */
static void video_screen_tap_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_PRESSED && code != LV_EVENT_RELEASED) return;
    video_show_controls();
}

/* ===== 保持视频控制栏显示 ===== */
static void video_control_touch_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    video_show_controls();
}

/* ===== 更新当前视频信息 ===== */
static void video_update_media_info(void)
{
    if(video_name_label == NULL) return;

    if(video_current == NULL || h_video == NULL || h_video->num <= 0) {
        lv_label_set_text(video_name_label, "No video found in /ww/video");
        return;
    }

    char * name = strrchr(video_current->data, '/');
    lv_label_set_text(video_name_label, name != NULL ? name + 1 : video_current->data);
}

/* ===== 更新视频播放进度界面 ===== */
static void video_update_progress_ui(void)
{
    int current_seconds = video_position > 0.0 ? (int)video_position : 0;
    int total_seconds = video_duration > 0.0 ? (int)video_duration : 0;

    if(video_time_label != NULL) {
        char time_text[40];
        snprintf(time_text, sizeof(time_text), "%02d:%02d / %02d:%02d",
                 current_seconds / 60, current_seconds % 60,
                 total_seconds / 60, total_seconds % 60);
        lv_label_set_text(video_time_label, time_text);
    }

    if(video_duration > 0.0) {
        video_percent = (int)(video_position * 100.0 / video_duration);
    }
    if(video_percent < 0) video_percent = 0;
    if(video_percent > 100) video_percent = 100;

    if(video_slider != NULL && !lv_obj_has_state(video_slider, LV_STATE_PRESSED)) {
        lv_slider_set_value(video_slider, video_percent, LV_ANIM_OFF);
    }
}

/* ===== 创建并检查视频控制管道 ===== */
static int video_ensure_fifo(void)
{
    struct stat st;
    if(stat(video_fifo_path, &st) == 0) {
        if(S_ISFIFO(st.st_mode)) return 0;
        fprintf(stderr, "%s exists but is not a FIFO\n", video_fifo_path);
        return -1;
    }

    if(errno != ENOENT) {
        perror("stat video fifo");
        return -1;
    }

    if(mkfifo(video_fifo_path, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo video");
        return -1;
    }
    return 0;
}

/* ===== 关闭视频播放器通信通道 ===== */
static void video_close_channels(void)
{
    if(video_fifo_fd >= 0) {
        close(video_fifo_fd);
        video_fifo_fd = -1;
    }
    if(video_output_fd >= 0) {
        close(video_output_fd);
        video_output_fd = -1;
    }
    video_output_length = 0;
    video_output_buffer[0] = '\0';
}

/* ===== 更新视频停止状态 ===== */
static void video_set_stopped_state(void)
{
    video_is_playing = 0;
    video_is_paused = 0;
    if(video_play_label != NULL) {
        lv_label_set_text(video_play_label, LV_SYMBOL_PLAY);
    }
}

/* ===== 停止视频播放进程 ===== */
static void video_stop_player(void)
{
    if(video_pid > 0) {
        int status = 0;
        int reaped = 0;
        kill(video_pid, SIGTERM);

        for(int i = 0; i < 10; i++) {
            pid_t result = waitpid(video_pid, &status, WNOHANG);
            if(result == video_pid || (result == -1 && errno == ECHILD)) {
                reaped = 1;
                break;
            }
            usleep(20000);
        }

        if(!reaped) {
            kill(video_pid, SIGKILL);
            waitpid(video_pid, &status, 0);
        }
        video_pid = 0;
    }

    video_close_channels();
    video_set_stopped_state();
}

/* ===== 发送视频播放器控制命令 ===== */
static int video_send_command(const char * command)
{
    if(video_fifo_fd < 0 || command == NULL) return -1;

    size_t command_length = strlen(command);
    ssize_t written = write(video_fifo_fd, command, command_length);
    return written == (ssize_t)command_length ? 0 : -1;
}

/* ===== 解析视频播放器响应 ===== */
static void video_process_output_line(const char * line)
{
    double value = 0.0;
    int percent = 0;

    if(sscanf(line, "ANS_TIME_POSITION=%lf", &value) == 1) {
        video_position = value;
    }
    else if(sscanf(line, "ANS_LENGTH=%lf", &value) == 1) {
        video_duration = value;
    }
    else if(sscanf(line, "ANS_PERCENT_POSITION=%d", &percent) == 1) {
        video_percent = percent;
    }
}

/* ===== 读取视频播放器响应 ===== */
static void video_read_player_output(void)
{
    if(video_output_fd < 0) return;

    while(video_output_length < sizeof(video_output_buffer) - 1) {
        ssize_t count = read(video_output_fd,
                             video_output_buffer + video_output_length,
                             sizeof(video_output_buffer) - 1 - video_output_length);
        if(count > 0) {
            video_output_length += (size_t)count;
            video_output_buffer[video_output_length] = '\0';
            continue;
        }
        if(count == -1 && errno == EINTR) continue;
        break;
    }

    char * line_start = video_output_buffer;
    char * newline = NULL;
    while((newline = strchr(line_start, '\n')) != NULL) {
        *newline = '\0';
        video_process_output_line(line_start);
        line_start = newline + 1;
    }

    size_t remaining = video_output_length - (size_t)(line_start - video_output_buffer);
    if(remaining > 0 && line_start != video_output_buffer) {
        memmove(video_output_buffer, line_start, remaining);
    }
    video_output_length = remaining;
    video_output_buffer[video_output_length] = '\0';
    if(video_output_length == sizeof(video_output_buffer) - 1 &&
       strchr(video_output_buffer, '\n') == NULL) {
        video_output_length = 0;
        video_output_buffer[0] = '\0';
    }
    video_update_progress_ui();
}

/* ===== 定时更新视频播放进度 ===== */
static void video_progress_tick_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    static int query_tick = 0;

    if(video_pid <= 0) return;

    video_read_player_output();

    int status = 0;
    pid_t result = waitpid(video_pid, &status, WNOHANG);
    if(result == video_pid || (result == -1 && errno == ECHILD)) {
        if(video_duration > 0.0) {
            video_position = video_duration;
        }
        video_percent = 100;
        video_update_progress_ui();
        video_pid = 0;
        video_close_channels();
        video_set_stopped_state();
        return;
    }

    query_tick++;
    if(query_tick >= 2) {
        video_send_command("pausing_keep_force get_time_pos\n");
        video_send_command("pausing_keep_force get_time_length\n");
        video_send_command("pausing_keep_force get_percent_pos\n");
        query_tick = 0;
    }
}

/* ===== 启动当前视频播放 ===== */
static void video_start_play(void)
{
    if(video_current == NULL) return;

    video_stop_player();
    video_update_media_info();
    video_position = 0.0;
    video_duration = 0.0;
    video_percent = 0;
    video_update_progress_ui();

    if(access("/usr/bin/mplayer64", X_OK) != 0) {
        if(video_time_label != NULL) lv_label_set_text(video_time_label, "mplayer64 unavailable");
        perror("access mplayer64");
        return;
    }
    if(video_ensure_fifo() != 0) {
        if(video_time_label != NULL) lv_label_set_text(video_time_label, "FIFO unavailable");
        return;
    }

    int output_pipe[2];
    if(pipe(output_pipe) == -1) {
        perror("pipe video");
        return;
    }

    video_pid = fork();
    if(video_pid == -1) {
        perror("fork video");
        close(output_pipe[0]);
        close(output_pipe[1]);
        video_pid = 0;
        return;
    }

    if(video_pid == 0) {
        char input_arg[128];
        snprintf(input_arg, sizeof(input_arg), "file=%s", video_fifo_path);

        close(output_pipe[0]);
        dup2(output_pipe[1], STDOUT_FILENO);
        close(output_pipe[1]);

        int null_fd = open("/dev/null", O_WRONLY);
        if(null_fd >= 0) {
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        }

        execl("/usr/bin/mplayer64", "mplayer64",
              "-vo", "fbdev2",
              "-slave", "-quiet",
              "-input", input_arg,
              "-geometry", "0:0",
              "-zoom", "-x", "1024", "-y", "510",
              video_current->data, (char *)NULL);
        _exit(127);
    }

    close(output_pipe[1]);
    video_output_fd = output_pipe[0];
    int output_flags = fcntl(video_output_fd, F_GETFL, 0);
    if(output_flags >= 0) {
        fcntl(video_output_fd, F_SETFL, output_flags | O_NONBLOCK);
    }

    video_fifo_fd = open(video_fifo_path, O_RDWR | O_NONBLOCK);
    if(video_fifo_fd == -1) {
        perror("open video fifo");
        video_stop_player();
        return;
    }

    video_is_playing = 1;
    video_is_paused = 0;
    if(video_play_label != NULL) lv_label_set_text(video_play_label, LV_SYMBOL_PAUSE);

    int volume = video_vol_slider != NULL ? lv_slider_get_value(video_vol_slider) : 70;
    char volume_command[32];
    snprintf(volume_command, sizeof(volume_command), "volume %d 1\n", volume);
    video_send_command(volume_command);
}

/* ===== 退出视频播放器并返回主界面 ===== */
static void video_back_to_main_cb(lv_event_t * e)
{
    lv_obj_t * win = lv_event_get_user_data(e);

    video_stop_player();
    if(video_hide_timer != NULL) {
        lv_timer_delete(video_hide_timer);
        video_hide_timer = NULL;
    }
    if(video_progress_timer != NULL) {
        lv_timer_delete(video_progress_timer);
        video_progress_timer = NULL;
    }

    lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
    lv_obj_delete_async(win);

    video_win = NULL;
    video_control_bar = NULL;
    video_play_btn = NULL;
    video_play_label = NULL;
    video_slider = NULL;
    video_vol_slider = NULL;
    video_time_label = NULL;
    video_name_label = NULL;

}

/* ===== 播放上一个视频 ===== */
static void video_prev_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(video_current == NULL) return;
    video_current = video_current->prev;
    video_start_play();
    video_show_controls();
}

/* ===== 切换视频播放和暂停状态 ===== */
static void video_play_pause_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(video_current == NULL) return;

    if(!video_is_playing) {
        video_start_play();
    }
    else {
        if(video_send_command("pause\n") == 0) {
            video_is_paused = !video_is_paused;
            if(video_play_label != NULL) {
                lv_label_set_text(video_play_label,
                                  video_is_paused ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
            }
        }
        else {
            perror("pause video");
        }
    }
    video_show_controls();
}

/* ===== 播放下一个视频 ===== */
static void video_next_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(video_current == NULL) return;
    video_current = video_current->next;
    video_start_play();
    video_show_controls();
}

/* ===== 调节视频播放音量 ===== */
static void video_vol_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    int volume = lv_slider_get_value(slider);
    char command[32];
    snprintf(command, sizeof(command), "volume %d 1\n", volume);
    video_send_command(command);
    video_show_controls();
}

/* ===== 跳转视频播放进度 ===== */
static void video_seek_cb(lv_event_t * e)
{
    if(!video_is_playing) return;

    lv_obj_t * slider = lv_event_get_target(e);
    int percent = lv_slider_get_value(slider);
    char command[32];
    snprintf(command, sizeof(command), "seek %d 1\n", percent);
    video_send_command(command);

    video_percent = percent;
    if(video_duration > 0.0) {
        video_position = video_duration * percent / 100.0;
    }
    video_update_progress_ui();
    video_show_controls();
}

/* ===== 创建视频播放器界面 ===== */
void Video_Interface(void)
{
    video_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(video_win, 1024, 600);
    lv_obj_set_pos(video_win, 0, 0);
    lv_obj_set_style_bg_color(video_win, lv_color_hex(0x000000), 0);
    lv_obj_set_style_radius(video_win, 0, 0);
    lv_obj_set_style_border_width(video_win, 0, 0);
    lv_obj_set_style_pad_all(video_win, 0, 0);


 

    /* 点击屏幕唤醒/隐藏控制栏 */
    lv_obj_add_event_cb(video_win, video_screen_tap_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(video_win, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* ====== 左上角返回按钮 ====== */
    lv_obj_t * btn_back = lv_button_create(video_win);
    lv_obj_set_size(btn_back, 80, 40);
    lv_obj_set_pos(btn_back, 10, 10);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x333333), 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_t * lb_back = lv_label_create(btn_back);
    lv_label_set_text(lb_back, "Back");
    lv_obj_set_style_text_color(lb_back, lv_color_hex(0xCCCCCC), 0);
    lv_obj_center(lb_back);
    lv_obj_add_event_cb(btn_back, video_back_to_main_cb, LV_EVENT_CLICKED, video_win);

    /* ====== 底部控制栏 ====== */
    video_control_bar = lv_obj_create(video_win);
    lv_obj_set_size(video_control_bar, 1024, 90);
    lv_obj_set_pos(video_control_bar, 0, 510);
    lv_obj_set_style_bg_color(video_control_bar, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(video_control_bar, LV_OPA_70, 0);
    lv_obj_set_style_radius(video_control_bar, 0, 0);
    lv_obj_set_style_border_width(video_control_bar, 0, 0);
    lv_obj_set_style_pad_all(video_control_bar, 0, 0);

    lv_obj_add_event_cb(video_control_bar, video_control_touch_cb, LV_EVENT_PRESSED, NULL);

    /* 进度条 */
    video_slider = lv_slider_create(video_control_bar);
    lv_obj_set_size(video_slider, 700, 6);
    lv_obj_set_pos(video_slider, 10, 5);
    lv_obj_set_style_bg_color(video_slider, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_bg_color(video_slider, lv_color_hex(0x0099FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(video_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_slider_set_range(video_slider, 0, 100);
    lv_slider_set_value(video_slider, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(video_slider, video_seek_cb, LV_EVENT_RELEASED, NULL);

    /* 时间标签 */
    video_time_label = lv_label_create(video_control_bar);
    lv_label_set_text(video_time_label, "00:00 / 00:00");
    lv_obj_set_style_text_color(video_time_label, lv_color_hex(0xBBBBBB), 0);
    lv_obj_set_style_text_font(video_time_label, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(video_time_label, 15, 18);

    video_name_label = lv_label_create(video_control_bar);
    lv_label_set_text(video_name_label, "No video found in /ww/video");
    lv_label_set_long_mode(video_name_label, LV_LABEL_LONG_DOT);
    lv_obj_set_size(video_name_label, 520, 24);
    lv_obj_set_style_text_color(video_name_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(video_name_label, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(video_name_label, 180, 18);

    /* ====== 按钮组 ====== */
    int btn_y = 40, btn_w = 54, btn_h = 40, gap = 10, cx = 400;

    /* 上个视频 */
    lv_obj_t * btn_prev = lv_button_create(video_control_bar);
    lv_obj_set_size(btn_prev, btn_w, btn_h);
    lv_obj_set_pos(btn_prev, cx, btn_y);
    lv_obj_set_style_bg_opa(btn_prev, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0x444444), 0);
    lv_obj_set_style_shadow_width(btn_prev, 0, 0);
    lv_obj_set_style_radius(btn_prev, 8, 0);
    lv_obj_t * l_prev = lv_label_create(btn_prev);
    lv_label_set_text(l_prev, LV_SYMBOL_PREV);
    lv_obj_set_style_text_color(l_prev, lv_color_hex(0xCCCCCC), 0);
    lv_obj_center(l_prev);
    lv_obj_add_event_cb(btn_prev, video_prev_cb, LV_EVENT_CLICKED, NULL);

    cx += btn_w + gap;

    /* 播放/暂停 */
    video_play_btn = lv_button_create(video_control_bar);
    lv_obj_set_size(video_play_btn, btn_w, btn_h);
    lv_obj_set_pos(video_play_btn, cx, btn_y);
    lv_obj_set_style_bg_opa(video_play_btn, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(video_play_btn, lv_color_hex(0x444444), 0);
    lv_obj_set_style_shadow_width(video_play_btn, 0, 0);
    lv_obj_set_style_radius(video_play_btn, 8, 0);
    video_play_label = lv_label_create(video_play_btn);
    lv_label_set_text(video_play_label, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(video_play_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_center(video_play_label);
    lv_obj_add_event_cb(video_play_btn, video_play_pause_cb, LV_EVENT_CLICKED, NULL);

    cx += btn_w + gap;

    /* 下个视频 */
    lv_obj_t * btn_next = lv_button_create(video_control_bar);
    lv_obj_set_size(btn_next, btn_w, btn_h);
    lv_obj_set_pos(btn_next, cx, btn_y);
    lv_obj_set_style_bg_opa(btn_next, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0x444444), 0);
    lv_obj_set_style_shadow_width(btn_next, 0, 0);
    lv_obj_set_style_radius(btn_next, 8, 0);
    lv_obj_t * l_next = lv_label_create(btn_next);
    lv_label_set_text(l_next, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_color(l_next, lv_color_hex(0xCCCCCC), 0);
    lv_obj_center(l_next);
    lv_obj_add_event_cb(btn_next, video_next_cb, LV_EVENT_CLICKED, NULL);

    /* ====== 右侧音量 ====== */
    lv_obj_t * vol_icon = lv_label_create(video_control_bar);
    lv_label_set_text(vol_icon, LV_SYMBOL_VOLUME_MID);
    lv_obj_set_style_text_color(vol_icon, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(vol_icon, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(vol_icon, 860, btn_y + 10);

    video_vol_slider = lv_slider_create(video_control_bar);
    lv_obj_set_size(video_vol_slider, 80, 4);
    lv_obj_set_pos(video_vol_slider, 890, btn_y + 18);
    lv_obj_set_style_bg_color(video_vol_slider, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_bg_color(video_vol_slider, lv_color_hex(0x0099FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(video_vol_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_slider_set_range(video_vol_slider, 0, 100);
    lv_slider_set_value(video_vol_slider, 70, LV_ANIM_OFF);
    lv_obj_add_event_cb(video_vol_slider, video_vol_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if(h_video != NULL && h_video->first != NULL && h_video->num > 0) {
        if(video_current == NULL) video_current = h_video->first;
        video_update_media_info();
    }
    else {
        lv_obj_add_state(btn_prev, LV_STATE_DISABLED);
        lv_obj_add_state(video_play_btn, LV_STATE_DISABLED);
        lv_obj_add_state(btn_next, LV_STATE_DISABLED);
        lv_obj_add_state(video_slider, LV_STATE_DISABLED);
        lv_obj_add_state(video_vol_slider, LV_STATE_DISABLED);
    }

    /* 控制栏隐藏倒计时和播放器进度查询 */
    video_hide_timer = lv_timer_create(video_hide_tick_cb, 1000, NULL);
    video_progress_timer = lv_timer_create(video_progress_tick_cb, 250, NULL);
    video_show_controls();
}










/* ===== 记事本入口按钮回调 ===== */
static void Note_cb(lv_event_t * e)    //记事本回调函数
{
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * lb = lv_obj_get_child(btn, 0);
    const char * text = lv_label_get_text(lb);
    if(strcmp(text, "Note") == 0)
    {
        lv_obj_add_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
        Note_Interface();
    }

}


/* ===== 设置记事本按钮样式 ===== */
static void note_style_button(lv_obj_t * button)
{
    lv_obj_set_style_bg_color(button, lv_color_hex(0x086CD9), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x0454AD), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, lv_color_hex(0x2B8CFF), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_text_color(button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_outline_color(button, lv_color_hex(0x9ACBFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(button, 2, LV_STATE_FOCUSED);
}

/* ===== 设置记事本文本框样式 ===== */
static void note_style_textarea(lv_obj_t * textarea)
{
    lv_obj_set_style_bg_color(textarea, lv_color_hex(0x0C1118), 0);
    lv_obj_set_style_bg_opa(textarea, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(textarea, lv_color_hex(0x263445), 0);
    lv_obj_set_style_border_color(textarea, lv_color_hex(0x1687FF), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(textarea, 1, 0);
    lv_obj_set_style_border_width(textarea, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(textarea, 8, 0);
    lv_obj_set_style_pad_all(textarea, 12, 0);
    lv_obj_set_style_text_color(textarea, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(textarea, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(textarea, lv_color_hex(0x66788B), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(textarea, lv_color_hex(0x1687FF), LV_PART_CURSOR);
}

/* ===== 刷新记事本文件列表 ===== */
static void note_refresh_list(void)
{
    if(Note_win == NULL) return;

    if(notes_list != NULL) {
        lv_obj_delete(notes_list);
        notes_list = NULL;
    }

    if(note_count_label != NULL) {
        lv_label_set_text_fmt(note_count_label, "%d NOTES", h_notes != NULL ? h_notes->num : 0);
    }

    notes_list = lv_list_create(Note_win);
    lv_obj_set_size(notes_list, 928, 416);
    lv_obj_set_pos(notes_list, 48, 100);
    lv_obj_set_style_bg_color(notes_list, lv_color_hex(0x0C1118), 0);
    lv_obj_set_style_bg_opa(notes_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(notes_list, lv_color_hex(0x202C3A), 0);
    lv_obj_set_style_border_width(notes_list, 1, 0);
    lv_obj_set_style_radius(notes_list, 8, 0);
    lv_obj_set_style_pad_all(notes_list, 8, 0);
    lv_obj_set_style_pad_row(notes_list, 6, 0);
    lv_obj_set_scrollbar_mode(notes_list, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t * section = lv_list_add_text(notes_list, "ALL NOTES");
    lv_obj_set_style_text_color(section, lv_color_hex(0x6F9DC5), 0);
    lv_obj_set_style_text_font(section, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_opa(section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_hor(section, 12, 0);
    lv_obj_set_style_pad_ver(section, 10, 0);

    if(h_notes != NULL && h_notes->first != NULL) {
        DNode * pn = h_notes->first;
        do {
            char * pname = strrchr(pn->data, '/');
            const char * show = pname != NULL ? pname + 1 : pn->data;
            lv_obj_t * button = lv_list_add_button(notes_list, LV_SYMBOL_FILE, show);
            lv_obj_set_height(button, 58);
            lv_obj_set_style_bg_color(button, lv_color_hex(0x111A25), 0);
            lv_obj_set_style_bg_color(button, lv_color_hex(0x075DBD), LV_STATE_PRESSED);
            lv_obj_set_style_text_color(button, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(button, &lv_font_montserrat_18, 0);
            lv_obj_set_style_border_color(button, lv_color_hex(0x243244), 0);
            lv_obj_set_style_border_width(button, 1, 0);
            lv_obj_set_style_radius(button, 6, 0);
            lv_obj_set_style_pad_hor(button, 16, 0);
            lv_obj_set_style_pad_column(button, 14, 0);
            lv_obj_add_event_cb(button, note_edit_cb, LV_EVENT_CLICKED, (void *)pn);
            pn = pn->next;
        } while(pn != h_notes->first);
    }
    else {
        lv_obj_t * empty = lv_list_add_text(notes_list, "No notes available");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x718196), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_bg_opa(empty, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_ver(empty, 60, 0);
    }
}

/* ===== 关闭记事本并返回主界面 ===== */
static void note_close_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
    if(Note_win != NULL) lv_obj_delete_async(Note_win);
    Note_win = NULL;
    notes_list = NULL;
    note_count_label = NULL;
}

/* ===== 创建记事本列表界面 ===== */
void Note_Interface(void)
{
    Note_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(Note_win, 1024, 600);
    lv_obj_set_pos(Note_win, 0, 0);
    lv_obj_set_style_bg_color(Note_win, lv_color_hex(0x05070B), 0);
    lv_obj_set_style_bg_opa(Note_win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(Note_win, 0, 0);
    lv_obj_set_style_radius(Note_win, 0, 0);
    lv_obj_set_style_pad_all(Note_win, 0, 0);
    lv_obj_remove_flag(Note_win, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * btn_back = lv_button_create(Note_win);
    lv_obj_set_size(btn_back, 52, 48);
    lv_obj_set_pos(btn_back, 48, 24);
    note_style_button(btn_back);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, note_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * title = lv_label_create(Note_win);
    lv_label_set_text(title, "NOTES");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_30, 0);
    lv_obj_set_pos(title, 124, 31);

    note_count_label = lv_label_create(Note_win);
    lv_label_set_text(note_count_label, "0 NOTES");
    lv_obj_set_style_text_color(note_count_label, lv_color_hex(0x8B9AAA), 0);
    lv_obj_set_style_text_font(note_count_label, &lv_font_montserrat_16, 0);
    lv_obj_align(note_count_label, LV_ALIGN_TOP_RIGHT, -48, 39);

    lv_obj_t * header_rule = lv_obj_create(Note_win);
    lv_obj_set_size(header_rule, 928, 1);
    lv_obj_set_pos(header_rule, 48, 88);
    lv_obj_set_style_bg_color(header_rule, lv_color_hex(0x202833), 0);
    lv_obj_set_style_bg_opa(header_rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_rule, 0, 0);
    lv_obj_remove_flag(header_rule, LV_OBJ_FLAG_SCROLLABLE);

    note_refresh_list();

    lv_obj_t * btn_new = lv_button_create(Note_win);
    lv_obj_set_size(btn_new, 176, 52);
    lv_obj_set_pos(btn_new, 800, 532);
    note_style_button(btn_new);
    lv_obj_t * label_new = lv_label_create(btn_new);
    lv_label_set_text(label_new, LV_SYMBOL_PLUS "  New note");
    lv_obj_set_style_text_font(label_new, &lv_font_montserrat_18, 0);
    lv_obj_center(label_new);
    lv_obj_add_event_cb(btn_new, note_new_cb, LV_EVENT_CLICKED, NULL);
}



/* 
   记事本 - 新建笔记
*                              */
static void note_new_cb(lv_event_t * e)
{
    /* 只有用户点"新建"才清空，note_edit_cb 调用时 e==NULL 不清 */
    if(e != NULL) {
        note_current_file[0] = '\0';
    }
    note_is_modified = 0;

    /* 隐藏列表，显示编辑器 */
    lv_obj_add_flag(Note_win, LV_OBJ_FLAG_HIDDEN);

    if(note_editor_win) 
        lv_obj_delete(note_editor_win);

    note_editor_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(note_editor_win, 1024, 600);
    lv_obj_set_pos(note_editor_win, 0, 0);
    lv_obj_set_style_bg_color(note_editor_win, lv_color_hex(0x05070B), 0);
    lv_obj_set_style_bg_opa(note_editor_win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(note_editor_win, 0, 0);
    lv_obj_set_style_radius(note_editor_win, 0, 0);
    lv_obj_set_style_pad_all(note_editor_win, 0, 0);
    lv_obj_remove_flag(note_editor_win, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * btn_back = lv_button_create(note_editor_win);
    lv_obj_set_size(btn_back, 52, 44);
    lv_obj_set_pos(btn_back, 32, 12);
    note_style_button(btn_back);
    lv_obj_t * lb_back = lv_label_create(btn_back);
    lv_label_set_text(lb_back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(lb_back, &lv_font_montserrat_22, 0);
    lv_obj_center(lb_back);
    lv_obj_add_event_cb(btn_back, note_back_list_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * editor_title = lv_label_create(note_editor_win);
    lv_label_set_text(editor_title, note_current_file[0] == '\0' ? "NEW NOTE" : "EDIT NOTE");
    lv_obj_set_style_text_color(editor_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(editor_title, &lv_font_montserrat_26, 0);
    lv_obj_set_pos(editor_title, 104, 20);

    lv_obj_t * btn_save = lv_button_create(note_editor_win);
    lv_obj_set_size(btn_save, 120, 44);
    lv_obj_set_pos(btn_save, 856, 12);
    note_style_button(btn_save);
    lv_obj_t * lb_save = lv_label_create(btn_save);
    lv_label_set_text(lb_save, LV_SYMBOL_SAVE "  Save");
    lv_obj_set_style_text_font(lb_save, &lv_font_montserrat_16, 0);
    lv_obj_center(lb_save);
    lv_obj_add_event_cb(btn_save, note_save_cb, LV_EVENT_CLICKED, NULL);

    if(note_current_file[0] != '\0') {
        lv_obj_t * btn_del = lv_button_create(note_editor_win);
        lv_obj_set_size(btn_del, 48, 44);
        lv_obj_set_pos(btn_del, 796, 12);
        lv_obj_set_style_bg_color(btn_del, lv_color_hex(0xB3261E), 0);
        lv_obj_set_style_bg_color(btn_del, lv_color_hex(0x8E1B16), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(btn_del, lv_color_hex(0xE05B52), 0);
        lv_obj_set_style_border_width(btn_del, 1, 0);
        lv_obj_set_style_radius(btn_del, 8, 0);
        lv_obj_set_style_shadow_width(btn_del, 0, 0);
        lv_obj_t * lb_del = lv_label_create(btn_del);
        lv_label_set_text(lb_del, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(lb_del, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(lb_del, &lv_font_montserrat_18, 0);
        lv_obj_center(lb_del);
        lv_obj_add_event_cb(btn_del, note_delete_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t * header_rule = lv_obj_create(note_editor_win);
    lv_obj_set_size(header_rule, 960, 1);
    lv_obj_set_pos(header_rule, 32, 68);
    lv_obj_set_style_bg_color(header_rule, lv_color_hex(0x202833), 0);
    lv_obj_set_style_bg_opa(header_rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_rule, 0, 0);
    lv_obj_remove_flag(header_rule, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * lb_fn = lv_label_create(note_editor_win);
    lv_label_set_text(lb_fn, "FILE NAME");
    lv_obj_set_style_text_color(lb_fn, lv_color_hex(0x6F9DC5), 0);
    lv_obj_set_style_text_font(lb_fn, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lb_fn, 32, 82);

    note_filename_ta = lv_textarea_create(note_editor_win);
    lv_obj_set_size(note_filename_ta, 300, 50);
    lv_obj_set_pos(note_filename_ta, 32, 104);
    lv_textarea_set_placeholder_text(note_filename_ta, "File name");
    lv_textarea_set_one_line(note_filename_ta, true);
    note_style_textarea(note_filename_ta);

    lv_obj_t * label_title = lv_label_create(note_editor_win);
    lv_label_set_text(label_title, "TITLE");
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x6F9DC5), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_title, 350, 82);

    note_title_ta = lv_textarea_create(note_editor_win);
    lv_obj_set_size(note_title_ta, 626, 50);
    lv_obj_set_pos(note_title_ta, 350, 104);
    lv_textarea_set_placeholder_text(note_title_ta, "Note title");
    lv_textarea_set_one_line(note_title_ta, true);
    note_style_textarea(note_title_ta);

    lv_obj_t * label_content = lv_label_create(note_editor_win);
    lv_label_set_text(label_content, "CONTENT");
    lv_obj_set_style_text_color(label_content, lv_color_hex(0x6F9DC5), 0);
    lv_obj_set_style_text_font(label_content, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_content, 32, 166);

    note_content_ta = lv_textarea_create(note_editor_win);
    lv_obj_set_size(note_content_ta, 944, 154);
    lv_obj_set_pos(note_content_ta, 32, 188);
    lv_textarea_set_placeholder_text(note_content_ta, "Write your note...");
    note_style_textarea(note_content_ta);

    note_kb = lv_keyboard_create(note_editor_win);
    lv_obj_set_size(note_kb, 1024, 246);
    lv_obj_align(note_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(note_kb, lv_color_hex(0x080C12), 0);
    lv_obj_set_style_bg_opa(note_kb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(note_kb, lv_color_hex(0x202C3A), 0);
    lv_obj_set_style_border_width(note_kb, 1, 0);
    lv_obj_set_style_radius(note_kb, 0, 0);
    lv_obj_set_style_pad_all(note_kb, 8, 0);
    lv_obj_set_style_pad_row(note_kb, 6, 0);
    lv_obj_set_style_pad_column(note_kb, 6, 0);
    lv_obj_set_style_bg_color(note_kb, lv_color_hex(0x152131), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(note_kb, lv_color_hex(0x075DBD), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(note_kb, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_set_style_text_font(note_kb, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_border_width(note_kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(note_kb, 6, LV_PART_ITEMS);
    lv_keyboard_set_textarea(note_kb, note_filename_ta);
    lv_obj_add_flag(note_kb, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(note_filename_ta, note_keyboard_cb, LV_EVENT_ALL, note_kb);
    lv_obj_add_event_cb(note_title_ta, note_keyboard_cb, LV_EVENT_ALL, note_kb);
    lv_obj_add_event_cb(note_content_ta, note_keyboard_cb, LV_EVENT_ALL, note_kb);

    /* 编辑已有笔记时填入文件名 */
    if(note_current_file[0] != '\0') {
        char *pname = strrchr(note_current_file, '/');
        if(pname) {
            pname++;
            char fname[256];
            strncpy(fname, pname, sizeof(fname) - 1);
            char *dot = strrchr(fname, '.');
            if(dot) *dot = '\0';
            lv_textarea_set_text(note_filename_ta, fname);
        }
    }
}

/*  
    记事本 - 编辑已有笔记
 *                               */
static void note_edit_cb(lv_event_t * e)
{
    DNode *pn = (DNode *)lv_event_get_user_data(e);
    if(!pn) return;

    /* 保存当前编辑的文件路径 */
    strncpy(note_current_file, pn->data, sizeof(note_current_file) - 1);

    /* 读取文件内容 */
    FILE *fp = fopen(pn->data, "r");
    if(!fp) return;

    char title[256] = {0};
    char content[1024] = {0};
    char line[512];
    int is_first = 1;

    while(fgets(line, sizeof(line), fp)) {
        /* 去掉末尾换行 */
        size_t len = strlen(line);
        if(len > 0 && line[len-1] == '\n') line[len-1] = '\0';

        if(is_first) {
            strncpy(title, line, sizeof(title) - 1);
            is_first = 0;
        } else {
            if(content[0] != '\0') strcat(content, "\n");
            strncat(content, line, sizeof(content) - strlen(content) - 1);
        }
    }
    fclose(fp);

    /* 打开编辑器并填入内容 */
    note_new_cb(NULL);

    lv_textarea_set_text(note_title_ta, title);
    lv_textarea_set_text(note_content_ta, content);
}

/* ===== 自动关闭记事本提示框 ===== */
static void msgbox_close_cb(lv_timer_t * timer)
{
    lv_obj_t * mbox = lv_timer_get_user_data(timer);
    if(mbox != NULL && lv_obj_is_valid(mbox)) lv_msgbox_close(mbox);
}

/* ===== 保存记事本内容 ===== */
static void note_save_cb(lv_event_t * e)
{
    const char *title = lv_textarea_get_text(note_title_ta);
    const char *content = lv_textarea_get_text(note_content_ta);

    if(strlen(title) == 0 && strlen(content) == 0) 
        return;

    /* 如果是新文件，用用户输入的文件名 */
    if(note_current_file[0] == '\0') 
    {
        const char *fname = lv_textarea_get_text(note_filename_ta);
        if(strlen(fname) > 0) 
        {
            snprintf(note_current_file, sizeof(note_current_file),
                     "/ww/notes/%s.txt", fname);
        } 
        else 
        {
            int idx = 0;
            while(1) 
            {
                snprintf(note_current_file, sizeof(note_current_file),
                         "/ww/notes/note_%d.txt", idx);
                FILE *ftest = fopen(note_current_file, "r");
                if(!ftest) break;
                fclose(ftest);
                idx++;
            }
        }
    }

    /* 写文件：第一行标题，空行，内容 */
    FILE *fp = fopen(note_current_file, "w");
    if(!fp) 
    {
        printf("save failed: cannot open %s\n", note_current_file);
        return;
    }

    fprintf(fp, "%s\n", title);
    fprintf(fp, "%s\n", content);
    fclose(fp);

    /* 如果是新文件，加入链表 */
    if(h_notes && note_current_file[0] != '\0') 
    {
        DNode *pn = h_notes->first;
        int found = 0;
        if(pn) 
        {
            do {
                if(strcmp(pn->data, note_current_file) == 0)
                    { found = 1; break; }
                pn = pn->next;
            } while(pn != h_notes->first);
        }
        if(!found) creat_link(h_notes, note_current_file);
    }

    note_is_modified = 0;

    /* 返回列表并刷新 */
    lv_obj_delete(note_editor_win);
    note_editor_win = NULL;
    lv_obj_remove_flag(Note_win, LV_OBJ_FLAG_HIDDEN);

    note_refresh_list();

    /* 提示保存成功 */
    lv_obj_t * mbox = lv_msgbox_create(NULL);
    lv_obj_set_width(mbox, 400);
    lv_obj_set_style_bg_color(mbox, lv_color_hex(0x0C1118), 0);
    lv_obj_set_style_border_color(mbox, lv_color_hex(0x2B8CFF), 0);
    lv_obj_set_style_border_width(mbox, 1, 0);
    lv_obj_set_style_radius(mbox, 8, 0);
    lv_obj_set_style_text_color(mbox, lv_color_hex(0xFFFFFF), 0);
    lv_msgbox_add_title(mbox, "NOTE SAVED");
    lv_msgbox_add_text(mbox, "Your changes were saved successfully.");
    lv_obj_t * btn_close = lv_msgbox_add_close_button(mbox);
    note_style_button(btn_close);

    /* 2秒后自动关闭 */
    lv_timer_t * timer = lv_timer_create(msgbox_close_cb, 2000, mbox);
    lv_timer_set_repeat_count(timer, 1);


}

/* 
 *  记事本 - 删除
                                 */
/* 删除确认的回调 */
static void note_delete_confirm_cb(lv_event_t * ev)
{
    lv_obj_t * mbox = lv_event_get_user_data(ev);
    if(mbox) lv_msgbox_close(mbox);

    /* 删除文件 */
    remove(note_current_file);

    /* 从链表删除 */
    if(h_notes && h_notes->first) {
        DNode *pn = h_notes->first;
        do 
        {
            if(strcmp(pn->data, note_current_file) == 0) 
            {
                delete_link(h_notes, pn);
                break;
            }
            pn = pn->next;
        } while(pn != h_notes->first);
    }

    /* 回到列表，刷新 */
    if(note_editor_win) 
    {
        lv_obj_delete(note_editor_win);
        note_editor_win = NULL;
    }
    lv_obj_remove_flag(Note_win, LV_OBJ_FLAG_HIDDEN);
    note_refresh_list();
}

/* ===== 取消删除记事本 ===== */
static void note_delete_cancel_cb(lv_event_t * ev)
{
    lv_obj_t * mbox = lv_event_get_user_data(ev);
    if(mbox) lv_msgbox_close(mbox);
}

/* ===== 显示记事本删除确认框 ===== */
static void note_delete_cb(lv_event_t * e)
{
    if(note_current_file[0] == '\0') return;

    /* 确认删除的 msgbox */
    lv_obj_t * mbox = lv_msgbox_create(NULL);
    lv_obj_set_width(mbox, 460);
    lv_obj_set_style_bg_color(mbox, lv_color_hex(0x0C1118), 0);
    lv_obj_set_style_border_color(mbox, lv_color_hex(0x263445), 0);
    lv_obj_set_style_border_width(mbox, 1, 0);
    lv_obj_set_style_radius(mbox, 8, 0);
    lv_obj_set_style_text_color(mbox, lv_color_hex(0xFFFFFF), 0);
    lv_msgbox_add_title(mbox, "DELETE NOTE");
    lv_msgbox_add_text(mbox, "This action cannot be undone.");

    /* 确定按钮 */
    lv_obj_t * btn_ok = lv_msgbox_add_footer_button(mbox, "Delete");
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0xB3261E), 0);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x8E1B16), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btn_ok, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(btn_ok, 8, 0);
    lv_obj_add_event_cb(btn_ok, note_delete_confirm_cb, LV_EVENT_CLICKED, mbox);

    /* 取消按钮 */
    lv_obj_t * btn_cancel = lv_msgbox_add_footer_button(mbox, "Cancel");
    note_style_button(btn_cancel);
    lv_obj_add_event_cb(btn_cancel, note_delete_cancel_cb, LV_EVENT_CLICKED, mbox);
}

/* 
    记事本 - 返回列表
 *                      */
static void note_back_list_cb(lv_event_t * e)
{
    lv_obj_delete(note_editor_win);
    note_editor_win = NULL;
    lv_obj_remove_flag(Note_win, LV_OBJ_FLAG_HIDDEN);
}

/*  
    记事本 - 键盘绑定
 *                      */
static void note_keyboard_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    lv_obj_t * kb = (lv_obj_t *)lv_event_get_user_data(e);

    if(code == LV_EVENT_PRESSED || code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(kb);
    }

    if(code == LV_EVENT_READY) {
        /* 回车：文件名→标题→内容 */
        if(ta == note_filename_ta) {
            lv_keyboard_set_textarea(kb, note_title_ta);
        }
        else if(ta == note_title_ta) {
            lv_keyboard_set_textarea(kb, note_content_ta);
        }
        else {
            lv_keyboard_set_textarea(kb, NULL);
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if(code == LV_EVENT_CANCEL) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}















/* ===== 音乐播放器入口按钮回调 ===== */
static void Music_cb(lv_event_t * e)    //音乐回调函数
{
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * lb = lv_obj_get_child(btn, 0);
    const char * text = lv_label_get_text(lb);
    if(strcmp(text, "Music") == 0)
    {
        lv_obj_add_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
        Music_Interface();
    }
}

/* ===== 关闭音乐播放器并返回主界面 ===== */
static void music_back_to_main_cb(lv_event_t * e)
{
    lv_obj_t * win = lv_event_get_user_data(e);

    lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
    lv_obj_delete_async(win);

    music_win = NULL;
    music_label = NULL;
    music_slider = NULL;
    play_btn = NULL;
    prev_btn = NULL;
    next_btn = NULL;
    music_status_label = NULL;
    music_counter_label = NULL;
}

/* ===== 设置音乐播放器按钮样式 ===== */
static void music_style_button(lv_obj_t * button)
{
    lv_obj_set_style_bg_color(button, lv_color_hex(0x086CD9), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x0454AD), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, lv_color_hex(0x2B8CFF), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_text_color(button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_outline_color(button, lv_color_hex(0x9ACBFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(button, 2, LV_STATE_FOCUSED);
}

/* ===== 更新当前音乐信息 ===== */
static void music_update_track_info(void)
{
    if(music_label == NULL || music_counter_label == NULL || music_status_label == NULL) return;

    if(music_current == NULL || h_music == NULL || h_music->num <= 0) {
        lv_label_set_text(music_label, "No music found");
        lv_label_set_text(music_counter_label, "0 TRACKS");
        lv_label_set_text(music_status_label, "EMPTY LIBRARY");
        return;
    }

    char * pname = strrchr(music_current->data, '/');
    lv_label_set_text(music_label, pname != NULL ? pname + 1 : music_current->data);
    lv_label_set_text_fmt(music_counter_label, "%d TRACKS", h_music->num);
    lv_label_set_text(music_status_label, music_is_playing ? "PLAYING" : "READY");
}

/* ===== 创建音乐播放器界面 ===== */
void Music_Interface(void)
{
    music_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(music_win, 1024, 600);
    lv_obj_set_pos(music_win, 0, 0);
    lv_obj_set_style_bg_color(music_win, lv_color_hex(0x05070B), 0);
    lv_obj_set_style_bg_opa(music_win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(music_win, 0, 0);
    lv_obj_set_style_radius(music_win, 0, 0);
    lv_obj_set_style_pad_all(music_win, 0, 0);
    lv_obj_remove_flag(music_win, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * btn_back = lv_button_create(music_win);
    lv_obj_set_size(btn_back, 52, 48);
    lv_obj_set_pos(btn_back, 48, 24);
    music_style_button(btn_back);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, music_back_to_main_cb, LV_EVENT_CLICKED, music_win);

    lv_obj_t * title = lv_label_create(music_win);
    lv_label_set_text(title, "MUSIC");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_30, 0);
    lv_obj_set_pos(title, 124, 31);

    music_counter_label = lv_label_create(music_win);
    lv_label_set_text(music_counter_label, "0 TRACKS");
    lv_obj_set_style_text_color(music_counter_label, lv_color_hex(0x8B9AAA), 0);
    lv_obj_set_style_text_font(music_counter_label, &lv_font_montserrat_16, 0);
    lv_obj_align(music_counter_label, LV_ALIGN_TOP_RIGHT, -48, 39);

    lv_obj_t * header_rule = lv_obj_create(music_win);
    lv_obj_set_size(header_rule, 928, 1);
    lv_obj_set_pos(header_rule, 48, 95);
    lv_obj_set_style_bg_color(header_rule, lv_color_hex(0x202833), 0);
    lv_obj_set_style_bg_opa(header_rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_rule, 0, 0);
    lv_obj_remove_flag(header_rule, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * cover = lv_obj_create(music_win);
    lv_obj_set_size(cover, 320, 320);
    lv_obj_set_pos(cover, 64, 126);
    lv_obj_set_style_bg_color(cover, lv_color_hex(0x0C1825), 0);
    lv_obj_set_style_bg_opa(cover, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cover, lv_color_hex(0x1C5E9E), 0);
    lv_obj_set_style_border_width(cover, 1, 0);
    lv_obj_set_style_radius(cover, 8, 0);
    lv_obj_set_style_pad_all(cover, 0, 0);
    lv_obj_remove_flag(cover, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * cover_icon = lv_label_create(cover);
    lv_label_set_text(cover_icon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(cover_icon, lv_color_hex(0x1687FF), 0);
    lv_obj_set_style_text_font(cover_icon, &lv_font_montserrat_48, 0);
    lv_obj_center(cover_icon);

    lv_obj_t * playing_caption = lv_label_create(music_win);
    lv_label_set_text(playing_caption, "NOW PLAYING");
    lv_obj_set_style_text_color(playing_caption, lv_color_hex(0x6F9DC5), 0);
    lv_obj_set_style_text_font(playing_caption, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(playing_caption, 424, 148);

    music_label = lv_label_create(music_win);
    lv_label_set_text(music_label, "No music found");
    lv_label_set_long_mode(music_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_size(music_label, 552, 52);
    lv_obj_set_pos(music_label, 424, 188);
    lv_obj_set_style_text_color(music_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(music_label, &lv_myfont_30, 0);

    lv_obj_t * source_label = lv_label_create(music_win);
    lv_label_set_text(source_label, "LOCAL LIBRARY");
    lv_obj_set_style_text_color(source_label, lv_color_hex(0x718196), 0);
    lv_obj_set_style_text_font(source_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(source_label, 424, 252);

    lv_obj_t * track_rule = lv_obj_create(music_win);
    lv_obj_set_size(track_rule, 552, 1);
    lv_obj_set_pos(track_rule, 424, 286);
    lv_obj_set_style_bg_color(track_rule, lv_color_hex(0x263241), 0);
    lv_obj_set_style_bg_opa(track_rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(track_rule, 0, 0);
    lv_obj_remove_flag(track_rule, LV_OBJ_FLAG_SCROLLABLE);

    music_slider = lv_slider_create(music_win);
    lv_obj_set_size(music_slider, 552, 10);
    lv_obj_set_pos(music_slider, 424, 318);
    lv_obj_set_style_bg_color(music_slider, lv_color_hex(0x1B2735), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(music_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(music_slider, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_color(music_slider, lv_color_hex(0x1687FF), LV_PART_INDICATOR);
    lv_obj_set_style_radius(music_slider, 5, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(music_slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_slider_set_range(music_slider, 0, 100);
    lv_slider_set_value(music_slider, 0, LV_ANIM_OFF);

    music_status_label = lv_label_create(music_win);
    lv_label_set_text(music_status_label, "READY");
    lv_obj_set_style_text_color(music_status_label, lv_color_hex(0x6F9DC5), 0);
    lv_obj_set_style_text_font(music_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(music_status_label, 424, 348);

    lv_obj_t * control_rule = lv_obj_create(music_win);
    lv_obj_set_size(control_rule, 896, 1);
    lv_obj_set_pos(control_rule, 64, 466);
    lv_obj_set_style_bg_color(control_rule, lv_color_hex(0x202833), 0);
    lv_obj_set_style_bg_opa(control_rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(control_rule, 0, 0);
    lv_obj_remove_flag(control_rule, LV_OBJ_FLAG_SCROLLABLE);

    play_btn = lv_button_create(music_win);
    lv_obj_set_size(play_btn, 88, 72);
    lv_obj_align(play_btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    music_style_button(play_btn);
    lv_obj_t * play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, music_is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(play_label, &lv_font_montserrat_28, 0);
    lv_obj_center(play_label);
    lv_obj_add_event_cb(play_btn, music_play_pause_cb, LV_EVENT_CLICKED, NULL);

    prev_btn = lv_button_create(music_win);
    lv_obj_set_size(prev_btn, 64, 56);
    lv_obj_align(prev_btn, LV_ALIGN_BOTTOM_MID, -116, -48);
    music_style_button(prev_btn);
    lv_obj_t * prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(prev_label, &lv_font_montserrat_22, 0);
    lv_obj_center(prev_label);
    lv_obj_add_event_cb(prev_btn, music_prev_cb, LV_EVENT_CLICKED, NULL);

    next_btn = lv_button_create(music_win);
    lv_obj_set_size(next_btn, 64, 56);
    lv_obj_align(next_btn, LV_ALIGN_BOTTOM_MID, 116, -48);
    music_style_button(next_btn);
    lv_obj_t * next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(next_label, &lv_font_montserrat_22, 0);
    lv_obj_center(next_label);
    lv_obj_add_event_cb(next_btn, music_next_cb, LV_EVENT_CLICKED, NULL);

    if(h_music != NULL && h_music->first != NULL && h_music->num > 0) {
        if(music_current == NULL) {
            music_current = h_music->first;
        }
    }
    else {
        lv_obj_add_state(play_btn, LV_STATE_DISABLED);
        lv_obj_add_state(prev_btn, LV_STATE_DISABLED);
        lv_obj_add_state(next_btn, LV_STATE_DISABLED);
    }

    music_update_track_info();
}

/* ===== 停止音乐播放进程 ===== */
static void music_kill(void)
{
    if(music_pid > 0) 
    {
        kill(music_pid, SIGTERM);
        usleep(50000);
        music_pid = 0;
    }
}

/* ===== 启动当前音乐播放 ===== */
static void music_start_play(void)
{
    if(!music_current) return;

    music_kill();

    // 只显示文件名
    char *pname = strrchr(music_current->data, '/');
    lv_label_set_text(music_label, pname ? pname + 1 : music_current->data);

    // 启动新播放
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mpg123 %s &", music_current->data);
    system(cmd);
    usleep(150000);

    // 获取 PID
    char buf[32] = {0};
    FILE * fp = popen("pgrep -xn mpg123 2>/dev/null | tail -1", "r");
    if(fp) 
    {
        if(fgets(buf, sizeof(buf), fp))
            music_pid = atoi(buf);
        pclose(fp);
    }

    if(music_pid > 0) 
    {
        music_is_playing = 1;
        lv_label_set_text(lv_obj_get_child(play_btn, 0), LV_SYMBOL_PAUSE);
        lv_slider_set_value(music_slider, 0, LV_ANIM_OFF);
    }

    music_update_track_info();
}

/* ===== 处理音乐播放暂停操作 ===== */
static void music_play_pause_cb(lv_event_t * e)
{
    if(!music_current) return;

    if(music_is_playing) 
    {
        // 暂停 → 杀掉进程
        music_kill();
        music_is_playing = 0;
        lv_label_set_text(lv_obj_get_child(play_btn, 0), LV_SYMBOL_PLAY);
        music_update_track_info();
    } else 
    {
        // 播放 → 重新启动
        music_start_play();
    }
}

/* ===== 播放下一首音乐 ===== */
static void music_next_cb(lv_event_t * e)
{
    if(!music_current) 
        return;
    music_current = music_current->next;
    music_start_play();
}

/* ===== 播放上一首音乐 ===== */
static void music_prev_cb(lv_event_t * e)
{
    if(!music_current) return;
    music_current = music_current->prev;
    music_start_play();
}













/* ===== 电子相册入口按钮回调 ===== */
static void Album_cb(lv_event_t * e)    //相册回调函数
{
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * lb = lv_obj_get_child(btn, 0);
    const char * text = lv_label_get_text(lb);
    if(strcmp(text, "Album") == 0)
    {
        lv_obj_add_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
        Album_Interface();
    }
    

}

/* 旧版电子相册代码：完整保留，仅停止参与编译。 */
#if 0
/* ===== 旧版电子相册界面（保留代码） ===== */
void Album_Interface(void)
{

    album_content = lv_obj_create(lv_screen_active());
    lv_obj_set_size(album_content, 1024, 600);
    lv_obj_set_pos(album_content, 0, 0);

    /*
        背景的颜色
    */
    static lv_style_t s_bg;
    lv_style_init(&s_bg);
    lv_style_set_bg_color(&s_bg, lv_color_hex(0x000000));
    lv_style_set_bg_opa(&s_bg, LV_OPA_100);
    lv_obj_add_style(album_content, &s_bg, LV_STATE_DEFAULT);

    /*
        设置下一张照片的按键
    */
    lv_obj_t * button_next = lv_button_create(album_content);
    lv_obj_set_size(button_next, 200, 100);
    lv_obj_set_align(button_next, LV_ALIGN_BOTTOM_RIGHT);

    //创建一个标签
    lv_obj_t * lb_next = lv_label_create(button_next);
    lv_label_set_text(lb_next, "Next");
    lv_obj_set_align(lb_next, LV_ALIGN_CENTER);
    lv_obj_set_width(lb_next, 100);

    //为标签中的内容设置一种样式
    static lv_style_t  lbn_s;
    lv_style_init(&lbn_s);
    lv_style_set_text_color(&lbn_s, lv_color_hex(0xFFFFFF));
    lv_style_set_text_opa(&lbn_s, LV_OPA_70);
    lv_style_set_text_font(&lbn_s, &lv_font_montserrat_40);
    lv_obj_add_style(lb_next, &lbn_s, LV_STATE_DEFAULT);


    /*
        设置上一张照片的按键
    */
    lv_obj_t * button_last = lv_button_create(album_content);
    lv_obj_set_size(button_last, 200, 100);
    lv_obj_set_align(button_last, LV_ALIGN_BOTTOM_LEFT);

    //创建一个标签
    lv_obj_t * lb_last = lv_label_create(button_last);
    lv_label_set_text(lb_last, "Last");
    lv_obj_set_align(lb_last, LV_ALIGN_CENTER);
    lv_obj_set_width(lb_last, 100);

    //为标签中的内容设置一种样式
    static lv_style_t  lbl_s;
    lv_style_init(&lbl_s);
    lv_style_set_text_color(&lbl_s, lv_color_hex(0xFFFFFF));
    lv_style_set_text_opa(&lbl_s, LV_OPA_70);
    lv_style_set_text_font(&lbl_s, &lv_font_montserrat_40);
    lv_obj_add_style(lb_last, &lbl_s, LV_STATE_DEFAULT);


    /*
        在屏幕中间的下方设置一个窗口显示文字
        "Welcome To My Digital Photo Album!"
    */
    lv_obj_t * lbg_bottom = lv_label_create(album_content);
    lv_label_set_text(lbg_bottom, "Welcome To My Digital Photo Album!");
    lv_obj_align(lbg_bottom, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_label_set_long_mode(lbg_bottom, LV_LABEL_LONG_SCROLL);
    lv_obj_set_width(lbg_bottom, 400);
    
    //为文字设置一种样式
    static lv_style_t sbg_bottom;
    lv_style_init(&sbg_bottom);
    lv_style_set_text_color(&sbg_bottom, lv_color_hex(0xFFFFFF));
    lv_style_set_text_opa(&sbg_bottom, LV_OPA_40);
    lv_style_set_text_font(&sbg_bottom, &lv_font_montserrat_30);
    lv_obj_add_style(lbg_bottom, &sbg_bottom, LV_STATE_DEFAULT);

    //显示第一张图片
    if (h != NULL && h->first != NULL) {
        p = h->first;
        char lv_path[1024];
        sprintf(lv_path, "A:%s", p->data);
        g_bmp = lv_image_create(album_content);
        lv_image_set_src(g_bmp, lv_path);
        lv_obj_set_pos(g_bmp, 0, 0);
    }

    //当按下按钮时，切换照片
    lv_obj_add_event_cb(button_next, pic_next, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(button_last, pic_last, LV_EVENT_CLICKED, NULL);

    lv_obj_move_foreground(button_next);
    lv_obj_move_foreground(button_last);

    lv_obj_t * btn_back = lv_button_create(album_content);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_center(lbl_back);
    lv_obj_set_size(btn_back, 100, 50);
    lv_obj_set_pos(btn_back, 10, 10);
    lv_obj_add_event_cb(btn_back, back_to_main_cb, LV_EVENT_CLICKED, album_content);

}

/* ===== 旧版相册首图显示函数（保留代码） ===== */
void show_bmp(void)
{
    if (h == NULL || h->first == NULL) {
        printf("No pictures found!\n");
        return;
    }
    p = h->first;
    char lv_path[1024];
    sprintf(lv_path, "A:%s", p->data);
    g_bmp = lv_image_create(lv_screen_active());
    lv_image_set_src(g_bmp, lv_path);
    lv_obj_set_pos(g_bmp, 0, 0);
}


/* ===== 旧版相册下一张回调（保留代码） ===== */
static void pic_next(lv_event_t * e)
{
    if (h == NULL || h->first == NULL) return;

    lv_obj_t * bu = e->original_target;
    lv_obj_t *lb = lv_obj_get_child(bu,0);
    char * str = lv_label_get_text(lb);

    lv_obj_remove_flag(g_bmp,LV_OBJ_FLAG_HIDDEN);

    if(strcmp(str,"Next") != 0) return;

    p = p->next;
    char lv_path[1024];
    sprintf(lv_path, "A:%s", p->data);

    lv_image_set_src(g_bmp, lv_path);
    lv_obj_set_pos(g_bmp, 0, 0);
}

/* ===== 旧版相册上一张回调（保留代码） ===== */
static void pic_last(lv_event_t * e)
{
    if (h == NULL || h->first == NULL) return;

    lv_obj_t * bu = e->original_target;
    lv_obj_t *lb = lv_obj_get_child(bu,0);
    char * str = lv_label_get_text(lb);
    
    lv_obj_remove_flag(g_bmp,LV_OBJ_FLAG_HIDDEN);

    if(strcmp(str,"Last") != 0) return;

    p = p->prev;
    char lv_path[1024];
    sprintf(lv_path, "A:%s", p->data);

    lv_image_set_src(g_bmp, lv_path);
    lv_obj_set_pos(g_bmp, 0, 0);
}
#endif

/* ===== 设置电子相册按钮样式 ===== */
static void album_style_button(lv_obj_t * button)
{
    lv_obj_set_style_bg_color(button, lv_color_hex(0x086CD9), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x0454AD), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, lv_color_hex(0x2B8CFF), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_text_color(button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(button, &lv_font_montserrat_24, 0);
    lv_obj_set_style_outline_color(button, lv_color_hex(0x9ACBFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(button, 2, LV_STATE_FOCUSED);
}

/* ===== 更新电子相册当前图片 ===== */
static void album_update_image(void)
{
    if(h == NULL || h->first == NULL || p == NULL || g_bmp == NULL) return;

    char lv_path[1024];
    snprintf(lv_path, sizeof(lv_path), "A:%s", p->data);
    lv_image_set_src(g_bmp, lv_path);

    /* BMP uses line-by-line decoding; keep the scaling attempt for reference. */
#if 0
    lv_image_header_t image_info;
    if(lv_image_decoder_get_info(lv_path, &image_info) == LV_RESULT_OK &&
       image_info.w > 0 && image_info.h > 0) {
        uint32_t scale_w = (920U * 256U) / image_info.w;
        uint32_t scale_h = (390U * 256U) / image_info.h;
        uint32_t scale = scale_w < scale_h ? scale_w : scale_h;
        if(scale > 256U) scale = 256U;
        if(scale == 0U) scale = 1U;
        lv_image_set_scale(g_bmp, scale);
    }
#endif

    lv_image_set_scale(g_bmp, LV_SCALE_NONE);
    lv_obj_center(g_bmp);
    lv_obj_remove_flag(g_bmp, LV_OBJ_FLAG_HIDDEN);

    const char * filename = strrchr(p->data, '/');
    filename = filename != NULL ? filename + 1 : p->data;
    lv_label_set_text(album_filename_label, filename);
    lv_label_set_text_fmt(album_counter_label, "%d / %d", album_current_index, h->num);
}

/* ===== 关闭电子相册并返回主界面 ===== */
static void album_back_cb(lv_event_t * e)
{
    lv_obj_t * win = lv_event_get_user_data(e);
    lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
    lv_obj_delete_async(win);
    album_content = NULL;
    g_bmp = NULL;
    album_filename_label = NULL;
    album_counter_label = NULL;
    album_current_index = 0;
}

/* ===== 创建电子相册界面 ===== */
void Album_Interface(void)
{
    album_content = lv_obj_create(lv_screen_active());
    lv_obj_set_size(album_content, 1024, 600);
    lv_obj_set_pos(album_content, 0, 0);
    lv_obj_set_style_bg_color(album_content, lv_color_hex(0x05070B), 0);
    lv_obj_set_style_bg_opa(album_content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(album_content, 0, 0);
    lv_obj_set_style_radius(album_content, 0, 0);
    lv_obj_set_style_pad_all(album_content, 0, 0);
    lv_obj_remove_flag(album_content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * btn_back = lv_button_create(album_content);
    lv_obj_set_size(btn_back, 52, 48);
    lv_obj_set_pos(btn_back, 32, 16);
    album_style_button(btn_back);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, album_back_cb, LV_EVENT_CLICKED, album_content);

    lv_obj_t * title = lv_label_create(album_content);
    lv_label_set_text(title, "PHOTO ALBUM");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_30, 0);
    lv_obj_set_pos(title, 108, 24);

    album_counter_label = lv_label_create(album_content);
    lv_label_set_text(album_counter_label, "0 / 0");
    lv_obj_set_style_text_color(album_counter_label, lv_color_hex(0x8B9AAA), 0);
    lv_obj_set_style_text_font(album_counter_label, &lv_font_montserrat_18, 0);
    lv_obj_align(album_counter_label, LV_ALIGN_TOP_RIGHT, -32, 31);

    lv_obj_t * header_rule = lv_obj_create(album_content);
    lv_obj_set_size(header_rule, 960, 1);
    lv_obj_set_pos(header_rule, 32, 77);
    lv_obj_set_style_bg_color(header_rule, lv_color_hex(0x202833), 0);
    lv_obj_set_style_bg_opa(header_rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_rule, 0, 0);
    lv_obj_remove_flag(header_rule, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * image_view = lv_obj_create(album_content);
    lv_obj_set_size(image_view, 960, 424);
    lv_obj_set_pos(image_view, 32, 88);
    lv_obj_set_style_bg_color(image_view, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(image_view, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(image_view, lv_color_hex(0x202C3A), 0);
    lv_obj_set_style_border_width(image_view, 1, 0);
    lv_obj_set_style_radius(image_view, 8, 0);
    lv_obj_set_style_pad_all(image_view, 0, 0);
    lv_obj_remove_flag(image_view, LV_OBJ_FLAG_SCROLLABLE);

    g_bmp = lv_image_create(image_view);

    lv_obj_t * footer = lv_obj_create(album_content);
    lv_obj_set_size(footer, 960, 60);
    lv_obj_set_pos(footer, 32, 524);
    lv_obj_set_style_bg_color(footer, lv_color_hex(0x0C1118), 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(footer, lv_color_hex(0x202C3A), 0);
    lv_obj_set_style_border_width(footer, 1, 0);
    lv_obj_set_style_radius(footer, 8, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_remove_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    album_filename_label = lv_label_create(footer);
    lv_label_set_text(album_filename_label, "No images found");
    lv_label_set_long_mode(album_filename_label, LV_LABEL_LONG_DOT);
    lv_obj_set_size(album_filename_label, 680, 26);
    lv_obj_set_style_text_color(album_filename_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(album_filename_label, &lv_font_montserrat_18, 0);
    lv_obj_align(album_filename_label, LV_ALIGN_LEFT_MID, 18, 0);

    lv_obj_t * button_prev = lv_button_create(footer);
    lv_obj_set_size(button_prev, 58, 44);
    lv_obj_align(button_prev, LV_ALIGN_RIGHT_MID, -82, 0);
    album_style_button(button_prev);
    lv_obj_t * icon_prev = lv_label_create(button_prev);
    lv_label_set_text(icon_prev, LV_SYMBOL_PREV);
    lv_obj_center(icon_prev);
    lv_obj_add_event_cb(button_prev, pic_last, LV_EVENT_CLICKED, NULL);

    lv_obj_t * button_next = lv_button_create(footer);
    lv_obj_set_size(button_next, 58, 44);
    lv_obj_align(button_next, LV_ALIGN_RIGHT_MID, -12, 0);
    album_style_button(button_next);
    lv_obj_t * icon_next = lv_label_create(button_next);
    lv_label_set_text(icon_next, LV_SYMBOL_NEXT);
    lv_obj_center(icon_next);
    lv_obj_add_event_cb(button_next, pic_next, LV_EVENT_CLICKED, NULL);

    if(h != NULL && h->first != NULL && h->num > 0) {
        p = h->first;
        album_current_index = 1;
        album_update_image();
    }
    else {
        lv_obj_add_state(button_prev, LV_STATE_DISABLED);
        lv_obj_add_state(button_next, LV_STATE_DISABLED);
        lv_obj_add_flag(g_bmp, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t * empty_label = lv_label_create(image_view);
        lv_label_set_text(empty_label, "No images available");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x6F8193), 0);
        lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_22, 0);
        lv_obj_center(empty_label);
    }
}

/* ===== 显示电子相册首张图片 ===== */
void show_bmp(void)
{
    if(h == NULL || h->first == NULL || g_bmp == NULL) return;
    p = h->first;
    album_current_index = 1;
    album_update_image();
}

/* ===== 显示电子相册下一张图片 ===== */
static void pic_next(lv_event_t * e)
{
    LV_UNUSED(e);
    if(h == NULL || h->first == NULL || h->num <= 0 || p == NULL) return;
    p = p->next;
    album_current_index = album_current_index >= h->num ? 1 : album_current_index + 1;
    album_update_image();
}

/* ===== 显示电子相册上一张图片 ===== */
static void pic_last(lv_event_t * e)
{
    LV_UNUSED(e);
    if(h == NULL || h->first == NULL || h->num <= 0 || p == NULL) return;
    p = p->prev;
    album_current_index = album_current_index <= 1 ? h->num : album_current_index - 1;
    album_update_image();
}

/* ===== 向循环双向链表添加节点 ===== */
void creat_link(Head *h,char*file_name)
{
	DNode *pnew=malloc(sizeof(DNode));
    pnew->data = (char*)malloc(strlen(file_name) + 1);
    strcpy(pnew->data, file_name);
	pnew->next=NULL;
	pnew->prev=NULL;
	if(h->first==NULL)
	{
		h->first=pnew;
		h->last=pnew;
		h->first->prev=h->last;
		h->last->next=h->first;
	}
	else
	{
		h->last->next=pnew;
		pnew->prev=h->last;
		h->last=pnew;
		h->last->next=h->first;
		h->first->prev=h->last;
	}
	h->num++;
}

/* ===== 删除循环双向链表节点 ===== */
void delete_link(Head *h, DNode *pn)
{
    if(!h || !pn || h->num == 0) return;

    if(h->num == 1) {
        h->first = NULL;
        h->last = NULL;
    } else {
        pn->prev->next = pn->next;
        pn->next->prev = pn->prev;
        if(pn == h->first) h->first = pn->next;
        if(pn == h->last)  h->last  = pn->prev;
    }
    free(pn->data);
    free(pn);
    h->num--;
}

/* ===== 递归扫描图片文件 ===== */
void find_pic(char*path,Head *h)
{
	DIR * dirp = opendir(path);
	if(dirp==NULL)
	{
		perror("opendir error");
	 	return ;
	}
	while(1)
	{
		struct dirent * dt = readdir(dirp);
		if( dt == NULL ) //读取失败或者读完了
        {
        	perror("readdir");
            break;
        }
        //dt==>目录项===>文件
        char file_name[1024] = {0}; //保存文件的完整路径名
    	sprintf(file_name,"%s/%s",path,dt->d_name);
        struct stat st;
        if(stat(file_name,&st) == -1)
        {
            perror("stat error");
            continue;
        }
        if( S_ISDIR(st.st_mode) )
        {   
            //"."  ".." 也是目录要排除掉
            if( strcmp(dt->d_name,".")==0 || strcmp(dt->d_name,"..")==0 )
            {
                continue;
            }
            //是目录文件
            find_pic(file_name,h);
        }
        else 
        {
        	if(strcmp(dt->d_name+strlen(dt->d_name)-4,".bmp")==0 || strcmp(dt->d_name+strlen(dt->d_name)-4,".jpg")==0||strcmp(dt->d_name+strlen(dt->d_name)-4,".png")==0 || strcmp(dt->d_name+strlen(dt->d_name)-4,".gif")==0)
			{
        		creat_link(h,file_name);
        	}
        }
	 }
	 closedir(dirp);
}

/* ===== 递归扫描音乐文件 ===== */
void find_music(char *path, Head *h)    //扫描音乐文件
{
    DIR * dirp = opendir(path);
    if(dirp == NULL) {
        perror("opendir music");
        return;
    }
    while(1) {
        struct dirent * dt = readdir(dirp);
        if(dt == NULL) break;

        char file_name[1024] = {0};
        sprintf(file_name, "%s/%s", path, dt->d_name);
        struct stat st;
        if(stat(file_name, &st) == -1) {
            perror("stat error");
            continue;
        }
        if(S_ISDIR(st.st_mode)) {
            if(strcmp(dt->d_name, ".") == 0 || strcmp(dt->d_name, "..") == 0)
                continue;
            find_music(file_name, h);
        } else {
            int len = strlen(dt->d_name);
            if(len > 4 && strcmp(dt->d_name + len - 4, ".mp3") == 0)
                creat_link(h, file_name);
        }
    }
    closedir(dirp);
}

/* ===== 递归扫描视频文件 ===== */
void find_video(char *path, Head *h)    //扫描视频文件
{
    DIR * dirp = opendir(path);
    if(dirp == NULL) {
        perror("opendir video");
        return;
    }

    while(1) {
        struct dirent * dt = readdir(dirp);
        if(dt == NULL) break;

        if(strcmp(dt->d_name, ".") == 0 || strcmp(dt->d_name, "..") == 0)
            continue;

        char file_name[1024] = {0};
        int path_length = snprintf(file_name, sizeof(file_name), "%s/%s", path, dt->d_name);
        if(path_length < 0 || path_length >= (int)sizeof(file_name))
            continue;

        struct stat st;
        if(stat(file_name, &st) == -1) {
            perror("stat video");
            continue;
        }

        if(S_ISDIR(st.st_mode)) {
            find_video(file_name, h);
            continue;
        }

        const char * extension = strrchr(dt->d_name, '.');
        if(extension != NULL &&
           (strcasecmp(extension, ".mp4") == 0 ||
            strcasecmp(extension, ".avi") == 0 ||
            strcasecmp(extension, ".mkv") == 0 ||
            strcasecmp(extension, ".mov") == 0 ||
            strcasecmp(extension, ".flv") == 0 ||
            strcasecmp(extension, ".mpeg") == 0 ||
            strcasecmp(extension, ".mpg") == 0 ||
            strcasecmp(extension, ".wmv") == 0 ||
            strcasecmp(extension, ".ts") == 0)) {
            creat_link(h, file_name);
        }
    }

    closedir(dirp);
}

/* ===== 递归扫描记事本文件 ===== */
void find_notes(char *path, Head *h)    //扫描txt文件
{
    DIR *dirp = opendir(path);
    if(dirp == NULL) return;
    while(1) 
    {
        struct dirent *dt = readdir(dirp);
        if(dt == NULL) break;
        char file_name[1024] = {0};
        sprintf(file_name, "%s/%s", path, dt->d_name);
        struct stat st;
        if(stat(file_name, &st) == -1) 
            continue;
        if(S_ISDIR(st.st_mode)) 
        {
            if(strcmp(dt->d_name, ".")==0 || strcmp(dt->d_name, "..")==0) 
                continue;
            find_notes(file_name, h);
        } 
        else 
        {
            int len = strlen(dt->d_name);
            if(len > 4 && strcmp(dt->d_name + len - 4, ".txt") == 0)
                creat_link(h, file_name);
        }
    }
    closedir(dirp);
}











/* ===== 日历入口按钮回调 ===== */
static void Calendar_cb(lv_event_t * e)    // 日历按钮回调
{
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * lb = lv_obj_get_child(btn, 0);
    const char * text = lv_label_get_text(lb);
    if(strcmp(text, "Calendar") == 0)
    {
        lv_obj_add_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
        Calendar_Interface();
    }
}

/* ===== 处理日历日期选择事件 ===== */
static void event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_current_target(e);

    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_calendar_date_t date;
        if(lv_calendar_get_pressed_date(obj, &date) == LV_RESULT_OK) {
            lv_obj_t * selected_date = lv_event_get_user_data(e);
            if(selected_date != NULL) {
                lv_label_set_text_fmt(selected_date, "%04d / %02d / %02d",
                                      date.year, date.month, date.day);
            }
            LV_LOG_USER("Clicked date: %02d.%02d.%d", date.day, date.month, date.year);
        }
    }
}

/* ===== 隐藏当前界面并返回主界面 ===== */
static void back_to_main_cb(lv_event_t * e)
{
    lv_obj_t * win = lv_event_get_user_data(e);
    lv_obj_add_flag(win, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
}

/* ===== 关闭日历并返回主界面 ===== */
static void calendar_back_to_main_cb(lv_event_t * e)
{
    lv_obj_t * win = lv_event_get_user_data(e);
    lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
    lv_obj_delete_async(win);
}

/* ===== 创建日历界面 ===== */
void Calendar_Interface(void)
{
    uint32_t current_year = 2026;
    uint32_t current_month = 1;
    uint32_t current_day = 1;
    time_t now = time(NULL);
    struct tm * now_tm = localtime(&now);
    if(now_tm != NULL) {
        current_year = (uint32_t)(now_tm->tm_year + 1900);
        current_month = (uint32_t)(now_tm->tm_mon + 1);
        current_day = (uint32_t)now_tm->tm_mday;
    }

    lv_obj_t * Calendar_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(Calendar_win, 1024, 600);
    lv_obj_set_pos(Calendar_win, 0, 0);
    lv_obj_set_style_bg_color(Calendar_win, lv_color_hex(0x05070B), 0);
    lv_obj_set_style_bg_opa(Calendar_win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(Calendar_win, 0, 0);
    lv_obj_set_style_radius(Calendar_win, 0, 0);
    lv_obj_set_style_pad_all(Calendar_win, 0, 0);
    lv_obj_remove_flag(Calendar_win, LV_OBJ_FLAG_SCROLLABLE);

    /* Top bar */
    lv_obj_t * btn_back = lv_button_create(Calendar_win);
    lv_obj_set_size(btn_back, 52, 48);
    lv_obj_set_pos(btn_back, 48, 24);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x086CD9), 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x0454AD), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn_back, lv_color_hex(0x2B8CFF), 0);
    lv_obj_set_style_border_width(btn_back, 1, 0);
    lv_obj_set_style_radius(btn_back, 8, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_set_style_outline_color(btn_back, lv_color_hex(0x9ACBFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(btn_back, 2, LV_STATE_FOCUSED);

    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lbl_back, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, calendar_back_to_main_cb, LV_EVENT_CLICKED, Calendar_win);

    lv_obj_t * title = lv_label_create(Calendar_win);
    lv_label_set_text(title, "CALENDAR");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_30, 0);
    lv_obj_set_pos(title, 124, 31);

    lv_obj_t * system_date = lv_label_create(Calendar_win);
    lv_label_set_text_fmt(system_date, "%04d / %02d / %02d",
                          current_year, current_month, current_day);
    lv_obj_set_style_text_color(system_date, lv_color_hex(0x8B9AAA), 0);
    lv_obj_set_style_text_font(system_date, &lv_font_montserrat_18, 0);
    lv_obj_align(system_date, LV_ALIGN_TOP_RIGHT, -48, 37);

    lv_obj_t * header_rule = lv_obj_create(Calendar_win);
    lv_obj_set_size(header_rule, 928, 1);
    lv_obj_set_pos(header_rule, 48, 95);
    lv_obj_set_style_bg_color(header_rule, lv_color_hex(0x202833), 0);
    lv_obj_set_style_bg_opa(header_rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_rule, 0, 0);
    lv_obj_set_style_radius(header_rule, 0, 0);
    lv_obj_remove_flag(header_rule, LV_OBJ_FLAG_SCROLLABLE);

    /* Date information panel */
    lv_obj_t * info_panel = lv_obj_create(Calendar_win);
    lv_obj_set_size(info_panel, 264, 436);
    lv_obj_set_pos(info_panel, 712, 120);
    lv_obj_set_style_bg_color(info_panel, lv_color_hex(0x0C1118), 0);
    lv_obj_set_style_bg_opa(info_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(info_panel, lv_color_hex(0x202C3A), 0);
    lv_obj_set_style_border_width(info_panel, 1, 0);
    lv_obj_set_style_radius(info_panel, 8, 0);
    lv_obj_set_style_pad_all(info_panel, 0, 0);
    lv_obj_remove_flag(info_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * today_caption = lv_label_create(info_panel);
    lv_label_set_text(today_caption, "TODAY");
    lv_obj_set_style_text_color(today_caption, lv_color_hex(0x6F9DC5), 0);
    lv_obj_set_style_text_font(today_caption, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(today_caption, 24, 26);

    lv_obj_t * today_date = lv_label_create(info_panel);
    lv_label_set_text_fmt(today_date, "%04d / %02d / %02d",
                          current_year, current_month, current_day);
    lv_obj_set_style_text_color(today_date, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(today_date, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(today_date, 24, 62);

    lv_obj_t * info_rule = lv_obj_create(info_panel);
    lv_obj_set_size(info_rule, 216, 1);
    lv_obj_set_pos(info_rule, 24, 126);
    lv_obj_set_style_bg_color(info_rule, lv_color_hex(0x263241), 0);
    lv_obj_set_style_bg_opa(info_rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(info_rule, 0, 0);
    lv_obj_set_style_radius(info_rule, 0, 0);
    lv_obj_remove_flag(info_rule, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * selected_caption = lv_label_create(info_panel);
    lv_label_set_text(selected_caption, "SELECTED DATE");
    lv_obj_set_style_text_color(selected_caption, lv_color_hex(0x6F9DC5), 0);
    lv_obj_set_style_text_font(selected_caption, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(selected_caption, 24, 158);

    lv_obj_t * selected_date = lv_label_create(info_panel);
    lv_label_set_text_fmt(selected_date, "%04d / %02d / %02d",
                          current_year, current_month, current_day);
    lv_obj_set_style_text_color(selected_date, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(selected_date, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(selected_date, 24, 194);

    /* Calendar */
    lv_obj_t * calendar = lv_calendar_create(Calendar_win);
    lv_obj_set_size(calendar, 640, 436);
    lv_obj_set_pos(calendar, 48, 120);
    lv_obj_set_style_bg_color(calendar, lv_color_hex(0x0C1118), 0);
    lv_obj_set_style_bg_opa(calendar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(calendar, lv_color_hex(0x202C3A), 0);
    lv_obj_set_style_border_width(calendar, 1, 0);
    lv_obj_set_style_radius(calendar, 8, 0);
    lv_obj_set_style_pad_all(calendar, 14, 0);

    static const char * day_names[7] = {
        "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
    };
    lv_calendar_set_day_names(calendar, day_names);
    lv_calendar_set_today_date(calendar, current_year, current_month, current_day);
    lv_calendar_set_showed_date(calendar, current_year, current_month);

    lv_obj_t * days = lv_calendar_get_btnmatrix(calendar);
    lv_obj_set_style_bg_opa(days, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(days, 0, 0);
    lv_obj_set_style_pad_all(days, 4, 0);
    lv_obj_set_style_pad_row(days, 4, 0);
    lv_obj_set_style_pad_column(days, 4, 0);
    lv_obj_set_style_text_color(days, lv_color_hex(0xEAF2FA), LV_PART_ITEMS);
    lv_obj_set_style_text_font(days, &lv_font_montserrat_18, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(days, lv_color_hex(0x111A25), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(days, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_color(days, lv_color_hex(0x243244), LV_PART_ITEMS);
    lv_obj_set_style_border_width(days, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(days, 6, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(days, lv_color_hex(0x075DBD),
                              LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(days, lv_color_hex(0xFFFFFF),
                                LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(days, lv_color_hex(0x637386),
                                LV_PART_ITEMS | LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(days, lv_color_hex(0x0A1017),
                              LV_PART_ITEMS | LV_STATE_DISABLED);
    lv_obj_set_style_border_color(days, lv_color_hex(0x17212D),
                                  LV_PART_ITEMS | LV_STATE_DISABLED);

#if LV_USE_CALENDAR_HEADER_ARROW
    lv_obj_t * calendar_header = lv_calendar_header_arrow_create(calendar);
    lv_obj_set_height(calendar_header, 58);
    lv_obj_set_style_bg_opa(calendar_header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(calendar_header, 0, 0);
    lv_obj_set_style_pad_hor(calendar_header, 0, 0);
    lv_obj_set_style_pad_top(calendar_header, 0, 0);
    lv_obj_set_style_pad_bottom(calendar_header, 10, 0);
    lv_obj_set_style_pad_column(calendar_header, 12, 0);

    lv_obj_t * month_prev = lv_obj_get_child(calendar_header, 0);
    lv_obj_t * month_title = lv_obj_get_child(calendar_header, 1);
    lv_obj_t * month_next = lv_obj_get_child(calendar_header, 2);
    lv_obj_set_size(month_prev, 46, 40);
    lv_obj_set_size(month_next, 46, 40);
    lv_obj_set_style_bg_color(month_prev, lv_color_hex(0x086CD9), 0);
    lv_obj_set_style_bg_color(month_next, lv_color_hex(0x086CD9), 0);
    lv_obj_set_style_bg_color(month_prev, lv_color_hex(0x0454AD), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(month_next, lv_color_hex(0x0454AD), LV_STATE_PRESSED);
    lv_obj_set_style_radius(month_prev, 8, 0);
    lv_obj_set_style_radius(month_next, 8, 0);
    lv_obj_set_style_shadow_width(month_prev, 0, 0);
    lv_obj_set_style_shadow_width(month_next, 0, 0);
    lv_obj_set_style_bg_image_recolor(month_prev, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_image_recolor(month_next, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_image_recolor_opa(month_prev, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_image_recolor_opa(month_next, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(month_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(month_title, &lv_font_montserrat_22, 0);
#endif

    lv_obj_add_event_cb(calendar, event_handler, LV_EVENT_VALUE_CHANGED, selected_date);
}






/*
    密码系统
*/

lv_obj_t * Login_box = NULL;    //定义登录界面沙盒全局变量，方便读取
lv_obj_t * Login_error_box = NULL;    //定义登录界面错误沙盒全局变量，方便读取

/* ===== 关闭登录成功提示框 ===== */
static void Box_close_cd(lv_event_t * e)
{
    if(login_timer) 
    {
        lv_timer_delete(login_timer);
        login_timer = NULL;
    }
    lv_msgbox_close(lv_event_get_user_data(e));
    login_ok = 1;
}

/* ===== 处理登录成功提示框超时 ===== */
static void login_timeout_cb(lv_timer_t * timer)
{
    login_timer = NULL;
    lv_obj_t * mbox = lv_timer_get_user_data(timer);
    lv_msgbox_close(mbox);
    login_ok = 1;
}

/* ===== 处理登录失败提示框超时 ===== */
static void login_error_timeout_cb(lv_timer_t * timer)
{
    login_timer = NULL;
    lv_obj_t * mbox = lv_timer_get_user_data(timer);
    lv_msgbox_close(mbox);
}


/* ===== 处理密码输入框事件 ===== */
static void ta_Password_event_cb(lv_event_t * e)
{

    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e); //当前的触发事件的对象(文本框)
    lv_obj_t * kb = lv_event_get_user_data(e);

    if(code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }

        if(code == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }

    if( code == LV_EVENT_READY ) //敲了回车===>文本输入完成了
    {
        const char * user = lv_textarea_get_text(g_ta_user);   // 读用户名
        const char * pass = lv_textarea_get_text(g_ta_pass);   // 读密码
        

        if (strcmp(user, "ww") == 0 && strcmp(pass, "123456") == 0) 
        {
            printf("登陆成功\n");
            Login_box = lv_msgbox_create(NULL);
            lv_msgbox_add_title(Login_box,"Login Successful!");
            lv_msgbox_add_text(Login_box,"Welcome To The World's Top System!");
            lv_msgbox_add_close_button(Login_box);
            lv_obj_t * btn ;
            btn = lv_msgbox_add_footer_button(Login_box,"Ok");
            lv_obj_add_event_cb(btn,Box_close_cd,LV_EVENT_CLICKED,Login_box);
            lv_timer_t * timer = lv_timer_create(login_timeout_cb, 3000, Login_box);
            lv_timer_set_repeat_count(timer, 1);
            login_timer = timer;
            
        }
        else 
        {
            printf("用户不存在或者密码错误\n");
            Login_error_box = lv_msgbox_create(NULL);
            lv_msgbox_add_title(Login_error_box,"System message");
            lv_msgbox_add_text(Login_error_box,"User unexit or Password error,please input again!");
            lv_msgbox_add_close_button(Login_error_box);
            lv_timer_t * timer = lv_timer_create(login_error_timeout_cb, 3000, Login_error_box);
            lv_timer_set_repeat_count(timer, 1);
            // login_timer = timer;
        }
    }


    // else if(code == LV_EVENT_CANCEL)
    // {
    //     lv_keyboard_set_textarea(kb, NULL);
    //     lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    // }



}

/* ===== 处理用户名输入框事件 ===== */
static void ta_User_event_cb(lv_event_t * e)
{

    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e); //当前的触发事件的对象(文本框)
    lv_obj_t * kb = lv_event_get_user_data(e);
    if(code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }

    if(code == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }

    if( code == LV_EVENT_READY ) //敲了回车===>文本输入完成了
    {
        const char * my_text_ta = lv_textarea_get_text(ta); //获取文本框输入的文本
        //const char * my_text_tb = lv_textarea_get_text(tb); //获取文本框输入的文本
        printf("%s\n",my_text_ta);
        //printf("%s\n",my_text_tb);
        if( strcmp(my_text_ta,"ww") == 0 )
        {
            printf("User exit\n");
        }
        else
        {
            printf("User unexit\n");
        }
    }
}

/* ===== 创建密码登录界面 ===== */
void lv_example_keyboard_1(void)    //密码系统
{
    

    
    lv_obj_t * login_screen = lv_screen_active();   //登录时保护屏幕


    lv_obj_set_style_bg_color(login_screen, lv_color_hex(0x05070B), 0);
    lv_obj_set_style_bg_opa(login_screen, LV_OPA_COVER, 0);

    lv_obj_t * login_title_mark = lv_obj_create(login_screen);
    lv_obj_set_size(login_title_mark, 5, 38);
    lv_obj_set_pos(login_title_mark, 48, 30);
    lv_obj_set_style_bg_color(login_title_mark, lv_color_hex(0x1687FF), 0);
    lv_obj_set_style_bg_opa(login_title_mark, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(login_title_mark, 0, 0);
    lv_obj_set_style_radius(login_title_mark, 2, 0);
    lv_obj_remove_flag(login_title_mark, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * login_title = lv_label_create(login_screen);
    lv_label_set_text(login_title, "EDUCATION");
    lv_obj_set_style_text_color(login_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(login_title, &lv_font_montserrat_32, 0);
    lv_obj_set_pos(login_title, 68, 30);

    lv_obj_t * login_header_rule = lv_obj_create(login_screen);
    lv_obj_set_size(login_header_rule, 928, 1);
    lv_obj_set_pos(login_header_rule, 48, 91);
    lv_obj_set_style_bg_color(login_header_rule, lv_color_hex(0x202833), 0);
    lv_obj_set_style_bg_opa(login_header_rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(login_header_rule, 0, 0);
    lv_obj_set_style_radius(login_header_rule, 0, 0);
    lv_obj_remove_flag(login_header_rule, LV_OBJ_FLAG_SCROLLABLE);

    /*Create a keyboard to use it with an of the text areas*/
    lv_obj_t * kb = lv_keyboard_create(login_screen);
    lv_obj_set_size(kb, 1024, 246);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x080C12), 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(kb, lv_color_hex(0x202C3A), 0);
    lv_obj_set_style_border_width(kb, 1, 0);
    lv_obj_set_style_radius(kb, 0, 0);
    lv_obj_set_style_pad_all(kb, 8, 0);
    lv_obj_set_style_pad_row(kb, 6, 0);
    lv_obj_set_style_pad_column(kb, 6, 0);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x152131), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x075DBD), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(kb, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 6, LV_PART_ITEMS);

    /*Create a text area. The keyboard will write here*/
    lv_obj_t * ta;
    ta = lv_textarea_create(lv_screen_active());
    g_ta_user = ta;           // 第一个文本框（User）创建后保存
    lv_obj_align(ta, LV_ALIGN_CENTER, -160, -80);
    lv_obj_add_event_cb(ta, ta_User_event_cb, LV_EVENT_ALL, kb);
    lv_textarea_set_placeholder_text(ta, "USER NAME");
    lv_obj_set_size(ta, 280, 64);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x0C1118), 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(0x2B8CFF), 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(0x1687FF), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ta, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(ta, 8, 0);
    lv_obj_set_style_pad_all(ta, 14, 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0x6F9DC5), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x1687FF), LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_keyboard_set_textarea(kb, ta);

    ta = lv_textarea_create(lv_screen_active());
    g_ta_pass = ta;           // 第二个文本框（password）创建后保存
    lv_obj_align(ta, LV_ALIGN_CENTER, 160, -80);
    lv_obj_add_event_cb(ta, ta_Password_event_cb, LV_EVENT_ALL, kb);
    lv_textarea_set_placeholder_text(ta, "PASSWORD");
    lv_obj_set_size(ta, 280, 64);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x0C1118), 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(0x2B8CFF), 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(0x1687FF), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ta, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(ta, 8, 0);
    lv_obj_set_style_pad_all(ta, 14, 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0x6F9DC5), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x1687FF), LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);   // 强制显示键盘
}













/* ===== 程序入口与主循环 ===== */
int main(void)
{
    lv_init();

    /*Linux frame buffer device init*/
    lv_display_t *disp = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, "/dev/fb0");
    lv_indev_t *indev = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event6");
    /*Create a Demo*/
    // lv_demo_widgets();
    // lv_demo_widgets_start_slideshow();
    char path[100] = {"/ww"};
    h = malloc(sizeof(Head));
    h->first = NULL;
    h->last = NULL;
    h->num = 0;
    find_pic(path,h);

    // 扫描音乐文件
    char music_path[100] = {"/ww/music"};
    h_music = malloc(sizeof(Head));
    h_music->first = NULL;
    h_music->last = NULL;
    h_music->num = 0;
    find_music(music_path, h_music);

    // 扫描视频文件
    char video_path[100] = {"/ww/video"};
    h_video = malloc(sizeof(Head));
    h_video->first = NULL;
    h_video->last = NULL;
    h_video->num = 0;
    find_video(video_path, h_video);


    // 扫描笔记文件
    // mkdir("/ww/notes", 0777);   // 确保目录存在
    char notes_path[100] = {"/ww/notes"};
    h_notes = malloc(sizeof(Head));
    h_notes->first = NULL;
    h_notes->last = NULL;
    h_notes->num = 0;
    find_notes(notes_path, h_notes);


    lv_example_keyboard_1();
    Main_Interface();

    /*Handle LVGL tasks*/
    while (1)
    {
        lv_timer_handler();

        if(login_ok)
        {
            login_ok = 0;
            lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_HIDDEN);  // 显示主界面（含网格）
        }
        if(music_is_playing && music_slider) {
        // 每隔一段时间读一次 mpg123 的进度
        // 简单做法：每50ms slider 加1
        static int tick = 0;
        tick++;
            if(tick >= 10) 
            { // 约50ms*10=500ms 走一格
                int val = lv_slider_get_value(music_slider);
                if(val < 100) lv_slider_set_value(music_slider, val + 1, LV_ANIM_OFF);
                else lv_slider_set_value(music_slider, 0, LV_ANIM_OFF);
                tick = 0;
            }
        }

        usleep(5000);
    }

    return 0;
}
