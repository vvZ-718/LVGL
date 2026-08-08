#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "lv_myfont_30.h"
#include <stdio.h>



//示例1：创建窗口
void my_test_1(void)
{
    //在活动屏幕上创建窗口
    lv_obj_t * test_win1 = lv_obj_create(lv_screen_active());

    //设置窗口的大小
    lv_obj_set_size(test_win1,150,100);

    //设置窗口的位置
    lv_obj_set_pos(test_win1,0,0);

    //设置窗口的颜色
    // lv_obj_set_style_bg_color(test_win1,lv_color_hex(0x888833),0);
    // lv_obj_set_style_border_color(test_win1,lv_color_hex(0x000000),0);
    // lv_obj_set_style_border_opa(test_win1,LV_OPA_COVER,127);


    //在活动屏幕上创建窗口
    lv_obj_t * test_win2 = lv_obj_create(lv_screen_active());

    //设置窗口的大小
    lv_obj_set_size(test_win2,150,100);

    //设置窗口的位置
    lv_obj_set_align(test_win2,LV_ALIGN_CENTER);

    //设置窗口的颜色
    // lv_obj_set_style_bg_color(test_win2,lv_color_hex(0xEEE0E5),0);
    // lv_obj_set_style_border_color(test_win2,lv_color_hex(0x000000),0);
    // lv_obj_set_style_border_opa(test_win2,LV_OPA_COVER,0);
 
}



void my_test_2(void)
{
    //在活动屏幕上创建窗口
    lv_obj_t * test_win1 = lv_obj_create(lv_screen_active());

    //设置窗口的大小
    lv_obj_set_size(test_win1,150,100);

    //设置窗口的位置
    lv_obj_set_align(test_win1,LV_ALIGN_CENTER);


    /***********给test_win1设置默认样式***************/

    //1.定义并初始化
    static lv_style_t style1;   //创建一个样式
    lv_style_init(&style1);     //初始化一个样式

    //2.设置样式的属性
    lv_style_set_bg_color(&style1,lv_color_hex(0x00FFFF));  //背景的颜色：天蓝色
    lv_style_set_bg_opa(&style1,LV_OPA_50); //背景的透明度
    lv_style_set_border_color(&style1,lv_color_hex(0x000000));  //边框的颜色:黑色
    lv_style_set_border_width(&style1,10);   //边框的宽度：10像素


    //3.添加样式
    lv_obj_add_style(test_win1,&style1,LV_STATE_DEFAULT);


    /***********给test_win1设置按下状态样式***************/

    //1.定义并初始化
    static lv_style_t style2;   //创建一个样式
    lv_style_init(&style2);     //初始化一个样式

    //2.设置样式的属性
    lv_style_set_bg_color(&style2,lv_color_hex(0xFF0000));  //背景的颜色：红色
    lv_style_set_bg_opa(&style2,LV_OPA_50); //背景的透明度
    lv_style_set_border_color(&style2,lv_color_hex(0x000000));  //边框的颜色:黑色
    lv_style_set_border_width(&style2,2);   //边框的宽度：10像素


    //3.添加样式
    lv_obj_add_style(test_win1,&style2,LV_STATE_PRESSED);
    
}



/* ====== 新增：短按点击切换 CHECKED 状态回调 ====== */
static void my_test_3_event_cb(lv_event_t * e)
{  
    lv_obj_t * obj = lv_event_get_target(e);          // 获取被点击的窗口对象
    if(lv_obj_get_state(obj) & LV_STATE_CHECKED) {    // 如果当前已是选中状态
        lv_obj_clear_state(obj, LV_STATE_CHECKED);    //   取消选中 → 恢复默认色
    } else {
        lv_obj_add_state(obj, LV_STATE_CHECKED);       //   否则设为选中 → 白色
    }
}
/* ====== 结束 ====== */

void my_test_3(void)
{
    static lv_style_t style_wite;   //创建一个样式
    lv_style_init(&style_wite);     //初始化一个样式
    //2.设置点击的样式的属性
    lv_style_set_bg_color(&style_wite,lv_color_hex(0xFFFFFF));  //背景的颜色：白色
    lv_style_set_bg_opa(&style_wite,LV_OPA_50); //背景的透明度
    lv_style_set_border_color(&style_wite,lv_color_hex(0x000000));  //边框的颜色:黑色
    lv_style_set_border_width(&style_wite,2);   //边框的宽度：2像素



    static lv_style_t style_red;   //创建一个样式
    lv_style_init(&style_red);     //初始化一个样式
    //2.设置点击的样式的属性
    lv_style_set_bg_color(&style_red,lv_color_hex(0xFF0000));  //背景的颜色：红色
    lv_style_set_bg_opa(&style_red,LV_OPA_50); //背景的透明度
    lv_style_set_border_color(&style_red,lv_color_hex(0x000000));  //边框的颜色:黑色
    lv_style_set_border_width(&style_red,2);   //边框的宽度：2像素


    uint32_t color[9] = {0xFF0000, 0xFF8800, 0xFFFF00, 
                         0x00FF00, 0x00FFFF, 0x000000, 
                         0x0000FF, 0x8800FF, 0xD4D4D4};


    lv_obj_t * win[9];
    static lv_style_t s[9];

    for( int i = 0 ; i < 9 ; i++ )
    {

            win[i] = lv_obj_create(lv_screen_active());  //创建窗口

            lv_obj_set_size(win[i],341,200);  //设置窗口大小
            lv_obj_set_align(win[i],i + 1 );    //设置窗口位置

            lv_style_init(&s[i]); 
            
            //设置背景、边框样式
            lv_style_set_bg_color(&s[i],lv_color_hex(color[i]) );
            lv_style_set_bg_opa(&s[i],LV_OPA_100);  
            lv_style_set_border_color(&s[i],lv_color_hex(0x000000));
            lv_style_set_border_opa(&s[i],LV_OPA_20);
            lv_style_set_border_width(&s[i],4);

            lv_obj_add_style(win[i] ,&s[i] ,LV_STATE_DEFAULT);
            //lv_obj_add_style(win[i] , &style_wite , LV_STATE_PRESSED);   // 【改】长按 → 红色
            lv_obj_add_style(win[i] , &style_red , LV_STATE_FOCUSED); // 【改】短按点击切换 → 白色
            //lv_obj_add_event_cb(win[i], my_test_3_event_cb, LV_EVENT_SHORT_CLICKED, NULL); // 【新增】注册短按事件

    }

}



//显示字符及图标等
void my_test_4(void)
{
    lv_obj_t * win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(win,300,250);
    lv_obj_set_align(win,LV_ALIGN_CENTER);

    static lv_style_t sw;
    lv_style_init(&sw);

    lv_style_set_bg_color(&sw,lv_color_hex(0x000000));
    lv_style_set_opa(&sw,LV_OPA_80);

    lv_obj_add_style(win,&sw,LV_STATE_DEFAULT);



    lv_obj_t * la = lv_label_create(win);
    lv_obj_set_align(la,LV_ALIGN_OUT_LEFT_TOP);
    lv_obj_set_width(la,200);
    lv_label_set_long_mode(la,LV_LABEL_LONG_SCROLL);
    lv_label_set_text_fmt(la,LV_SYMBOL_GPS"GPS");
    
    lv_obj_t * la_1 = lv_label_create(win);
    lv_obj_set_align(la_1,LV_ALIGN_BOTTOM_MID);
    lv_obj_set_width(la_1,200);
    lv_label_set_long_mode(la_1,LV_LABEL_LONG_SCROLL);
    lv_label_set_text_fmt(la_1,"%s:%d","WenXiZhaoSiLaoFeng",666);

    lv_obj_t * la_bat = lv_label_create(win);
    lv_obj_set_align(la_bat,LV_ALIGN_TOP_RIGHT);
    lv_obj_set_width(la_bat,35);
    lv_label_set_long_mode(la_bat,LV_LABEL_LONG_SCROLL);
    lv_label_set_text_fmt(la_bat,LV_SYMBOL_BATTERY_FULL);


    lv_obj_t * la_h = lv_label_create(win);
    lv_obj_set_align(la_h,LV_ALIGN_TOP_LEFT);
    lv_obj_set_height(la_h,400);
    lv_label_set_long_mode(la_h,LV_LABEL_LONG_SCROLL);
    lv_label_set_text_fmt(la_h,"\n\n\n\n\n\n\n\n\nhello\nwrold");


    static lv_style_t sl;
    lv_style_init(&sl);

    lv_style_set_text_color(&sl,lv_color_hex(0xFFFFFF));
    lv_style_set_text_font(&sl,&lv_font_montserrat_26);



    lv_obj_add_style(la_1,&sl,LV_STATE_DEFAULT);
    lv_obj_add_style(la_h,&sl,LV_STATE_DEFAULT);


    static lv_style_t sl_1;
    lv_style_init(&sl_1);

    lv_style_set_text_color(&sl_1,lv_color_hex(0x0000FF));
    lv_style_set_text_font(&sl_1,&lv_font_montserrat_26);

    lv_obj_add_style(la,&sl_1,LV_STATE_DEFAULT);

    static lv_style_t sl_bat;
    lv_style_init(&sl_bat);

    lv_style_set_text_color(&sl_bat,lv_color_hex(0x00FF00));
    lv_style_set_text_font(&sl_bat,&lv_font_montserrat_26);

    lv_obj_add_style(la_bat,&sl_bat,LV_STATE_DEFAULT);

}


//显示中文字符
void my_test_5(void)
{
    lv_obj_t * win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(win,300,250);
    lv_obj_set_align(win,LV_ALIGN_CENTER);

    static lv_style_t sw;
    lv_style_init(&sw);

    lv_style_set_bg_color(&sw,lv_color_hex(0x000000));
    lv_style_set_opa(&sw,LV_OPA_80);

    lv_obj_add_style(win,&sw,LV_STATE_DEFAULT);



    lv_obj_t * la = lv_label_create(win);
    lv_obj_set_align(la,LV_ALIGN_CENTER);
    lv_obj_set_width(la,100);
    lv_label_set_long_mode(la,LV_LABEL_LONG_SCROLL);
    lv_label_set_text_fmt(la,"%s:%d","你好世界",666);



    static lv_style_t sl;
    lv_style_init(&sl);

    lv_style_set_text_color(&sl,lv_color_hex(0xFFFFFF));
    lv_style_set_text_font(&sl,&lv_myfont_30);


    lv_obj_add_style(la,&sl,LV_STATE_DEFAULT);

}





static void button_handler2( lv_event_t * e )
{
    lv_obj_t * la = e->user_data;   //接收用户传进来的参数
    //按下
    if(e->code == LV_EVENT_PRESSED)
    {
        printf("Hello\n");
        lv_label_set_text(la,LV_SYMBOL_PLAY);
    }
}


static void button_handler( lv_event_t * e )
{
    lv_obj_t * la2 = e->user_data;   //接收用户传进来的参数
    //按下
    if(e->code == LV_EVENT_PRESSED)
    {
        printf("Hello\n");
        lv_label_set_text(la2,LV_SYMBOL_EYE_CLOSE"close");
    }
    //松开
    if(e->code == LV_EVENT_RELEASED)
    {
        printf("Beybey\n");
        lv_label_set_text(la2,LV_SYMBOL_EYE_OPEN"open");
    }
}


//按钮控件且绑定事件
void my_test_6()
{
    //创建按钮
    lv_obj_t * bu = lv_button_create( lv_screen_active() );
    lv_obj_set_size(bu,200,100);
    lv_obj_set_align(bu,LV_ALIGN_CENTER);

    //创建按钮2
    lv_obj_t * bu2 = lv_button_create( lv_screen_active() );
    lv_obj_set_size(bu2,100,50);
    lv_obj_set_align(bu2,LV_ALIGN_TOP_LEFT);

    //在窗口上创建标签
    lv_obj_t * la = lv_label_create(bu);
    
    //设置长模式的处理方式:滚动
    lv_label_set_text(la,LV_SYMBOL_EYE_OPEN"open"); //设置文本
    lv_obj_set_align(la,LV_ALIGN_CENTER); //标签设置在窗口中心


    //在窗口上创建标签2
    lv_obj_t * la2 = lv_label_create(bu2);
    
    //设置长模式的处理方式:滚动
    lv_label_set_text(la2,LV_SYMBOL_PLAY); //设置文本
    lv_obj_set_align(la2,LV_ALIGN_CENTER); //标签设置在窗口中心

    //样式
    static lv_style_t ls;
    lv_style_init(&ls);
    //设置颜色
    lv_style_set_text_color(&ls,lv_color_hex(0xFFFFFF)); //白
    //设置字体
    lv_style_set_text_font(&ls,&lv_font_montserrat_30);
    lv_obj_add_style(la,&ls,LV_STATE_DEFAULT);
    lv_obj_add_style(la2,&ls,LV_STATE_DEFAULT);

    //绑定事件
    lv_obj_add_event_cb(bu,button_handler,LV_EVENT_PRESSED,(void*)la);
    lv_obj_add_event_cb(bu,button_handler,LV_EVENT_RELEASED,(void*)la);

    lv_obj_add_event_cb(bu2,button_handler2,LV_EVENT_PRESSED,(void*)la2);
}

void test_bmp(void)
{
    lv_obj_t * bmp = lv_image_create(lv_screen_active());
    lv_image_set_src(bmp,"A:/ww/1.bmp");
    lv_obj_set_pos(bmp,0,0);

}

void test_gif(void)
{
    lv_obj_t * gif = lv_gif_create(lv_screen_active());
    lv_gif_set_src(gif,"A:/ww/1.gif");
    lv_obj_set_pos(gif,600,0);

}



static void ta_event_cb(lv_event_t * e)
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
        const char * my_text = lv_textarea_get_text(ta); //获取文本框输入的文本
        printf("%s\n",my_text);

        if( strcmp(my_text,"123456")  == 0)
        {
            printf("登录成功\n");
        }
        else
        {
            printf("密码错误\n");
        }
    }
}

void lv_example_keyboard_1(void)
{
    /*Create a keyboard to use it with an of the text areas*/
    lv_obj_t * kb = lv_keyboard_create(lv_screen_active());

    /*Create a text area. The keyboard will write here*/
    lv_obj_t * ta;
    ta = lv_textarea_create(lv_screen_active());
    lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_ALL, kb);
    lv_textarea_set_placeholder_text(ta, "Hello");
    lv_obj_set_size(ta, 140, 80);

    ta = lv_textarea_create(lv_screen_active());
    lv_obj_align(ta, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_ALL, kb);
    lv_obj_set_size(ta, 140, 80);

    lv_keyboard_set_textarea(kb, ta);
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

    lv_example_keyboard_1();
    test_bmp();
    test_gif();




    /*Handle LVGL tasks*/
    while (1)
    {
        lv_timer_handler();
        usleep(5000);
    }

    return 0;
}
