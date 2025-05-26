#pragma once

/// \file
/// The XPBD solver.

#include <vector>
#include <list>
#include <deque>
#include <optional>

#include "lotus/collision/shape.h"
#include "lotus/physics/body.h"
#include "constraints/spring.h"
#include "constraints/contact.h"
#include "constraints/face.h"
#include "constraints/bend.h"

namespace lotus::physics {
	class world;
}

namespace lotus::physics::xpbd {
	/// The XPBD solver.
	class solver {
	public:
		/// Executes one time step with the given delta time in seconds and the given number of iterations.
		void timestep(scalar dt, u32 iters);


		/// Handles the collision between a plane and a particle.
		static bool handle_shape_particle_collision(const collision::shapes::plane&, const body_state&, vec3&);
		/// Handles the collision between a kinematic sphere and a particle.
		static bool handle_shape_particle_collision(const collision::shapes::sphere&, const body_state&, vec3&);
		/// Handles the collision between a kinematic convex polyhedron and a particle.
		static bool handle_shape_particle_collision(const collision::shapes::convex_polyhedron&, const body_state&, vec3&);


		world *physics_world = nullptr; ///< The physics world.

		std::vector<particle> particles; ///< The list of particles.

		std::vector<constraints::particle_spring> particle_spring_constraints; ///< The list of spring constraints.
		std::vector<scalar> spring_lambdas; ///< Lambda values for all spring constraints.

		/// Determines how face constraints are projected.
		constraints::face::projection_type face_constraint_projection_type =
			constraints::face::projection_type::gauss_seidel;
		std::vector<constraints::face> face_constraints; ///< The list of face constraints.
		std::vector<column_vector<6, scalar>> face_lambdas; ///< Lambda values for all face constraints.

		std::vector<constraints::bend> bend_constraints; ///< The list of bend constraints.
		std::vector<scalar> bend_lambdas; ///< Lambda values for bend constraints.

		std::deque<constraints::body_contact> contact_constraints; ///< Contact constraints.
		std::vector<std::pair<scalar, scalar>> contact_lambdas; ///< Lambda values for contact constraints.
	};
}
