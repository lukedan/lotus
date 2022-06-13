set(LOTUS_SYSTEM_PLATFORM_INCLUDE_NAME "LOTUS_SYSTEM_PLATFORM_INCLUDE")

add_library(lotus_system STATIC)
configure_lotus_module(lotus_system)

target_sources(lotus_system
	PUBLIC
		"include/lotus/system/application.h"
		"include/lotus/system/common.h"
		"include/lotus/system/window.h")

target_include_directories(lotus_system PUBLIC "include/")

if(WIN32)
	target_include_directories(lotus_system PUBLIC "include/")
	target_sources(lotus_system
		PUBLIC
			"include/lotus/system/platforms/windows/application.h"
			"include/lotus/system/platforms/windows/details.h"
			"include/lotus/system/platforms/windows/window.h"

			"include/lotus/system/platforms/windows.h"
		PRIVATE
			"src/system/platforms/windows/application.cpp"
			"src/system/platforms/windows/window.cpp")
	target_compile_definitions(lotus_system
		PUBLIC
			"${LOTUS_SYSTEM_PLATFORM_INCLUDE_NAME}=<lotus/system/platforms/windows.h>"
			WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
	target_link_libraries(lotus_system PUBLIC lotus_core)
elseif(UNIX)
	# TODO
endif()
