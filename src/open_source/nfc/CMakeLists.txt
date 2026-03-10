#===============================================================================
# @brief    cmake file
# Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
#===============================================================================
set(COMPONENT_NAME "nfc")

set(CMAKE_NFC_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR})

set(SOURCES
    ${CMAKE_NFC_SOURCE_DIR}/nfc/nfc.c
    ${CMAKE_NFC_SOURCE_DIR}/nfc/NT3H.c
    ${CMAKE_NFC_SOURCE_DIR}/nfc/ndef/ndef.c 
    ${CMAKE_NFC_SOURCE_DIR}/nfc/ndef/rtd/nfcForum.c
    ${CMAKE_NFC_SOURCE_DIR}/nfc/ndef/rtd/rtdText.c
    ${CMAKE_NFC_SOURCE_DIR}/nfc/ndef/rtd/rtdUri.c

 
)

set(PUBLIC_HEADER
    ${CMAKE_NFC_SOURCE_DIR}/nfc
    ${CMAKE_NFC_SOURCE_DIR}/nfc/ndef/rtd
    ${CMAKE_NFC_SOURCE_DIR}/nfc/ndef
)

set(PRIVATE_HEADER
    ${CMAKE_NFC_SOURCE_DIR}/nfc
)

# use this when you want to add ccflags like -include xxx
set(COMPONENT_PUBLIC_CCFLAGS
)

set(COMPONENT_CCFLAGS
    -Wno-error=logical-op
    -Wno-error=sign-compare
    -Wno-error=jump-misses-init
    -Wno-sign-compare
    -Wno-jump-misses-init
    -Wno-error=unused-parameter
    -Wno-unused-parameter
    -Wno-unused-but-set-variable
    -Wno-error=unused-variable
)

set(WHOLE_LINK
    true
)

set(MAIN_COMPONENT
    false
)



build_component()