#pragma once

/// \file
/// Device-related classes.

#include "lotus/common.h"
#include LOTUS_GRAPHICS_BACKEND_INCLUDE
#include "commands.h"

namespace lotus::graphics {
	class context;
	class adapter;
	class device;

	/// A command queue.
	class command_queue : public backend::command_queue {
		friend device;
	public:
		/// No copy construction.
		command_queue(const command_queue&) = delete;
		/// No copy assignment.
		command_queue &operator=(const command_queue&) = delete;
	protected:
		/// Initializes the backend command queue.
		command_queue(backend::command_queue q) : backend::command_queue(std::move(q)) {
		}
	};

	/// Interface to the graphics device.
	class device : public backend::device {
		friend adapter;
	public:
		/// Does not create a device.
		device(std::nullptr_t) : backend::device(nullptr) {
		}
		/// No copy construction.
		device(const device&) = delete;
		/// Move construction.
		device(device &&src) : backend::device(std::move(src)) {
		}
		/// No copy assignment.
		device &operator=(const device&) = delete;
		/// Move assignment.
		device &operator=(device &&src) {
			backend::device::operator=(std::move(src));
			return *this;
		}

		/// Creates a \ref command_queue.
		[[nodiscard]] command_queue create_command_queue() {
			return backend::device::create_command_queue();
		}
		/// Creates a \ref command_allocator.
		[[nodiscard]] command_allocator create_command_allocator() {
			return backend::device::create_command_allocator();
		}
	protected:
		/// Creates a device from a backend device.
		device(backend::device d) : backend::device(std::move(d)) {
		}
	};

	/// Represents a generic interface to an adapter that a device can be created from.
	class adapter : public backend::adapter {
		friend context;
	public:
		/// Creates an empty adapter.
		adapter(std::nullptr_t) : backend::adapter(nullptr) {
		}
		/// No copy construction.
		adapter(const adapter&) = delete;
		/// Move construction.
		adapter(adapter &&src) : backend::adapter(std::move(src)) {
		}
		/// No copy assignment.
		adapter &operator=(const adapter&) = delete;

		/// Creates a device that uses this adapter.
		[[nodiscard]] device create_device() {
			return backend::adapter::create_device();
		}
		/// Retrieves information about this adapter.
		[[nodiscard]] adapter_properties get_properties() const {
			return backend::adapter::get_properties();
		}
	protected:
		/// Creates an adapter from a backend adapter.
		adapter(backend::adapter a) : backend::adapter(std::move(a)) {
		}
	};
}
