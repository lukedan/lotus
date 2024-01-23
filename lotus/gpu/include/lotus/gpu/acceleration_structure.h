#pragma once

/// \file
/// Ray-tracing acceleration structures.

#include LOTUS_GPU_BACKEND_INCLUDE

namespace lotus::gpu {
	class bottom_level_acceleration_structure;
	class device;


	/// Contains geometry description for bottom-level acceleration structures.
	class bottom_level_acceleration_structure_geometry :
		public backend::bottom_level_acceleration_structure_geometry {
		friend device;
	public:
		/// Initializes this object to empty.
		bottom_level_acceleration_structure_geometry(std::nullptr_t) :
			backend::bottom_level_acceleration_structure_geometry(nullptr) {
		}
		/// Move construction.
		bottom_level_acceleration_structure_geometry(bottom_level_acceleration_structure_geometry &&src) :
			backend::bottom_level_acceleration_structure_geometry(std::move(src)) {
		}
		/// No copy construction.
		bottom_level_acceleration_structure_geometry(const bottom_level_acceleration_structure_geometry&) = delete;
		/// Move assignment.
		bottom_level_acceleration_structure_geometry &operator=(bottom_level_acceleration_structure_geometry &&src) {
			backend::bottom_level_acceleration_structure_geometry::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		bottom_level_acceleration_structure_geometry &operator=(
			const bottom_level_acceleration_structure_geometry&
		) = delete;
	protected:
		/// Initializes the base object.
		bottom_level_acceleration_structure_geometry(backend::bottom_level_acceleration_structure_geometry &&src) :
			backend::bottom_level_acceleration_structure_geometry(std::move(src)) {
		}
	};

	/// Describes an instance in a top-level acceleration structure. This struct is used to directly describe one
	/// instance in GPU memory.
	class instance_description : public backend::instance_description {
		friend device;
	public:
		/// No initialization.
		instance_description(uninitialized_t) : backend::instance_description(uninitialized) {
		}
	protected:
		/// Initializes the base object.
		instance_description(backend::instance_description &&desc) : backend::instance_description(std::move(desc)) {
		}
	};
	static_assert(
		sizeof(instance_description) == sizeof(backend::instance_description),
		"Incorrect instance_description size"
	);
	static_assert(
		std::is_standard_layout_v<instance_description> && std::is_trivially_copyable_v<instance_description>,
		"instance_description does not match the type requirements"
	);

	/// Handle of a bottom-level acceleration structure.
	class bottom_level_acceleration_structure : public backend::bottom_level_acceleration_structure {
		friend device;
	public:
		/// Creates an empty acceleration structure.
		bottom_level_acceleration_structure(std::nullptr_t) : backend::bottom_level_acceleration_structure(nullptr) {
		}
		/// Move construction.
		bottom_level_acceleration_structure(bottom_level_acceleration_structure &&src) :
			backend::bottom_level_acceleration_structure(std::move(src)) {
		}
		/// No copy construction.
		bottom_level_acceleration_structure(const bottom_level_acceleration_structure&) = delete;
		/// Move assignment.
		bottom_level_acceleration_structure &operator=(bottom_level_acceleration_structure &&src) {
			backend::bottom_level_acceleration_structure::operator=(std::move(src));
			return *this;
		}
		/// No copy construction.
		bottom_level_acceleration_structure &operator=(const bottom_level_acceleration_structure&) = delete;

		/// Returns whether this object is valid.
		[[nodiscard]] bool is_valid() const {
			return backend::bottom_level_acceleration_structure::is_valid();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	protected:
		/// Initializes the base object.
		bottom_level_acceleration_structure(backend::bottom_level_acceleration_structure &&src) :
			backend::bottom_level_acceleration_structure(std::move(src)) {
		}
	};

	/// Handle of a top-level acceleration structure.
	class top_level_acceleration_structure : public backend::top_level_acceleration_structure {
		friend device;
	public:
		/// Creates an empty acceleration structure.
		top_level_acceleration_structure(std::nullptr_t) : backend::top_level_acceleration_structure(nullptr) {
		}
		/// Move constructor.
		top_level_acceleration_structure(top_level_acceleration_structure &&src) :
			backend::top_level_acceleration_structure(std::move(src)) {
		}
		/// No copy construction.
		top_level_acceleration_structure(const top_level_acceleration_structure&) = delete;
		/// Move assignment.
		top_level_acceleration_structure &operator=(top_level_acceleration_structure &&src) {
			backend::top_level_acceleration_structure::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		top_level_acceleration_structure &operator=(const top_level_acceleration_structure&) = delete;

		/// Returns whether this object is valid.
		[[nodiscard]] bool is_valid() const {
			return backend::top_level_acceleration_structure::is_valid();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	protected:
		/// Initializes the base object.
		top_level_acceleration_structure(backend::top_level_acceleration_structure &&src) :
			backend::top_level_acceleration_structure(std::move(src)) {
		}
	};
}
