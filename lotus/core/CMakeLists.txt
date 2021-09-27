add_library(lotus_core STATIC)
set_target_compile_options(lotus_core)

target_include_directories(lotus_core PUBLIC "include/")
target_sources(lotus_core
	PUBLIC
		"include/lotus/algorithms/convex_hull.h"

		"include/lotus/math/aab.h"
		"include/lotus/math/constants.h"
		"include/lotus/math/matrix.h"
		"include/lotus/math/quaternion.h"
		"include/lotus/math/vector.h"

		"include/lotus/utils/camera.h"
		"include/lotus/utils/event_closure.h"
		"include/lotus/utils/stack_allocator.h"
		"include/lotus/utils/static_function.h"

		"include/lotus/color.h"
		"include/lotus/common.h"
		"include/lotus/memory.h"
	PRIVATE
		"src/utils/stack_allocator.cpp")
