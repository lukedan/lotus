#pragma once

/// \file
/// The PBD simulation engine.

#include <vector>
#include <list>
#include <deque>
#include <optional>

#include "pbd/shapes/shape.h"
#include "pbd/constraints/spring.h"
#include "pbd/constraints/face.h"
#include "pbd/constraints/bend.h"
#include "pbd/body.h"

namespace pbd {
	/// The PBD simulation engine.
	class engine {
	public:
		/// Result of collision detection.
		struct collision_detection_result {
			/// No initialization.
			collision_detection_result(uninitialized_t) {
			}
			/// Creates a new object.
			[[nodiscard]] inline static collision_detection_result create(cvec3d c1, cvec3d c2, cvec3d n) {
				collision_detection_result result = uninitialized;
				result.contact1 = c1;
				result.contact2 = c2;
				result.normal = n;
				return result;
			}

			cvec3d contact1 = uninitialized; ///< Contact point on the first object in world space.
			cvec3d contact2 = uninitialized; ///< Contact point on the second object in world space.
			/// Normalized contact normal. There's no guarantee of its direction.
			cvec3d normal = uninitialized;
		};

		/// Executes one time step with the given delta time in seconds and the given number of iterations.
		void timestep(double dt, std::size_t iters);


		/// Detects collision between two generic shapes.
		[[nodiscard]] static std::optional<collision_detection_result> detect_collision(
			const shape&, const body_state&, const shape&, const body_state&
		);
		/// Fallback case for collision detection between generic shapes - this always returns \p std::nullopt and
		/// should only be used internally.
		template <
			typename Shape1, typename Shape2
		> [[nodiscard]] static std::optional<engine::collision_detection_result> detect_collision(
			const Shape1&, const body_state&, const Shape2&, const body_state&
		);
		/// Detects collision between a sphere and a plane.
		[[nodiscard]] static std::optional<collision_detection_result> detect_collision(
			const shapes::sphere&, const body_state&, const shapes::plane&, const body_state&
		);
		/// Detects collision between two spheres.
		[[nodiscard]] static std::optional<collision_detection_result> detect_collision(
			const shapes::sphere&, const body_state&, const shapes::sphere&, const body_state&
		);
		/// Detects collision between a plane and a polyhedron.
		[[nodiscard]] static std::optional<collision_detection_result> detect_collision(
			const shapes::plane&, const body_state&, const shapes::polyhedron&, const body_state&
		);
		/// Detects collision between a sphere and a polyhedron.
		[[nodiscard]] static std::optional<collision_detection_result> detect_collision(
			const shapes::sphere&, const body_state&, const shapes::polyhedron&, const body_state&
		);
		/// Detects collision between two polyhedra.
		[[nodiscard]] static std::optional<collision_detection_result> detect_collision(
			const shapes::polyhedron&, const body_state&, const shapes::polyhedron&, const body_state&
		);

		/// Handles the collision between a plane and a particle.
		static bool handle_shape_particle_collision(const shapes::plane&, const body_state&, cvec3d&);
		/// Handles the collision between a kinematic sphere and a particle.
		static bool handle_shape_particle_collision(const shapes::sphere&, const body_state&, cvec3d&);
		/// Handles the collision between a kinematic polyhedron and a particle.
		static bool handle_shape_particle_collision(const shapes::polyhedron&, const body_state&, cvec3d&);


		/// The list of shapes. This provides a convenient place to store shapes, but the user can store shapes
		/// anywhere.
		std::deque<shape> shapes;
		std::list<body> bodies; ///< The list of bodies.

		std::vector<particle> particles; ///< The list of particles.

		std::vector<constraints::particle_spring> particle_spring_constraints; ///< The list of spring constraints.
		std::vector<double> spring_lambdas; ///< Lambda values for all spring constraints.

		/// Determines how face constraints are projected.
		constraints::face::projection_type face_constraint_projection_type =
			constraints::face::projection_type::gauss_seidel;
		std::vector<constraints::face> face_constraints; ///< The list of face constraints.
		std::vector<column_vector<6, double>> face_lambdas; ///< Lambda values for all face constraints.

		std::vector<constraints::bend> bend_constraints; ///< The list of bend constraints.
		std::vector<double> bend_lambdas; ///< Lambda values for bend constraints.

		std::deque<constraints::body_contact> contact_constraints; ///< Contact constraints.
		std::vector<double> contact_lambdas; ///< Lambda values for contact constraints.

		cvec3d gravity = zero; ///< Gravity.
	};
}
