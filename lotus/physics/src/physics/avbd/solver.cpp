#include "lotus/physics/avbd/solver.h"

/// \file
/// Implementation of the AVBD solver.

#include "lotus/physics/world.h"

namespace lotus::physics::avbd {
	/// Association from a particle back to related constraints.
	struct _particle_association {
		std::vector<u32> stretch_shear_constraints; ///< Related stretching shearing constraints.
	};
	/// Association from an orientation back to related constraints.
	struct _orientation_association {
		std::vector<u32> bend_constraints; ///< Related bend constraints.
		u32 stretch_shear_constraint = std::numeric_limits<u32>::max(); ///< Related stretching shearing constraints.
	};

	void solver::timestep(scalar dt, u32 iters) {
		std::vector<_particle_association> par_assoc(particles.size());
		std::vector<_orientation_association> ori_assoc(orientations.size());

		// compute inverse association
		for (usize i = 0; i < rod_bend_twist_constraints.size(); ++i) {
			const constraints::cosserat_rod::bend_twist &bc = rod_bend_twist_constraints[i];
			ori_assoc[bc.orientation1].bend_constraints.emplace_back(static_cast<u32>(i));
			ori_assoc[bc.orientation2].bend_constraints.emplace_back(static_cast<u32>(i));
		}
		for (usize i = 0; i < rod_stretch_shear_constraints.size(); ++i) {
			const constraints::cosserat_rod::stretch_shear &sc = rod_stretch_shear_constraints[i];
			par_assoc[sc.particle1].stretch_shear_constraints.emplace_back(static_cast<u32>(i));
			par_assoc[sc.particle2].stretch_shear_constraints.emplace_back(static_cast<u32>(i));
			crash_if(ori_assoc[sc.orientation].stretch_shear_constraint != std::numeric_limits<u32>::max());
			ori_assoc[sc.orientation].stretch_shear_constraint = static_cast<u32>(i);
		}

		// compute inertial positions
		std::vector<vec3> initial_positions;
		std::vector<vec3> inertial_positions;
		initial_positions.reserve(particles.size());
		inertial_positions.reserve(particles.size());
		for (const particle &p : particles) {
			initial_positions.emplace_back(p.state.position);
			inertial_positions.emplace_back(
				p.state.position + dt * (p.state.velocity + dt * physics_world->gravity)
			);
		}

		particle_prev_accelerations.resize(particles.size(), zero);

		// initial guess
		for (usize i = 0; i < particles.size(); ++i) {
			particle &p = particles[i];
			if (p.properties.inverse_mass <= 0.0f) {
				continue;
			}
			// compute adaptive acceleration
			const vec3 prev_accel = particle_prev_accelerations[i];
			const vec3 cur_accel = physics_world->gravity;
			const scalar len_factor = std::clamp(
				vec::dot(prev_accel, cur_accel) / cur_accel.squared_norm(), 0.0f, 1.0f
			);
			const vec3 adaptive_accel = cur_accel * len_factor;
			p.state.position += dt * (p.state.velocity + dt * adaptive_accel);
		}

		// iterations
		for (u32 iter = 0; iter < iters; ++iter) {
			// position update
			for (usize ip = 0; ip < particles.size(); ++ip) {
				particle &p = particles[ip];
				if (p.properties.inverse_mass <= 0.0f) {
					continue;
				}

				vec3 f = -1.0f / (dt * dt * p.properties.inverse_mass) * (p.state.position - inertial_positions[ip]);
				// the hessian for Cosserat rods simplifies to a multiple of the identity matrix
				scalar h = 1.0f / (dt * dt * p.properties.inverse_mass);
				for (const u32 ss_constraint_index : par_assoc[ip].stretch_shear_constraints) {
					const constraints::cosserat_rod::stretch_shear &constraint =
						rod_stretch_shear_constraints[ss_constraint_index];
					const scalar stiffness = constraint.stiffness * constraint.initial_length;
					const particle &p1 = particles[constraint.particle1];
					const particle &p2 = particles[constraint.particle2];
					const orientation &o = orientations[constraint.orientation];
					const vec3 c =
						(p2.state.position - p1.state.position) / constraint.initial_length -
						o.state.orientation.rotate(constraints::cosserat_rod::direction_basis);
					const scalar derivative_sign = ip == constraint.particle1 ? -1 : 1;
					f -= (derivative_sign * stiffness / constraint.initial_length) * c;
					h += stiffness / (constraint.initial_length * constraint.initial_length);
				}

				p.state.position += f / h;
			}

			// orientation update
			for (usize io = 0; io < orientations.size(); ++io) {
				orientation &ori = orientations[io];
				if (ori.inv_inertia <= 0.0f) {
					continue;
				}

				const _orientation_association &assoc = ori_assoc[io];
				crash_if(assoc.stretch_shear_constraint == std::numeric_limits<u32>::max());

				// stretching shearing part
				const constraints::cosserat_rod::stretch_shear &sc =
					rod_stretch_shear_constraints[assoc.stretch_shear_constraint];
				const vec3 v =
					-2.0f * sc.stiffness *
					(particles[sc.particle2].state.position - particles[sc.particle1].state.position);

				// bending part
				quats b = zero;
				for (const u32 bend_id : assoc.bend_constraints) {
					const constraints::cosserat_rod::bend_twist &bc = rod_bend_twist_constraints[bend_id];
					b += bc.stiffness * (
						io == bc.orientation1 ?
						orientations[bc.orientation2].state.orientation * bc.initial_bend :
						orientations[bc.orientation1].state.orientation * bc.initial_bend.conjugate()
					);
				}

				const scalar v2 = v.squared_norm();

				const scalar lambda = std::sqrt(v2) + b.magnitude();
				const quats qv = quat::from_vec3_xyz(v);
				const quats qe = quat::from_vec3_xyz(constraints::cosserat_rod::direction_basis);
				ori.state.orientation = quatu::normalize(qv * b * qe + lambda * b);
			}
		}

		// compute velocities
		for (usize ip = 0; ip < particles.size(); ++ip) {
			particle &p = particles[ip];
			const vec3 new_velocity = (p.state.position - initial_positions[ip]) / dt;
			particle_prev_accelerations[ip] = (new_velocity - p.state.velocity) / dt;
			p.state.velocity = new_velocity;
		}
	}
}
