#===============================================================================
# @brief    Source CMake file for mine SLE UART slave demo
#===============================================================================

set(SOURCES "${SOURCES}"
    "${CMAKE_SOURCE_DIR}/application/mine/common/LD2402/LD2402.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_slave.c"
    "${CMAKE_SOURCE_DIR}/application/mine/common/ssd1306/hal_bsp_ssd1306.c"
)

list(REMOVE_DUPLICATES SOURCES)
set(SOURCES "${SOURCES}" PARENT_SCOPE)
