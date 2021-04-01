#pragma once

/// \file
/// The PBD simulation engine.

#include <vector>

#include "shapes/shape.h"
#include "constraints/spring.h"
#include "body.h"

namespace pbd {
	/// Data associated with a single body.
	struct body {
		/// No initialization.
		body(uninitialized_t) {
		}

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
		cvec3d_t prev_position = uninitialized;
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

			for (std::size_t i = 0; i < iters; ++i) {
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
			}

			for (particle &p : particles) {
				p.state.velocity = (p.state.position - p.prev_position) / dt;
			}
		}

		/// Updates this physics engine using a fixed timestep.
		void update(double &dt, double timestep_dt, std::size_t iters) {
			while (dt >= timestep_dt) {
				timestep(timestep_dt, iters);
				dt -= timestep_dt;
			}
		}

		std::vector<body> bodies; ///< A list of bodies.
		std::vector<particle> particles; ///< A list of particles.

		std::vector<constraints::spring> spring_constraints; ///< A list of spring constraints.
		std::vector<double> spring_lambdas; ///< Lambda values for all spring constraints.
		cvec3d_t gravity = zero; ///< Gravity.
	};
}
