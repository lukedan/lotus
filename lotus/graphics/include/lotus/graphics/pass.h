#pragma once

/// \file
/// Render passes.

#include LOTUS_GRAPHICS_BACKEND_INCLUDE

namespace lotus::graphics {
	class device;

	/// Describes the behavior regarding the input/output of a render pass.
	class pass_resources : public backend::pass_resources {
		friend device;
	public:
		/// Creates an empty object.
		pass_resources(std::nullptr_t) : backend::pass_resources(nullptr) {
		}
		/// Move constructor.
		pass_resources(pass_resources &&src) : backend::pass_resources(std::move(src)) {
		}
		/// No copy construction.
		pass_resources(const pass_resources&) = delete;
		/// Move assignment.
		pass_resources &operator=(pass_resources &&src) {
			backend::pass_resources::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		pass_resources &operator=(const pass_resources&) = delete;
	protected:
		/// Initializes the base class.
		pass_resources(backend::pass_resources base) : backend::pass_resources(std::move(base)) {
		}
	};
}
