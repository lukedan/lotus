#include "lotus/physics/rigid_body/constraints/contact_set_blcp.h"

/// \file
/// Implementation of rigid body contact constraints.

namespace lotus::physics::rigid_body::constraints {
	contact_set_blcp::contact_data::contact_data(const body &b1, const body &b2, contact_info ci) {
		const mat33s ntb = ci.tangents.get_tangent_to_world_matrix().transposed();
		const vec3 r1 = ci.contact - b1.state.position.position;
		const vec3 r2 = ci.contact - b2.state.position.position;
		matrix<6, 6, scalar> m1 = zero;
		matrix<6, 6, scalar> m2 = zero;
		m1.set_block(0, 0, b1.properties.inverse_mass * mat33s::identity());
		m2.set_block(0, 0, b2.properties.inverse_mass * mat33s::identity());
		m1.set_block(3, 3, b1.properties.inverse_inertia);
		m2.set_block(3, 3, b2.properties.inverse_inertia);

		j1 = mat::concat_columns(-ntb, ntb * vec::cross_matrix(r1));
		j2 = mat::concat_columns(ntb, -ntb * vec::cross_matrix(r2));
		j1m = j1 * m1;
		j2m = j2 * m2;
		inv_dii = (j1m * j1.transposed() + j2m * j2.transposed()).inverse();
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

	void contact_set_blcp::apply_impulses() const {
		for (usize i = 0; i < contacts_data.size(); ++i) {
			const contact_info &ci = contacts_info[i];
			const vec3 l = lambda[i];
			const vec3 impulse = ci.tangents.get_tangent_to_world_matrix() * l;
			bodies[ci.body1].b->apply_impulse(ci.contact, -impulse);
			bodies[ci.body2].b->apply_impulse(ci.contact, impulse);
		}
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
