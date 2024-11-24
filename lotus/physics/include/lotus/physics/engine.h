#pragma once

/// \file
/// The PBD simulation engine.

#include <vector>
#include <list>
#include <deque>
#include <optional>

#include "lotus/collision/shape.h"
#include "constraints/spring.h"
#include "constraints/contact.h"
#include "constraints/face.h"
#include "constraints/bend.h"
#include "body.h"

namespace lotus::physics {
	/// The PBD simulation engine.
	class engine {
	public:
		/// Result of collision detection.
		struct collision_detection_result {
			/// No initialization.
			collision_detection_result(uninitialized_t) {
			}
			/// Creates a new object.
			[[nodiscard]] inline static collision_detection_result create(vec3 c1, vec3 c2, vec3 n) {
				collision_detection_result result = uninitialized;
				result.contact1 = c1;
				result.contact2 = c2;
				result.normal = n;
				return result;
			}

			vec3 contact1 = uninitialized; ///< Contact point on the first object in local space.
			vec3 contact2 = uninitialized; ///< Contact point on the second object in local space.
			/// Normalized contact normal. There's no guarantee of its direction.
			vec3 normal = uninitialized;
		};

		/// Executes one time step with the given delta time in seconds and the given number of iterations.
		void timestep(scalar dt, std::uint32_t iters);


		/// Detects collision between two generic shapes.
		[[nodiscard]] static std::optional<collision_detection_result> detect_collision(
			const collision::shape&, const body_state&, const collision::shape&, const body_state&
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
			const collision::shapes::sphere&, const body_state&, const collision::shapes::plane&, const body_state&
		);
		/// Detects collision between two spheres.
		[[nodiscard]] static std::optional<collision_detection_result> detect_collision(
			const collision::shapes::sphere&, const body_state&, const collision::shapes::sphere&, const body_state&
		);
		/// Detects collision between a plane and a polyhedron.
		[[nodiscard]] static std::optional<collision_detection_result> detect_collision(
			const collision::shapes::plane&, const body_state&,
			const collision::shapes::polyhedron&, const body_state&
		);
		/// Detects collision between a sphere and a polyhedron.
		[[nodiscard]] static std::optional<collision_detection_result> detect_collision(
			const collision::shapes::sphere&, const body_state&,
			const collision::shapes::polyhedron&, const body_state&
		);
		/// Detects collision between two polyhedra.
		[[nodiscard]] static std::optional<collision_detection_result> detect_collision(
			const collision::shapes::polyhedron&, const body_state&,
			const collision::shapes::polyhedron&, const body_state&
		);

		/// Handles the collision between a plane and a particle.
		static bool handle_shape_particle_collision(const collision::shapes::plane&, const body_state&, vec3&);
		/// Handles the collision between a kinematic sphere and a particle.
		static bool handle_shape_particle_collision(const collision::shapes::sphere&, const body_state&, vec3&);
		/// Handles the collision between a kinematic polyhedron and a particle.
		static bool handle_shape_particle_collision(const collision::shapes::polyhedron&, const body_state&, vec3&);


		/// The list of shapes. This provides a convenient place to store shapes, but the user can store shapes
		/// anywhere.
		std::deque<collision::shape> shapes;
		std::list<body> bodies; ///< The list of bodies.

		std::vector<particle> particles; ///< The list of particles.

		std::vector<constraints::particle_spring> particle_spring_constraints; ///< The list of spring constraints.
		std::vector<scalar> spring_lambdas; ///< Lambda values for all spring constraints.

		/// Determines how face constraints are projected.
		constraints::face::projection_type face_constraint_projection_type =
			constraints::face::projection_type::gauss_seidel;
		std::vector<constraints::face> face_constraints; ///< The list of face constraints.
		std::vector<column_vector<6, scalar>> face_lambdas; ///< Lambda values for all face constraints.

		std::vector<constraints::bend> bend_constraints; ///< The list of bend constraints.
		std::vector<scalar> bend_lambdas; ///< Lambda values for bend constraints.

		std::deque<constraints::body_contact> contact_constraints; ///< Contact constraints.
		std::vector<std::pair<scalar, scalar>> contact_lambdas; ///< Lambda values for contact constraints.

		vec3 gravity = zero; ///< Gravity.
	};
}
