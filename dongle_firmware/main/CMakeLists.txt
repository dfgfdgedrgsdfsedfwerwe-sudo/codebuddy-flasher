idf_component_register(
    SRCS
        "main.c"
        "audio_ringbuf.c"
        "espnow_receiver.c"
        "usb_descriptors.c"
        "led_indicator.c"
        "pairing.c"
        "pc_link.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        driver
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        freertos
        espressif__tinyusb
    PRIV_REQUIRES
        usb
)

# 强制链接器保留 TinyUSB 回调符号（防止 --gc-sections 剔除弱符号覆盖）
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-u tud_descriptor_device_cb"
    "-u tud_descriptor_configuration_cb"
    "-u tud_descriptor_string_cb"
    "-u tud_hid_descriptor_report_cb"
)
