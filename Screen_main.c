/* Windows SDL virtual screen entry for the existing Demo UI. */
#ifndef _WIN32_WINNT                                                           //new+ 确保 MinGW 在包含 Windows 头文件前公开现代 WinSock 接口
#define _WIN32_WINNT 0x0600                                                    //new+ Windows Vista 及以上版本提供 InetPton 和完整 WinSock 2 接口
#endif                                                                         //new+ 保留工具链或调用者已经指定的更高 Windows 目标版本
#define SCREEN_SIMULATOR 1
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#define main demo_board_main

/* Process-control calls are not executed by the simulator.  These aliases
 * let the unchanged Linux-only player functions compile on Windows. */
#define WNOHANG 1
#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x0004
#endif
#define mkfifo(path, mode) (-1)
#define kill(pid, signal_number) (0)
#define waitpid(pid, status, options) ((pid_t)-1)
#define pipe(descriptors) (-1)
#define fork() ((pid_t)-1)
#define fcntl(descriptor, command, ...) (-1)
#define lv_linux_fbdev_create() ((lv_display_t *)0)
#define lv_linux_fbdev_set_file(display, path) ((void)0)
#define lv_evdev_create(type, path) ((lv_indev_t *)0)

#include "main.c"

#undef main

/* ===== 清理 Windows 虚拟屏网络库 ===== */ //new+ 标记进程退出时释放 WinSock 的辅助函数
static void screen_winsock_cleanup(void)                                      //new+ 为 atexit 提供无参数无返回值的 WinSock 清理回调
{                                                                              //new+ 开始执行 Windows 虚拟屏网络资源清理
    WSACleanup();                                                              //new+ 释放本进程通过 WSAStartup 获取的 WinSock 运行库引用
}                                                                              //new+ 结束 Windows 虚拟屏网络资源清理

/* ===== Windows virtual screen program entry ===== */
int main(void)
{
    WSADATA winsock_data;                                                      //new+ 保存 WinSock 2.2 初始化返回的实现信息
    int winsock_result = WSAStartup(MAKEWORD(2, 2), &winsock_data);             //new+ 在任何 QQ socket 调用前初始化 Windows 网络库
    if(winsock_result != 0) {                                                  //new+ WinSock 初始化失败时虚拟屏无法可靠使用 QQ 网络功能
        fprintf(stderr, "Unable to initialize WinSock, error: %d\n", winsock_result); //new+ 在终端输出明确错误码便于定位系统网络问题
        return EXIT_FAILURE;                                                   //new+ 停止启动缺少网络运行库支持的虚拟屏
    }                                                                          //new+ 结束 WinSock 初始化失败处理
    if(atexit(screen_winsock_cleanup) != 0) {                                  //new+ 注册进程正常退出时执行 WinSock 清理
        WSACleanup();                                                          //new+ 注册失败时立即撤销已经完成的 WinSock 初始化
        fprintf(stderr, "Unable to register WinSock cleanup\n");              //new+ 输出清理回调注册失败提示
        return EXIT_FAILURE;                                                   //new+ 避免在无法管理网络运行库生命周期时继续启动
    }                                                                          //new+ 结束 WinSock 清理回调注册失败处理

    lv_init();

    lv_display_t * display = lv_sdl_window_create(1024, 600);
    if(display == NULL) {
        fprintf(stderr, "Unable to create the 1024x600 virtual screen\n");
        return EXIT_FAILURE;
    }

    lv_sdl_window_set_title(display, "LVGL Demo Virtual Screen");
    lv_sdl_window_set_resizeable(display, false);
    lv_indev_t * pointer = lv_sdl_mouse_create();
    if(pointer == NULL) {
        fprintf(stderr, "Unable to create the virtual mouse input\n");
        return EXIT_FAILURE;
    }

    h = calloc(1, sizeof(*h));
    h_music = calloc(1, sizeof(*h_music));
    h_video = calloc(1, sizeof(*h_video));
    h_notes = calloc(1, sizeof(*h_notes));
    if(h == NULL || h_music == NULL || h_video == NULL || h_notes == NULL) {
        fprintf(stderr, "Unable to allocate simulator lists\n");
        return EXIT_FAILURE;
    }

    lv_example_keyboard_1();
    Main_Interface();

    while(1) {
        uint32_t delay_ms = lv_timer_handler();
        if(login_ok) {
            login_ok = 0;
            lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
        }
        if(delay_ms < 1) delay_ms = 1;
        if(delay_ms > 20) delay_ms = 20;
        SDL_Delay(delay_ms);
    }
}
