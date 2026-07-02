#pragma once

/// \file
/// The sequential impulse solver.

#include "lotus/physics/constraints/contact.h"

namespace lotus::physics {
	class world;
}

namespace lotus::physics::solvers::sequential_impulse {
	/// The sequential impulse solver.
	class solver {
	public:
		/// Solves all contacts.
		void timestep(scalar delta_time);

		world *physics_world = nullptr; ///< The physics world.
		u32 num_iterations = 8; ///< The number of iterations per time step.
	};
}
