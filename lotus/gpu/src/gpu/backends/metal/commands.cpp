#include "lotus/gpu/backends/metal/commands.h"

/// \file
/// Implementation of Metal command buffers.

#include <Metal/Metal.hpp>

namespace lotus::gpu::backends::metal {
	command_queue::command_queue(command_queue&&) noexcept = default;

	command_queue::command_queue(const command_queue&) = default;

	command_queue &command_queue::operator=(command_queue&&) noexcept = default;

	command_queue &command_queue::operator=(const command_queue&) = default;

	command_queue::~command_queue() = default;

	command_queue::command_queue(_details::metal_ptr<MTL::CommandQueue> q) : _q(std::move(q)) {
	}
}
