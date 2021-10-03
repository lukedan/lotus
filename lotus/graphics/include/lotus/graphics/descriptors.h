#pragma once

/// \file
/// Descriptor pools.

#include LOTUS_GRAPHICS_BACKEND_INCLUDE

namespace lotus::graphics {
	class device;


	/// A pool of descriptors that descriptor sets can be allocated from.
	class descriptor_pool : public backend::descriptor_pool {
		friend device;
	public:
		/// No copy construction.
		descriptor_pool(const descriptor_pool&) = delete;
		/// No copy assignment.
		descriptor_pool &operator=(const descriptor_pool&) = delete;
	protected:
		/// Initializes the base type.
		descriptor_pool(backend::descriptor_pool base) : backend::descriptor_pool(std::move(base)) {
		}
	};

	/// Specifies the layout of a descriptor set.
	class descriptor_set_layout : public backend::descriptor_set_layout {
		friend device;
	public:
		/// Creates an invalid object.
		descriptor_set_layout(std::nullptr_t) : backend::descriptor_set_layout(nullptr) {
		}
		/// Move construction.
		descriptor_set_layout(descriptor_set_layout &&src) : backend::descriptor_set_layout(std::move(src)) {
		}
		/// No copy construction.
		descriptor_set_layout(const descriptor_set_layout&) = delete;
		/// Move assignment.
		descriptor_set_layout &operator=(descriptor_set_layout &&src) {
			backend::descriptor_set_layout::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		descriptor_set_layout &operator=(const descriptor_set_layout&) = delete;
	protected:
		/// Initializes the base type.
		descriptor_set_layout(backend::descriptor_set_layout base) :
			backend::descriptor_set_layout(std::move(base)) {
		}
	};

	/// A set of descriptors.
	class descriptor_set : public backend::descriptor_set {
		friend device;
	public:
		/// Creates an empty object.
		descriptor_set(std::nullptr_t) : backend::descriptor_set(nullptr) {
		}
		/// Move constructor.
		descriptor_set(descriptor_set &&src) noexcept : backend::descriptor_set(std::move(src)) {
		}
		/// No copy construction.
		descriptor_set(const descriptor_set&) = delete;
		/// Move assignment.
		descriptor_set &operator=(descriptor_set &&src) noexcept {
			backend::descriptor_set::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		descriptor_set &operator=(const descriptor_set&) = delete;
	protected:
		/// Initializes the base type.
		descriptor_set(backend::descriptor_set base) : backend::descriptor_set(std::move(base)) {
		}
	};
}
