#pragma once

/// \file
/// The augmented vertex block descent (AVBD) solver.

#include <vector>

#include "lotus/physics/body.h"
#include "lotus/physics/world.h"
#include "lotus/physics/solvers/avbd/constraints/cosserat_rod.h"

namespace lotus::physics {
	class world;
}

namespace lotus::physics::solvers::avbd {
	namespace constraints {
		using namespace ::lotus::physics::constraints;
	}

	/// The AVBD solver.
	class solver {
	public:
		constexpr static scalar contact_damping = 0.95f; ///< α - explosive error correction prevention.
		constexpr static scalar stiffness_ramping = 10.0f; ///< β - the speed at which stiffness increases.
		constexpr static scalar minimum_stiffness = 1.0f; ///< Minimum value of k.
		constexpr static scalar maximum_stiffness = 10000000000.0f; ///< Maximum value of k.

		// not an elegant solution, but works
		/// Constant added to hinge constraints to allow the stiffness to increase normally. Must be greater than 1.
		constexpr static scalar hinge_constant = 2.0f;

		/// Advances the simulation by one timestep.
		void timestep(scalar dt);

		world *physics_world = nullptr; ///< The physics world.
		u32 num_iterations = 8; ///< The number of iterations per time step.

		std::vector<particle> particles; ///< The list of particles.
		std::vector<orientation> orientations; ///< The list of orientations.

		/// Previous frame accelerations used for the initial position estimation.
		std::vector<vec3> particle_prev_accelerations; // TODO prevent this from going out of sync

		/// All Cosserat rod bending-twisting constraints.
		std::vector<constraints::cosserat_rod::bend_twist> rod_bend_twist_constraints;
		/// All Cosserat rod stretching-shearing constraints.
		std::vector<constraints::cosserat_rod::stretch_shear> rod_stretch_shear_constraints;

		bool has_indefinite_hessians = false; ///< Whether the last step produced indefinite Hessians.
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

		/// Dual variables for a contact constraint.
		struct _contact_dual {
			/// Dual variables for a single contact point.
			struct point {
				/// Zero initialization.
				point(zero_t) {
				}

				vec3 stiffness = zero; ///< Soft stiffness.
				vec3 force = zero; ///< The force applied during this step, i.e. the dual variable.
				vec3 initial_error = zero; ///< Contact error at the beginning of the time step.
			};
			std::vector<point> contact_points; ///< All contact points.
		};
		/// Dual variables for a pin constraint.
		struct _pin_dual {
			/// Zero initialization.
			_pin_dual(zero_t) {
			}

			vec3 stiffness = zero; ///< Soft stiffness.
			vec3 force = zero; ///< The force applied during this step.
		};
		/// Dual variables for a hinge constraint.
		struct _hinge_dual {
			/// Zero initialization.
			_hinge_dual(zero_t) {
			}

			scalar stiffness = 0.0f; ///< Soft stiffness.
			scalar force = 0.0f; ///< The force applied during this step.
		};

		/// Data associated with all rigid bodies within a single time step.
		struct _body_step_data {
			/// Constraints that involve a body.
			struct constraints {
				std::vector<u32> contact_constraints; ///< Related contact constraints.
				std::vector<u32> spring_constraints; ///< Related spring constraints.
				std::vector<u32> pin_constraints; ///< Related pin constraints.
				std::vector<u32> hinge_constraints; ///< Related hinge constraints.
			};

			std::vector<body_position> initial_positions; ///< Initial positions.
			std::vector<body_position> inertial_positions; ///< Inertial positions.
			std::vector<constraints> constraint_association; ///< Constraint association.

			std::vector<_contact_dual> contact_duals; ///< Dual variables for contact constraints.
			std::vector<_pin_dual> pin_duals; ///< Dual variables for pin constraints.
			std::vector<_hinge_dual> hinge_duals; ///< Dual variables for hinge constraints.
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
		) const;
		/// Computes the contact error at the given contact point with \ref alpha applied.
		[[nodiscard]] vec3 _compute_contact_error(
			const constraints::rigid_body_contact &contact,
			const constraints::rigid_body_contact::point &contact_point,
			const _contact_dual::point &dual_point
		) const {
			return _compute_raw_contact_error(contact, contact_point) - contact_damping * dual_point.initial_error;
		}
		/// Prepares rigid body simulation by computing body step data for all bodies.
		[[nodiscard]] _body_step_data _prepare_bodies(scalar dt) const;
		/// Computes initial estimates for all rigid bodies.
		void _init_solve_bodies(scalar dt, const _body_step_data&);
		/// Updates all rigid bodies by a single iteration.
		void _solve_bodies(scalar dt, const _body_step_data&);
		/// Updates all rigid body dual variables.
		void _update_body_dual_variables(_body_step_data&);
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
