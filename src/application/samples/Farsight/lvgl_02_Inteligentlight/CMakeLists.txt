set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_light_demo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/hal_bsp_ap3216.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/hal_bsp_aw2013.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/icon_illumination.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/light_background.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/lightoff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/lighton.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/title.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/mqtt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/wifi_connect.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/ui.c
)
set(HEADER_LIST 
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp
)

set(SOURCES "${SOURCES}" ${SOURCES_LIST} PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER}" ${HEADER_LIST} PARENT_SCOPE)