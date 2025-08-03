#pragma once

/// \file
/// Contact constraints.

#include "lotus/math/tangent_frame.h"
#include "lotus/physics/common.h"
#include "lotus/physics/body.h"

namespace lotus::physics::sequential_impulse::constraints {
	/// Contact constraints between a set of bodies solved using the splitting box LCP solver.
	struct contact_set_blcp {
	public:
		using jacobian_mat = matrix<3, 6, scalar>; ///< Jacobian matrix type.
		/// Information about a contact.
		struct contact_info {
			/// No initialization.
			contact_info(uninitialized_t) {
			}

			vec3 contact = uninitialized; ///< Contact point.
			tangent_frame<scalar> tangents = uninitialized; ///< Contact tangent frame.
			u32 body1; ///< Index of the first body.
			u32 body2; ///< Index of the second body.
		};

		/// Describes how a body involved in a contact.
		struct body_contact {
		public:
			/// No initialization.
			body_contact(uninitialized_t) {
			}
			/// Initializes this object to describe the first body of the given contact.
			[[nodiscard]] static body_contact first_of(u32 i) {
				return body_contact(false, i);
			}
			/// Initializes this object to describe the second body of the given contact.
			[[nodiscard]] static body_contact second_of(u32 i) {
				return body_contact(true, i);
			}

			bool second_body  : 1;  ///< Whether this body is the second one involved in the contact.
			u32 contact_index : 31; ///< Index of the contact.
		private:
			/// Initializes all fields of this struct.
			body_contact(bool sb, u32 ci) : second_body(sb), contact_index(ci) {
			}
		};
		/// Data associated with a body.
		struct body_data {
			/// Initializes \ref b.
			explicit body_data(body *bd) : b(bd) {
			}

			body *b; ///< The body.
			std::vector<body_contact> contacts; ///< Indices of the contacts involving this body.
		};
		/// Precomputed information about a contact.
		struct contact_data {
			/// No initialization.
			contact_data(uninitialized_t) {
			}
			/// Initializes the contact with the given pair of bodies and collision coordinate space.
			contact_data(const body &b1, const body &b2, contact_info ci);

			jacobian_mat j1 = uninitialized; ///< Jacobian of the first body.
			jacobian_mat j2 = uninitialized; ///< Jacobian of the second body.
			jacobian_mat j1m = uninitialized; ///< The product J1.M.
			jacobian_mat j2m = uninitialized; ///< The product J2.M.
			mat33s inv_dii = uninitialized; ///< Inverse diagonal component of the A matrix.
			vec3 b = uninitialized; ///< The b vector.
		};

		/// Initializes this object to empty.
		contact_set_blcp() = default;
		/// Initializes the contact set with the given bodies and contacts.
		[[nodiscard]] static contact_set_blcp create(std::span<body *const>, std::span<const contact_info>);

		/// Updates all contacts once in a Gauss-Seidel fashion.
		void solve_iteration(scalar dt);

		/// Returns the impulse of the given contact. This impulse is intended for the second body of the contact,
		/// i.e., its negative is intended to be applied to the first body.
		[[nodiscard]] vec3 get_impulse(usize contact_index) const;
		/// Updates the velocities of all bodies.
		void apply_impulses() const;

		/// Computes the optimal contact tangent frame, so that the tangent is aligned with the relative velocity at
		/// the contact point.
		[[nodiscard]] static tangent_frame<scalar> select_tangent_frame_for_contact(
			const body&, const body&, vec3 contact_point, vec3 contact_normal
		);

		std::vector<body_data> bodies; ///< All bodies involved in this set of contacts.
		std::vector<contact_info> contacts_info; ///< Information about all contacts.
		std::vector<contact_data> contacts_data; ///< All contacts.
		/// The multipliers (force magnitudes) along the normal, tangent, and bitangent directions.
		std::vector<vec3> lambda;
	private:
		/// Computes the a vector for the given body.
		[[nodiscard]] column_vector<6, scalar> _a(u32 body_index) const;
	};
}
