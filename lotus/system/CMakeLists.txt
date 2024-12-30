add_library(lotus_system STATIC)
configure_lotus_module(lotus_system)

target_sources(lotus_system
	PUBLIC
		"include/lotus/system/application.h"
		"include/lotus/system/common.h"
		"include/lotus/system/dear_imgui.h"
		"include/lotus/system/identifier.h"
		"include/lotus/system/window.h")

target_include_directories(lotus_system PUBLIC "include/")
target_link_libraries(lotus_system PUBLIC lotus_core)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	enable_language(OBJCXX)
	target_sources(lotus_system
		PUBLIC
			"include/lotus/system/platforms/macos/application.h"
			"include/lotus/system/platforms/macos/common.h"
			"include/lotus/system/platforms/macos/window.h"
		PRIVATE
			"src/system/platforms/macos/application.mm"
			"src/system/platforms/macos/window.mm")
	set(LOTUS_SYSTEM_PLATFORM_INCLUDE_DIR "lotus/system/platforms/macos/")

	find_library(FOUNDATION_LIB Foundation)
	find_library(APP_KIT_LIB AppKit)
	find_library(QUARTZ_CORE_LIB QuartzCore)
	find_library(METAL_LIB Metal)
	target_link_libraries(lotus_system
		PUBLIC
			"${FOUNDATION_LIB}" "${APP_KIT_LIB}" "${QUARTZ_CORE_LIB}" "${METAL_LIB}")
	target_compile_options(lotus_system
		PUBLIC
			-fobjc-arc)
elseif(WIN32)
	target_sources(lotus_system
		PUBLIC
			"include/lotus/system/platforms/windows/application.h"
			"include/lotus/system/platforms/windows/common.h"
			"include/lotus/system/platforms/windows/details.h"
			"include/lotus/system/platforms/windows/window.h"
		PRIVATE
			"src/system/platforms/windows/application.cpp"
			"src/system/platforms/windows/details.cpp"
			"src/system/platforms/windows/window.cpp")
	set(LOTUS_SYSTEM_PLATFORM_INCLUDE_DIR "lotus/system/platforms/windows/")
	target_compile_definitions(lotus_system
		PUBLIC
			WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
elseif(UNIX)
	# TODO
endif()

target_compile_definitions(lotus_system
	PUBLIC
		"LOTUS_SYSTEM_PLATFORM_INCLUDE_APPLICATION=<${LOTUS_SYSTEM_PLATFORM_INCLUDE_DIR}application.h>"
		"LOTUS_SYSTEM_PLATFORM_INCLUDE_COMMON=<${LOTUS_SYSTEM_PLATFORM_INCLUDE_DIR}common.h>"
		"LOTUS_SYSTEM_PLATFORM_INCLUDE_WINDOW=<${LOTUS_SYSTEM_PLATFORM_INCLUDE_DIR}window.h>")
