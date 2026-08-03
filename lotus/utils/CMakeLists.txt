add_library(lotus_utils STATIC)

target_include_directories(lotus_utils PUBLIC "include/")
target_sources(lotus_utils
	PUBLIC
		"include/lotus/utils/application.h"
		"include/lotus/utils/camera.h"
		"include/lotus/utils/dds.h"
		"include/lotus/utils/job_system.h"
		"include/lotus/utils/mpeg.h"
		"include/lotus/utils/profiler.h"
	PRIVATE
		"src/job_system.cpp"
		"src/profiler.cpp")
target_link_libraries(lotus_utils PUBLIC lotus_system)
