set(LOTUS_SYSTEM_PLATFORM_INCLUDE_NAME "LOTUS_SYSTEM_PLATFORM_INCLUDE")

add_library(lotus_system STATIC)
configure_lotus_module(lotus_system)

target_sources(lotus_system
	PUBLIC
		"include/lotus/system/application.h"
		"include/lotus/system/common.h"
		"include/lotus/system/dear_imgui.h"
		"include/lotus/system/window.h")

target_include_directories(lotus_system PUBLIC "include/")
target_link_libraries(lotus_system PUBLIC lotus_core)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	enable_language(OBJCXX)
	target_sources(lotus_system
		PUBLIC
			"include/lotus/system/platforms/macos/application.h"
			"include/lotus/system/platforms/macos/window.h"

			"include/lotus/system/platforms/macos.h"
		PRIVATE
			"src/system/platforms/macos/application.mm"
			"src/system/platforms/macos/window.mm")
	target_compile_definitions(lotus_system
		PUBLIC
			"${LOTUS_SYSTEM_PLATFORM_INCLUDE_NAME}=<lotus/system/platforms/macos.h>")

	find_library(FOUNDATION_LIB Foundation)
	find_library(APP_KIT_LIB AppKit)
	find_library(QUARTZ_CORE_LIB QuartzCore)
	find_library(METAL_LIB Metal)
	target_link_libraries(lotus_system
		PUBLIC
			${FOUNDATION_LIB} ${APP_KIT_LIB} ${QUARTZ_CORE_LIB} ${METAL_LIB})
elseif(WIN32)
	target_sources(lotus_system
		PUBLIC
			"include/lotus/system/platforms/windows/application.h"
			"include/lotus/system/platforms/windows/details.h"
			"include/lotus/system/platforms/windows/window.h"

			"include/lotus/system/platforms/windows.h"
		PRIVATE
			"src/system/platforms/windows/application.cpp"
			"src/system/platforms/windows/details.cpp"
			"src/system/platforms/windows/window.cpp")
	target_compile_definitions(lotus_system
		PUBLIC
			"${LOTUS_SYSTEM_PLATFORM_INCLUDE_NAME}=<lotus/system/platforms/windows.h>"
			WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
elseif(UNIX)
	# TODO
endif()
