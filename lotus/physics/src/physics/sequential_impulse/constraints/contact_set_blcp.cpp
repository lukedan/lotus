#include "lotus/physics/sequential_impulse/constraints/contact_set_blcp.h"

/// \file
/// Implementation of rigid body contact constraints.

namespace lotus::physics::sequential_impulse::constraints {
	contact_set_blcp::contact_data::contact_data(const body &b1, const body &b2, contact_info ci) {
		const mat33s ntb = ci.tangents.get_tangent_to_world_matrix().transposed();
		const vec3 r1 = ci.contact - b1.state.position.position;
		const vec3 r2 = ci.contact - b2.state.position.position;
		const mat33s rot1 = b1.state.position.orientation.into_matrix();
		const mat33s rot2 = b2.state.position.orientation.into_matrix();
		matrix<6, 6, scalar> m1 = zero;
		matrix<6, 6, scalar> m2 = zero;
		m1.set_block(0, 0, b1.properties.inverse_mass * mat33s::identity());
		m2.set_block(0, 0, b2.properties.inverse_mass * mat33s::identity());
		m1.set_block(3, 3, rot1 * b1.properties.inverse_inertia * rot1.transposed());
		m2.set_block(3, 3, rot2 * b2.properties.inverse_inertia * rot2.transposed());

		j1 = mat::concat_columns(-ntb, ntb * vec::cross_matrix(r1));
		j2 = mat::concat_columns(ntb, -ntb * vec::cross_matrix(r2));
		j1m = j1 * m1;
		j2m = j2 * m2;
		inv_dii = (j1m * j1.transposed() + j2m * j2.transposed()).inverse();
		crash_if(inv_dii.has_nan());
		b = j1 * b1.state.velocity.get_vector() + j2 * b2.state.velocity.get_vector();
	}


	contact_set_blcp contact_set_blcp::create(std::span<body *const> bs, std::span<const contact_info> cs) {
		contact_set_blcp result;
		result.bodies.reserve(bs.size());
		result.contacts_info = std::vector(cs.begin(), cs.end());
		result.contacts_data.reserve(cs.size());
		result.lambda.resize(cs.size(), zero);

		for (body *b : bs) {
			result.bodies.emplace_back(b);
		}

		for (usize i = 0; i < cs.size(); ++i) {
			const contact_info &c = cs[i];
			result.contacts_data.emplace_back(*bs[c.body1], *bs[c.body2], c);
			result.bodies[c.body1].contacts.emplace_back(body_contact::first_of(static_cast<u32>(i)));
			result.bodies[c.body2].contacts.emplace_back(body_contact::second_of(static_cast<u32>(i)));
		}

		return result;
	}

	void contact_set_blcp::solve_iteration(scalar dt) {
		for (usize i = 0; i < contacts_data.size(); ++i) {
			const contact_info &ci = contacts_info[i];
			const contact_data &contact = contacts_data[i];
			const vec3 r = contact.j1m * _a(ci.body1) + contact.j2m * _a(ci.body2) + contact.b;
			lambda[i] -= contact.inv_dii * r;
			lambda[i][0] = std::max<scalar>(0.0f, lambda[i][0]);
			// TODO: how to handle different friction coefficients?
			const scalar friction_coefficient = std::min(
				bodies[ci.body1].b->material.static_friction,
				bodies[ci.body1].b->material.static_friction
			);
			const scalar friction_limit = friction_coefficient * lambda[i][0];
			lambda[i][1] = std::clamp(lambda[i][1], -friction_limit, friction_limit);
			lambda[i][2] = std::clamp(lambda[i][2], -friction_limit, friction_limit);
		}
	}

	vec3 contact_set_blcp::get_impulse(usize contact_index) const {
		return contacts_info[contact_index].tangents.get_tangent_to_world_matrix() * lambda[contact_index];
	}

	void contact_set_blcp::apply_impulses() const {
		for (usize i = 0; i < contacts_data.size(); ++i) {
			const contact_info &ci = contacts_info[i];
			const vec3 impulse = get_impulse(i);
			crash_if(impulse.has_nan());
			bodies[ci.body1].b->apply_impulse(ci.contact, -impulse);
			bodies[ci.body2].b->apply_impulse(ci.contact, impulse);
		}
	}

	tangent_frame<scalar> contact_set_blcp::select_tangent_frame_for_contact(
		const body &b1, const body &b2, vec3 contact_point, vec3 contact_normal
	) {
		const vec3 rel_velocity =
			b1.state.velocity.get_velocity_at(contact_point - b1.state.position.position) -
			b2.state.velocity.get_velocity_at(contact_point - b2.state.position.position);
		const vec3 bitangent_dir = vec::cross(contact_normal, rel_velocity);
		const scalar bitangent_len = bitangent_dir.squared_norm();
		if (bitangent_len < epsilons::contact_tangent) {
			return tangent_frame<scalar>::from_normal(contact_normal);
		}
		const vec3 bitangent = bitangent_dir / std::sqrt(bitangent_len);
		const vec3 tangent = vec::cross(bitangent, contact_normal);
		return tangent_frame<scalar>::from_ntb(contact_normal, tangent, bitangent);
	}

	column_vector<6, scalar> contact_set_blcp::_a(u32 body_index) const {
		column_vector<6, scalar> a = zero;
		for (body_contact bc : bodies[body_index].contacts) {
			const contact_data &other = contacts_data[bc.contact_index];
			a += (bc.second_body ? other.j2 : other.j1).transposed() * lambda[bc.contact_index];
		}
		return a;
	}
}
