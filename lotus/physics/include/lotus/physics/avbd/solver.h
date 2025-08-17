#pragma once

/// \file
/// The augmented vertex block descent (AVBD) solver.

#include <vector>

#include "lotus/physics/body.h"
#include "lotus/physics/avbd/constraints/cosserat_rod.h"

namespace lotus::physics {
	class world;
}

namespace lotus::physics::avbd {
	/// The AVBD solver.
	class solver {
	public:
		/// Advances the simulation by one timestep.
		void timestep(scalar dt, u32 iters);

		world *physics_world = nullptr; ///< The physics world.

		std::vector<particle> particles; ///< The list of particles.
		std::vector<orientation> orientations; ///< The list of orientations.

		/// All Cosserat rod bending-twisting constraints.
		std::vector<constraints::cosserat_rod::bend_twist> rod_bend_twist_constraints;
		/// All Cosserat rod stretching-shearing constraints.
		std::vector<constraints::cosserat_rod::stretch_shear> rod_stretch_shear_constraints;
	};
}
