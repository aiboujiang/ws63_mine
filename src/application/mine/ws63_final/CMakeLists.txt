#===============================================================================
# @brief    CMake file for mine wk2114 final layered framework
#===============================================================================

set(COMPONENT_NAME "mine_ws63_final")

set(SOURCES
    "Common/ws63_final_common.c"
    "BSP/ws63_final_bsp.c"
    "Driver/wk2114.c"
    "Driver/ld2402.c"
    "Driver/zw101.c"
    "Driver/ws63_rgb_ws2812.c"
    "Middleware/ws63_final_osal.c"
    "App/Task/ws63_final_task.c"
    "App/Main/ws63_final_main.c"
)

set(PUBLIC_HEADER
)

set(PRIVATE_HEADER
    "Config"
    "Common"
    "BSP"
    "Driver"
    "Middleware"
    "App/Task"
    "App/Main"
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
