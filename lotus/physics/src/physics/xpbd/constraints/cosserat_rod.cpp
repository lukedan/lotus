#include "lotus/physics/xpbd/constraints/cosserat_rod.h"

/// \file
/// Projection implementation of Cosserat rod constraints.

namespace lotus::physics::xpbd::constraints::cosserat_rod {
	void stretch_shear::project(
		particle &p1, particle &p2, physics::orientation &o, scalar inv_dt2, lagrangians &lambda
	) const {
		const scalar inv_m1 = p1.properties.inverse_mass;
		const scalar inv_m2 = p2.properties.inverse_mass;

		const vec3 d = o.state.orientation.rotate(direction_basis);
		const vec3 c = inv_initial_length * (p2.state.position - p1.state.position) - d;
		const scalar dcdp1 = -inv_initial_length;
		const scalar dcdp2 = inv_initial_length;

		const scalar sum_grad_c = dcdp1 * inv_m1 * dcdp1 + dcdp2 * inv_m2 * dcdp2 + o.inv_inertia;
		const vec3 delta_lambda = (-c - compliance * inv_dt2 * lambda) / (sum_grad_c + compliance * inv_dt2);

		const vec3 delta_p1 = inv_m1 * dcdp1 * delta_lambda;
		const vec3 delta_p2 = inv_m2 * dcdp2 * delta_lambda;
		const quats delta_o =
			-2.0f * o.inv_inertia *
			quat::from_vec3_xyz(delta_lambda) * o.state.orientation * quat::from_vec3_xyz(-direction_basis);

		p1.state.position += delta_p1;
		p2.state.position += delta_p2;
		o.state.orientation = quatu::normalize(o.state.orientation + delta_o);

		lambda += delta_lambda;
	}

	void bend_twist::project(orientation &o1, orientation &o2, scalar inv_dt2, lagrangians &lambda) const {
		const vec4 c = (o1.state.orientation.conjugate() * o2.state.orientation - initial_bend).into_vector_wxyz();

		const scalar sum_grad_c = o1.inv_inertia + o2.inv_inertia;
		const vec4 delta_lambda = (-c - compliance * inv_dt2 * lambda) / (sum_grad_c + compliance * inv_dt2);
		const auto delta_lambda_quat = quat::from_vec4_wxyz(delta_lambda);

		const quats delta_o1 = o1.inv_inertia * o2.state.orientation * delta_lambda_quat.conjugate();
		const quats delta_o2 = o2.inv_inertia * o1.state.orientation * delta_lambda_quat;

		o1.state.orientation = quatu::normalize(o1.state.orientation + delta_o1);
		o2.state.orientation = quatu::normalize(o2.state.orientation + delta_o2);

		lambda += delta_lambda;
	}
}
