#===============================================================================
# @brief    CMake file for mine SLE UART slave demo
#===============================================================================

add_subdirectory_if_exist(src)

set(SOURCES "${SOURCES}" PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER}"
	"${CMAKE_CURRENT_SOURCE_DIR}/inc"
	"${CMAKE_SOURCE_DIR}/application/mine/common"
	PARENT_SCOPE)
