add_library(lotus_physics STATIC)
configure_lotus_module(lotus_physics)

target_include_directories(lotus_physics PUBLIC "include/")
target_sources(lotus_physics
	PUBLIC
		"include/lotus/collision/algorithms/epa.h"
		"include/lotus/collision/algorithms/gjk.h"
		"include/lotus/collision/algorithms/sat.h"

		"include/lotus/collision/shapes/convex_polyhedron.h"
		"include/lotus/collision/shapes/simple.h"

		"include/lotus/collision/common.h"
		"include/lotus/collision/shape.h"

		"include/lotus/physics/avbd/constraints/cosserat_rod.h"

		"include/lotus/physics/avbd/solver.h"

		"include/lotus/physics/sequential_impulse/constraints/contact_set_blcp.h"

		"include/lotus/physics/sequential_impulse/solver.h"

		"include/lotus/physics/xpbd/constraints/bend.h"
		"include/lotus/physics/xpbd/constraints/contact.h"
		"include/lotus/physics/xpbd/constraints/cosserat_rod.h"
		"include/lotus/physics/xpbd/constraints/face.h"
		"include/lotus/physics/xpbd/constraints/spring.h"

		"include/lotus/physics/xpbd/solver.h"

		"include/lotus/physics/body.h"
		"include/lotus/physics/body_properties.h"
		"include/lotus/physics/common.h"
		"include/lotus/physics/world.h"
	PRIVATE
		"src/collision/algorithms/epa.cpp"
		"src/collision/algorithms/gjk.cpp"
		"src/collision/algorithms/sat.cpp"

		"src/collision/shapes/convex_polyhedron.cpp"

		"src/collision/common.cpp"

		"src/physics/avbd/solver.cpp"

		"src/physics/sequential_impulse/constraints/contact_set_blcp.cpp"

		"src/physics/sequential_impulse/solver.cpp"

		"src/physics/xpbd/constraints/contact.cpp"
		"src/physics/xpbd/constraints/cosserat_rod.cpp"

		"src/physics/xpbd/solver.cpp"

		"src/physics/world.cpp")
target_link_libraries(lotus_physics PUBLIC lotus_core)
