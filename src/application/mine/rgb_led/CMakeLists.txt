#===============================================================================
# @brief    CMake file for mine RGB LED demo
#===============================================================================

set(COMPONENT_NAME "mine_rgb_led")

set(SOURCES
    "src/mine_rgb_led_drv.c"
    "src/mine_rgb_led_demo.c"
)

set(PUBLIC_HEADER
)

set(PRIVATE_HEADER
    "inc"
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
