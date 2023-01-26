add_library(lotus_core STATIC)
configure_lotus_module(lotus_core)

target_include_directories(lotus_core PUBLIC "include/")
target_sources(lotus_core
	PUBLIC
		"include/lotus/algorithms/convex_hull.h"

		"include/lotus/math/aab.h"
		"include/lotus/math/constants.h"
		"include/lotus/math/matrix.h"
		"include/lotus/math/quaternion.h"
		"include/lotus/math/sequences.h"
		"include/lotus/math/vector.h"

		"include/lotus/containers/intrusive_linked_list.h"
		"include/lotus/containers/maybe_uninitialized.h"
		"include/lotus/containers/pool.h"
		"include/lotus/containers/pooled_hash_table.h"
		"include/lotus/containers/short_vector.h"
		"include/lotus/containers/static_optional.h"

		"include/lotus/memory/block.h"
		"include/lotus/memory/common.h"
		"include/lotus/memory/managed_allocator.h"
		"include/lotus/memory/stack_allocator.h"

		"include/lotus/utils/camera.h"
		"include/lotus/utils/custom_float.h"
		"include/lotus/utils/dds.h"
		"include/lotus/utils/index.h"
		"include/lotus/utils/misc.h"
		"include/lotus/utils/static_function.h"
		"include/lotus/utils/strings.h"

		"include/lotus/color.h"
		"include/lotus/common.h"
		"include/lotus/logging.h"
	PRIVATE
		"src/memory/common.cpp"
		"src/memory/stack_allocator.cpp"
		
		"src/logging.cpp")

if("${LOTUS_USE_ALLOCATOR}" STREQUAL "mimalloc")
	find_package(mimalloc CONFIG REQUIRED)
	target_link_libraries(lotus_core PUBLIC mimalloc)
	target_compile_definitions(lotus_core PUBLIC LOTUS_USE_MIMALLOC)
elseif(NOT "${LOTUS_USE_ALLOCATOR}" STREQUAL "default")
	message(FATAL_ERROR "Unknown allocator: ${LOTUS_USE_ALLOCATOR}")
endif()
