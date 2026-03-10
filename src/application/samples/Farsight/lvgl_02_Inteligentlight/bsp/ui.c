#include "ui.h"

// 声明图像资源
LV_IMG_DECLARE(title)              // 标题图像
LV_IMG_DECLARE(lighton)            // 灯光开启图标
LV_IMG_DECLARE(lightoff)           // 灯光关闭图标
LV_IMG_DECLARE(light_background)   // 灯光背景图
LV_IMG_DECLARE(icon_illumination)  // 光照传感器图标

// 声明字体
LV_FONT_DECLARE(syht_font_10);     /* 10号字体 */

char STA_IP[20];  // 存储WIFI IP地址

// 样式定义
static lv_style_t BtnStyle;         // 按钮样式
static lv_style_t TextareaStyle;    // 文本框样式
static lv_style_t StatePanelStyle;  // 状态面板样式

// UI对象声明
lv_obj_t *Msgbox;                   // 消息框

lv_obj_t *lightimg;                 // 灯光图像
lv_obj_t *modeswitch;               // 模式切换开关
lv_obj_t *lightswitch;              // 灯光开关
lv_obj_t *lightstateLabel;          // 灯光状态标签

static lv_obj_t *WifiLogo;          // WIFI Logo
static lv_obj_t *WifiIPLabel;       // WIFI IP地址标签
static lv_obj_t *WifiPasswordTextarea;  // WIFI密码文本框
static lv_obj_t *WifiUsrnameTextarea;   // WIFI用户名文本框

static lv_obj_t *KeyBoardObj;       // 键盘对象

lv_obj_t *MqttConnectBtn;           // MQTT连接按钮
lv_obj_t *MqttConnectState;         // MQTT连接状态
lv_obj_t *photosensitivestateLabel; // 光敏传感器状态标签

/**
 * @brief 消息框回调函数
 * @param e 事件对象
 */
static void MsgboxCb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_current_target(e);

    // 如果点击了关闭按钮（索引2）
    if (lv_msgbox_get_active_btn(target) == 2)
    {
        lv_obj_add_flag(Msgbox, LV_OBJ_FLAG_HIDDEN);  // 隐藏消息框
    }
}

/**
 * @brief 文本框事件回调函数
 * @param e 事件对象
 */
static void Textarea_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    char *Userdata = (char *)lv_event_get_user_data(e);

    if (LV_EVENT_FOCUSED == code)  // 获取焦点事件
    {
        if (NULL != Userdata)
        {
            lv_keyboard_set_mode(KeyBoardObj, LV_KEYBOARD_MODE_NUMBER); /* 仅使用数字键盘 */
        }
        if (NULL == Userdata)
        {
            lv_keyboard_set_mode(KeyBoardObj, LV_KEYBOARD_MODE_TEXT_LOWER); // 使用文本键盘
        }
        if (lv_indev_get_type(lv_indev_get_act()) != LV_INDEV_TYPE_KEYPAD)
        {
            lv_keyboard_set_textarea(KeyBoardObj, target); /* 将键盘与文本框关联 */
            lv_obj_set_style_max_height(KeyBoardObj, LV_HOR_RES * 1 / 3, 0);
            lv_obj_clear_flag(KeyBoardObj, LV_OBJ_FLAG_HIDDEN);  // 显示键盘
            lv_obj_scroll_to_view_recursive(target, LV_ANIM_OFF); // 滚动到视图
        }
        return;
    }
    else if (code == LV_EVENT_DEFOCUSED)  // 失去焦点事件
    {
        lv_keyboard_set_textarea(KeyBoardObj, NULL);
        lv_obj_add_flag(KeyBoardObj, LV_OBJ_FLAG_HIDDEN);  // 隐藏键盘
    }
    else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)  // 完成或取消事件
    {
        lv_obj_add_flag(KeyBoardObj, LV_OBJ_FLAG_HIDDEN);  // 隐藏键盘
        lv_obj_clear_state(target, LV_STATE_FOCUSED);      // 清除焦点状态
        lv_indev_reset(NULL, target); /* 忘记最后点击的对象，使其可再次聚焦 */
    }
}

/**
 * @brief 灯光开关事件处理函数
 * @param e 事件对象
 */
static void LightSwitchCb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);

    if (code != LV_EVENT_CLICKED)  // 只处理点击事件
        return;

    if (lv_obj_has_state(target, LV_STATE_CHECKED))  // 开关开启状态
    {
        lv_img_set_src(lightimg, &lighton);                    // 设置开启图标
        lv_label_set_text(lightstateLabel, "Switch:ON");       // 更新状态文本
        AW2013_Control_RGB(0xff,0xff,0xff);                    // 开启RGB LED（白光）
        My_Sensor.led_state = LIGHTON;                         // 更新传感器状态
    }
    else  // 开关关闭状态
    {
        lv_img_set_src(lightimg, &lightoff);                   // 设置关闭图标
        lv_label_set_text(lightstateLabel, "Switch:OFF");      // 更新状态文本
        AW2013_Control_RGB(0,0,0);                             // 关闭RGB LED
        My_Sensor.led_state = LIGHTOFF;                        // 更新传感器状态
    }
}

/**
 * @brief 模式切换开关事件处理函数
 * @param e  LVGL事件结构体指针，包含事件相关信息
 */
static void modeswitchCb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);    //获取事件类型代码
    lv_obj_t *target = lv_event_get_target(e);      //获取触发事件的对象（可能是开关、按钮等）

    if (code != LV_EVENT_CLICKED)  // 只处理点击事件
        return;

    if (lv_obj_has_state(target, LV_STATE_CHECKED))  // 智能模式
    {
        My_Mode.CurrentMode = SMART;                          // 设置为智能模式
        send_Bool_data("smart", 0, "false", "true");          // 发送智能模式状态
    }
    else  // 手动模式
    {
        My_Mode.CurrentMode = USR;                            // 设置为手动模式
        send_Bool_data("smart", 0, "false", "false");         // 发送手动模式状态
    }
}

/**
 * @brief WIFI连接按钮事件处理函数
 * @param e 事件对象
 */
static void WifiConnectBtnCb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    lv_obj_t *user = lv_event_get_user_data(e);

    if (LV_EVENT_CLICKED != code)  // 只处理点击事件
        return;

    // 获取WIFI用户名和密码
    const char *WifiNameBuffer = lv_textarea_get_text(WifiUsrnameTextarea);
    const char *WifiPassBuffer = lv_textarea_get_text(WifiPasswordTextarea);

    // 验证输入有效性
    if (0 == strlen(WifiNameBuffer) || 8 > strlen(WifiPassBuffer))
    {
        lv_obj_t *text = lv_msgbox_get_text(Msgbox);
        lv_label_set_text(text, "The account or password is incorrect");  // 显示错误消息
        lv_obj_clear_flag(Msgbox, LV_OBJ_FLAG_HIDDEN);                   // 显示消息框
        return;
    }
   
    /* WIFI连接 */
    if (ERRCODE_SUCC == wifi_connect((char *)WifiNameBuffer, (char *)WifiPassBuffer))
    {
        /* WIFI连接成功 */
        lv_obj_clear_flag(target, LV_OBJ_FLAG_CLICKABLE);     // 禁用按钮点击
        lv_label_set_text(user, "Connected");                 // 更新连接状态
        lv_obj_clear_flag(WifiLogo, LV_OBJ_FLAG_HIDDEN);      // 显示WIFI Logo
        lv_obj_align_to(user, target, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
        
        /* 获取IP地址 */
        lv_label_set_text_fmt(WifiIPLabel, "IP:%s", STA_IP);  // 显示IP地址
        lv_obj_align_to(WifiIPLabel, user, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

        osDelay(100);
			
        mqtt_connect();  // 连接MQTT
        return;
    }

    /* 连接失败处理 */
    lv_obj_t *text = lv_msgbox_get_text(Msgbox);
    lv_label_set_text(text, "WIFI Connect timeout");          // 显示超时消息
    lv_obj_clear_flag(Msgbox, LV_OBJ_FLAG_HIDDEN);           // 显示消息框
    wifi_sta_disable();                                       // 禁用WIFI
}

/**
 * @brief 数据展示界面初始化
 * @param parent 父对象（数据展示标签页指针）
 */
static void DataObjInit(lv_obj_t *parent)
{
    /* LED灯光显示区域 */
    lv_obj_t *lightBg = lv_img_create(parent);
    lv_img_set_src(lightBg, &light_background);              // 设置背景图
    lv_obj_align(lightBg, LV_ALIGN_TOP_RIGHT, -25, 4);      // 对齐到右上角

    lightimg = lv_img_create(lightBg);
    lv_img_set_src(lightimg, &lightoff);                     // 默认关闭图标
    lv_obj_center(lightimg);                                // 居中显示

    /* 灯光状态面板 */
    lv_obj_t *lightstatePanel = lv_obj_create(parent);
    lv_obj_add_style(lightstatePanel, &StatePanelStyle, LV_PART_MAIN);  // 应用样式
    lv_obj_align_to(lightstatePanel, lightBg, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lightstateLabel = lv_label_create(lightstatePanel);
    lv_label_set_text_fmt(lightstateLabel, "Switch:%s", "OFF");  // 初始状态为关闭
    lv_obj_center(lightstateLabel);

    /* 光敏传感器显示区域 */
    lv_obj_t *photosensitiveBg = lv_img_create(parent);
    lv_img_set_src(photosensitiveBg, &light_background);
    lv_obj_align(photosensitiveBg, LV_ALIGN_TOP_RIGHT, -25, 103);

    lv_obj_t *photosensitiveimg = lv_img_create(photosensitiveBg);
    lv_img_set_src(photosensitiveimg, &icon_illumination);  // 光照传感器图标
    lv_obj_center(photosensitiveimg);

    /* 光敏传感器状态面板 */
    lv_obj_t *photosensitivestate = lv_obj_create(parent);
    lv_obj_add_style(photosensitivestate, &StatePanelStyle, LV_PART_MAIN);
    lv_obj_align_to(photosensitivestate, photosensitiveBg, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    photosensitivestateLabel = lv_label_create(photosensitivestate);
    lv_obj_center(photosensitivestateLabel);  // 光照数值将动态更新
}

/**
 * @brief 本地控制界面初始化
 * @param parent 父对象（本地控制标签页指针）
 */
static void LocalObjInit(lv_obj_t *parent)
{
    // 设置基础样式
    lv_obj_set_style_text_font(parent, &syht_font_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(parent, lv_color_make(32, 53, 116), LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN);

    /* 圆形样式定义（用于开关指示器） */
    static lv_style_t circlestyle;
    lv_style_init(&circlestyle);
    lv_style_set_pad_all(&circlestyle, 0);
    lv_style_set_radius(&circlestyle, LV_RADIUS_CIRCLE);        // 圆形
    lv_style_set_border_color(&circlestyle, lv_color_make(60, 115, 246));
    lv_style_set_bg_color(&circlestyle, lv_color_make(60, 115, 246));
    lv_style_set_size(&circlestyle, 10);

    /* 模式选择界面 */
    lv_obj_t *ModeLabel = lv_label_create(parent);
    lv_label_set_text(ModeLabel, "模式选择");
    lv_obj_align(ModeLabel, LV_ALIGN_TOP_LEFT, 60, 10);

    modeswitch = lv_switch_create(parent);
    lv_obj_set_style_bg_color(modeswitch, lv_color_make(183, 200, 236), LV_PART_MAIN);
    lv_obj_align_to(modeswitch, ModeLabel, LV_ALIGN_OUT_BOTTOM_MID, 2, 5);

    // 手动模式标签
    lv_obj_t *usermode = lv_label_create(parent);
    lv_label_set_text(usermode, "手动");
    lv_obj_align_to(usermode, modeswitch, LV_ALIGN_OUT_LEFT_MID, -5, 0);

    // 智能模式标签
    lv_obj_t *aimode = lv_label_create(parent);
    lv_label_set_text(aimode, "智能");
    lv_obj_align_to(aimode, modeswitch, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

    /* 灯光控制界面 */
    lv_obj_t *lightswitchlabel = lv_label_create(parent);
    lv_label_set_text(lightswitchlabel, "灯光控制");
    lv_obj_align(lightswitchlabel, LV_ALIGN_TOP_LEFT, 60, 60);

    lightswitch = lv_switch_create(parent);
    lv_obj_set_style_bg_color(lightswitch, lv_color_make(183, 200, 236), LV_PART_MAIN);
    lv_obj_align_to(lightswitch, lightswitchlabel, LV_ALIGN_OUT_BOTTOM_MID, 2, 5);

    // 关闭标签
    lv_obj_t *lightofflabel = lv_label_create(parent);
    lv_label_set_text(lightofflabel, "关闭");
    lv_obj_align_to(lightofflabel, lightswitch, LV_ALIGN_OUT_LEFT_MID, -5, 0);

    // 打开标签
    lv_obj_t *lightonlabel = lv_label_create(parent);
    lv_label_set_text(lightonlabel, "打开");
    lv_obj_align_to(lightonlabel, lightswitch, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

    // 注册事件回调
    lv_obj_add_event_cb(modeswitch, modeswitchCb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(lightswitch, LightSwitchCb, LV_EVENT_CLICKED, NULL);
}

/**
 * @brief WIFI连接界面初始化
 * @param parent 父对象（WIFI标签页指针）
 */
static void WifiObjInit(lv_obj_t *parent)
{
    lv_obj_set_style_text_font(parent, &syht_font_10, LV_PART_MAIN);

    /* WIFI连接标题 */
    lv_obj_t *WifiTvNameLabel = lv_label_create(parent);
    lv_label_set_text(WifiTvNameLabel, "WIFI连接");
    lv_obj_align(WifiTvNameLabel, LV_ALIGN_TOP_MID, 0, 0);

    /* WIFI用户名输入框 */
    WifiUsrnameTextarea = lv_textarea_create(parent);
    lv_textarea_set_one_line(WifiUsrnameTextarea, true);      // 单行输入
    lv_obj_set_width(WifiUsrnameTextarea, 100);
    lv_obj_add_style(WifiUsrnameTextarea, &TextareaStyle, LV_PART_MAIN);
    lv_textarea_set_placeholder_text(WifiUsrnameTextarea, "请输入");  // 占位符文本
    lv_textarea_set_text(WifiUsrnameTextarea, WifiUsrname);   // 设置默认文本
    lv_obj_align_to(WifiUsrnameTextarea, WifiTvNameLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *UsrnameLabel = lv_label_create(parent);
    lv_label_set_text(UsrnameLabel, "名称");
    lv_obj_align_to(UsrnameLabel, WifiUsrnameTextarea, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    /* WIFI密码输入框 */
    WifiPasswordTextarea = lv_textarea_create(parent);
    lv_textarea_set_one_line(WifiPasswordTextarea, true);     // 单行输入
    lv_obj_set_width(WifiPasswordTextarea, 100);
    lv_obj_add_style(WifiPasswordTextarea, &TextareaStyle, LV_PART_MAIN);
    lv_textarea_set_placeholder_text(WifiPasswordTextarea, "请输入");  // 占位符文本
    lv_textarea_set_text(WifiPasswordTextarea, WifiPassword); // 设置默认文本
    lv_obj_align_to(WifiPasswordTextarea, WifiUsrnameTextarea, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 10);

    lv_obj_t *passwordLabel = lv_label_create(parent);
    lv_label_set_text(passwordLabel, "密码");
    lv_obj_align_to(passwordLabel, WifiPasswordTextarea, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    /* WIFI连接按钮 */
    lv_obj_t *WifiConnectBtn = lv_btn_create(parent);
    lv_obj_add_style(WifiConnectBtn, &BtnStyle, LV_PART_MAIN);
    lv_obj_align_to(WifiConnectBtn, WifiPasswordTextarea, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    lv_obj_t *WifiConnectBtnName = lv_label_create(WifiConnectBtn);
    lv_obj_set_style_opa(WifiConnectBtnName, LV_OPA_COVER, LV_PART_MAIN);
    lv_label_set_text(WifiConnectBtnName, "连接");
    lv_obj_center(WifiConnectBtnName);

    /* WIFI连接状态标签 */
    lv_obj_t *WifiStateLabel = lv_label_create(parent);
    lv_obj_set_style_text_color(WifiStateLabel, lv_color_make(61, 119, 178), LV_PART_MAIN);
    lv_label_set_text(WifiStateLabel, "Disconnect");          // 初始状态为未连接
    lv_obj_align_to(WifiStateLabel, WifiConnectBtn, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    /* IP地址显示标签 */
    WifiIPLabel = lv_label_create(parent);
    lv_obj_set_style_text_color(WifiIPLabel, lv_color_make(61, 119, 178), LV_PART_MAIN);
    lv_label_set_text(WifiIPLabel, "0.0.0.0");                // 初始IP地址
    lv_obj_align_to(WifiIPLabel, WifiStateLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    // 注册文本框事件回调
    lv_obj_add_event_cb(WifiUsrnameTextarea, Textarea_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(WifiPasswordTextarea, Textarea_cb, LV_EVENT_ALL, NULL);

    // 注册连接按钮事件回调
    lv_obj_add_event_cb(WifiConnectBtn, WifiConnectBtnCb, LV_EVENT_CLICKED, WifiStateLabel);
}

/**
 * @brief 样式初始化
 * @note 初始化文本框、按钮和数据展示面板的样式
 */
void StyleInit(void)
{
    /* WIFI文本框样式 */
    lv_style_init(&TextareaStyle);
    lv_style_set_bg_color(&TextareaStyle, lv_color_make(228, 235, 242));
    lv_style_set_border_width(&TextareaStyle, 1);
    lv_style_set_border_color(&TextareaStyle, lv_color_make(161, 183, 214));

    /* 控制按钮样式 */
    lv_style_init(&BtnStyle);
    lv_style_set_width(&BtnStyle, 80);
    lv_style_set_height(&BtnStyle, 35);
    lv_style_set_bg_color(&BtnStyle, lv_color_make(60, 115, 246));
    lv_style_set_radius(&BtnStyle, 6);

    /* 传感器状态文本框样式 */
    lv_style_init(&StatePanelStyle);
    lv_style_set_height(&StatePanelStyle, 25);
    lv_style_set_width(&StatePanelStyle, 100);
    lv_style_set_pad_all(&StatePanelStyle, 0);
    lv_style_set_bg_color(&StatePanelStyle, lv_color_make(57, 135, 253));
    lv_style_set_text_font(&StatePanelStyle, &syht_font_10);
    lv_style_set_text_color(&StatePanelStyle, lv_color_hex(0xFFFFFF));
}

/**
 * @brief UI初始化主函数
 * @note 创建整个用户界面的布局和组件
 */
void UI_Init(void)
{
    StyleInit();  // 初始化样式

    /* 获取当前活动屏幕对象 设置整个屏幕背景为淡蓝色 */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(221, 226, 252), LV_PART_MAIN);
    
    /* 标题栏 */
    lv_obj_t *TitleObj = lv_obj_create(lv_scr_act());   //创建一个最基本的矩形容器
    lv_obj_set_size(TitleObj, 320, 25);
    lv_obj_set_style_radius(TitleObj, 0, LV_PART_MAIN); //标题栏四个角都是直角
    lv_obj_set_style_border_width(TitleObj, 0, 0);      //无边框
    lv_obj_set_style_bg_color(TitleObj, lv_color_make(58, 112, 245), LV_PART_MAIN);     //标题栏背景为蓝色
    lv_obj_set_style_pad_all(TitleObj, 0, LV_PART_MAIN);    //内容紧贴边框，无内边距
    lv_obj_align(TitleObj, LV_ALIGN_TOP_MID, 0, 0);         //对象对齐到屏幕顶部中央
    
    /* 标题图像 */
    lv_obj_t *TitleImg = lv_img_create(TitleObj);
    lv_img_set_src(TitleImg, &title);   //设置图像源
    lv_obj_center(TitleImg);

    /* WIFI Logo */
    WifiLogo = lv_label_create(TitleObj);
    lv_label_set_text(WifiLogo, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(WifiLogo, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_add_flag(WifiLogo, LV_OBJ_FLAG_HIDDEN);  // 右侧对齐 初始隐藏
    lv_obj_align(WifiLogo, LV_ALIGN_RIGHT_MID, -20, 0);

    /* 创建数据展示区域 */
    lv_obj_t *DataObj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(DataObj, 120, 200);     // 设置尺寸
    lv_obj_set_style_pad_all(DataObj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(DataObj, lv_color_make(244, 248, 255), LV_PART_MAIN);
    lv_obj_set_style_opa(DataObj, LV_OPA_COVER, LV_PART_MAIN);  //设置背景完全不透明
    lv_obj_set_style_border_width(DataObj, 2, LV_PART_MAIN);    //设置边框宽度 为数据区域定义清晰的边界
    lv_obj_set_style_border_color(DataObj, lv_color_make(161, 183, 214), LV_PART_MAIN);
    lv_obj_align(DataObj, LV_ALIGN_BOTTOM_LEFT, 10, -10);   //左下角对齐

    /* 标签视图（包含Local和WIFI标签页） */
    lv_obj_t *tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 42);    //设置标签栏按钮的高度
    lv_obj_set_size(tabview, 175, 200);                     //设置标签尺寸
    lv_obj_set_style_radius(tabview, 10, LV_PART_MAIN);     //标签框的四角设置为圆角
    lv_obj_set_style_bg_color(tabview, lv_color_make(244, 248, 255), LV_PART_MAIN); //设置标签的背景色
    lv_obj_set_style_border_width(tabview, 2, LV_PART_MAIN);            //设置边框宽度
    lv_obj_set_style_border_color(tabview, lv_color_make(161, 183, 214), LV_PART_MAIN);
    lv_obj_align_to(tabview, DataObj, LV_ALIGN_OUT_RIGHT_MID, 10, 0);   //相对于数据展示区向右偏移10像素

    /* 标签按钮样式设置 */
    lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_radius(tab_btns, 3, 0);
    lv_obj_set_style_bg_color(tab_btns, lv_color_make(179, 208, 251), LV_PART_MAIN);    //未选中为浅蓝色
    lv_obj_set_style_bg_color(tab_btns, lv_color_make(62, 113, 213), LV_PART_ITEMS | LV_STATE_CHECKED); //被选中的标签按钮显示深蓝色，

    /* 创建标签页 */
    lv_obj_t *Local = lv_tabview_add_tab(tabview, "Local"); //标签页的内容容器，可以在其中添加控件
    lv_obj_t *MY_WIFI = lv_tabview_add_tab(tabview, "WIFI");

    /* 创建键盘（初始隐藏） */
    KeyBoardObj = lv_keyboard_create(lv_scr_act());
    lv_obj_add_flag(KeyBoardObj, LV_OBJ_FLAG_HIDDEN);

    /* 创建WiFi连接消息框 */
    static const char *msgboxbtns[] = {" ", " ", "Close", ""};
    Msgbox = lv_msgbox_create(lv_scr_act(), LV_SYMBOL_WARNING " Notice", "msgbox", msgboxbtns, false);
    lv_obj_set_style_bg_opa(Msgbox, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(Msgbox, LV_OBJ_FLAG_HIDDEN);  // 初始隐藏
    lv_obj_center(Msgbox);

    /* 消息框样式设置 */
    lv_obj_t *msgboxtitle = lv_msgbox_get_title(Msgbox);
    lv_obj_set_style_text_font(msgboxtitle, &lv_font_montserrat_14, LV_STATE_DEFAULT);//设置标题字体
    lv_obj_set_style_text_color(msgboxtitle, lv_color_hex(0xFF0000), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(Msgbox, MsgboxCb, LV_EVENT_VALUE_CHANGED, NULL);//添加消息框事件回调,消息被点击时隐藏该消息

    lv_obj_t *msgboxbtn = lv_msgbox_get_btns(Msgbox);
    lv_obj_set_style_bg_opa(msgboxbtn, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(msgboxbtn, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_color(msgboxbtn, lv_color_hex(0x2271df), LV_PART_ITEMS);

    /* 初始化各功能界面 */
    DataObjInit(DataObj);    // 数据展示界面
    LocalObjInit(Local);     // 本地控制界面
    WifiObjInit(MY_WIFI);    // WIFI连接界面
}