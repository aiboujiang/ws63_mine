#===============================================================================
# @brief    CMake file for custom mine demos
#===============================================================================

set(COMPONENT_NAME "mine")

set(SOURCES
)

set(PUBLIC_HEADER
)

if(DEFINED CONFIG_MINE_SUPPORT_SLE_UART_HOST_DEMO)
    add_subdirectory_if_exist(sle_uart_host)
endif()

if(DEFINED CONFIG_MINE_SUPPORT_SLE_UART_SLAVE_DEMO)
    add_subdirectory_if_exist(sle_uart_slave)
endif()

if(DEFINED CONFIG_MINE_SUPPORT_UART2_EXT_WK2114)
    add_subdirectory_if_exist(wk2114_uart2_ext)
endif()

set(PRIVATE_HEADER
)

set(PRIVATE_DEFINES
)

set(PUBLIC_DEFINES
)

set(COMPONENT_PUBLIC_CCFLAGS
)

set(COMPONENT_CCFLAGS
)

set(WHOLE_LINK
    true
)

set(MAIN_COMPONENT
    false
)

if(NOT "${COMPONENT_NAME}" IN_LIST RAM_COMPONENT)
    list(APPEND RAM_COMPONENT ${COMPONENT_NAME})
endif()

build_component()
