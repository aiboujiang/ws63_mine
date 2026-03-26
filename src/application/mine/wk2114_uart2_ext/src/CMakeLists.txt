#===============================================================================
# @brief    Source CMake file for mine WK2114 UART2 extension module
#===============================================================================

set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/mine_wk2114_uart2_ext.c"
)

list(REMOVE_DUPLICATES SOURCES)
set(SOURCES "${SOURCES}" PARENT_SCOPE)
