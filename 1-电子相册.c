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


void show_bmp(void);
static void pic_next(lv_event_t * e);
static void pic_last(lv_event_t * e);
void screen(void);   /* 前向声明，解决编译警告 */
void test_bmp(void);
void creat_link(Head *h,char*file_name);
void find_pic(char*path,Head *h);
Head *h = NULL;
static DNode *p = NULL;
static lv_obj_t * g_bmp = NULL;
static volatile int login_ok = 0;
static lv_obj_t * album_content = NULL;  // 相册容器，初始隐藏

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
        const char * my_text_ta = lv_textarea_get_text(ta); //获取文本框输入的文本
        if( strcmp(my_text_ta,"123456") == 0 )
        {
            printf("登录成功\n");
            login_ok = 1;
        }
        else
        {
            printf("密码错误\n");
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
        if( strcmp(my_text_ta,"Wuyanzu") == 0 )
        {
            printf("User exit\n");
        }
        else
        {
            printf("User unexit\n");
        }
    }
}

/*
    键盘函数
*/
void lv_example_keyboard_1(void)
{
    lv_obj_t * login_screen = lv_screen_active();   //登录时保护屏幕


    /*Create a keyboard to use it with an of the text areas*/
    lv_obj_t * kb = lv_keyboard_create(login_screen);

    /*Create a text area. The keyboard will write here*/
    lv_obj_t * ta;
    ta = lv_textarea_create(lv_screen_active());
    lv_obj_align(ta, LV_ALIGN_CENTER, -90, -80);
    lv_obj_add_event_cb(ta, ta_User_event_cb, LV_EVENT_ALL, kb);
    lv_textarea_set_placeholder_text(ta, "User");
    lv_obj_set_size(ta, 140, 80);
    lv_keyboard_set_textarea(kb, ta);

    ta = lv_textarea_create(lv_screen_active());
    lv_obj_align(ta, LV_ALIGN_CENTER, 90, -80);
    lv_obj_add_event_cb(ta, ta_Password_event_cb, LV_EVENT_ALL, kb);
    lv_textarea_set_placeholder_text(ta, "password");
    lv_obj_set_size(ta, 140, 80);
    lv_keyboard_set_textarea(kb, ta);
}

void screen(void)
{
    //在活动屏幕上创建相册容器（不切换屏幕）
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

    //初始隐藏，登录成功后才显示
    lv_obj_add_flag(album_content, LV_OBJ_FLAG_HIDDEN);
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
    lv_example_keyboard_1();
    h = malloc(sizeof(Head));
    h->first = NULL;
    h->last = NULL;
    h->num = 0;
    find_pic(path,h);

    screen();  //预创建相册容器（初始隐藏）

    /*Handle LVGL tasks*/
    while (1)
    {
        lv_timer_handler();

        if (login_ok) {
            login_ok = 0;
            lv_obj_remove_flag(album_content, LV_OBJ_FLAG_HIDDEN);
        }

        usleep(5000);
    }

    return 0;
}
