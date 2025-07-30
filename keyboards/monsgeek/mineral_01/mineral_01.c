// Copyright 2024 yangzheng20003 (@yangzheng20003)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "wls/wls.h"
#include "rgb_record/rgb_record.h"
#include "rgb_record/rgb_rgblight.h"

#ifdef WIRELESS_ENABLE
#    include "wireless.h"
#    include "usb_main.h"
#    include "lowpower.h"
#endif

#include "rgb_light_mode.h"
extern confinfo_t confinfo;

typedef struct {
    bool active;
    uint32_t timer;
    uint32_t interval;
    uint32_t times;
    uint8_t index;
    RGB rgb;
    void (*blink_cb)(uint8_t);
} hs_rgb_indicator_t;

enum layers {
    _BL = 0,
    _FL,
    _MBL,
    _MFL,
    _FBL,
};

hs_rgb_indicator_t hs_rgb_indicators[HS_RGB_INDICATOR_COUNT];
hs_rgb_indicator_t hs_rgb_bat[HS_RGB_BAT_COUNT];
void rgb_blink_dir(void);
void hs_reset_settings(void);
void rgb_matrix_hs_indicator(void);
void rgb_matrix_hs_indicator_set(uint8_t index, RGB rgb, uint32_t interval, uint8_t times);
void rgb_matrix_hs_set_remain_time(uint8_t index, uint8_t remain_time);

#define keymap_is_mac_system() ((get_highest_layer(default_layer_state) == _MBL) || (get_highest_layer(default_layer_state) == _MFL))
#define keymap_is_base_layer() ((get_highest_layer(default_layer_state) == _BL) || (get_highest_layer(default_layer_state) == _FL))

uint32_t post_init_timer     = 0x00;
bool inqbat_flag             = false;
bool mac_status              = false;
bool charging_state          = false;
bool bat_full_flag           = false;
bool enable_bat_indicators   = true;
uint32_t bat_indicator_cnt   = true;
static uint32_t ee_clr_timer = 0;
// static uint32_t rec_time;
bool test_white_light_flag = false;
HSV start_hsv;
bool no_record_fg;
bool lower_sleep = false;
bool led_status;
bool hs_color_test;
bool hs_frequency_test;
extern bool hs_led_flag;
uint32_t hs_color_time;
uint8_t hs_color_flag;
uint8_t buff[]   = {14, 8, 2, 1, 1, 1, 1, 1, 1, 1, 0};

void eeconfig_confinfo_update(void) {

    eeconfig_update_kb(confinfo.raw);
}

uint32_t eeconfig_confinfo_read(void) {

    return eeconfig_read_kb();
}

uint8_t hs_get_sleep_timeout(void){
    return confinfo.sleep_timeout;
}

void hs_set_sleep_timeout(uint8_t sleep_timeout){
    confinfo.sleep_timeout = sleep_timeout;
}

bool hs_get_rgb_enable(void) {
#ifdef RGBLIGHT_ENABLE
    return confinfo.rgb_enable;
#else
    return rgb_matrix_config.enable;
#endif
}

void hs_set_rgb_enable(bool state) {
#ifdef RGBLIGHT_ENABLE
    confinfo.rgb_enable = state;
    if (confinfo.rgb_enable)
        rgblight_enable();
    else
        rgblight_disable();
    eeconfig_confinfo_update();
#else
    rgb_matrix_config.enable = state;
#endif
}

void eeconfig_confinfo_default(void) {
    hs_led_flag = false;
    confinfo.flag             = true;
    // confinfo.record_channel   = 0;
    confinfo.record_last_mode = 0xff;
    confinfo.last_btdevs      = 1;
    confinfo.dir_flag         = 0;
    confinfo.ctrl_app_flag    = 0;
    confinfo.sleep_timeout    = 7;
    confinfo.rgb_enable = true;
    confinfo.moude = 0;
    confinfo.rgb_color = 0;
    keymap_config.no_gui = 0;

#if defined(NKRO_ENABLE) && defined(FORCE_NKRO)
    keymap_config.nkro = 1;
#endif
    eeconfig_update_keymap(keymap_config.raw);
    //#ifdef WIRELESS_ENABLE
    //     confinfo.devs = DEVS_USB;
    // #endif
   
    confinfo.devs = wireless_get_current_devs();
    confinfo.rgb_tog_filp = false;
    eeconfig_init_user_datablock();
    eeconfig_confinfo_update();

    rgblight_mode(1);
    HSV hsv = rgblight_get_hsv();
    rgblight_sethsv(200, 255, hsv.v);

// #ifdef RGBLIGHT_ENABLE
//     rgblight_mode(buff[0]);
// #endif
}

void eeconfig_confinfo_init(void) {

    confinfo.raw = eeconfig_confinfo_read();
    if (!confinfo.raw) {
        eeconfig_confinfo_default();
    }
}
static uint8_t mode = 0;
static bool usb_status = false;
static uint32_t usb_time;
void keyboard_post_init_kb(void) {

    /* 设置键盘usb枚举和rgb灯抢占式控制处理 */
    {
        rgb_matrix_disable();
        usb_status = true;
        usb_time = timer_read32();
    }
    
#ifdef CONSOLE_ENABLE
    debug_enable = true;
#endif

    eeconfig_confinfo_init();

#ifdef LED_POWER_EN_PIN
    gpio_set_pin_output(LED_POWER_EN_PIN);
    if (rgb_matrix_get_val() != 0 && hs_get_rgb_enable()) gpio_write_pin_high(LED_POWER_EN_PIN);

    gpio_set_pin_output(HS_LED_BOOSTING_PIN);
    gpio_write_pin_high(HS_LED_BOOSTING_PIN);

    gpio_set_pin_output(B9);
#endif

#ifdef HS_BT_DEF_PIN
    setPinInputHigh(HS_BT_DEF_PIN);
#endif

#ifdef HS_2G4_DEF_PIN
    setPinInputHigh(HS_2G4_DEF_PIN);
#endif

#ifdef USB_POWER_EN_PIN
    gpio_write_pin_low(USB_POWER_EN_PIN);
    gpio_set_pin_output(USB_POWER_EN_PIN);
#endif

#ifdef HS_BAT_CABLE_PIN
    setPinInput(HS_BAT_CABLE_PIN);
#endif

#ifdef BAT_FULL_PIN
    setPinInputHigh(BAT_FULL_PIN);
#endif

    setPinInputHigh(SYSTEM_WIN_PIN);
    setPinInputHigh(SYSTEM_MAC_PIN);

    gpio_set_pin_output(CPAS_PIN);
    gpio_set_pin_output(WIN_PIN );

#ifdef WIRELESS_ENABLE
    wireless_init();
#    if (!(defined(HS_BT_DEF_PIN) && defined(HS_2G4_DEF_PIN)))

    wireless_devs_change(!DEVS_USB, confinfo.devs, false);
    
#    endif
    post_init_timer = timer_read32();
#endif

    keyboard_post_init_user();

    rgbrec_init(1);
    // rgblight_enable();
    
#ifdef RGB_MATRIX_ENABLE
#    if defined(RGBLIGHT_ENABLE) && defined(LED_POWER_EN_PIN)
    if ((rgblight_is_enabled() != true) && (hs_get_rgb_enable() != true)) {
        // rgb_matrix_disable();
        gpio_write_pin_low(LED_POWER_EN_PIN);

    } else {
        gpio_write_pin_high(LED_POWER_EN_PIN);

    }
#    elif defined(LED_POWER_EN_PIN)
    if (rgb_matrix_is_enabled() != true) {
        // rgb_matrix_disable();
        gpio_write_pin_low(LED_POWER_EN_PIN);

    } else {
        gpio_write_pin_high(LED_POWER_EN_PIN);
    }
#    endif
#endif
    start_hsv = rgb_matrix_get_hsv();
    if (!rgblight_is_enabled()) mode = 10;

}

#ifdef WIRELESS_ENABLE

void usb_power_connect(void) {

#    ifdef USB_POWER_EN_PIN
    gpio_write_pin_low(USB_POWER_EN_PIN);
#    endif
}

void usb_power_disconnect(void) {

#    ifdef USB_POWER_EN_PIN
    gpio_write_pin_high(USB_POWER_EN_PIN);
#    endif
}
bool temp = false;
void suspend_power_down_kb(void) {

#    ifdef LED_POWER_EN_PIN
    gpio_write_pin_low(LED_POWER_EN_PIN);
#    endif
    
    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {

#    ifdef LED_POWER_EN_PIN
    if (rgb_matrix_get_val() != 0 && hs_get_rgb_enable()) gpio_write_pin_high(LED_POWER_EN_PIN);
#    endif

    wireless_devs_change(wireless_get_current_devs(), wireless_get_current_devs(), false);
    suspend_wakeup_init_user();
    hs_rgb_blink_set_timer(timer_read32());
    temp = false;
}

bool lpwr_is_allow_timeout_hook(void) {

    gpio_write_pin_high(WIN_PIN);
    gpio_write_pin_high(CPAS_PIN);

    // if (wireless_get_current_devs() == DEVS_USB) {
    //     return false;
    // }

    return true;
}

void wireless_post_task(void) {

    // auto switching devs
    if (post_init_timer && timer_elapsed32(post_init_timer) >= 100) {

        md_send_devctrl(MD_SND_CMD_DEVCTRL_FW_VERSION);   // get the module fw version.
        md_send_devctrl(MD_SND_CMD_DEVCTRL_SLEEP_BT_EN);  // timeout 30min to sleep in bt mode, enable
        md_send_devctrl(MD_SND_CMD_DEVCTRL_SLEEP_2G4_DIS); // timeout 30min to sleep in 2.4g mode, enable
        wireless_devs_change(!confinfo.devs, confinfo.devs, false);
        post_init_timer = 0x00;
        if (confinfo.devs == DEVS_2G4){
            md_send_devctrl(0xFF);
        }
        else{
            md_send_devctrl(0x80+30);
        }
    }

    hs_mode_scan(false, confinfo.devs, confinfo.last_btdevs);

}
bool state = false;

uint32_t wls_process_long_press(uint32_t trigger_time, void *cb_arg) {
    uint16_t keycode = *((uint16_t *)cb_arg);

    switch (keycode) {
        case KC_BT1: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT1, true);
                // if (wireless_get_current_devs() != DEVS_BT1) {
                    state = true;
                    hs_rgb_blink_set_timer(timer_read32());
                // }
            }

        } break;
        case KC_BT2: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT2, true);
                // if (wireless_get_current_devs() != DEVS_BT1) {
                    state = true;
                    hs_rgb_blink_set_timer(timer_read32());
                // }
            }
        } break;
        case KC_BT3: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT3, true);
                // if (wireless_get_current_devs() != DEVS_BT1) {
                    state = true;
                    hs_rgb_blink_set_timer(timer_read32());
                // }
            }
        } break;
        case KC_2G4: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_2g4) || (mode == hs_wireless) || (mode == hs_none)) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_2G4, true);
                // if (wireless_get_current_devs() != DEVS_BT1) {
                    state = true;
                    hs_rgb_blink_set_timer(timer_read32());
                // }
            }
        } break;
        case BT_TEST: {
            // md_send_devctrl(0x62);
            // wait_ms(30);
            // wireless_devs_change(wireless_get_current_devs(), DEVS_BT2, true);
        } break;
        default:
            break;
    }

    return 0;
}

bool process_record_wls(uint16_t keycode, keyrecord_t *record) {
    static uint16_t keycode_shadow                     = 0x00;
    static deferred_token wls_process_long_press_token = INVALID_DEFERRED_TOKEN;

    keycode_shadow = keycode;

#    ifndef WLS_KEYCODE_PAIR_TIME
#        define WLS_KEYCODE_PAIR_TIME 3000
#    endif

#    define WLS_KEYCODE_EXEC(wls_dev)                                                                                          \
        do {                                                                                                                   \
            if (record->event.pressed) {                                                                                       \
                if (wireless_get_current_devs() != wls_dev)                                                                    \
                    wireless_devs_change(wireless_get_current_devs(), wls_dev, false);                                         \
                if (wls_process_long_press_token == INVALID_DEFERRED_TOKEN) {                                                  \
                    wls_process_long_press_token = defer_exec(WLS_KEYCODE_PAIR_TIME, wls_process_long_press, &keycode_shadow); \
                }                                                                                                              \
            } else {                                                                                                           \
                cancel_deferred_exec(wls_process_long_press_token);                                                            \
                wls_process_long_press_token = INVALID_DEFERRED_TOKEN;                                                         \
            }                                                                                                                  \
        } while (false)

    switch (keycode) {
        // case BT_TEST:{
            // WLS_KEYCODE_EXEC(wireless_get_current_devs());
        // }break;

        case KC_BT1: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_BT1);
                md_send_devctrl(0x80+30);
                if (record->event.pressed) {suspend_wakeup_init();state = false;
                hs_rgb_blink_set_timer(timer_read32());}
            }

        } break;
        case KC_BT2: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_BT2);
                md_send_devctrl(0x80+30);
                if (record->event.pressed) {suspend_wakeup_init();state = false;
                hs_rgb_blink_set_timer(timer_read32());}
            }
        } break;
        case KC_BT3: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_BT3);
                md_send_devctrl(0x80+30);
                if (record->event.pressed) {suspend_wakeup_init();state = false;
                hs_rgb_blink_set_timer(timer_read32());}
            }
        } break;
        case KC_2G4: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_2g4) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_2G4);
                md_send_devctrl(0xFF);
                if (record->event.pressed) {suspend_wakeup_init();state = false;
                hs_rgb_blink_set_timer(timer_read32());}
            }
        } break;

        case KC_USB: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_2g4) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_USB);
                state = false;
                hs_rgb_blink_set_timer(timer_read32());
            }
        } break;
        default:
            return true;
    }

    return false;
}
#endif

bool process_record_user(uint16_t keycode, keyrecord_t *record) {

    if (test_white_light_flag && record->event.pressed) {
        test_white_light_flag = false;
        rgb_matrix_set_color_all(0x00, 0x00, 0x00);
    }

    if (*md_getp_state() == MD_STATE_CONNECTED && record->event.pressed) {
        hs_rgb_blink_set_timer(timer_read32());
    }

    switch (keycode) {
        case MO(_FL):
        case MO(_MFL): {
            if (!record->event.pressed && rgbrec_is_started()) {
                if (no_record_fg == true) {
                    no_record_fg = false;
                    rgbrec_register_record(keycode, record);
                }
                no_record_fg = true;
            }
            break;
        }
        case RP_END:
        case RP_P0:
        case RP_P1:
        case RP_P2:
        case RGB_MOD:
            break;
        default: {
            if (rgbrec_is_started()) {
                if (!IS_QK_MOMENTARY(keycode) && record->event.pressed) {
                    rgbrec_register_record(keycode, record);

                    return false;
                }
            }
        } break;
    }

    if (rgbrec_is_started() && (!(keycode == RP_P0 || keycode == RP_P1 || keycode == RP_P2 || keycode == RP_END || keycode == RGB_MOD || keycode == MO(_FL) || keycode == MO(_MFL)))) {

        return false;
    }

    return true;
}

void im_rgblight_increase(void) {
    HSV rgb;
    uint8_t moude;

    moude = rgblight_get_mode();
    if (moude == 1) {
        rgb = rgblight_get_hsv();
        if (rgb.h == 0 && rgb.s != 0)
            mode = 3;
        else
            mode = 9;
        switch (rgb.h) {
            case 40: {
                mode = 4;
            } break;
            case 80: {
                mode = 5;
            } break;
            case 120: {
                mode = 6;
            } break;
            case 160: {
                mode = 7;
            } break;
            case 200: {
                mode = 8;
            } break;
            default:
                break;
        }
    }
    else if (moude == 8) mode = 1;
    else if (moude == 2) mode = 2;
    mode++;
    if (mode == 11) mode = 0;
    if (mode == 10) {
        rgb = rgblight_get_hsv();
        rgblight_sethsv(0, 255, rgb.v);
        rgblight_disable();
    } else {
        rgblight_enable();
        rgblight_mode(buff[mode]);
    }

    rgb = rgblight_get_hsv();
    switch (mode) {
        case 3: {
            rgblight_sethsv(0, 255, rgb.v);
        } break;
        case 4: {
            rgblight_sethsv(40, 255, rgb.v);
        } break;
        case 5: {
            rgblight_sethsv(80, 255, rgb.v);
        } break;
        case 6: {
            rgblight_sethsv(120, 255, rgb.v);
        } break;
        case 7: {
            rgblight_sethsv(160, 255, rgb.v);
        } break;
        case 8: {
            rgblight_sethsv(200, 255, rgb.v);
        } break;
        case 9: {
            rgblight_sethsv(0, 0, rgb.v);
        } break;
        case 0: {
            rgblight_set_speed(255);
        } break;
        default: {
            rgblight_set_speed(200);
        } break;
    }
}

uint32_t hs_ct_time;
RGB rgb_test_open;
bool im_test_rate_flag;
bool process_record_kb(uint16_t keycode, keyrecord_t *record) {

    if (process_record_user(keycode, record) != true) {
        return false;
    }

#ifdef WIRELESS_ENABLE
    if (process_record_wls(keycode, record) != true) {
        return false;
    }
#endif

    if ((confinfo.devs != DEVS_USB && *md_getp_state() == MD_STATE_DISCONNECTED)){
        if(IS_BASIC_KEYCODE(keycode) || IS_MODIFIER_KEYCODE(keycode)){
            return false;
        }
    }

    if (hs_frequency_test && hs_color_test){
        if (!(keycode >= KC_1 && keycode <= KC_4))
            return false;
    } 

    switch (keycode) {
        case EX_RATE: {
            if (record->event.pressed) {
                im_test_rate_flag = true;
            } else {
                im_test_rate_flag = false;
            }
            return false;
        } break;
        case KC_CUT: { //
            if(record->event.pressed){
                register_code16(KC_LCTL);
                register_code16(KC_X);
            }
            else{
                unregister_code16(KC_LCTL);
                unregister_code16(KC_X);
            }
            return false;
        } break;
        case KC_COPY: { //
            if(record->event.pressed){
                register_code16(KC_LCTL);
                register_code16(KC_C);
            }
            else{
                unregister_code16(KC_LCTL);
                unregister_code16(KC_C);
            }
            return false;
        } break;
        case KC_PASTE: { //
            if(record->event.pressed){
                register_code16(KC_LCTL);
                register_code16(KC_V);
            }
            else{
                unregister_code16(KC_LCTL);
                unregister_code16(KC_V);
            }
            return false;
        } break;
        case KC_FIND: { //
            if(record->event.pressed){
                register_code16(KC_LCTL);
                register_code16(KC_F);
            }
            else{
                unregister_code16(KC_LCTL);
                unregister_code16(KC_F);
            }
            return false;
        } break;
        case KC_1: { //
            if(hs_frequency_test && hs_color_test){
                if (record->event.pressed) {
                    md_send_devctrl(0x60);
                    md_rf_send_carrier(0, 5, 0x01);
                
                } 
                return false;
            }
        } break;

        case KC_2: { // 
            if(hs_frequency_test && hs_color_test){
                if (record->event.pressed) {
                    md_send_devctrl(0x60);
                    md_rf_send_carrier(19, 5, 0x01);
                   
                } 
                return false;
            }
        } break;

        case KC_3: { // 
            if(hs_frequency_test && hs_color_test){
                if (record->event.pressed) {
                    md_send_devctrl(0x60);
                    md_rf_send_carrier(38, 5, 0x01);
                   
                } 
                return false;
            }
        } break;

        case KC_4: { // 
            if(hs_frequency_test && hs_color_test){
                if (record->event.pressed) {
                    md_rf_send_stop();
                } 
                return false;
            } 
        } break;
        
        case KC_LED: { //
            if (record->event.pressed) {
               led_status = !led_status;
            
            } 
            return false;
            
        } break;
        case KC_MODE: {
            if (record->event.pressed) {
                confinfo.rgb_tog_filp = !confinfo.rgb_tog_filp;
                eeconfig_confinfo_update();
            }
            return false;
        } break;
        case KC_ENCW:{ //侧灯模式
            if (record->event.pressed) {
                confinfo.moude++;
                if (confinfo.moude > 4) confinfo.moude = 0;
                eeconfig_confinfo_update();
            }
            return false;
        }break;
        case KC_ENCCW:{ //侧灯模式
            if (confinfo.moude == 0 || confinfo.moude == 4) return false;
            if (record->event.pressed) {
                confinfo.rgb_color++;
                if (confinfo.rgb_color > 6) confinfo.rgb_color = 0;
                eeconfig_confinfo_update();
            }
            return false;
        }break;
        case QK_BOOT: {
            if (record->event.pressed) {
                dprintf("into boot!!!\r\n");
                eeconfig_disable();
                bootloader_jump();
            }
        } break;

        case BT_TEST: {
            if (record->event.pressed) {
                md_send_devctrl(0x62);
            }
            return false;
        } break;
        case NK_TOGG: {
            if (rgbrec_is_started()) {

                return false;
            }
            if (record->event.pressed) {
                rgb_matrix_hs_indicator_set(0xFF, (RGB){0x00, 0x6E, 0x00}, 250, 1);
            }
        } break;
        case RL_MOD: {
            if (rgbrec_is_started()) {

                return false;
            }
            if (record->event.pressed) {
                im_rgblight_increase();
            }

            return false;
        } break;
        case EE_CLR: {
            if (record->event.pressed) {
                ee_clr_timer = timer_read32();
            } else {
                ee_clr_timer = 0;
            }

            return false;
        } break;
        case RGB_SPI: {
            // if (rgb_matrix_get_val() == 0 || !hs_get_rgb_enable()) return false;
            if (record->event.pressed) {
                if (rgb_matrix_get_speed() != 255) {
                    rgb_blink_dir();
                }
            }
        } break;
        case RGB_SPD: {
            dprintf("rgb_matrix_get_speed = %d\r\n",rgb_matrix_get_speed());
            // if (rgb_matrix_get_val() == 0 || !hs_get_rgb_enable()) return false;
            if (record->event.pressed) {
                if (rgb_matrix_get_speed() <= 102) {
                    if (rgb_matrix_get_speed() != 51)
                        rgb_blink_dir();
                    rgb_matrix_set_speed(51);

                    return false;
                }
                rgb_blink_dir();
            }
            else{
                if (rgb_matrix_get_speed() == 51)
                    return false;
            }
        } break;
        case RGB_VAI: {
            // if (!hs_get_rgb_enable()) return false;
            if (record->event.pressed) {           
                if(rgb_matrix_is_enabled()) gpio_write_pin_high(LED_POWER_EN_PIN);
                if (rgb_matrix_get_val() != RGB_MATRIX_MAXIMUM_BRIGHTNESS) rgb_blink_dir();
            }
        } break;
        case RGB_VAD: {
            // if (!hs_get_rgb_enable()) return false;
            if (record->event.pressed) {
                if (rgb_matrix_get_val() <= RGB_MATRIX_VAL_STEP) {
                    // gpio_write_pin_low(LED_POWER_EN_PIN); 
                    if(rgb_matrix_get_val() != 0){
                        for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
                            rgb_matrix_set_color(i, 0, 0, 0);
                        }
                    }
                }
                if (rgb_matrix_get_val() != 0) rgb_blink_dir();
            }
        } break;
        case RGB_SAI: {
            if (!hs_get_rgb_enable()) return false;
            if (record->event.pressed) {
                uint8_t index;
                index = record_color_hsv(true);
                if ((index != 0xFF)) {
                    rgb_blink_dir();
                }
                return false;
            }
        } break;
        case RGB_SAD: {
            if (!hs_get_rgb_enable()) return false;
            if (record->event.pressed) {
                uint8_t index;
                index = record_color_hsv(false);
                if (index != 0xFF) {
                    rgb_blink_dir();
                }
                return false;
            }
        } break;
        case TO(_BL): {
            if (record->event.pressed) {
                rgb_matrix_hs_set_remain_time(HS_RGB_BLINK_INDEX_MAC, 0);
                rgb_matrix_hs_indicator_set(HS_RGB_BLINK_INDEX_WIN, (RGB){RGB_WHITE}, 250, 3);
                if (keymap_is_mac_system()) {
                    set_single_persistent_default_layer(_BL);
                    layer_move(0);
                }
            }

            return false;
        } break;
        case TO(_MBL): {
            if (record->event.pressed) {
                rgb_matrix_hs_set_remain_time(HS_RGB_BLINK_INDEX_WIN, 0);
                rgb_matrix_hs_indicator_set(HS_RGB_BLINK_INDEX_MAC, (RGB){RGB_WHITE}, 250, 3);
                if (!keymap_is_mac_system()) {
                    set_single_persistent_default_layer(_MBL);
                    layer_move(0);
                }
            }

            return false;
        } break;
        // case RP_P0: {
        //     if (record->event.pressed) {
        //         confinfo.record_channel = 0;
        //         rgbrec_read_current_channel(confinfo.record_channel);
        //         rgbrec_end(confinfo.record_channel);
        //         eeconfig_confinfo_update();
        //         rgbrec_show(confinfo.record_channel);
        //         dprintf("confinfo.record_last_mode = %d\r\n", confinfo.record_last_mode);
        //     }

        //     return false;
        // } break;
        // case RP_P1: {
        //     if (record->event.pressed) {
        //         confinfo.record_channel = 1;
        //         rgbrec_read_current_channel(confinfo.record_channel);
        //         rgbrec_end(confinfo.record_channel);
        //         eeconfig_confinfo_update();
        //         rgbrec_show(confinfo.record_channel);
        //     }

        //     return false;
        // } break;
        // case RP_P2: {
        //     if (record->event.pressed) {
        //         confinfo.record_channel = 2;
        //         rgbrec_read_current_channel(confinfo.record_channel);
        //         rgbrec_end(confinfo.record_channel);
        //         eeconfig_confinfo_update();
        //         rgbrec_show(confinfo.record_channel);
        //     }

        //     return false;
        // } break;
        // case RP_END: {
        //     if (record->event.pressed) {
        //         if (rgb_matrix_get_mode() != RGB_MATRIX_CUSTOM_RGBR_PLAY) {

        //             return false;
        //         }
        //         if (!rgbrec_is_started()) {
        //             rgbrec_start(confinfo.record_channel);
        //             no_record_fg = false;
        //             rec_time     = timer_read32();
        //             rgbrec_set_close_all(HSV_BLACK);
        //         } else {
        //             rec_time = 0;
        //             rgbrec_end(confinfo.record_channel);
        //         }
        //         dprintf("confinfo.record_last_mode = %d\r\n", confinfo.record_last_mode);
        //     }

        //     return false;
        // } break;
        case RGB_MOD: {
            if (record->event.pressed) {
                rgb_blink_dir();
                // if (rgb_matrix_get_mode() == RGB_MATRIX_CUSTOM_RGBR_PLAY) {
                //     if (rgbrec_is_started()) {
                //         rgbrec_read_current_channel(confinfo.record_channel);
                //         rgbrec_end(confinfo.record_channel);
                //         no_record_fg = false;
                //     }
                //     if (confinfo.record_last_mode != 0xFF)
                //         rgb_matrix_mode(confinfo.record_last_mode);
                //     else
                //         rgb_matrix_mode(RGB_MATRIX_DEFAULT_MODE);
                //     eeconfig_confinfo_update();
                //     dprintf("confinfo.record_last_mode = %d\r\n", confinfo.record_last_mode);
                //     start_hsv = rgb_matrix_get_hsv();
                //     return false;
                // }
                record_rgbmatrix_increase(&(confinfo.record_last_mode));
                eeconfig_confinfo_update();
                start_hsv = rgb_matrix_get_hsv();
                hs_led_flag = false;
            }

            return false;
        } break;
        case RGB_HUI: {
            if (record->event.pressed) {
                record_color_hsv(true);
            }

            return false;
        } break;
        case KC_LCMD: {
            if (keymap_is_mac_system()) {
                if (keymap_config.no_gui && !rgbrec_is_started()) {
                    if (record->event.pressed) {
                        register_code16(KC_LCMD);
                    } else {
                        unregister_code16(KC_LCMD);
                    }
                }
            }

            return true;
        } break;
        case KC_RCMD: {
            if (keymap_is_mac_system()) {
                if (keymap_config.no_gui && !rgbrec_is_started()) {
                    if (record->event.pressed) {
                        register_code16(KC_RCMD);
                    } else {
                        unregister_code16(KC_RCMD);
                    }
                }
            }

            return true;
        } break;
        case HS_BATQ: {
            extern bool rk_bat_req_flag;
            rk_bat_req_flag = (confinfo.devs != DEVS_USB) && record->event.pressed;
            if (!hs_get_rgb_enable() &&  rk_bat_req_flag ){
                writePin(LED_POWER_EN_PIN, 1);
                rgb_matrix_enable();
            }
            else if(!hs_get_rgb_enable()){
                // writePin(LED_POWER_EN_PIN, 0);
                // rgb_matrix_disable();
            }
            return false;
        } break;
        case HS_DIR: {
            if (record->event.pressed) {
                confinfo.dir_flag = !confinfo.dir_flag;
                rgb_test_open     = hsv_to_rgb((HSV){.h = 0, .s = 0, .v = RGB_MATRIX_VAL_STEP * 5});
                rgb_matrix_hs_indicator_set(0xFF, (RGB){0, 0, 0}, 250, 1);
                eeconfig_confinfo_update();
            }
            return false;
        } break;

        case KC_A: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_LEFT);
                } else {
                    unregister_code16(KC_LEFT);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_S: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_DOWN);
                } else {
                    unregister_code16(KC_DOWN);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_D: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_RGHT);
                } else {
                    unregister_code16(KC_RGHT);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_W: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_UP);
                } else {
                    unregister_code16(KC_UP);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_LEFT: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_A);
                } else {
                    unregister_code16(KC_A);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_DOWN: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_S);
                } else {
                    unregister_code16(KC_S);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_RGHT: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_D);
                } else {
                    unregister_code16(KC_D);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_UP: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_W);
                } else {
                    unregister_code16(KC_W);
                }
                return false;
            } else {
                return true;
            }
        } break;
        case HS_CT_A: {
            if (record->event.pressed) {
                hs_ct_time = timer_read32();
            } else {
                hs_ct_time = 0;
            }
            return false;
        } break;
        case KC_RCTL: {
            if (confinfo.ctrl_app_flag) {
                if (record->event.pressed) {
                    register_code16(KC_APP);
                } else {
                    unregister_code16(KC_APP);
                }
                return false;
            } else {
                return true;
            }
        } break;
        case HS_SIRI: {
            if (record->event.pressed) {
                register_code(KC_LCMD);
                register_code(KC_SPC);
                wait_ms(20);
            } else {
                unregister_code(KC_SPC);
                unregister_code(KC_LCMD);
            }
            return false;
        } break;
        case KC_MCTL: {
            if (record->event.pressed) {
                register_code(KC_LCTL);
                register_code(KC_UP);

            } else {
                unregister_code(KC_LCTL);
                unregister_code(KC_UP);
            }
            return false;
        } break;
        case RGB_TOG: {
#    ifdef RGBLIGHT_ENABLE
            if (record->event.pressed) {
                hs_set_rgb_enable(!hs_get_rgb_enable());
                if (hs_get_rgb_enable()){
                    rgb_matrix_enable();
                    if (mode == 10) {
                        HSV rgb = rgblight_get_hsv();
                        rgblight_sethsv(0, 255, rgb.v);
                        rgblight_disable();
                    }
#        ifdef LED_POWER_EN_PIN
                    writePin(LED_POWER_EN_PIN, 1);
#        endif
                }
                else{
                    if ((rgblight_is_enabled() != true) && (hs_get_rgb_enable() != true)) {
                        // rgb_matrix_disable();

#        ifdef LED_POWER_EN_PIN
                        // writePin(LED_POWER_EN_PIN, 0);
#        endif
                    }
                }
            } 
            return false;
#else

            if (record->event.pressed) {
                hs_set_rgb_enable(!hs_get_rgb_enable());
                if (hs_get_rgb_enable()){
#        ifdef LED_POWER_EN_PIN
                    writePin(LED_POWER_EN_PIN, 1);
#        endif

                }
                else{
#        ifdef LED_POWER_EN_PIN
                    // writePin(LED_POWER_EN_PIN, 0);
#        endif

                }
            }
#endif
        return false;
        } break;

        default:
            break;
    }

    return true;
}
bool system_usb = true;

void wireless_pre_task(void) {
    if (wireless_get_current_devs() == DEVS_USB) { 
        if (USB_DRIVER.state != USB_ACTIVE || !charging_state) {
            usb_status = true;
            if (usb_time && timer_elapsed32(usb_time) > 500){
                usb_time = 0;
                rgb_matrix_enable(); 
            }
        } else {
            if (usb_status) {
                usb_time = timer_read32();
                rgb_matrix_disable();
            }

            if (usb_time && timer_elapsed32(usb_time) > 500)  {
                rgb_matrix_enable(); 
            }
            usb_status = false;
        }
    } else {
        usb_status = false;
        usb_time = 0;
        rgb_matrix_enable(); 
    }
}

void housekeeping_task_user(void) { // loop
    uint8_t hs_now_mode;
    static uint32_t hs_current_time;
    // static bool val_value = false;
    
    charging_state = readPin(HS_BAT_CABLE_PIN);

    bat_full_flag = readPin(BAT_FULL_PIN);

    if (charging_state && (bat_full_flag)) {
        hs_now_mode = MD_SND_CMD_DEVCTRL_CHARGING_DONE;
    } else if (charging_state) {
        hs_now_mode = MD_SND_CMD_DEVCTRL_CHARGING;
    } else {
        hs_now_mode = MD_SND_CMD_DEVCTRL_CHARGING_STOP;
    }

    if (!hs_current_time || timer_elapsed32(hs_current_time) > 1000) {

        hs_current_time = timer_read32();
        md_send_devctrl(hs_now_mode);
        md_send_devctrl(MD_SND_CMD_DEVCTRL_INQVOL);
    }

    if (timer_elapsed32(hs_ct_time) > 3000 && hs_ct_time) {
        confinfo.ctrl_app_flag = !confinfo.ctrl_app_flag;
        // rgb_test_open          = hsv_to_rgb((HSV){.h = 0, .s = 0, .v = RGB_MATRIX_VAL_STEP * 5});
        rgb_matrix_hs_indicator_set(0xFF, (RGB){0, 0, 0}, 250, 1);
        eeconfig_confinfo_update();
        hs_ct_time = 0;
    }

    if ((readPin(SYSTEM_MAC_PIN) == 0)) { // mac system
        system_usb = true;
        if (!keymap_is_mac_system()) {
            set_single_persistent_default_layer(_MBL);
            layer_move(0);
        }
    } else if (readPin(SYSTEM_WIN_PIN) == 0){ // win system
        system_usb = true;
        if (keymap_is_mac_system()) {
            set_single_persistent_default_layer(_BL);
            layer_move(0);
        }
    }
    else{
        if (system_usb && charging_state){
            if (keymap_is_mac_system()) {
                set_single_persistent_default_layer(_BL);
                layer_move(0);
            }
            wait_ms(100);
            if((readPin(SYSTEM_WIN_PIN) != 0) && (readPin(SYSTEM_MAC_PIN) != 0))
                wireless_devs_change(wireless_get_current_devs(), DEVS_USB, false);
        }
        system_usb = false;
    }
    
    /* 设置键盘usb枚举和rgb灯抢占式控制处理 */
    if (lpwr_get_state() == LPWR_NORMAL) {

        if(charging_state || !hs_get_rgb_enable()){
            gpio_write_pin_low(B9);
        }
        else{
            gpio_write_pin_high(B9);
        }
    }
    
}

#ifdef RGB_MATRIX_ENABLE

#    ifdef WIRELESS_ENABLE
bool wls_rgb_indicator_reset        = false;
uint32_t wls_rgb_indicator_timer    = 0x00;
uint32_t wls_rgb_indicator_interval = 0;
uint32_t wls_rgb_indicator_times    = 0;
uint32_t wls_rgb_indicator_index    = 0;
RGB wls_rgb_indicator_rgb           = {0};

void rgb_matrix_wls_indicator_set(uint8_t index, RGB rgb, uint32_t interval, uint8_t times) {

    wls_rgb_indicator_timer = timer_read32();

    wls_rgb_indicator_index    = index;
    wls_rgb_indicator_interval = interval;
    wls_rgb_indicator_times    = times * 2;
    wls_rgb_indicator_rgb      = rgb;
}

void wireless_devs_change_kb(uint8_t old_devs, uint8_t new_devs, bool reset) {

    wls_rgb_indicator_reset = reset;
    if (confinfo.devs != wireless_get_current_devs()) {
        confinfo.devs = wireless_get_current_devs();
        if (confinfo.devs > 0 && confinfo.devs < 4) confinfo.last_btdevs = confinfo.devs;
        eeconfig_confinfo_update();
    }

    switch (new_devs) {
        case DEVS_USB: {
            if (reset) {
                // rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_USB, (RGB){HS_LBACK_COLOR_USB}, 200, 1);
            } else {
                // rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_USB, (RGB){HS_PAIR_COLOR_USB}, 500, 1);
            }
        } break;
        case DEVS_BT1: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT1, (RGB){HS_LBACK_COLOR_BT1}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT1, (RGB){HS_PAIR_COLOR_BT1}, 500, 1);
            }
        } break;
        case DEVS_BT2: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT2, (RGB){HS_LBACK_COLOR_BT2}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT2, (RGB){HS_PAIR_COLOR_BT2}, 500, 1);
            }
        } break;
        case DEVS_BT3: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT3, (RGB){HS_LBACK_COLOR_BT3}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT3, (RGB){HS_PAIR_COLOR_BT3}, 500, 1);
            }
        } break;
        case DEVS_BT4: {
            if (reset) {
                rgb_matrix_wls_indicator_set(41, (RGB){RGB_BLUE}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(41, (RGB){RGB_BLUE}, 500, 1);
            }
        } break;
        case DEVS_BT5: {
            if (reset) {
                rgb_matrix_wls_indicator_set(42, (RGB){RGB_BLUE}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(42, (RGB){RGB_BLUE}, 500, 1);
            }
        } break;
        case DEVS_2G4: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_2G4, (RGB){HS_LBACK_COLOR_2G4}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_2G4, (RGB){HS_LBACK_COLOR_2G4}, 500, 1);
            }
        } break;
        default:
            break;
    }
}

bool rgb_matrix_wls_indicator_cb(void) {

    if (*md_getp_state() != MD_STATE_CONNECTED) {
        if (!(wireless_get_current_devs() == DEVS_USB && USB_DRIVER.state == USB_ACTIVE)) wireless_devs_change_kb(wireless_get_current_devs(), wireless_get_current_devs(), wls_rgb_indicator_reset);
        return true;
    }

    // refresh led
    led_wakeup();

    return false;
}

void rgb_matrix_wls_indicator(void) {

    if (wls_rgb_indicator_timer) {

        if (timer_elapsed32(wls_rgb_indicator_timer) >= wls_rgb_indicator_interval) {
            wls_rgb_indicator_timer = timer_read32();

            if (wls_rgb_indicator_times) {
                wls_rgb_indicator_times--;
            }

            if (wls_rgb_indicator_times <= 0) {
                wls_rgb_indicator_timer = 0x00;
                if (rgb_matrix_wls_indicator_cb() != true) {
                    return;
                }
            }
        }

        if (wls_rgb_indicator_times % 2) {
            rgb_matrix_set_color(wls_rgb_indicator_index, wls_rgb_indicator_rgb.g, wls_rgb_indicator_rgb.r, wls_rgb_indicator_rgb.b);
        } else {
            rgb_matrix_set_color(wls_rgb_indicator_index, 0x00, 0x00, 0x00);
        }
    }
}

void rgb_matrix_hs_bat_set(uint8_t index, RGB rgb, uint32_t interval, uint8_t times) {
    for (int i = 0; i < HS_RGB_BAT_COUNT; i++) {
        if (!hs_rgb_bat[i].active) {
            hs_rgb_bat[i].active   = true;
            hs_rgb_bat[i].timer    = timer_read32();
            hs_rgb_bat[i].interval = interval;
            hs_rgb_bat[i].times    = times * 2;
            hs_rgb_bat[i].index    = index;
            hs_rgb_bat[i].rgb      = rgb;
            break;
        }
    }
}

void rgb_matrix_hs_bat(void) {
    for (int i = 0; i < HS_RGB_BAT_COUNT; i++) {
        if (hs_rgb_bat[i].active) {
            if (timer_elapsed32(hs_rgb_bat[i].timer) >= hs_rgb_bat[i].interval) {
                hs_rgb_bat[i].timer = timer_read32();

                if (hs_rgb_bat[i].times) {
                    hs_rgb_bat[i].times--;
                }

                if (hs_rgb_bat[i].times <= 0) {
                    hs_rgb_bat[i].active = false;
                    hs_rgb_bat[i].timer  = 0x00;
                }
            }

            if (hs_rgb_bat[i].times % 2) {
                if (hs_get_rgb_enable()) rgb_matrix_set_color(hs_rgb_bat[i].index, hs_rgb_bat[i].rgb.g, hs_rgb_bat[i].rgb.r, hs_rgb_bat[i].rgb.b);
            } else {
                rgb_matrix_set_color(hs_rgb_bat[i].index, 0x00, 0x00, 0x00);
            }
        }
    }
}

void bat_indicators(void) {
    static uint32_t battery_process_time = 0;

    if (charging_state && (bat_full_flag)) {
        battery_process_time = 0;
        // if (!led_chaning) rgb_matrix_set_color(HS_MATRIX_BLINK_INDEX_BAT, 0x00, 0x00, 0x00);
    } else if (charging_state) {

        battery_process_time = 0;
        if (!led_status && rgb_matrix_get_val()) {
            if (hs_get_rgb_enable()) rgb_matrix_set_color(80, 0xFF, 0x00, 0x00);
        }
    } else if (*md_getp_bat() <= BATTERY_CAPACITY_LOW) {

        if (!led_status && rgb_matrix_get_val()) {
            rgb_matrix_hs_bat_set(80, (RGB){0xFF, 0x00, 0x00}, 250, 1);
        }
        if (*md_getp_bat() <= BATTERY_CAPACITY_STOP) {
            if (!battery_process_time) {
                battery_process_time = timer_read32();
            }

            if (battery_process_time && timer_elapsed32(battery_process_time) > 60000) {
                battery_process_time = 0;
                lower_sleep          = true;
                md_send_devctrl(MD_SND_CMD_DEVCTRL_USB);
                lpwr_set_timeout_manual(true);
            }
        }
    } else {
        // rgb_matrix_set_color(HS_MATRIX_BLINK_INDEX_BAT, 0x00, 0x00, 0x00);
        battery_process_time = 0;
    
    }
}

#    endif

#endif

void rgb_blink_dir(void) {
    rgb_matrix_hs_indicator_set(0xFF, (RGB){0, 0, 0}, 250, 1);
}

bool hs_reset_settings_user(void) {

    rgb_matrix_hs_indicator_set(0xFF, (RGB){0x10, 0x10, 0x10}, 250, 3);

    return true;
}

void nkr_indicators_hook(uint8_t index) {

    if ((hs_rgb_indicators[index].rgb.r == 0x6E) && (hs_rgb_indicators[index].rgb.g == 0x00) && (hs_rgb_indicators[index].rgb.b == 0x00)) {

        rgb_matrix_hs_indicator_set(0xFF, (RGB){0x00, 0x00, 0x6E}, 250, 1);

    } else if ((hs_rgb_indicators[index].rgb.r == 0x00) && (hs_rgb_indicators[index].rgb.g == 0x00) && (hs_rgb_indicators[index].rgb.b == 0x6E)) {

        rgb_matrix_hs_indicator_set(0xFF, (RGB){0x6E, 0x00, 0x00}, 250, 1);
    }
}

void rgb_matrix_hs_indicator_set(uint8_t index, RGB rgb, uint32_t interval, uint8_t times) {

    for (int i = 0; i < HS_RGB_INDICATOR_COUNT; i++) {
        if (!hs_rgb_indicators[i].active) {
            hs_rgb_indicators[i].active   = true;
            hs_rgb_indicators[i].timer    = timer_read32();
            hs_rgb_indicators[i].interval = interval;
            hs_rgb_indicators[i].times    = times * 2;
            hs_rgb_indicators[i].index    = index;
            hs_rgb_indicators[i].rgb      = rgb;
            if (index != 0xFF)
                hs_rgb_indicators[i].blink_cb = NULL;
            else {
                hs_rgb_indicators[i].blink_cb = nkr_indicators_hook;
            }
            break;
        }
    }
}

void rgb_matrix_hs_set_remain_time(uint8_t index, uint8_t remain_time) {

    for (int i = 0; i < HS_RGB_INDICATOR_COUNT; i++) {
        if (hs_rgb_indicators[i].index == index) {
            hs_rgb_indicators[i].times  = 0;
            hs_rgb_indicators[i].active = false;
            break;
        }
    }
}

void rgb_matrix_hs_indicator(void) {

    for (int i = 0; i < HS_RGB_INDICATOR_COUNT; i++) {
        if (hs_rgb_indicators[i].active) {
            if (timer_elapsed32(hs_rgb_indicators[i].timer) >= hs_rgb_indicators[i].interval) {
                hs_rgb_indicators[i].timer = timer_read32();

                if (hs_rgb_indicators[i].times) {
                    hs_rgb_indicators[i].times--;
                }

                if (hs_rgb_indicators[i].times <= 0) {
                    hs_rgb_indicators[i].active = false;
                    hs_rgb_indicators[i].timer  = 0x00;
                    if (hs_rgb_indicators[i].blink_cb != NULL)
                        hs_rgb_indicators[i].blink_cb(i);
                    continue;
                }
            }

            if ((hs_rgb_indicators[i].times % 2)) {
                if (hs_rgb_indicators[i].index == 0xFF) {
                    rgb_matrix_set_color_all(hs_rgb_indicators[i].rgb.r, hs_rgb_indicators[i].rgb.g, hs_rgb_indicators[i].rgb.b);
                } else {
                    rgb_matrix_set_color(hs_rgb_indicators[i].index, hs_rgb_indicators[i].rgb.r, hs_rgb_indicators[i].rgb.g, hs_rgb_indicators[i].rgb.b);
                }
            } else {
                if (hs_rgb_indicators[i].index == 0xFF) {
                    rgb_matrix_set_color_all(0x00, 0x00, 0x00);
                } else {
                    rgb_matrix_set_color(hs_rgb_indicators[i].index, 0x00, 0x00, 0x00);
                }
            }
        }
    }
}

void hs_blink_rgb_matrix(uint8_t r, uint8_t g, uint8_t b) {
    int i;
    for(i = 0;i < RGB_MATRIX_LED_COUNT;i++){
        rgb_matrix_set_color(i, r,g,b);
    }
}
uint32_t hs_usb_rec_time;
uint32_t hs_usb_succeed_time;
bool rec_filp = false;
bool hs_wlr_succeed;
void rgb_matrix_start_rec(void) {
   
    if (confinfo.devs == DEVS_USB && USB_DRIVER.state != USB_ACTIVE) {
        hs_usb_succeed_time = timer_read32();
        if (!hs_usb_rec_time || timer_elapsed32(hs_usb_rec_time) > 500) {
            hs_usb_rec_time = timer_read32();
            rec_filp = !rec_filp;
        }
        if (rec_filp) {
            rgb_matrix_set_color(HS_RGB_BLINK_INDEX_USB, RGB_WHITE);
         
        } else {
            rgb_matrix_set_color(HS_RGB_BLINK_INDEX_USB, RGB_BLACK);
           
        }
    } else if (confinfo.devs == DEVS_USB || hs_wlr_succeed){
        hs_usb_rec_time = 0;
        rec_filp = false;
        if (timer_elapsed32(hs_usb_succeed_time) < 2000){
            switch (confinfo.devs)
            {
            case DEVS_BT1:
                rgb_matrix_set_color(HS_RGB_BLINK_INDEX_BT1, HS_LBACK_COLOR_BT1);
            break;
            case DEVS_BT2:
                rgb_matrix_set_color(HS_RGB_BLINK_INDEX_BT2, HS_LBACK_COLOR_BT2);
            break;
            case DEVS_BT3:
                rgb_matrix_set_color(HS_RGB_BLINK_INDEX_BT3, HS_LBACK_COLOR_BT3);
            break;
            case DEVS_2G4:
                rgb_matrix_set_color(HS_RGB_BLINK_INDEX_2G4, HS_LBACK_COLOR_2G4);
            break;
            
            default:
                rgb_matrix_set_color(HS_RGB_BLINK_INDEX_USB, RGB_WHITE);
                break;
            }
           
        }
        else{
            hs_wlr_succeed = false;
        }
    }
}

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    RGB rgb_white = hsv_to_rgb((HSV){.h = 0, .s = 0, .v = RGB_MATRIX_VAL_STEP*5+10}); 
    if (hs_led_flag) rgb_matrix_set_color_all(0,0,0);
    if (test_white_light_flag) {
        RGB rgb_test_open = hsv_to_rgb((HSV){.h = 0, .s = 0, .v = RGB_MATRIX_VAL_STEP * 5});
        rgb_matrix_set_color_all(rgb_test_open.r, rgb_test_open.g, rgb_test_open.b);

        return false;
    }
#ifdef RGBLIGHT_ENABLE
    if (rgb_matrix_indicators_advanced_user(led_min, led_max) != true) {

        return false;
    }
#endif

    if (!hs_get_rgb_enable() || confinfo.rgb_tog_filp){
        
        rgb_matrix_set_color_all(0,0,0);

    }

    if (host_keyboard_led_state().caps_lock){
        writePin(LED_POWER_EN_PIN, 1);
        rgb_matrix_set_color(58, rgb_white.r,rgb_white.g,rgb_white.b);
    }
        
    if (!keymap_is_mac_system() && keymap_config.no_gui){
        writePin(LED_POWER_EN_PIN, 1);
        rgb_matrix_set_color(79, rgb_white.r,rgb_white.g,rgb_white.b);
    }
    
    // if (host_keyboard_led_state().num_lock){
    //     writePin(LED_POWER_EN_PIN, 1);
    //     rgb_matrix_set_color(25, rgb_white.r,rgb_white.g,rgb_white.b);
    // }
    extern bool rk_bat_req_flag;
    if ((confinfo.devs != DEVS_USB && *md_getp_state() != MD_STATE_CONNECTED)){
        writePin(LED_POWER_EN_PIN, 1);
    }
    else if (!hs_get_rgb_enable() && !rk_bat_req_flag && !host_keyboard_led_state().caps_lock && !(!keymap_is_mac_system() && keymap_config.no_gui)){
        writePin(LED_POWER_EN_PIN, 0);
    }

    rgb_matrix_start_rec();
    if (ee_clr_timer && timer_elapsed32(ee_clr_timer) > 3000) {
        hs_reset_settings();
        ee_clr_timer = 0;
    }

#ifdef RGBLIGHT_ENABLE
    if (rgb_matrix_indicators_advanced_rgblight(led_min, led_max) != true) {

        return false;
    }
#endif

#ifdef WIRELESS_ENABLE
    rgb_matrix_wls_indicator();

    if (enable_bat_indicators && !inqbat_flag && !rgbrec_is_started()) {
        rgb_matrix_hs_bat();
        bat_indicators();
        bat_indicator_cnt = timer_read32();
    }

    if (!enable_bat_indicators) {
        if (timer_elapsed32(bat_indicator_cnt) > 250) {
            enable_bat_indicators = true;
            bat_indicator_cnt     = timer_read32();
        }
    }

#endif

    rgb_matrix_hs_indicator();

    query();

    if(hs_color_test == true && hs_frequency_test == false) {
        writePin(LED_POWER_EN_PIN, 1);
        if ((timer_elapsed32(hs_color_time) > 4000)) {
            hs_color_time = timer_read32();
            hs_color_flag ++;
        }
        // µ÷ÓÃº¯Êý£¬´«µÝ²»Í¬µÄÑÕÉ«Öµ
        switch (hs_color_flag)
        {
            case 0:
                hs_blink_rgb_matrix(WIITE_B, 0x00, 0x00); // °×É«
                break;
            case 1:
                hs_blink_rgb_matrix(0x00, WIITE_B, 0x00); // ÂÌÉ«
                break;
            case 2:
                hs_blink_rgb_matrix(0x00, 0x00, WIITE_B); // ÂÌÉ«
                break;
            case 3:
                hs_blink_rgb_matrix(WIITE_B, WIITE_B, WIITE_B); // ×ÏÉ«
                break;
            
            default:
                hs_color_test = false;
                break;
        }
    }
    return true;
}

void hs_reset_settings(void) {
    enable_bat_indicators = false;
    eeconfig_init();
    eeconfig_update_rgb_matrix_default();
    extern uint32_t suspend_timer;
    suspend_timer = 0;
#ifdef RGBLIGHT_ENABLE
    extern void rgblight_init(void);
    is_rgblight_initialized = false;
    rgblight_init();
    eeconfig_update_rgblight_default();
    rgblight_enable();
#endif

    // #if defined(WIRELESS_ENABLE)
    //     wireless_devs_change(wireless_get_current_devs(), DEVS_USB, false);
    // #endif

    if (hs_reset_settings_user() != true) {

        return;
    }
    hs_rgb_blink_set_timer(timer_read32());
    keyboard_post_init_kb();
}

void lpwr_wakeup_hook(void) {
    hs_mode_scan(false, confinfo.devs, confinfo.last_btdevs);

    if (rgb_matrix_get_val() != 0 && hs_get_rgb_enable())
        gpio_write_pin_high(LED_POWER_EN_PIN);
    else
        gpio_write_pin_low(LED_POWER_EN_PIN);

    gpio_write_pin_high(HS_LED_BOOSTING_PIN);
}


bool rgb_matrix_indicators_advanced_rgblight(uint8_t led_min, uint8_t led_max) {
    extern LED_TYPE led[RGBLED_NUM];
    extern bool charging_state;
    extern bool bat_full_flag;

    if (!hs_get_rgb_enable() || confinfo.rgb_tog_filp){
        for (uint8_t i = 0; i < RGBLED_NUM; i++) {
            rgb_matrix_set_color(RGB_MATRIX_LED_COUNT - RGBLED_NUM + i, 0, 0, 0); // rgb light
        }
    }
    else{
        for (uint8_t i = 0; i < RGBLED_NUM; i++) {
            rgb_matrix_set_color(RGB_MATRIX_LED_COUNT - RGBLED_NUM + i, led[i].r, led[i].g, led[i].b); // rgb light
        }
    }
    
    // if ((!(charging_state && (!bat_full_flag))) || ((!charging_state) && (*md_getp_bat() <= BATTERY_CAPACITY_LOW)) || led_status){
    //     switch (confinfo.moude) {
    //         case 0:{extern void rgblight_effect_rainbow_swirl2(void); rgblight_effect_rainbow_swirl2();}break;
    //         case 1:{extern void rgblight_effect_breathing1(void);rgblight_effect_breathing1();}break;
    //         case 2:{extern void paoma(void);paoma();}break;
    //         case 3:{extern void changliang(void);changliang();}break;
    //         case 4:{rgb_matrix_set_color(0,0,0,0);rgb_matrix_set_color(1,0,0,0);rgb_matrix_set_color(2,0,0,0);}break;
    //         default:break;
    //     }
    // }
    return true;
}
