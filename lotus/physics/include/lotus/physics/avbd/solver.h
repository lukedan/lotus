#pragma once

/// \file
/// The augmented vertex block descent (AVBD) solver.

#include <vector>

#include "lotus/physics/body.h"
#include "lotus/physics/world.h"
#include "lotus/physics/avbd/constraints/contact.h"
#include "lotus/physics/avbd/constraints/cosserat_rod.h"

namespace lotus::physics {
	class world;
}

namespace lotus::physics::avbd {
	/// The AVBD solver.
	class solver {
	public:
		constexpr static scalar contact_damping = 0.95f; ///< α - explosive error correction prevention.
		constexpr static scalar stiffness_ramping = 10.0f; ///< β - the speed at which stiffness increases.
		constexpr static scalar minimum_stiffness = 1.0f; ///< Minimum value of k.
		constexpr static scalar maximum_stiffness = 10000000000.0f; ///< Maximum value of k.

		/// Advances the simulation by one timestep.
		void timestep(scalar dt, u32 iters);

		world *physics_world = nullptr; ///< The physics world.

		std::vector<particle> particles; ///< The list of particles.
		std::vector<orientation> orientations; ///< The list of orientations.

		/// Previous frame accelerations used for the initial position estimation.
		std::vector<vec3> particle_prev_accelerations; // TODO prevent this from going out of sync

		/// All Cosserat rod bending-twisting constraints.
		std::vector<constraints::cosserat_rod::bend_twist> rod_bend_twist_constraints;
		/// All Cosserat rod stretching-shearing constraints.
		std::vector<constraints::cosserat_rod::stretch_shear> rod_stretch_shear_constraints;

		std::vector<constraints::rigid_body_contact> contacts; ///< All contacts in the current time step.
	private:
		/// Clamped contact force.
		struct _contact_force {
			/// Zero initialization.
			_contact_force(zero_t) {
			}
			/// Clamps the normal force to be larger than zero, and the friction force to be within the friction
			/// cone.
			[[nodiscard]] static _contact_force clamp(
				vec3 force, const material_properties&, const material_properties&
			);

			vec3 force = zero; ///< Clamped contact force.
			bool normal_clamped = false; ///< Whether the normal force is clamped.
			bool friction_clamped = false; ///< Whether the friction force is clamped.
		};

		/// Data associated with all rigid bodies within a single time step.
		struct _body_step_data {
			/// Constraints that involve a body.
			struct constraints {
				std::vector<u32> contact_constraints; ///< Related contact constraints.
			};

			std::vector<body_position> initial_positions; ///< Initial positions.
			std::vector<body_position> inertial_positions; ///< Inertial positions.
			std::vector<constraints> constraint_association; ///< Constraint association.
		};
		/// Data associated with all particles within a single time step.
		struct _particle_step_data {
			/// Constraints that involve a particle.
			struct constraints {
				std::vector<u32> stretch_shear_constraints; ///< Related stretching shearing constraints.
			};

			std::vector<vec3> initial_positions; ///< Initial positions.
			std::vector<vec3> inertial_positions; ///< Inertial positions.
			std::vector<constraints> constraint_association; ///< Constraint association.
		};
		/// Data associated with all orientations within a single time step.
		struct _orientation_step_data {
			/// Constraints that involve an orientation.
			struct constraints {
				std::vector<u32> bend_constraints; ///< Related bend constraints.
				/// Related stretching shearing constraints.
				u32 stretch_shear_constraint = std::numeric_limits<u32>::max();
			};

			std::vector<constraints> constraint_association; ///< Constraint association.
		};

		/// Computes the raw (\ref alpha not applied) contact error at the given contact point.
		[[nodiscard]] vec3 _compute_raw_contact_error(
			const constraints::rigid_body_contact&, const constraints::rigid_body_contact::point&
		);
		/// Computes the contact error at the given contact point with \ref alpha applied.
		[[nodiscard]] vec3 _compute_contact_error(
			const constraints::rigid_body_contact &contact,
			const constraints::rigid_body_contact::point &contact_point
		) {
			return
				_compute_raw_contact_error(contact, contact_point) -
				contact_damping * contact_point.initial_error;
		}
		/// Updates all contact constraints. This needs to happen before the association between bodies and
		/// constraints are computed in \ref _prepare_bodies().
		void _update_body_contacts();
		/// Prepares rigid body simulation by computing body step data for all bodies.
		[[nodiscard]] _body_step_data _prepare_bodies(scalar dt) const;
		/// Computes initial estimates for all rigid bodies.
		void _init_solve_bodies(scalar dt, const _body_step_data&);
		/// Updates all rigid bodies by a single iteration.
		void _solve_bodies(scalar dt, const _body_step_data&);
		/// Updates all rigid body dual variables.
		void _update_body_dual_variables(const _body_step_data&);
		/// Updates the velocities of all bodies at the end of a time step.
		void _compute_body_velocities(scalar dt, const _body_step_data&);

		/// Prepares particle simulation by computing particle step data for all particles.
		[[nodiscard]] _particle_step_data _prepare_particles(scalar dt) const;
		/// Initializes all particles for solving by computing initial estimates for their new positions.
		void _init_solve_particles(scalar dt);
		/// Updates all particles by a single iteration.
		void _solve_particles(scalar dt, const _particle_step_data&);
		/// Updates the velocities of all particles at the end of a time step.
		void _compute_particle_velocities(scalar dt, const _particle_step_data&);

		/// Prepares orientation simulation by computing orientation step data for all particles.
		[[nodiscard]] _orientation_step_data _prepare_orientations() const;
		/// Updates all orientations by a single iteration.
		void _solve_orientations(const _orientation_step_data&);
	};
}
