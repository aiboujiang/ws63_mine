/*
 * UI.c
 *
 *  Created on: Jul 1, 2024
 *      Author: 12433
 */

#include "ui.h"
char STA_IP[20];

static lv_obj_t *TitleObj;
static lv_obj_t *WifiLogo; /* WiFi Logo Obj */
static lv_style_t BtnStyle;
static lv_style_t TextareaStyle;
static lv_obj_t *KeyBoardObj;
lv_obj_t *modeswitch;

LV_IMG_DECLARE(fan);           /* Fan Img */
LV_IMG_DECLARE(text);          /* Title Img */
LV_FONT_DECLARE(syht_font_10); /* FONT 16 */

/* Data display panel:temperature, humidity */
lv_obj_t *Hum_arc;
lv_obj_t *Tmp_arc;
lv_obj_t *Tem_label;
lv_obj_t *Hum_label;
lv_obj_t *Msgbox;
/* Local control panel */
lv_obj_t *FanOnOff;
lv_obj_t *FanSpeed_1;
lv_obj_t *FanSpeed_2;
lv_obj_t *FanSpeed_3;
lv_obj_t *FanSpeedLabel;
lv_obj_t *CurrentSpreedBtn = NULL; /* Records the key that is currently pressed */
lv_obj_t *MqttConnectState;
lv_obj_t *MqttConnectBtn;


char MQTT_SUB_TOPIC[128];
char MQTT_PUB_TOPIC[128];
char MQTT_ENABLE_TLS[2] = {0};

struct Threshold_t Threshold;

static lv_obj_t *WifiPasswordTextarea;
static lv_obj_t *WifiUsrnameTextarea;
static lv_obj_t *WifiIPLabel;


/* MsbBox event */
static void MsgboxCb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_current_target(e);

    if (lv_msgbox_get_active_btn(target) == 2)
    {
        lv_obj_add_flag(Msgbox, LV_OBJ_FLAG_HIDDEN);
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
        lv_obj_set_style_bg_color(target, lv_color_make(157, 172, 186), LV_PART_MAIN);
        lv_label_set_text(user, "Connected");
        lv_obj_clear_flag(WifiLogo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align_to(user, target, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
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

/* Mode switching */
static void modeswitchCb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);

    if (code != LV_EVENT_CLICKED)
        return;

    if (lv_obj_has_state(target, LV_STATE_CHECKED))
    {
        My_Mode.CurrentMode = SMART;
        //send_Bool_data("smart", 0, "false", "true");
    }
    else
    {
        My_Mode.CurrentMode = USR;
       // send_Bool_data("smart", 0, "false", "false");
    }
}
/* Fan speed control button callback */
static void FanSpeed_Cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);

    if (LV_EVENT_CLICKED != code)
        return;

    if (CurrentSpreedBtn != target)
    {
        lv_obj_add_flag(CurrentSpreedBtn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(CurrentSpreedBtn, lv_color_make(10, 38, 116), LV_PART_MAIN);
        /* Updates the currently pressed key */
        CurrentSpreedBtn = target;

        lv_obj_clear_flag(target, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(target, lv_color_make(47, 174, 108), LV_PART_MAIN);

        if (target == FanSpeed_1)
        {
            user_pwm_change(FANSPREED1);
            My_Sensor.FanCurrentState = FANSPREED1;
            
        }
        else if (target == FanSpeed_2)
        {
            user_pwm_change(FANSPREED2);
            My_Sensor.FanCurrentState = FANSPREED2;
        }
        else if (target == FanSpeed_3)
        {
            user_pwm_change(FANSPREED3);
            My_Sensor.FanCurrentState = FANSPREED3;
        }
        else if (target == FanOnOff)
        {
            user_pwm_change(FANSPREEDOFF);
            My_Sensor.FanCurrentState = FANSPREEDOFF;
        }
        
        if (0 == My_Sensor.FanCurrentState)
        {
            lv_label_set_text(FanSpeedLabel, "FAN:OFF");
        }
        else
        {
            lv_label_set_text_fmt(FanSpeedLabel, "LEVEL:%d", My_Sensor.FanCurrentState);
        }
        send_Num_data("fan", 0, "true", My_Sensor.FanCurrentState);
    }

 
}

/* Data display panel Init */
static void DataObjInit(lv_obj_t *Parent)
{
    /* set DataObj style */
    lv_obj_set_style_text_font(Parent, &syht_font_10, LV_PART_MAIN);
    lv_obj_set_style_text_opa(Parent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(Parent, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    /* humidity arc */
    Hum_arc = lv_arc_create(Parent);
    lv_obj_clear_flag(Hum_arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(Hum_arc, 90, 90);
    lv_arc_set_range(Hum_arc, 0, 100);
    lv_arc_set_angles(Hum_arc, 120, 60);
    lv_arc_set_bg_angles(Hum_arc, 120, 60);
    lv_obj_remove_style(Hum_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_color(Hum_arc, lv_color_make(16, 32, 84), LV_PART_MAIN);
    lv_obj_set_style_arc_color(Hum_arc, lv_color_hex(0xFFFF00), LV_PART_INDICATOR);
    lv_obj_align(Hum_arc, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* temperature arc */
    Tmp_arc = lv_arc_create(Hum_arc);
    lv_obj_clear_flag(Tmp_arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(Tmp_arc, LV_PCT(80), LV_PCT(80));
    lv_arc_set_range(Tmp_arc, -20, 50);
    lv_arc_set_angles(Tmp_arc, 120, 60);
    lv_arc_set_bg_angles(Tmp_arc, 120, 60);
    lv_obj_remove_style(Tmp_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_color(Tmp_arc, lv_color_make(16, 32, 84), LV_PART_MAIN);
    lv_obj_set_style_arc_color(Tmp_arc, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
    lv_obj_center(Tmp_arc);

    /* Style panel */
    lv_obj_t *panel1 = lv_obj_create(Tmp_arc);
    lv_obj_set_size(panel1, LV_PCT(80), LV_PCT(80));
    lv_obj_set_style_radius(panel1, 360, LV_PART_MAIN);
    lv_obj_set_style_bg_color(panel1, lv_color_make(100, 100, 100), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(panel1, lv_color_hex(0x66FFE6), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(panel1, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel1, lv_color_make(45, 50, 60), LV_PART_MAIN);
    lv_obj_set_style_text_font(panel1, &syht_font_10, LV_PART_MAIN);
    lv_obj_set_style_text_opa(panel1, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(panel1, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(panel1);

    /* temperature display label */
    Tem_label = lv_label_create(panel1);
    lv_obj_align(Tem_label, LV_ALIGN_CENTER, 0, -10);

    /* humidity display label */
    Hum_label = lv_label_create(panel1);
    lv_obj_align(Hum_label, LV_ALIGN_CENTER, 0, 5);

    /* temperature color tag */
    lv_obj_t *Tem_tag = lv_obj_create(Parent);
    lv_obj_set_size(Tem_tag, 12, 12);
    lv_obj_set_style_pad_all(Tem_tag, 0, 0);
    lv_obj_set_style_bg_color(Tem_tag, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_align_to(Tem_tag, Hum_arc, LV_ALIGN_OUT_TOP_LEFT, -20, 0);
    lv_obj_t *TemTagLabel = lv_label_create(Parent);
    lv_label_set_text(TemTagLabel, "温度");
    lv_obj_align_to(TemTagLabel, Tem_tag, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    /* humidity color tag */
    lv_obj_t *Hum_tag = lv_obj_create(Parent);
    lv_obj_set_size(Hum_tag, 12, 12);
    lv_obj_set_style_pad_all(Hum_tag, 0, 0);
    lv_obj_set_style_bg_color(Hum_tag, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_align_to(Hum_tag, Tem_tag, LV_ALIGN_OUT_RIGHT_MID, 70, 0);
    lv_obj_t *HumTagLabel = lv_label_create(Parent);
    lv_label_set_text(HumTagLabel, "湿度");
    lv_obj_set_style_opa(HumTagLabel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align_to(HumTagLabel, Hum_tag, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    /* fan image init */
    lv_obj_t *fan_img = lv_img_create(Parent);
    lv_img_set_src(fan_img, &fan);
    lv_obj_align(fan_img, LV_ALIGN_TOP_RIGHT, -5, 20);

    /* fan spreed state */
    lv_obj_t *FanSpeedObj = lv_obj_create(Parent);
    lv_obj_set_size(FanSpeedObj, LV_PCT(50), LV_PCT(20));
    lv_obj_set_style_radius(FanSpeedObj, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(FanSpeedObj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(FanSpeedObj, lv_color_make(34, 54, 133), LV_PART_MAIN);
    lv_obj_set_style_border_width(FanSpeedObj, 5, LV_PART_MAIN);
    lv_obj_set_style_border_color(FanSpeedObj, lv_color_make(40, 60, 150), LV_PART_MAIN);
    lv_obj_set_style_shadow_spread(FanSpeedObj, 3, LV_PART_MAIN);
    lv_obj_align_to(FanSpeedObj, fan_img, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    lv_obj_set_style_text_color(FanSpeedObj, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    FanSpeedLabel = lv_label_create(FanSpeedObj);
    lv_label_set_text(FanSpeedLabel, "FAN:OFF");
    lv_obj_center(FanSpeedLabel);
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

/* Local control panel init */
static void LocalTvInit(lv_obj_t *parent)
{

    /* set obj font style */
    lv_obj_set_style_text_font(parent, &syht_font_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(parent, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN);

    lv_obj_t *ModeSw = lv_label_create(parent);
    lv_label_set_text(ModeSw, "模式选择");
    lv_obj_align(ModeSw, LV_ALIGN_TOP_LEFT, 60, 10);

    modeswitch = lv_switch_create(parent);
    lv_obj_set_style_bg_color(modeswitch, lv_color_make(183, 200, 236), LV_PART_MAIN);
    lv_obj_align_to(modeswitch, ModeSw, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_add_event_cb(modeswitch, modeswitchCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *usermode = lv_label_create(parent);
    lv_label_set_text(usermode, "手动");
    lv_obj_align_to(usermode, modeswitch, LV_ALIGN_OUT_LEFT_MID, -5, 0);

    lv_obj_t *aimode = lv_label_create(parent);
    lv_label_set_text(aimode, "智能");
    lv_obj_align_to(aimode, modeswitch, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

    /* Fan control panel */
    lv_obj_t *FanCtlObj = lv_obj_create(parent);
    lv_obj_set_size(FanCtlObj, 150, 95);
    lv_obj_set_style_bg_color(FanCtlObj, lv_color_make(17, 42, 109), LV_PART_MAIN);
    lv_obj_set_style_border_color(FanCtlObj, lv_color_make(25, 59, 154), LV_PART_MAIN);
    lv_obj_set_style_pad_all(FanCtlObj, 6, LV_PART_MAIN);
    lv_obj_align_to(FanCtlObj, ModeSw, LV_ALIGN_OUT_BOTTOM_LEFT, -60, 40);

    lv_obj_t *FanCtlObjTitle = lv_label_create(FanCtlObj);
    lv_label_set_text(FanCtlObjTitle, "风扇强度控制");
    lv_obj_set_style_text_color(FanCtlObjTitle, lv_color_make(116, 144, 219), LV_PART_MAIN);

    /* FanSpeed_1 FanSpeed_2 FanSpeed_3 FanOnOff: Fan Ctl Btn */
    FanSpeed_1 = lv_btn_create(FanCtlObj);
    lv_obj_add_style(FanSpeed_1, &BtnStyle, LV_PART_MAIN);
    lv_obj_align_to(FanSpeed_1, FanCtlObjTitle, LV_ALIGN_OUT_BOTTOM_LEFT, 1, 1);

    lv_obj_t *FanSpeed_1Label = lv_label_create(FanSpeed_1);
    lv_label_set_text(FanSpeed_1Label, "一档");
    lv_obj_set_style_text_font(FanSpeed_1Label, &syht_font_10, LV_PART_MAIN);
    lv_obj_center(FanSpeed_1Label);

    lv_obj_add_event_cb(FanSpeed_1, FanSpeed_Cb, LV_EVENT_CLICKED, NULL);

    FanSpeed_2 = lv_btn_create(FanCtlObj);
    lv_obj_add_style(FanSpeed_2, &BtnStyle, LV_PART_MAIN);
    lv_obj_align_to(FanSpeed_2, FanSpeed_1, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

    lv_obj_t *FanSpeed_2Label = lv_label_create(FanSpeed_2);
    lv_label_set_text(FanSpeed_2Label, "二档");
    lv_obj_set_style_text_font(FanSpeed_2Label, &syht_font_10, LV_PART_MAIN);
    lv_obj_center(FanSpeed_2Label);

    lv_obj_add_event_cb(FanSpeed_2, FanSpeed_Cb, LV_EVENT_CLICKED, NULL);

    FanSpeed_3 = lv_btn_create(FanCtlObj);
    lv_obj_add_style(FanSpeed_3, &BtnStyle, LV_PART_MAIN);
    lv_obj_align_to(FanSpeed_3, FanSpeed_1, LV_ALIGN_BOTTOM_MID, 0, 45);

    lv_obj_t *FanSpeed_3Label = lv_label_create(FanSpeed_3);
    lv_label_set_text(FanSpeed_3Label, "三档");
    lv_obj_set_style_text_font(FanSpeed_3Label, &syht_font_10, LV_PART_MAIN);
    lv_obj_center(FanSpeed_3Label);

    lv_obj_add_event_cb(FanSpeed_3, FanSpeed_Cb, LV_EVENT_CLICKED, NULL);

    FanOnOff = lv_btn_create(FanCtlObj);
    lv_obj_add_style(FanOnOff, &BtnStyle, LV_PART_MAIN);
    lv_obj_set_style_bg_color(FanOnOff, lv_color_make(47, 174, 108), LV_PART_MAIN);
    lv_obj_align_to(FanOnOff, FanSpeed_3, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    CurrentSpreedBtn = FanOnOff;

    lv_obj_t *FanOnOffLabel = lv_label_create(FanOnOff);
    lv_label_set_text(FanOnOffLabel, "OFF");
    lv_obj_center(FanOnOffLabel);

    lv_obj_add_event_cb(FanOnOff, FanSpeed_Cb, LV_EVENT_CLICKED, NULL);
}

/* WIFI control panel init */
static void WifiTvInit(lv_obj_t *parent)
{
    lv_obj_set_style_text_font(parent, &syht_font_10, LV_PART_MAIN);

    lv_obj_t *WifiTvNameLabel = lv_label_create(parent);
    lv_label_set_text(WifiTvNameLabel, "WIFI连接");
    lv_obj_set_style_text_color(WifiTvNameLabel, lv_color_make(255, 255, 255), LV_PART_MAIN);
    lv_obj_align(WifiTvNameLabel, LV_ALIGN_TOP_MID, 0, 0);

    WifiUsrnameTextarea = lv_textarea_create(parent);
    lv_textarea_set_one_line(WifiUsrnameTextarea, true);
    lv_obj_set_width(WifiUsrnameTextarea, 100);
    lv_obj_add_style(WifiUsrnameTextarea, &TextareaStyle, LV_PART_MAIN);
    lv_textarea_set_placeholder_text(WifiUsrnameTextarea, "请输入");
    lv_textarea_set_text(WifiUsrnameTextarea, WifiUsrname);
    lv_obj_align_to(WifiUsrnameTextarea, WifiTvNameLabel, LV_ALIGN_OUT_BOTTOM_MID, 10, 0);

    lv_obj_t *UsrnameLabel = lv_label_create(parent);
    lv_obj_set_style_text_color(UsrnameLabel, lv_color_make(255, 255, 255), LV_PART_MAIN);
    lv_label_set_text(UsrnameLabel, "名称");
    lv_obj_align_to(UsrnameLabel, WifiUsrnameTextarea, LV_ALIGN_OUT_LEFT_MID, -3, 0);

    WifiPasswordTextarea = lv_textarea_create(parent);
    lv_textarea_set_one_line(WifiPasswordTextarea, true);
    lv_obj_set_width(WifiPasswordTextarea, 100);
    lv_obj_add_style(WifiPasswordTextarea, &TextareaStyle, LV_PART_MAIN);
    lv_textarea_set_placeholder_text(WifiPasswordTextarea, "请输入");
    lv_textarea_set_text(WifiPasswordTextarea, WifiPassword);
    lv_obj_align_to(WifiPasswordTextarea, WifiUsrnameTextarea, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 5);

    lv_obj_t *passwordLabel = lv_label_create(parent);
    lv_obj_set_style_text_color(passwordLabel, lv_color_make(255, 255, 255), LV_PART_MAIN);
    lv_label_set_text(passwordLabel, "密码");
    lv_obj_align_to(passwordLabel, WifiPasswordTextarea, LV_ALIGN_OUT_LEFT_MID, -3, 0);

    lv_obj_t *WifiConnectBtn = lv_btn_create(parent);
    lv_obj_add_style(WifiConnectBtn, &BtnStyle, LV_PART_MAIN);
    lv_obj_align_to(WifiConnectBtn, WifiPasswordTextarea, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    lv_obj_t *WifiConnectBtnName = lv_label_create(WifiConnectBtn);
    lv_obj_set_style_opa(WifiConnectBtnName, LV_OPA_COVER, LV_PART_MAIN);
    lv_label_set_text(WifiConnectBtnName, "连接");
    lv_obj_center(WifiConnectBtnName);

    lv_obj_t *WifiStateLabel = lv_label_create(parent);
    lv_obj_set_style_text_color(WifiStateLabel, lv_color_make(255, 255, 255), LV_PART_MAIN);
    lv_label_set_text(WifiStateLabel, "Disconnect");
    lv_obj_align_to(WifiStateLabel, WifiConnectBtn, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    WifiIPLabel = lv_label_create(parent);
    lv_obj_set_style_text_color(WifiIPLabel, lv_color_make(255, 255, 255), LV_PART_MAIN);
    lv_label_set_text(WifiIPLabel, "0.0.0.0");
    lv_obj_align_to(WifiIPLabel, WifiStateLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_add_event_cb(WifiUsrnameTextarea, Textarea_cb, LV_EVENT_ALL, NULL);

    lv_obj_add_event_cb(WifiPasswordTextarea, Textarea_cb, LV_EVENT_ALL, NULL);

    lv_obj_add_event_cb(WifiConnectBtn, WifiConnectBtnCb, LV_EVENT_CLICKED, WifiStateLabel);
}

void StyleInit(void)
{
    /* Btn style Init */
    lv_style_init(&BtnStyle);
    lv_style_set_width(&BtnStyle, 60);
    lv_style_set_height(&BtnStyle, 33);
    lv_style_set_bg_color(&BtnStyle, lv_color_make(10, 38, 116));
    lv_style_set_shadow_width(&BtnStyle, 0);
    lv_style_set_border_width(&BtnStyle, 1);
    lv_style_set_radius(&BtnStyle, 6);
    lv_style_set_border_color(&BtnStyle, lv_color_make(20, 60, 170));

    /* Textarea style init */
    lv_style_init(&TextareaStyle);
    lv_style_set_bg_color(&TextareaStyle, lv_color_make(212, 220, 227));
    lv_style_set_border_width(&TextareaStyle, 1);
    lv_style_set_border_color(&TextareaStyle, lv_color_make(110, 140, 207));
}

/* UI INIT */
void UI_Init()
{
    /* style init */
    StyleInit();

    /* background color */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x131a3b), LV_PART_MAIN);

    /* Title */
    TitleObj = lv_img_create(lv_scr_act());
    lv_img_set_src(TitleObj, &text);
    lv_obj_align(TitleObj, LV_ALIGN_TOP_MID, 0, 0);

    /* Wifi Connect Logo */
    WifiLogo = lv_label_create(lv_scr_act());
    lv_label_set_text(WifiLogo, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(WifiLogo, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_add_flag(WifiLogo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align_to(WifiLogo, TitleObj, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    static lv_style_t PanelStyle;
    lv_style_init(&PanelStyle);
    lv_style_set_bg_color(&PanelStyle, lv_color_make(21, 49, 134));
    lv_style_set_border_width(&PanelStyle, 1);
    lv_style_set_border_color(&PanelStyle, lv_color_make(157, 185, 218));
    lv_style_set_shadow_width(&PanelStyle, 10);
    lv_style_set_shadow_spread(&PanelStyle, 2);
    lv_style_set_shadow_color(&PanelStyle, lv_color_make(22, 46, 124));
    lv_style_set_shadow_opa(&PanelStyle, LV_OPA_COVER);
    lv_style_set_pad_all(&PanelStyle, 0);

    static lv_style_t MainObjStyle;
    lv_style_init(&MainObjStyle);
    lv_style_set_bg_color(&MainObjStyle, lv_color_make(16, 32, 84));
    lv_style_set_border_width(&MainObjStyle, 0);
    lv_style_set_shadow_width(&MainObjStyle, 15);
    lv_style_set_shadow_spread(&MainObjStyle, 5);
    lv_style_set_shadow_color(&MainObjStyle, lv_color_make(16, 32, 84));
    lv_style_set_opa(&MainObjStyle, LV_OPA_COVER);
    lv_style_set_pad_all(&MainObjStyle, 0);

    /* Data display panel */
    lv_obj_t *DataObjPanel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(DataObjPanel, LV_PCT(45), LV_PCT(85));
    lv_obj_add_style(DataObjPanel, &PanelStyle, LV_PART_MAIN);
    lv_obj_align(DataObjPanel, LV_ALIGN_TOP_LEFT, 5, 30);

    lv_obj_t *DataObj = lv_obj_create(DataObjPanel);
    lv_obj_set_size(DataObj, LV_PCT(95), LV_PCT(96));
    lv_obj_add_style(DataObj, &MainObjStyle, LV_PART_MAIN);
    lv_obj_center(DataObj);

    /* Control panel */
    lv_obj_t *CtlObjPanel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(CtlObjPanel, LV_PCT(50), LV_PCT(85));
    lv_obj_add_style(CtlObjPanel, &PanelStyle, LV_PART_MAIN);
    lv_obj_align_to(CtlObjPanel, DataObj, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    lv_obj_t *CtlObj = lv_obj_create(CtlObjPanel);
    lv_obj_set_size(CtlObj, LV_PCT(95), LV_PCT(96));
    lv_obj_add_style(CtlObj, &MainObjStyle, LV_PART_MAIN);
    lv_obj_center(CtlObj);

    /* table View */
    lv_obj_t *tabview = lv_tabview_create(CtlObj, LV_DIR_TOP, 40);
    lv_obj_set_style_radius(tabview, 7, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tabview, lv_color_make(26, 37, 83), LV_PART_MAIN);

    lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_radius(tab_btns, 0, 0);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_color(tab_btns, lv_color_make(26, 121, 104), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tab_btns, lv_color_make(26, 121, 104), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_height(tab_btns, 35);

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

    /* KeyBoard */
    KeyBoardObj = lv_keyboard_create(lv_scr_act());
    lv_obj_add_flag(KeyBoardObj, LV_OBJ_FLAG_HIDDEN);

    /* Local control panel */
    lv_obj_t *LocalTv = lv_tabview_add_tab(tabview, "Local");

    /* wifi connection panel */
    lv_obj_t *WifiTv = lv_tabview_add_tab(tabview, "WIFI");

    /* Data display panel Init */
    DataObjInit(DataObj);

    //	/* Local control panel init */
    LocalTvInit(LocalTv);

    /* WIFI control panel init */
    WifiTvInit(WifiTv);
}
