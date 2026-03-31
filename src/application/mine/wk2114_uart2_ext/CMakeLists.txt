#===============================================================================
# @brief    CMake file for wk2114 uart2 ext
#===============================================================================

set(COMPONENT_NAME "mine_wk2114_uart2_ext")

set(SOURCES
    "src/mine_wk2114_uart2_ext.c"
    "../common/LD2402/LD2402.c"
)

set(PUBLIC_HEADER
)

set(PRIVATE_HEADER
    "inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
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
