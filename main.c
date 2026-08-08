#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "lv_myfont_30.h"
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "game_2048.h"
#include <signal.h>






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
static void pic_next(lv_event_t * e);
static void pic_last(lv_event_t * e);
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

/* 音乐播放器 */
static void Music_cb(lv_event_t * e);
void Music_Interface(void);
static void music_play_pause_cb(lv_event_t * e);
static void music_next_cb(lv_event_t * e);
static void music_prev_cb(lv_event_t * e);
static void music_kill(void);
static void music_start_play(void);


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
void find_notes(char *path, Head *h);
static void Games_cb(lv_event_t * e);
static void show_main_win(void);


/* 全局变量的实现 */
static int login_ok = 0;
static lv_obj_t * Main_win = NULL;
static   lv_obj_t * label = NULL;
static   lv_obj_t * obj = NULL;
static lv_obj_t * album_content = NULL;
Head *h = NULL;
static DNode *p = NULL;
static lv_obj_t * g_bmp = NULL;
static lv_obj_t * g_ta_user = NULL;
static lv_obj_t * g_ta_pass = NULL;

/* 音乐播放器全局变量 */
static lv_obj_t * music_win = NULL;
static lv_obj_t * music_label = NULL;
static lv_obj_t * music_slider = NULL;
static lv_obj_t * play_btn = NULL;
static lv_obj_t * prev_btn = NULL;
static lv_obj_t * next_btn = NULL;
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
static lv_timer_t * video_hide_timer = NULL;
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

void get_point()
{
    lv_obj_add_event_cb(lv_screen_active(), my_active_cb ,LV_EVENT_CLICKED,NULL);
}
/***************************************/



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
        "Music", "Note", "Games",
        "Reserved 1", "Reserved 2", "Reserved 3"
    };
    static const char * button_icons[9] = {
        LV_SYMBOL_LIST, LV_SYMBOL_IMAGE, LV_SYMBOL_VIDEO,
        LV_SYMBOL_AUDIO, LV_SYMBOL_EDIT, LV_SYMBOL_PLAY,
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
        lv_obj_add_style(label, i < 5 ? &label_style : &placeholder_label_style,
                         LV_STATE_DEFAULT);

        lv_obj_t * icon = lv_label_create(obj);
        lv_label_set_text(icon, button_icons[i]);
        lv_obj_add_style(icon, &icon_style, LV_STATE_DEFAULT);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, i < 5 ? 16 : 12);

        if(i < 5)
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



/*
    视频播放
*/
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

/* ===== 视频播放器辅助函数和回调占位 ===== */
static int video_hide_countdown = 0;

static void video_hide_tick_cb(lv_timer_t * t)
{
    if(video_hide_countdown > 0) {
        video_hide_countdown--;
        if(video_hide_countdown == 0)
            lv_obj_add_flag(video_control_bar, LV_OBJ_FLAG_HIDDEN);
    }
}

static void video_show_controls(void)
{
    video_hide_countdown = 3;   /* 喂狗：重置3秒倒计时 */
    lv_obj_remove_flag(video_control_bar, LV_OBJ_FLAG_HIDDEN);
}

static void video_screen_tap_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_PRESSED && code != LV_EVENT_RELEASED) return;
    video_show_controls();
}

static void video_control_touch_cb(lv_event_t * e)
{
    video_show_controls();
}

/* 回调占位 — 你自己实现 */
static void video_prev_cb(lv_event_t * e)        { /* TODO */ }
static void video_play_pause_cb(lv_event_t * e)  { /* TODO */ }
static void video_next_cb(lv_event_t * e)        { /* TODO */ }
static void video_vol_cb(lv_event_t * e)         { /* TODO */ }

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
    lv_obj_add_event_cb(btn_back, back_to_main_cb, LV_EVENT_CLICKED, video_win);

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

    /* 时间标签 */
    video_time_label = lv_label_create(video_control_bar);
    lv_label_set_text(video_time_label, "00:00 / 00:00");
    lv_obj_set_style_text_color(video_time_label, lv_color_hex(0xBBBBBB), 0);
    lv_obj_set_style_text_font(video_time_label, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(video_time_label, 15, 18);

    /* ====== 按钮组 ====== */
    int btn_y = 40, btn_w = 54, btn_h = 40, gap = 10, cx = 400;

    /* 上一首 */
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

    /* 下一首 */
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

    /* 启动倒计时 tick（每秒） */
    lv_timer_create(video_hide_tick_cb, 1000, NULL);

    /* 初始隐藏控制栏 */
    video_hide_countdown = 0;
    lv_obj_add_flag(video_control_bar, LV_OBJ_FLAG_HIDDEN);
}










/*
    记事本
*/

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


void Note_Interface(void)
{
    Note_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(Note_win,1024,600);
    lv_obj_set_pos(Note_win,0,0);

    // 2. 标题 " Note "
    lv_obj_t * title_bg = lv_obj_create(Note_win);
    lv_obj_set_style_bg_color(title_bg,lv_color_hex(0x000000),0);
    lv_obj_set_size(title_bg,250,100);
    lv_obj_align(title_bg,LV_ALIGN_TOP_MID,0,0);
    lv_obj_t * title = lv_label_create(title_bg);
    lv_label_set_text(title,"Note");
    lv_obj_set_style_text_color(title,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_bg_opa(title,LV_OPA_10,0);
    lv_obj_align(title,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_text_font(title,&lv_font_montserrat_48,0);

    // 3. 笔记列表（中间区域）
    notes_list = lv_list_create(Note_win);
    lv_obj_set_size(notes_list, 800, 400);
    lv_obj_align(notes_list, LV_ALIGN_CENTER, 0, 50);

    if(h_notes&&h_notes->first)
    {
        lv_list_add_text(notes_list,"All of notes");
        DNode *pn = h_notes->first;
        int first = 1;
        while(1) 
        {
            /* 只显示文件名 */
            char *pname = strrchr(pn->data, '/');
            const char *show = pname ? pname + 1 : pn->data;

            lv_obj_t * btn = lv_list_add_button(notes_list, LV_SYMBOL_FILE, show);
            lv_obj_add_event_cb(btn, note_edit_cb, LV_EVENT_CLICKED, (void*)pn);
            if(first) 
            { 
                first = 0; 
                pn = pn->next; 
            }
            else 
            { 
                pn = pn->next; 
            }
            if(pn == h_notes->first) 
                break;
        }
    } 
    else 
    {
        lv_list_add_text(notes_list, "No notes available");
    }
    
    /* 新建笔记按钮 */
    lv_obj_t * btn_new = lv_button_create(Note_win);
    lv_obj_align(btn_new,LV_ALIGN_BOTTOM_MID,0,-30);
    lv_obj_set_size(btn_new,280,80);
    lv_obj_t * label_new = lv_label_create(btn_new);
    lv_label_set_long_mode(label_new,LV_LABEL_LONG_SCROLL);
    lv_label_set_text(label_new,"Create a new note");
    lv_obj_set_style_text_font(label_new,&lv_font_montserrat_22,0);
    lv_obj_center(label_new);
    lv_obj_add_event_cb(btn_new,note_new_cb,LV_EVENT_CLICKED,NULL);
    


    // 6. 返回按钮（左上角，参考 Album_Interface）
    lv_obj_t * btn_back = lv_button_create(Note_win);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_center(lbl_back);
    lv_obj_set_size(btn_back, 100, 50);
    lv_obj_set_pos(btn_back, 10, 10);
    lv_obj_add_event_cb(btn_back, back_to_main_cb, LV_EVENT_CLICKED, Note_win);

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

    /* 文件名输入框 */
    lv_obj_t * lb_fn = lv_label_create(note_editor_win);
    lv_label_set_text(lb_fn, "File:");
    lv_obj_set_style_text_font(lb_fn, &lv_myfont_30, 0);
    lv_obj_set_pos(lb_fn, 50, 20);

    note_filename_ta = lv_textarea_create(note_editor_win);
    lv_obj_set_size(note_filename_ta, 400, 40);
    lv_obj_set_pos(note_filename_ta, 150, 15);
    lv_textarea_set_placeholder_text(note_filename_ta, "file name");
    lv_textarea_set_one_line(note_filename_ta, true);

    /* 标题输入框 */
    lv_obj_t * label_title = lv_label_create(note_editor_win);
    lv_label_set_text(label_title, "Title:");
    lv_obj_set_style_text_font(label_title, &lv_myfont_30, 0);
    lv_obj_set_pos(label_title, 50, 75);

    note_title_ta = lv_textarea_create(note_editor_win);
    lv_obj_set_size(note_title_ta, 600, 40);
    lv_obj_set_pos(note_title_ta, 150, 70);
    lv_textarea_set_placeholder_text(note_title_ta, "Input Title...");
    lv_textarea_set_one_line(note_title_ta, true);

    /* 内容输入框 */
    lv_obj_t * label_content = lv_label_create(note_editor_win);
    lv_label_set_text(label_content, "Content:");
    lv_obj_set_style_text_font(label_content, &lv_myfont_30, 0);
    lv_obj_set_pos(label_content, 50, 130);

    note_content_ta = lv_textarea_create(note_editor_win);
    lv_obj_set_size(note_content_ta, 800, 220);
    lv_obj_set_pos(note_content_ta, 50, 160);
    lv_textarea_set_placeholder_text(note_content_ta, "Input Content...");

    /* 键盘 */
    note_kb = lv_keyboard_create(note_editor_win);
    lv_keyboard_set_textarea(note_kb, note_filename_ta);
    lv_obj_remove_flag(note_kb, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(note_filename_ta, note_keyboard_cb, LV_EVENT_ALL, note_kb);
    lv_obj_add_event_cb(note_title_ta, note_keyboard_cb, LV_EVENT_ALL, note_kb);
    lv_obj_add_event_cb(note_content_ta, note_keyboard_cb, LV_EVENT_ALL, note_kb);

    /* 保存按钮 */
    lv_obj_t * btn_save = lv_button_create(note_editor_win);
    lv_obj_set_pos(btn_save, 850, 5);
    lv_obj_set_size(btn_save, 100, 50);
    lv_obj_t * lb_save = lv_label_create(btn_save);
    lv_label_set_text(lb_save, "Save");
    lv_obj_set_style_text_font(lb_save, &lv_myfont_30, 0);
    lv_obj_center(lb_save);
    lv_obj_add_event_cb(btn_save, note_save_cb, LV_EVENT_CLICKED, NULL);

    /* 删除按钮（编辑已有笔记时显示） */
    if(note_current_file[0] != '\0') 
    {
        lv_obj_t * btn_del = lv_button_create(note_editor_win);
        lv_obj_set_pos(btn_del, 850, 65);
        lv_obj_set_size(btn_del, 100, 50);
        lv_obj_t * lb_del = lv_label_create(btn_del);
        lv_label_set_text(lb_del, "Remove");
        lv_obj_set_style_text_font(lb_del, &lv_myfont_30, 0);
        lv_obj_center(lb_del);
        lv_obj_add_event_cb(btn_del, note_delete_cb, LV_EVENT_CLICKED, NULL);
    }

    /* 返回列表按钮 */
    lv_obj_t * btn_back = lv_button_create(note_editor_win);
    lv_obj_set_pos(btn_back, 850, 125);
    lv_obj_set_size(btn_back, 100, 50);
    lv_obj_t * lb_back = lv_label_create(btn_back);
    lv_label_set_text(lb_back, "Back");
    lv_obj_set_style_text_font(lb_back, &lv_myfont_30, 0);
    lv_obj_center(lb_back);
    lv_obj_add_event_cb(btn_back, note_back_list_cb, LV_EVENT_CLICKED, NULL);

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

/* 
    记事本 - 保存
*/
static void msgbox_close_cb(lv_timer_t * timer)
{
    lv_obj_t * mbox = lv_timer_get_user_data(timer);
    lv_msgbox_close(mbox);
}

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

    /* 重建列表 */
    if(notes_list) 
    { 
        lv_obj_delete(notes_list); notes_list = NULL; 
    }

    lv_obj_t * list = lv_list_create(Note_win);
    lv_obj_set_size(list, 800, 420);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 50);
    notes_list = list;

    if(h_notes && h_notes->first) 
    {
        lv_list_add_text(list, "All of notes");
        DNode *pn = h_notes->first;
        int first = 1;
        while(1) 
        {
            char *pname = strrchr(pn->data, '/');
            const char *show = pname ? pname + 1 : pn->data;
            lv_obj_t * btn = lv_list_add_button(list, LV_SYMBOL_FILE, show);
            lv_obj_add_event_cb(btn, note_edit_cb, LV_EVENT_CLICKED, (void*)pn);
            if(first) 
            { 
                first = 0;
                pn = pn->next; 
            }
            else 
            { 
                pn = pn->next; 
            }
            if(pn == h_notes->first) 
                break;
        }
    } 
    else 
    {
        lv_list_add_text(list, "No notes available");
    }

    /* 提示保存成功 */
    lv_obj_t * mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, "Prompt");
    lv_msgbox_add_text(mbox, "Saved successfully");
    lv_msgbox_add_close_button(mbox);

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
    if(notes_list) 
    {
        lv_obj_delete(notes_list);
        notes_list = NULL;
    }
    Note_Interface();  /* 重新加载列表 */
}

static void note_delete_cancel_cb(lv_event_t * ev)
{
    lv_obj_t * mbox = lv_event_get_user_data(ev);
    if(mbox) lv_msgbox_close(mbox);
}

static void note_delete_cb(lv_event_t * e)
{
    if(note_current_file[0] == '\0') return;

    /* 确认删除的 msgbox */
    lv_obj_t * mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, "Confirm to delete");
    lv_msgbox_add_text(mbox, "Are you sure you want to delete this note?");

    /* 确定按钮 */
    lv_obj_t * btn_ok = lv_msgbox_add_footer_button(mbox, "Sure");
    lv_obj_add_event_cb(btn_ok, note_delete_confirm_cb, LV_EVENT_CLICKED, mbox);

    /* 取消按钮 */
    lv_obj_t * btn_cancel = lv_msgbox_add_footer_button(mbox, "Can");
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

    if(code == LV_EVENT_FOCUSED) 
    {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
    if(code == LV_EVENT_DEFOCUSED) 
    {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
    if(code == LV_EVENT_READY) 
    {
        /* 回车：文件名→标题→内容 */
        if(ta == note_filename_ta) 
        {
            lv_keyboard_set_textarea(kb, note_title_ta);
        } 
        else if(ta == note_title_ta) 
        {
            lv_keyboard_set_textarea(kb, note_content_ta);
        }
    }
}















/*
    音乐播放器
*/

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

void Music_Interface(void)
{
    //音乐播放窗口
    music_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(music_win,1024,600);
    lv_obj_set_pos(music_win,0,0);

    // 1. 歌名显示在中上方
    music_label = lv_label_create(music_win);
    lv_obj_align(music_label , LV_ALIGN_TOP_MID , 0 , 30);
    lv_label_set_text(music_label , "No music");

    // 中文字体
    static lv_style_t style_music_label;
    lv_style_init(&style_music_label);
    lv_style_set_text_font(&style_music_label, &lv_myfont_30);
    lv_obj_add_style(music_label, &style_music_label, LV_STATE_DEFAULT);

    // 2. 进度条（中间偏下）
    music_slider = lv_slider_create(music_win);
    lv_obj_set_size(music_slider,600,20);
    lv_obj_align(music_slider,LV_ALIGN_CENTER,0,0);


    // 3. 播放/暂停按钮（进度条下方）
    play_btn = lv_button_create(music_win);
    lv_obj_align(play_btn,LV_ALIGN_BOTTOM_MID,0,-60);
    lv_obj_set_size(play_btn,100,100);      //播放

    lv_obj_t * play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    lv_obj_align(play_label,LV_ALIGN_CENTER,0,0);
    lv_obj_add_event_cb(play_btn,music_play_pause_cb,LV_EVENT_CLICKED,NULL);

    // 4. 上一首按钮（播放按钮左侧）
    prev_btn = lv_button_create(music_win);
    lv_obj_align(prev_btn,LV_ALIGN_BOTTOM_MID, -150, -60);
    lv_obj_set_size(prev_btn,80,80);      //上一首按键创建

    lv_obj_t * prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, LV_SYMBOL_PREV);
    lv_obj_align(prev_label,LV_ALIGN_CENTER,0,0);
    lv_obj_add_event_cb(prev_btn, music_prev_cb, LV_EVENT_CLICKED, NULL);    //上一首按键

    // 5. 下一首按钮（播放按钮右侧）
    next_btn = lv_button_create(music_win);
    lv_obj_align(next_btn,LV_ALIGN_BOTTOM_MID, 150, -60);
    lv_obj_set_size(next_btn,80,80);      //下一首按键创建

    lv_obj_t * next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_NEXT);
    lv_obj_align(next_label,LV_ALIGN_CENTER,0,0);
    lv_obj_add_event_cb(next_btn, music_next_cb, LV_EVENT_CLICKED, NULL);    //下一首按键

    // 6. 返回按钮（左上角，参考 Album_Interface）
    lv_obj_t * btn_back = lv_button_create(music_win);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_center(lbl_back);
    lv_obj_set_size(btn_back, 100, 50);
    lv_obj_set_pos(btn_back, 10, 10);
    lv_obj_add_event_cb(btn_back, back_to_main_cb, LV_EVENT_CLICKED, music_win);


    // 7. 默认选第一首歌
    if(h_music && h_music->first) 
    {
        music_current = h_music->first;
        char *pname = strrchr(music_current->data, '/');
        lv_label_set_text(music_label, pname ? pname + 1 : music_current->data);
    }

    lv_slider_set_range(music_slider, 0, 100);
    lv_slider_set_value(music_slider, 0, LV_ANIM_OFF);

}

static void music_kill(void)
{
    if(music_pid > 0) 
    {
        kill(music_pid, SIGTERM);
        usleep(50000);
        music_pid = 0;
    }
}

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
}

static void music_play_pause_cb(lv_event_t * e)
{
    if(!music_current) return;

    if(music_is_playing) 
    {
        // 暂停 → 杀掉进程
        music_kill();
        music_is_playing = 0;
        lv_label_set_text(lv_obj_get_child(play_btn, 0), LV_SYMBOL_PLAY);
    } else 
    {
        // 播放 → 重新启动
        music_start_play();
    }
}

static void music_next_cb(lv_event_t * e)
{
    if(!music_current) 
        return;
    music_current = music_current->next;
    music_start_play();
}

static void music_prev_cb(lv_event_t * e)
{
    if(!music_current) return;
    music_current = music_current->prev;
    music_start_play();
}













/*
    电子相册
*/

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

void find_music(char *path, Head *h)    //扫描音乐闻文件
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











/*
    日历
*/

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

static void back_to_main_cb(lv_event_t * e)
{
    lv_obj_t * win = lv_event_get_user_data(e);
    lv_obj_add_flag(win, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
}

static void calendar_back_to_main_cb(lv_event_t * e)
{
    lv_obj_t * win = lv_event_get_user_data(e);
    lv_obj_remove_flag(Main_win, LV_OBJ_FLAG_HIDDEN);
    lv_obj_delete_async(win);
}

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

static void login_timeout_cb(lv_timer_t * timer)
{
    login_timer = NULL;
    lv_obj_t * mbox = lv_timer_get_user_data(timer);
    lv_msgbox_close(mbox);
    login_ok = 1;
}

static void login_error_timeout_cb(lv_timer_t * timer)
{
    login_timer = NULL;
    lv_obj_t * mbox = lv_timer_get_user_data(timer);
    lv_msgbox_close(mbox);
}


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
}

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

void lv_example_keyboard_1(void)    //密码系统
{
    

    
    lv_obj_t * login_screen = lv_screen_active();   //登录时保护屏幕


    /*Create a keyboard to use it with an of the text areas*/
    lv_obj_t * kb = lv_keyboard_create(login_screen);

    /*Create a text area. The keyboard will write here*/
    lv_obj_t * ta;
    ta = lv_textarea_create(lv_screen_active());
    g_ta_user = ta;           // 第一个文本框（User）创建后保存
    lv_obj_align(ta, LV_ALIGN_CENTER, -90, -80);
    lv_obj_add_event_cb(ta, ta_User_event_cb, LV_EVENT_ALL, kb);
    lv_textarea_set_placeholder_text(ta, "User");
    lv_obj_set_size(ta, 140, 80);
    lv_keyboard_set_textarea(kb, ta);

    ta = lv_textarea_create(lv_screen_active());
    g_ta_pass = ta;           // 第二个文本框（password）创建后保存
    lv_obj_align(ta, LV_ALIGN_CENTER, 90, -80);
    lv_obj_add_event_cb(ta, ta_Password_event_cb, LV_EVENT_ALL, kb);
    lv_textarea_set_placeholder_text(ta, "password");
    lv_obj_set_size(ta, 140, 80);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);   // 强制显示键盘
}













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
