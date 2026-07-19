#pragma once

/// \file
/// The sequential impulse solver.

#include "lotus/physics/constraints/contact.h"
#include "lotus/physics/constraints/hinge.h"
#include "lotus/physics/constraints/pin.h"

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
		u32 num_substeps = 4;
		u32 num_velocity_iterations = 2; ///< The number of velocity iterations per time step.
		scalar baumgarte_stabilization = 10.0f; ///< Baumgarte stabilization coefficient.
	private:
		/// Precomputed data and state for a contact constraint.
		struct _contact_constraint_data {
			/// Data about a single contact point.
			struct point_data {
				/// Zero initialization.
				point_data(zero_t) {
				}

				vec3 offset1 = zero; ///< Offset of the contact point from the first body's center of mass.
				vec3 offset2 = zero; ///< Offset of the contact point from the second body's center of mass.
				scalar inv_effective_mass_n = 0.0f; ///< Inverse effective mass along the normal.
				mat22s inv_effective_mass_t = zero; ///< Inverse effective mass along the tangent plane.
				scalar stabilization = 0.0f; ///< Baumgarte stabilization term.

				vec3 lambda = zero; ///< Total applied impulse.

				/// Applies lambda using the given relative velocity in tangent space, applies clamping, and returns
				/// the amount by which \ref lambda has changed.
				[[nodiscard]] vec3 update_lambda(vec3 tangential_velocity, const constraints::rigid_body_contact&);
				/// Applies an impulse that corresponds to the given delta lambda value.
				void apply_impulses_for_delta_lambda(
					vec3 delta_lambda, const constraints::rigid_body_contact&, const _contact_constraint_data&
				) const;
			};

			std::vector<point_data> points; ///< Data for all contact points.
			mat33s inverse_inertia1 = zero; ///< Rotated inverse inertia of the first body.
			mat33s inverse_inertia2 = zero; ///< Rotated inverse inertia of the second body.

			/// Zero initialization.
			_contact_constraint_data(zero_t) {
			}

			/// Precomputes necessary information for the given contact constraint.
			void prepare(const constraints::rigid_body_contact&, scalar baumgarte_coeff, scalar collision_threshold);
			/// Iterates over all contact points and updates the impulse estimates and body velocities.
			void velocity_update(const constraints::rigid_body_contact&);
		};
		/// Precomputed data and state for a hinge constraint.
		struct _hinge_constraint_data {
			vec3 axis1 = zero; ///< Axis in world space on the first body.
			vec3 axis2 = zero; ///< Axis in world space on the second body.
			mat33s inverse_inertia1 = zero; ///< Rotated inverse inertia of the first body.
			mat33s inverse_inertia2 = zero; ///< Rotated inverse inertia of the second body.
			mat33s inv_effective_mass = zero; ///< Inverse effective mass.
			vec3 stabilization = zero; ///< Baumgarte stabilization term.

			vec3 lambda = zero; ///< Total applied torque.

			/// Zero initialization.
			_hinge_constraint_data(zero_t) {
			}

			/// Precomputes necessary information for the given hinge constraint.
			void prepare(const constraints::hinge&, scalar baumgarte_coeff);
			/// Updates the torque estimate and body velocities.
			void velocity_update(const constraints::hinge&);
			/// Applies impulses to the bodies corresponding to the given delta lambda value.
			void apply_impulses_for_delta_lambda(vec3 delta_lambda, const constraints::hinge&) const;
		};
		/// Precomputed data and state for a pin constraint.
		struct _pin_constraint_data {
			vec3 offset1 = zero; ///< Offset of the pin point from the first body's center of mass.
			vec3 offset2 = zero; ///< Offset of the pin point from the second body's center of mass.
			mat33s inverse_inertia1 = zero; ///< Rotated inverse inertia of the first body.
			mat33s inverse_inertia2 = zero; ///< Rotated inverse inertia of the second body.
			mat33s inv_effective_mass = zero; ///< Inverse effective mass.
			vec3 stabilization = zero; ///< Baumgarte stabilization term.

			vec3 lambda = zero; ///< Total applied impulse.

			/// Zero initialization.
			_pin_constraint_data(zero_t) {
			}

			/// Computes the effective mass.
			[[nodiscard]] static mat33s compute_effective_mass(
				mat33s inv_i1, mat33s inv_i2, scalar inv_m1, scalar inv_m2, vec3 o1, vec3 o2
			);

			/// Precomputes necessary information for the given pin constraint.
			void prepare(const constraints::pin&, scalar baumgarte_coeff);
			/// Updates the impulse estimate and body velocities.
			void velocity_update(const constraints::pin&);
			/// Applies impulses to the bodies corresponding to the given delta lambda value.
			void apply_impulses_for_delta_lambda(vec3 delta_lambda, const constraints::pin&) const;
		};

		/// Immediately Applies \p impulse to the \p body1, and \p -impulse to the second \p body2.
		static void _apply_impulses(
			body *body1, body *body2, mat33s inv_i1, mat33s inv_i2, vec3 off1, vec3 off2, vec3 impulse
		);
	};
}
