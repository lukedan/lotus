#include "pbd/engine.h"

/// \file
/// Implementation of the physics engine.

#include "pbd/algorithms/gjk_epa.h"

namespace pbd {
	void engine::timestep(double dt, std::size_t iters) {
		double dt2 = dt * dt;
		double inv_dt2 = 1.0 / dt2;

		for (particle &p : particles) {
			p.prev_position = p.state.position;
			if (p.properties.inverse_mass > 0.0) {
				p.state.velocity += dt * gravity;
			}
			p.state.position += dt * p.state.velocity;
		}
		for (body &b : bodies) {
			b.prev_position = b.state.position;
			if (b.properties.inverse_mass > 0.0) {
				b.state.linear_velocity += dt * gravity;
			}
			b.state.position += dt * b.state.linear_velocity;

			b.prev_rotation = b.state.rotation;
			// TODO external torque
			b.state.rotation = quat::unsafe_normalize(
				b.state.rotation + 0.5 * dt * quatd::from_vector(b.state.angular_velocity) * b.state.rotation
			);
		}

		// detect collisions
		contact_constraints.clear();
		// handle body collisions
		for (auto bi = bodies.begin(); bi != bodies.end(); ++bi) {
			auto bj = bi;
			for (++bj; bj != bodies.end(); ++bj) {
				if (auto res = detect_collision(*bi->body_shape, bi->state, *bj->body_shape, bj->state)) {
					contact_constraints.emplace_back(constraints::body_contact::create_for(
						*bi, *bj, res->contact1, res->contact2, res->normal
					));
				}
			}
		}

		// solve constraints
		contact_lambdas.resize(contact_constraints.size());
		std::fill(contact_lambdas.begin(), contact_lambdas.end(), std::make_pair(0.0, 0.0));

		spring_lambdas.resize(particle_spring_constraints.size());
		std::fill(spring_lambdas.begin(), spring_lambdas.end(), 0.0);

		face_lambdas.resize(face_constraints.size(), uninitialized);
		std::fill(face_lambdas.begin(), face_lambdas.end(), zero);

		bend_lambdas.resize(bend_constraints.size());
		std::fill(bend_lambdas.begin(), bend_lambdas.end(), 0.0);

		for (std::size_t i = 0; i < iters; ++i) {
			// project body contact constraints
			for (std::size_t j = 0; j < contact_constraints.size(); ++j) {
				contact_constraints[j].project(contact_lambdas[j].first, contact_lambdas[j].second);
			}

			// handle body-particle collisions
			for (const body &b : bodies) {
				if (b.properties.inverse_mass == 0.0) {
					for (particle &p : particles) {
						std::visit(
							[&](const auto &shape) {
								handle_shape_particle_collision(shape, b.state, p.state.position);
							},
							b.body_shape->value
						);
					}
				}
			}

			// project spring constraints
			for (std::size_t j = 0; j < particle_spring_constraints.size(); ++j) {
				const auto &s = particle_spring_constraints[j];
				particle &p1 = particles[s.particle1];
				particle &p2 = particles[s.particle2];
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
					inv_dt2, face_lambdas[j], face_constraint_projection_type
				);
			}

			// project bend constraints
			for (std::size_t j = 0; j < bend_constraints.size(); ++j) {
				constraints::bend &b = bend_constraints[j];
				particle &p1 = particles[b.particle_edge1];
				particle &p2 = particles[b.particle_edge2];
				particle &p3 = particles[b.particle3];
				particle &p4 = particles[b.particle4];
				b.project(
					p1.state.position, p2.state.position, p3.state.position, p4.state.position,
					p1.properties.inverse_mass, p2.properties.inverse_mass,
					p3.properties.inverse_mass, p4.properties.inverse_mass,
					inv_dt2, bend_lambdas[j]
				);
			}
		}

		for (particle &p : particles) {
			p.state.velocity = (p.state.position - p.prev_position) / dt;
		}
		for (body &b : bodies) {
			b.state.linear_velocity = (b.state.position - b.prev_position) / dt;
			auto dq = b.state.rotation * b.prev_rotation.inverse();
			b.state.angular_velocity = dq.axis() * (2.0 / dt);
			if (dq.w() < 0.0) {
				b.state.angular_velocity = -b.state.angular_velocity;
			}
		}
	}

	template <
		typename Shape1, typename Shape2
	> [[nodiscard]] std::optional<engine::collision_detection_result> engine::detect_collision(
		const Shape1&, const body_state&, const Shape2&, const body_state&
	) {
		return std::nullopt;
	}

	std::optional<engine::collision_detection_result> engine::detect_collision(
		const shape &s1, const body_state &st1, const shape &s2, const body_state &st2
	) {
		const shape *sa = &s1;
		const shape *sb = &s2;
		const body_state *sta = &st1;
		const body_state *stb = &st2;
		if (sa->get_type() > sb->get_type()) {
			auto res = std::visit([=](const auto &shapeb, const auto &shapea) {
				return detect_collision(shapeb, *stb, shapea, *sta);
			}, sb->value, sa->value);
			if (res) {
				std::swap(res->contact1, res->contact2);
				res->normal = -res->normal;
			}
			return res;
		} else {
			return std::visit([=](const auto &shapea, const auto &shapeb) {
				return detect_collision(shapea, *sta, shapeb, *stb);
			}, sa->value, sb->value);
		}
	}

	std::optional<engine::collision_detection_result> engine::detect_collision(
		const shapes::sphere&, const body_state&, const shapes::plane&, const body_state&
	) {
		return std::nullopt; // TODO
	}

	std::optional<engine::collision_detection_result> engine::detect_collision(
		const shapes::sphere&, const body_state&, const shapes::sphere&, const body_state&
	) {
		return std::nullopt; // TODO
	}

	std::optional<engine::collision_detection_result> engine::detect_collision(
		const shapes::plane&, const body_state &s1, const shapes::polyhedron &p2, const body_state &s2
	) {
		cvec3d norm_world = s1.rotation.rotate(cvec3d(0.0, 0.0, 1.0));
		cvec3d norm_local2 = s2.rotation.inverse().rotate(norm_world);
		cvec3d plane_pos = s2.rotation.inverse().rotate(s1.position - s2.position);
		double min_depth = 0.0;
		std::optional<cvec3d> contact;
		for (const auto &vert : p2.vertices) {
			double depth = vec::dot(vert - plane_pos, norm_local2);
			if (depth < min_depth) {
				min_depth = depth;
				contact = vert;
			}
		}
		if (contact) {
			cvec3d vert_world = s2.rotation.rotate(contact.value()) + s2.position;
			vert_world -= norm_world * min_depth;
			return engine::collision_detection_result::create(
				s1.rotation.inverse().rotate(vert_world - s1.position), contact.value(), norm_world
			);
		}
		return std::nullopt;
	}

	std::optional<engine::collision_detection_result> engine::detect_collision(
		const shapes::sphere&, const body_state&, const shapes::polyhedron&, const body_state&
	) {
		return std::nullopt; // TODO
	}

	std::optional<engine::collision_detection_result> engine::detect_collision(
		const shapes::polyhedron &p1, const body_state &s1, const shapes::polyhedron &p2, const body_state &s2
	) {
		auto bookmark = stack_allocator::scoped_bookmark::create();
		gjk_epa alg = gjk_epa::for_bodies(s1, p1, s2, p2);
		alg.center1 = s1.position;
		alg.orient1 = s1.rotation;
		alg.polyhedron1 = &p1;
		alg.center2 = s2.position;
		alg.orient2 = s2.rotation;
		alg.polyhedron2 = &p2;

		auto [intersect, state] = alg.gjk();
		if (!intersect) {
			return std::nullopt;
		}
		auto epa_res = alg.epa(state);

		engine::collision_detection_result result = uninitialized;
		result.normal = epa_res.normal;
		bool face_p1 =
			epa_res.vertices[0].index2 == epa_res.vertices[1].index2 &&
			epa_res.vertices[0].index2 == epa_res.vertices[2].index2;
		bool face_p2 =
			epa_res.vertices[0].index1 == epa_res.vertices[1].index1 &&
			epa_res.vertices[0].index1 == epa_res.vertices[2].index1;
		if (face_p1) { // a vertex from p2 and a face from p1
			result.contact2 = p2.vertices[epa_res.vertices[0].index2];
			cvec3d contact1 = s2.rotation.rotate(result.contact2) + s2.position + epa_res.depth * epa_res.normal;
			result.contact1 = s1.rotation.inverse().rotate(contact1 - s1.position);
		} else if (face_p2) { // a vertex from p1 and a face from p2
			result.contact1 = p1.vertices[epa_res.vertices[0].index1];
			cvec3d contact2 = s1.rotation.rotate(result.contact1) + s1.position - epa_res.depth * epa_res.normal;
			result.contact2 = s2.rotation.inverse().rotate(contact2 - s2.position);
		} else { // two edges
			std::array<cvec3d, 3> spx_pos = epa_res.simplex_positions;
			std::array<gjk_epa::simplex_vertex, 3> spx_id = epa_res.vertices;

			// adjust the arrays so that the two edges can be easily determined
			if (spx_id[0].index1 != spx_id[1].index1 && spx_id[0].index2 != spx_id[1].index2) {
				// spx_id[2] is the common vertex
				std::swap(spx_pos[0], spx_pos[2]);
				std::swap(spx_id[0], spx_id[2]);
			} else if (spx_id[0].index1 != spx_id[2].index1 && spx_id[0].index2 != spx_id[2].index2) {
				// spx_id[1] is the common vertex
				std::swap(spx_pos[0], spx_pos[1]);
				std::swap(spx_id[0], spx_id[1]);
			}
			if (spx_id[0].index1 == spx_id[1].index1) {
				std::swap(spx_pos[1], spx_pos[2]);
				std::swap(spx_id[1], spx_id[2]);
			}

			// solve for barycentric coordinates
			cvec3d diff12 = spx_pos[1] - spx_pos[0]; // also the x axis
			cvec3d diff13 = spx_pos[2] - spx_pos[0];
			cvec3d y = vec::cross(epa_res.normal, diff12);
			auto xform = matd::concat_columns(diff12 / diff12.squared_norm(), y / y.squared_norm()).transposed();
			cvec2d pos1 = xform * diff13;
			cvec3d contact_offset = epa_res.depth * epa_res.normal - spx_pos[0];
			cvec2d contact = xform * contact_offset;
			// [cx] = [1 px][bx]
			// [cy]   [0 py][by]
			//
			// [bx] = 1 / py [py -px][cx] = [cx - px * cy / py]
			// [by]          [0   1 ][cy]   [     cy / py     ]
			double y_ratio = contact[1] / pos1[1];
			cvec2d barycentric(contact[0] - pos1[0] * y_ratio, y_ratio);

			cvec3d local_contact2 =
				p2.vertices[spx_id[0].index2] * (1.0 - barycentric[1]) +
				p2.vertices[spx_id[2].index2] * barycentric[1];
			cvec3d local_contact1 =
				p1.vertices[spx_id[0].index1] * (1.0 - barycentric[0]) +
				p1.vertices[spx_id[1].index1] * barycentric[0];
			result.contact1 = local_contact1;
			result.contact2 = local_contact2;
		}
		return result;
	}

	bool engine::handle_shape_particle_collision(
		const shapes::plane&, const body_state &state, cvec3d &pos
	) {
		cvec3d plane_pos = state.rotation.inverse().rotate(pos - state.position);
		if (plane_pos[2] < 0.0) {
			plane_pos[2] = 0.0;
			pos = state.rotation.rotate(plane_pos) + state.position;
			return true;
		}
		return false;
	}

	bool engine::handle_shape_particle_collision(
		const shapes::sphere &shape, const body_state &state, cvec3d &pos
	) {
		auto diff = pos - (state.position + state.rotation.rotate(shape.offset));
		double sqr_dist = diff.squared_norm();
		if (sqr_dist < shape.radius * shape.radius) {
			pos = state.position + diff * (shape.radius / std::sqrt(sqr_dist));
			return true;
		}
		return false;
	}

	bool engine::handle_shape_particle_collision(
		const shapes::polyhedron&, const body_state&, cvec3d&
	) {
		// TODO
		return false;
	}
}
