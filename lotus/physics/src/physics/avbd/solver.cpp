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
			const constraints::cosserat_rod::stretch_shear &sc =
				rod_stretch_shear_constraints[i];
			par_assoc[sc.particle1].stretch_shear_constraints.emplace_back(static_cast<u32>(i));
			par_assoc[sc.particle2].stretch_shear_constraints.emplace_back(static_cast<u32>(i));
			crash_if(ori_assoc[sc.orientation].stretch_shear_constraint != std::numeric_limits<u32>::max());
			ori_assoc[sc.orientation].stretch_shear_constraint = static_cast<u32>(i);
		}

		// position estimation
		for (particle &p : particles) {
			p.state.position += dt * (p.state.velocity + dt * physics_world->gravity);
		}

		// iterations
		for (u32 iter = 0; iter < iters; ++iter) {
			// position update
			// TODO

			// orientation update
			for (usize io = 0; io < orientations.size(); ++io) {
				orientation &ori = orientations[io];
				const _orientation_association &assoc = ori_assoc[io];
				crash_if(assoc.stretch_shear_constraint == std::numeric_limits<u32>::max());

				// stretching shearing part
				const constraints::cosserat_rod::stretch_shear &sc =
					rod_stretch_shear_constraints[assoc.stretch_shear_constraint];
				const vec3 v =
					-2.0f * sc.stiffness / sc.initial_length *
					(particles[sc.particle2].prev_position - particles[sc.particle1].prev_position);

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
	}
}
