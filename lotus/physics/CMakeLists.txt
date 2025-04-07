add_library(lotus_physics STATIC)
configure_lotus_module(lotus_physics)

target_include_directories(lotus_physics PUBLIC "include/")
target_sources(lotus_physics
	PUBLIC
		"include/lotus/collision/algorithms/common.h"
		"include/lotus/collision/algorithms/gjk_epa.h"

		"include/lotus/collision/shapes/polyhedron.h"
		"include/lotus/collision/shapes/simple.h"

		"include/lotus/collision/common.h"
		"include/lotus/collision/shape.h"

		"include/lotus/physics/constraints/bend.h"
		"include/lotus/physics/constraints/contact.h"
		"include/lotus/physics/constraints/face.h"
		"include/lotus/physics/constraints/spring.h"

		"include/lotus/physics/body.h"
		"include/lotus/physics/body_properties.h"
		"include/lotus/physics/common.h"
		"include/lotus/physics/engine.h"
	PRIVATE
		"src/collision/algorithms/gjk_epa.cpp"
		
		"src/collision/shapes/polyhedron.cpp"

		"src/physics/body.cpp"
		"src/physics/engine.cpp")
target_link_libraries(lotus_physics PUBLIC lotus_core)
