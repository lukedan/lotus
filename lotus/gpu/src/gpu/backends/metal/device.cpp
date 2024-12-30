#include "lotus/gpu/backends/metal/device.h"

/// \file
/// Implementation of Metal devices.

#include <Metal/Metal.hpp>

namespace lotus::gpu::backends::metal {
	device::device(device&&) noexcept = default;

	device &device::operator=(device&&) noexcept = default;

	device::~device() = default;

	device::device(std::nullptr_t) {
	}

	device::device(_details::metal_ptr<MTL::Device> dev) : _dev(std::move(dev)) {
	}


	adapter::adapter(adapter&&) noexcept = default;

	adapter::adapter(const adapter&) = default;

	adapter &adapter::operator=(adapter&&) noexcept = default;

	adapter &adapter::operator=(const adapter&) = default;

	adapter::~adapter() = default;

	adapter::adapter(std::nullptr_t) {
	}

	std::pair<device, std::vector<command_queue>> adapter::create_device(
		std::span<const queue_family> families
	) {
		std::vector<command_queue> queues;
		queues.reserve(families.size());
		for (queue_family fam : families) {
			auto ptr = _details::take_ownership(_dev->newCommandQueue());
			queues.emplace_back(command_queue(std::move(ptr)));
		}
		std::pair<device, std::vector<command_queue>> res2(device(_dev), std::move(queues));
		return { device(_dev), std::move(queues) };
	}

	adapter::adapter(_details::metal_ptr<MTL::Device> dev) : _dev(std::move(dev)) {
	}
}
