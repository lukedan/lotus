#pragma once

/// \file
/// The rigid body solver.

#include <span>

#include "lotus/physics/rigid_body/constraints/contact_set_blcp.h"

namespace lotus::physics {
	class world;
}

namespace lotus::physics::rigid_body {
	/// The rigid body solver.
	class solver {
	public:
		/// Solves all contacts.
		void timestep(scalar delta_time, u32 iters);

		world *physics_world = nullptr; ///< The physics world.

		std::vector<constraints::contact_set_blcp> contact_constraints; ///< Contact constraints.
	};
}
