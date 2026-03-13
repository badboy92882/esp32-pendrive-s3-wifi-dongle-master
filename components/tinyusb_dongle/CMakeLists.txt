set(srcs
    "descriptors_control.c"
    "tinyusb.c"
    "usb_descriptors.c"
)

# Default TinyUSB task
if(NOT CONFIG_TINYUSB_NO_DEFAULT_TASK)
    list(APPEND srcs "tusb_tasks.c")
endif()

# CDC class
if(CONFIG_TINYUSB_CDC_ENABLED)
    list(APPEND srcs
        "cdc.c"
        "tusb_cdc_acm.c"
    )

    if(CONFIG_VFS_SUPPORT_IO)
        list(APPEND srcs
            "tusb_console.c"
            "vfs_tinyusb.c"
        )
    endif()
endif()

# Bluetooth class
if(CONFIG_TINYUSB_BTH_ENABLED)
    list(APPEND srcs
        "tusb_bth.c"
    )
endif()

# DFU class
if(CONFIG_TINYUSB_DFU_ENABLED)
    list(APPEND srcs
        "tusb_dfu.c"
        "tusb_dfu_ota.c"
    )
endif()

# Network classes (ECM / RNDIS / NCM)
if(CONFIG_TINYUSB_NET_MODE_NCM
    OR CONFIG_TINYUSB_NET_MODE_RNDIS
    OR CONFIG_TINYUSB_NET_MODE_ECM)
    list(APPEND srcs
        "tinyusb_net.c"
    )
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "include_private"
    PRIV_REQUIRES usb esp_netif
    REQUIRES fatfs vfs bt nvs_flash app_update
)