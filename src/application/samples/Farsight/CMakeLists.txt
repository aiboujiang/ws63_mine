if(DEFINED CONFIG_SAMPLE_SUPPORT_KERNEL_01_TASK)
    add_subdirectory_if_exist(kernel_01_task)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_KERNEL_02_TIMER)
    add_subdirectory_if_exist(kernel_02_timer)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_KERNEL_03_EVENT)
    add_subdirectory_if_exist(kernel_03_event)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_KERNEL_04_MUTEX)
    add_subdirectory_if_exist(kernel_04_mutex)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_KERNEL_05_MUTEX_SEMAPHORE)
    add_subdirectory_if_exist(kernel_05_mutex_Semaphore)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_KERNEL_06_SYNC_SEMAPHORE)
    add_subdirectory_if_exist(kernel_06_sync_Semaphore)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_KERNEL_07_COUNT_SEMAPHORE)
    add_subdirectory_if_exist(kernel_07_count_Semaphore)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_KERNEL_08_MESSGE_QUEQUE)
    add_subdirectory_if_exist(kernel_08_message_queque)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_KERNEL_09_NV)
    add_subdirectory_if_exist(kernel_09_nv)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_KERNEL_10_MENU)
    add_subdirectory_if_exist(kernel_10_menu)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_LEDBLINK)
    add_subdirectory_if_exist(base_01_ledblink)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_UART)
    add_subdirectory_if_exist(base_02_uart)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_OLED_SSD1306)
    add_subdirectory_if_exist(base_03_ssd1306)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_RGB)
    add_subdirectory_if_exist(base_04_rgb)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SHT20)
    add_subdirectory_if_exist(base_05_sht20)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_AP3216)
    add_subdirectory_if_exist(base_06_ap3216)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_LCD)
    add_subdirectory_if_exist(base_07_spi_lcd)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_LCD_TOUCH)
    add_subdirectory_if_exist(base_08_spi_lcd_touch)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_NFC)
    add_subdirectory_if_exist(base_09_nfc)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_IR_RECV)
    add_subdirectory_if_exist(base_10_ir)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SC12B)
    add_subdirectory_if_exist(base_11_remote_sc12b)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_TELECONTROL_OLED)
    add_subdirectory_if_exist(base_12_remote_st7789)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SMART_TRASHCAN)
    add_subdirectory_if_exist(module_01_smart_trashcan)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_EC800M)
    add_subdirectory_if_exist(module_02_EC800M)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_VOICE_CAR)
    add_subdirectory_if_exist(module_03_voice_car)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_FINGERPRINT_LOCK)
    add_subdirectory_if_exist(module_04_fingerprint_lock)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SMART_AGRICULTURE)
    add_subdirectory_if_exist(module_05_smart_agriculture)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_REVERSINGRADA)
    add_subdirectory_if_exist(sample_01_reversingrada)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SMARTTEMP)
    add_subdirectory_if_exist(sample_02_smarttemp)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SMARTSECURITY)
    add_subdirectory_if_exist(sample_03_smartsecurity)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SMARTLAMP)
    add_subdirectory_if_exist(sample_04_smartlamp)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SMARTFARM)
    add_subdirectory_if_exist(sample_05_smartfarm)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SMARTHOME)
    add_subdirectory_if_exist(sample_06_smarthome)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_WIFI_01_STA)
    add_subdirectory_if_exist(wifi_01_sta)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_WIFI_02_AP)
    add_subdirectory_if_exist(wifi_02_ap)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_WIFI_03_UDP)
    add_subdirectory_if_exist(wifi_03_udp)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_WIFI_04_TCP)
    add_subdirectory_if_exist(wifi_04_tcp)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_WIFI_05_MQTT)
    add_subdirectory_if_exist(wifi_05_mqtt)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_WIFI_06_HUAWEI_IOT)
    add_subdirectory_if_exist(wifi_06_huawei_iot)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_WIFI_07_HTTP)
    add_subdirectory_if_exist(wifi_07_http)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_WIFI_08_SPEED_AP)
    add_subdirectory_if_exist(wifi_08_speed_ap)
endif()


if(DEFINED CONFIG_SAMPLE_SUPPORT_WIFI_08_SPEED_STA)
    add_subdirectory_if_exist(wifi_08_speed_sta)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_BLE_TRANS_CLIENT)
    add_subdirectory_if_exist(ble_02_trans_client)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_BLE_TRANS_SERVER)
    add_subdirectory_if_exist(ble_01_trans_server)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_BLE_WIFI_CFG_SERVER)
    add_subdirectory_if_exist(ble_03_wifi_cfg_server)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_TRANS_SERVER)
    add_subdirectory_if_exist(sle_01_trans_server)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_TRANS_CLIENT)
    add_subdirectory_if_exist(sle_02_trans_client)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_TRANS_CLIENT_MULTI)
    add_subdirectory_if_exist(sle_02_client_multi_conn)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_SERVER_LED)
    add_subdirectory_if_exist(sle_03_server_led)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_REMOTECAR_CLIENT)
    add_subdirectory_if_exist(sle_03_client_remote)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_REMOTECAR_SERVER)
    add_subdirectory_if_exist(sle_03_server_remote)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_DISTRIBUTE_NETWORK)
    add_subdirectory_if_exist(sle_04_distribute_network)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_WIFI_COEXIST_CLIENT)
    add_subdirectory_if_exist(sle_05_wifi_coexist_client)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_WIFI_COEXIST_SERVER)
    add_subdirectory_if_exist(sle_05_wifi_coexist_server)
endif()


if(DEFINED CONFIG_SAMPLE_SUPPORT_LVGL_01_KEY_DEMO)
    add_subdirectory_if_exist(lvgl_01_key)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_LVGL_02_INTELIGENT_LIGHT)
    add_subdirectory_if_exist(lvgl_02_Inteligentlight)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_LVGL_03_GREEN_HOUSE)
    add_subdirectory_if_exist(lvgl_03_Greenhouse)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_LVGL_04_FLAME_ALARM)
    add_subdirectory_if_exist(lvgl_04_Flame)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_TOTAL_TEST)
    add_subdirectory_if_exist(total_test)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE)
    add_subdirectory_if_exist(sle_base)
endif()



set(SOURCES "${SOURCES}" PARENT_SCOPE)