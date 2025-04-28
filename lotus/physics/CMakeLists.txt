add_library(lotus_physics STATIC)
configure_lotus_module(lotus_physics)

target_include_directories(lotus_physics PUBLIC "include/")
target_sources(lotus_physics
	PUBLIC
		"include/lotus/collision/algorithms/epa.h"
		"include/lotus/collision/algorithms/gjk.h"
		"include/lotus/collision/algorithms/sat.h"

		"include/lotus/collision/shapes/polyhedron.h"
		"include/lotus/collision/shapes/simple.h"

		"include/lotus/collision/common.h"
		"include/lotus/collision/shape.h"

		"include/lotus/physics/xpbd/constraints/bend.h"
		"include/lotus/physics/xpbd/constraints/contact.h"
		"include/lotus/physics/xpbd/constraints/face.h"
		"include/lotus/physics/xpbd/constraints/spring.h"

		"include/lotus/physics/xpbd/solver.h"

		"include/lotus/physics/body.h"
		"include/lotus/physics/body_properties.h"
		"include/lotus/physics/common.h"
	PRIVATE
		"src/collision/algorithms/epa.cpp"
		"src/collision/algorithms/gjk.cpp"
		"src/collision/algorithms/sat.cpp"

		"src/collision/shapes/polyhedron.cpp"

		"src/collision/common.cpp"

		"src/physics/xpbd/solver.cpp"

		"src/physics/body.cpp")
target_link_libraries(lotus_physics PUBLIC lotus_core)
