#pragma once

/// \file
/// Command related classes.

#include LOTUS_GRAPHICS_BACKEND_INCLUDE

namespace lotus::graphics {
	class device;

	/// Used for allocating commands.
	class command_allocator : public backend::command_allocator {
		friend device;
	public:
		/// No copy construction.
		command_allocator(const command_allocator&) = delete;
		/// No copy assignment.
		command_allocator &operator=(const command_allocator&) = delete;
	protected:
		/// Implicit conversion from base type.
		command_allocator(backend::command_allocator base) : backend::command_allocator(std::move(base)) {
		}
	};
}
