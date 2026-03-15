#===============================================================================
# @brief    Source CMake file for mine WK2114 UART2 extension module
#===============================================================================

set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/mine_wk2114_uart2_ext.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mine_wk2114_uart2_ext_oled.c"
    "${CMAKE_SOURCE_DIR}/application/mine/common/ssd1306/hal_bsp_ssd1306.c"
)

list(REMOVE_DUPLICATES SOURCES)
set(SOURCES "${SOURCES}" PARENT_SCOPE)
