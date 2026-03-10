#include "ui.h"
char STA_IP[20];
LV_IMG_DECLARE(title)
LV_IMG_DECLARE(buzzer)
LV_IMG_DECLARE(fire)
LV_IMG_DECLARE(icon_bg)

LV_FONT_DECLARE(syht_font_10)

static lv_style_t TextareaStyle;
static lv_style_t BtnStyle;

lv_obj_t *FlamestateLabel;
lv_obj_t *buzzerstateLabel;
lv_obj_t *modeswitch;
lv_obj_t *beepswitch;
lv_obj_t *lightstateLabel;

static lv_obj_t *WifiLogo;
static lv_obj_t *KeyBoardObj;

static lv_obj_t *WifiPasswordTextarea;
static lv_obj_t *WifiUsrnameTextarea;
static lv_obj_t *WifiIPLabel;

lv_obj_t *Msgbox;

static void MsgboxCb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_current_target(e);

    if (lv_msgbox_get_active_btn(target) == 2)
    {
        lv_obj_add_flag(Msgbox, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Textarea callback */
static void Textarea_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);

    char *Userdata = (char *)lv_event_get_user_data(e);

    if (LV_EVENT_FOCUSED == code)
    {
        if (NULL != Userdata)
        {
            lv_keyboard_set_mode(KeyBoardObj, LV_KEYBOARD_MODE_NUMBER);
        }
        if (NULL == Userdata)
        {
            lv_keyboard_set_mode(KeyBoardObj, LV_KEYBOARD_MODE_TEXT_LOWER);
        }
        if (lv_indev_get_type(lv_indev_get_act()) != LV_INDEV_TYPE_KEYPAD)
        {
            lv_keyboard_set_textarea(KeyBoardObj, target);
            lv_obj_set_style_max_height(KeyBoardObj, LV_HOR_RES * 1 / 3, 0);
            lv_obj_clear_flag(KeyBoardObj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_scroll_to_view_recursive(target, LV_ANIM_OFF);
        }
        return;
    }
    else if (code == LV_EVENT_DEFOCUSED)
    {
        lv_keyboard_set_textarea(KeyBoardObj, NULL);
        lv_obj_add_flag(KeyBoardObj, LV_OBJ_FLAG_HIDDEN);
    }
    else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
    {
        lv_obj_add_flag(KeyBoardObj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(target, LV_STATE_FOCUSED);
        lv_indev_reset(NULL, target); /*To forget the last clicked object to make it focusable again*/
    }
}




static void WifiConnectBtnCb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    lv_obj_t *user = lv_event_get_user_data(e);

    if (LV_EVENT_CLICKED != code)
        return;
    // 获取WIFI用户名和密码
    const char *WifiNameBuffer = lv_textarea_get_text(WifiUsrnameTextarea);
    const char *WifiPassBuffer = lv_textarea_get_text(WifiPasswordTextarea);

    if (0 == strlen(WifiNameBuffer) || 8 > strlen(WifiPassBuffer))
    {
        lv_obj_t *text = lv_msgbox_get_text(Msgbox);
        lv_label_set_text(text, "The account or password is incorrect");
        lv_obj_clear_flag(Msgbox, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    /* WIFI connection */
    if (ERRCODE_SUCC == wifi_connect((char *)WifiNameBuffer, (char *)WifiPassBuffer))
    {
        /* WIFI connection successful */
        lv_obj_clear_flag(target, LV_OBJ_FLAG_CLICKABLE);
        lv_label_set_text(user, "连接成功");
        lv_obj_align_to(user, target, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
        lv_obj_clear_flag(WifiLogo, LV_OBJ_FLAG_HIDDEN);
        /* Get IP */
        lv_label_set_text_fmt(WifiIPLabel, "IP:%s", STA_IP);
        lv_obj_align_to(WifiIPLabel, user, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
        
        osDelay(100);
        mqtt_connect();  // 连接MQTT
        return;
    }

    /* Connection failure */
    lv_obj_t *text = lv_msgbox_get_text(Msgbox);
    lv_label_set_text(text, "WIFI Connect timeout");
    lv_obj_clear_flag(Msgbox, LV_OBJ_FLAG_HIDDEN);
    wifi_sta_disable();   
}

/* Data display panel */
static void DataObjInit(lv_obj_t *parent)
{
    static lv_style_t StatePanelStyle;
    lv_style_init(&StatePanelStyle);
    lv_style_set_height(&StatePanelStyle, 30);
    lv_style_set_width(&StatePanelStyle, 100);
    lv_style_set_pad_all(&StatePanelStyle, 0);
    lv_style_set_bg_color(&StatePanelStyle, lv_color_make(109, 117, 254));
    lv_style_set_text_font(&StatePanelStyle, &syht_font_10);
    lv_style_set_text_color(&StatePanelStyle, lv_color_hex(0xFFFFFF));

    lv_obj_t *buzzerBg = lv_img_create(parent);
    lv_img_set_src(buzzerBg, &icon_bg);
    lv_obj_align(buzzerBg, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *buzzerimg = lv_img_create(buzzerBg);
    lv_img_set_src(buzzerimg, &buzzer);
    lv_obj_center(buzzerimg);

    lv_obj_t *buzzerstatePanel = lv_obj_create(parent);
    lv_obj_add_style(buzzerstatePanel, &StatePanelStyle, LV_PART_MAIN);
    lv_obj_align_to(buzzerstatePanel, buzzerBg, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    buzzerstateLabel = lv_label_create(buzzerstatePanel);
    lv_label_set_text(buzzerstateLabel, "BEEP:OFF");//报警器状态:关
    lv_obj_center(buzzerstateLabel);

    lv_obj_t *FlamesensorBg = lv_img_create(parent);
    lv_img_set_src(FlamesensorBg, &icon_bg);
    lv_obj_align(FlamesensorBg, LV_ALIGN_BOTTOM_LEFT, 30, -40);

    lv_obj_t *Flamesensorimg = lv_img_create(FlamesensorBg);
    lv_img_set_src(Flamesensorimg, &fire);
    lv_obj_center(Flamesensorimg);

    lv_obj_t *Flamestate = lv_obj_create(parent);
    lv_obj_add_style(Flamestate, &StatePanelStyle, LV_PART_MAIN);
    lv_obj_align_to(Flamestate, FlamesensorBg, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    FlamestateLabel = lv_label_create(Flamestate);
    lv_label_set_text(FlamestateLabel, "Flame:");
    lv_obj_center(FlamestateLabel);
}

static void modeswitchCb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);

    if (code != LV_EVENT_CLICKED)
        return;

    if (lv_obj_has_state(target, LV_STATE_CHECKED))
    {
        My_Mode.CurrentMode = SMART;
    }
    else
    {
        My_Mode.CurrentMode = USR;
    }
}

static void BeepSwitchCb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);

    if (code != LV_EVENT_CLICKED)
        return;

    if (lv_obj_has_state(target, LV_STATE_CHECKED))
    {
        lv_label_set_text(buzzerstateLabel, "BEEP:ON");
         uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_HIGH);
        My_Sensor.beep_state = BEEPON;
    }
    else
    {
        lv_label_set_text(buzzerstateLabel, "BEEP:OFF");
        uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_LOW);
        My_Sensor.beep_state = BEEPOFF;
    }
}
static void LocalObjInit(lv_obj_t *parent)
{
    lv_obj_set_style_text_font(parent, &syht_font_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(parent, lv_color_make(32, 53, 116), LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN);

    static lv_style_t circlestyle;
    lv_style_init(&circlestyle);
    lv_style_set_pad_all(&circlestyle, 0);
    lv_style_set_radius(&circlestyle, LV_RADIUS_CIRCLE);
    lv_style_set_border_color(&circlestyle, lv_color_make(91, 98, 246));
    lv_style_set_bg_color(&circlestyle, lv_color_make(91, 98, 246));
    lv_style_set_size(&circlestyle, 15);

    lv_obj_t *ModeLabel = lv_label_create(parent);
    lv_label_set_text(ModeLabel, "模式选择");
    lv_obj_align(ModeLabel, LV_ALIGN_TOP_LEFT, 60, 10);

    modeswitch = lv_switch_create(parent);
    lv_obj_set_style_bg_color(modeswitch, lv_color_make(183, 200, 236), LV_PART_MAIN);
    lv_obj_align_to(modeswitch, ModeLabel, LV_ALIGN_OUT_BOTTOM_MID, 2, 5);

    lv_obj_t *usermode = lv_label_create(parent);
    lv_label_set_text(usermode, "手动");
    lv_obj_align_to(usermode, modeswitch, LV_ALIGN_OUT_LEFT_MID, -5, 0);

    lv_obj_t *aimode = lv_label_create(parent);
    lv_label_set_text(aimode, "智能");
    lv_obj_align_to(aimode, modeswitch, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_add_event_cb(modeswitch, modeswitchCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *beepswitchlabel = lv_label_create(parent);
    lv_label_set_text(beepswitchlabel, "报警控制");
    lv_obj_align(beepswitchlabel, LV_ALIGN_TOP_LEFT, 60, 60);

    beepswitch = lv_switch_create(parent);
    lv_obj_set_style_bg_color(beepswitch, lv_color_make(183, 200, 236), LV_PART_MAIN);
    lv_obj_align_to(beepswitch, beepswitchlabel, LV_ALIGN_OUT_BOTTOM_MID, 2, 5);

    lv_obj_t *lightofflabel = lv_label_create(parent);
    lv_label_set_text(lightofflabel, "关闭");
    lv_obj_align_to(lightofflabel, beepswitch, LV_ALIGN_OUT_LEFT_MID, -5, 0);

    lv_obj_t *lightonlabel = lv_label_create(parent);
    lv_label_set_text(lightonlabel, "打开");
    lv_obj_align_to(lightonlabel, beepswitch, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

    lv_obj_add_event_cb(beepswitch, BeepSwitchCb, LV_EVENT_CLICKED, NULL);
}

/* WIFI control panel init */
static void WifiObjInit(lv_obj_t *parent)
{
    lv_obj_set_style_text_font(parent, &syht_font_10, LV_PART_MAIN);

    lv_obj_t *WifiTvNameLabel = lv_label_create(parent);
    lv_label_set_text(WifiTvNameLabel, "WIFI连接");
    lv_obj_align(WifiTvNameLabel, LV_ALIGN_TOP_MID, 0, 0);

    WifiUsrnameTextarea = lv_textarea_create(parent);
    lv_textarea_set_one_line(WifiUsrnameTextarea, true);
    lv_obj_set_width(WifiUsrnameTextarea, 100);
    lv_obj_add_style(WifiUsrnameTextarea, &TextareaStyle, LV_PART_MAIN);
    lv_textarea_set_placeholder_text(WifiUsrnameTextarea, "请输入");
    lv_textarea_set_text(WifiUsrnameTextarea, WifiUsrname);
    lv_obj_align_to(WifiUsrnameTextarea, WifiTvNameLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *UsrnameLabel = lv_label_create(parent);
    lv_label_set_text(UsrnameLabel, "名称");
    lv_obj_align_to(UsrnameLabel, WifiUsrnameTextarea, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    WifiPasswordTextarea = lv_textarea_create(parent);
    lv_textarea_set_one_line(WifiPasswordTextarea, true);
    lv_obj_set_width(WifiPasswordTextarea, 100);
    lv_obj_add_style(WifiPasswordTextarea, &TextareaStyle, LV_PART_MAIN);
    lv_textarea_set_placeholder_text(WifiPasswordTextarea, "请输入");
    lv_textarea_set_text(WifiPasswordTextarea, WifiPassword);
    lv_obj_align_to(WifiPasswordTextarea, WifiUsrnameTextarea, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 10);

    lv_obj_t *passwordLabel = lv_label_create(parent);
    lv_label_set_text(passwordLabel, "密码");
    lv_obj_align_to(passwordLabel, WifiPasswordTextarea, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    lv_obj_t *WifiConnectBtn = lv_btn_create(parent);
    lv_obj_add_style(WifiConnectBtn, &BtnStyle, LV_PART_MAIN);
    lv_obj_align_to(WifiConnectBtn, WifiPasswordTextarea, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    lv_obj_t *WifiConnectBtnName = lv_label_create(WifiConnectBtn);
    lv_obj_set_style_opa(WifiConnectBtnName, LV_OPA_COVER, LV_PART_MAIN);
    lv_label_set_text(WifiConnectBtnName, "连接");
    lv_obj_center(WifiConnectBtnName);

    lv_obj_t *WifiStateLabel = lv_label_create(parent);
    lv_obj_set_style_text_color(WifiStateLabel, lv_color_make(61, 119, 178), LV_PART_MAIN);
    lv_label_set_text(WifiStateLabel, "未连接");
    lv_obj_align_to(WifiStateLabel, WifiConnectBtn, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    WifiIPLabel = lv_label_create(parent);
    lv_obj_set_style_text_color(WifiIPLabel, lv_color_make(61, 119, 178), LV_PART_MAIN);
    lv_label_set_text(WifiIPLabel, "0.0.0.0");
    lv_obj_align_to(WifiIPLabel, WifiStateLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_add_event_cb(WifiUsrnameTextarea, Textarea_cb, LV_EVENT_ALL, NULL);

    lv_obj_add_event_cb(WifiPasswordTextarea, Textarea_cb, LV_EVENT_ALL, NULL);

    lv_obj_add_event_cb(WifiConnectBtn, WifiConnectBtnCb, LV_EVENT_CLICKED, WifiStateLabel);
}


void StyleInit(void)
{
    /* Textarea style init */
    lv_style_init(&TextareaStyle);
    lv_style_set_bg_color(&TextareaStyle, lv_color_make(228, 235, 242));
    lv_style_set_border_width(&TextareaStyle, 1);
    lv_style_set_border_color(&TextareaStyle, lv_color_make(161, 183, 214));
    lv_style_set_text_color(&TextareaStyle, lv_color_make(81, 85, 160));

    /* btn style Init */
    lv_style_init(&BtnStyle);
    lv_style_set_width(&BtnStyle, 80);
    lv_style_set_height(&BtnStyle, 35);
    lv_style_set_bg_color(&BtnStyle, lv_color_make(102, 110, 254));
    lv_style_set_radius(&BtnStyle, 6);
}

void UI_Init()
{
    StyleInit();

    /* background color */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(210, 208, 251), LV_PART_MAIN);

    /* Title */
    lv_obj_t *TitleObj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(TitleObj, 320, 25);
    lv_obj_set_style_radius(TitleObj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(TitleObj, 0, 0);
    lv_obj_set_style_bg_color(TitleObj, lv_color_make(89, 98, 255), LV_PART_MAIN);
    lv_obj_set_style_pad_all(TitleObj, 0, LV_PART_MAIN);
    lv_obj_align(TitleObj, LV_ALIGN_TOP_MID, 0, 0);

    /* Title img */
    lv_obj_t *TitleImg = lv_img_create(TitleObj);
    lv_img_set_src(TitleImg, &title);
    lv_obj_center(TitleImg);

    /* Wifi Connect Logo */
    WifiLogo = lv_label_create(TitleObj);
    lv_label_set_text(WifiLogo, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(WifiLogo, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_add_flag(WifiLogo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(WifiLogo, LV_ALIGN_RIGHT_MID, -20, 0);

    /* data display panel */
    lv_obj_t *DataObj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(DataObj, 120, 200);
    lv_obj_set_style_pad_all(DataObj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(DataObj, lv_color_make(244, 248, 255), LV_PART_MAIN);
    lv_obj_set_style_opa(DataObj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(DataObj, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(DataObj, lv_color_make(161, 183, 214), LV_PART_MAIN);
    lv_obj_align(DataObj, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    lv_obj_t *tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 42);
    lv_obj_set_size(tabview, 175, 200);
    lv_obj_set_style_radius(tabview, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tabview, lv_color_make(244, 248, 255), LV_PART_MAIN);
    lv_obj_set_style_border_width(tabview, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(tabview, lv_color_make(161, 183, 214), LV_PART_MAIN);
    lv_obj_align_to(tabview, DataObj, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_radius(tab_btns, 5, 0);
    lv_obj_set_style_border_color(tab_btns, lv_color_make(85, 94, 249), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(tab_btns, lv_color_make(214, 216, 255), LV_PART_MAIN);
    lv_obj_set_style_bg_color(tab_btns, lv_color_make(147, 142, 252), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tab_btns, lv_color_make(48, 54, 171), LV_PART_MAIN);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_t *Local = lv_tabview_add_tab(tabview, "Local");
    lv_obj_t *WifiTv = lv_tabview_add_tab(tabview, "WIFI");

    /* KeyBoard */
    KeyBoardObj = lv_keyboard_create(lv_scr_act());
    lv_obj_add_flag(KeyBoardObj, LV_OBJ_FLAG_HIDDEN);

    /* the msgbox */
    static const char *msgboxbtns[] = {" ", " ", "Close", ""};
    Msgbox = lv_msgbox_create(lv_scr_act(), LV_SYMBOL_WARNING " Notice", "msgbox", msgboxbtns, false);
    lv_obj_set_style_bg_opa(Msgbox, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(Msgbox, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(Msgbox);

    lv_obj_t *msgboxtitle = lv_msgbox_get_title(Msgbox);
    lv_obj_set_style_text_font(msgboxtitle, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(msgboxtitle, lv_color_hex(0xFF0000), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(Msgbox, MsgboxCb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *msgboxbtn = lv_msgbox_get_btns(Msgbox);
    lv_obj_set_style_bg_opa(msgboxbtn, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(msgboxbtn, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_color(msgboxbtn, lv_color_hex(0x2271df), LV_PART_ITEMS);

    DataObjInit(DataObj);
    LocalObjInit(Local);
    WifiObjInit(WifiTv);
}
