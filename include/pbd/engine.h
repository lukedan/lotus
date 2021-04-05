#pragma once

/// \file
/// The PBD simulation engine.

#include <vector>

#include "shapes/shape.h"
#include "constraints/spring.h"
#include "constraints/face.h"
#include "body.h"

namespace pbd {
	/// Data associated with a single body.
	struct body {
		/// No initialization.
		body(uninitialized_t) {
		}

		shape body_shape; ///< The shape of this body.
		body_properties properties = uninitialized; ///< The properties of this body.
		body_state state = uninitialized; ///< The state of this body.
	};
	/// Data associated with a single particle.
	struct particle {
		/// No initialization.
		particle(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		particle(particle_properties props, particle_state st) :
			properties(props), state(st), prev_position(st.position) {
		}

		particle_properties properties = uninitialized; ///< The mass of this particle.
		particle_state state = uninitialized; ///< The state of this particle.
		cvec3d prev_position = uninitialized; ///< Position in the previous timestep.
	};


	/// The PBD simulation engine.
	class engine {
	public:
		/// Executes one timestep with the given delta time in seconds and the given number of iterations.
		void timestep(double dt, std::size_t iters) {
			double dt2 = dt * dt;
			double inv_dt2 = 1.0 / dt2;

			for (particle &p : particles) {
				p.prev_position = p.state.position;
				p.state.position = p.prev_position + dt * p.state.velocity;
				if (p.properties.inverse_mass > 0.0) {
					p.state.position += dt2 * gravity;
				}
			}

			spring_lambdas.resize(spring_constraints.size());
			std::fill(spring_lambdas.begin(), spring_lambdas.end(), 0.0);

			face_lambdas.resize(face_constraints.size(), uninitialized);
			std::fill(face_lambdas.begin(), face_lambdas.end(), zero);

			for (std::size_t i = 0; i < iters; ++i) {
				// handle collisions
				for (const body &b : bodies) {
					if (b.properties.inverse_mass == 0.0) {
						for (particle &p : particles) {
							std::visit(
								[&](const auto &shape) {
									_handle_shape_particle_collision(shape, b.state, p.state.position);
								},
								b.body_shape.value
							);
						}
					}
				}

				// project spring constraints
				for (std::size_t j = 0; j < spring_constraints.size(); ++j) {
					const constraints::spring &s = spring_constraints[j];
					particle &p1 = particles[s.object1];
					particle &p2 = particles[s.object2];
					s.project(
						p1.state.position, p2.state.position,
						p1.properties.inverse_mass, p2.properties.inverse_mass,
						inv_dt2, spring_lambdas[j]
					);
				}

				// project face constraints
				for (std::size_t j = 0; j < face_constraints.size(); ++j) {
					constraints::face &f = face_constraints[j];
					particle &p1 = particles[f.particle1];
					particle &p2 = particles[f.particle2];
					particle &p3 = particles[f.particle3];
					f.project(
						p1.state.position, p2.state.position, p3.state.position,
						p1.properties.inverse_mass, p2.properties.inverse_mass, p3.properties.inverse_mass,
						inv_dt2, face_lambdas[j]
					);
				}
			}

			for (particle &p : particles) {
				p.state.velocity = (p.state.position - p.prev_position) / dt;
			}
		}

		std::vector<body> bodies; ///< The list of bodies.
		std::vector<particle> particles; ///< The list of particles.

		std::vector<constraints::spring> spring_constraints; ///< The list of spring constraints.
		std::vector<double> spring_lambdas; ///< Lambda values for all spring constraints.

		std::vector<constraints::face> face_constraints; ///< The list of face constraints.
		std::vector<column_vector<6, double>> face_lambdas; ///< Lambda values for all face constraints.

		cvec3d gravity = zero; ///< Gravity.
	protected:
		/// Handles the collision between a plane and a particle.
		bool _handle_shape_particle_collision(const shapes::plane&, const body_state &state, cvec3d &pos) {
			cvec3d plane_pos = state.rotation.inverse().rotate(pos - state.position);
			if (plane_pos[2] < 0.0) {
				plane_pos[2] = 0.0;
				pos = state.rotation.rotate(plane_pos) + state.position;
				return true;
			}
			return false;
		}
		/// Handles the collision between a kinematic sphere and a particle.
		bool _handle_shape_particle_collision(const shapes::sphere &shape, const body_state &state, cvec3d &pos) {
			auto diff = pos - state.position;
			double sqr_dist = diff.squared_norm();
			if (sqr_dist < shape.radius * shape.radius) {
				pos = state.position + diff * (shape.radius / std::sqrt(sqr_dist));
				return true;
			}
			return false;
		}
	};
}
