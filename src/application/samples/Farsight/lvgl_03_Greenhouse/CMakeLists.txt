set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_greenhouse_demo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/hal_bsp_sht20.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/text.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/button_initial.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/fan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/mqtt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/wifi_connect.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/ui.c
)
set(HEADER_LIST 
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp
)

set(SOURCES "${SOURCES}" ${SOURCES_LIST} PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER}" ${HEADER_LIST} PARENT_SCOPE)