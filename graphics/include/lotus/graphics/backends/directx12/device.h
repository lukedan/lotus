#pragma once

/// \file
/// DirectX 12 devices and adapters.

#include <utility>
#include <vector>
#include <optional>

#include <d3d12.h>

#include "lotus/graphics/common.h"
#include "details.h"
#include "commands.h"
#include "descriptors.h"
#include "frame_buffer.h"
#include "pass.h"
#include "pipeline.h"
#include "resources.h"
#include "synchronization.h"

namespace lotus::graphics::backends::directx12 {
	class adapter;
	class context;
	class device;

	/// DirectX 12 device implementation.
	class device {
		friend adapter;
		friend context;
		friend _details::descriptor_heap;
		friend frame_buffer::~frame_buffer();
	protected:
		/// The capacity of \ref _rtv_descriptors and \ref _dsv_descriptors.
		constexpr static std::size_t descriptor_heap_size = 1024;

		/// Does not create a device.
		device(std::nullptr_t) {
		}

		/// Calls \p ID3D12Device::CreateCommandQueue().
		[[nodiscard]] command_queue create_command_queue();
		/// Calls \p ID3D12Device::CreateCommandAllocator().
		[[nodiscard]] command_allocator create_command_allocator();
		/// Calls \p ID3D12Device::CreateCommandList().
		[[nodiscard]] command_list create_command_list(command_allocator&);

		/// Fills out a \ref descriptor_set_layout object.
		[[nodiscard]] descriptor_set_layout create_descriptor_set_layout(
			std::span<const descriptor_range>, shader_stage_mask visible_stages
		);

		/// Calls \p D3D12SerializeVersionedRootSignature() to serialize the root signature, then calls
		/// \p ID3D12Device::CreateRootSignature() to create a \p ID3D12RootSignature.
		[[nodiscard]] pipeline_resources create_pipeline_resources(
			std::span<const graphics::descriptor_set_layout *const>
		);
		/// Calls \p ID3D12Device::CreateGraphicsPipelineState().
		[[nodiscard]] pipeline_state create_pipeline_state(
			pipeline_resources&,
			const shader *vertex_shader,
			const shader *pixel_shader,
			const shader *domain_shader,
			const shader *hull_shader,
			const shader *geometry_shader,
			const blend_options&,
			const rasterizer_options&,
			const depth_stencil_options&,
			std::span<const input_buffer_layout>,
			primitive_topology,
			const pass_resources&,
			std::size_t num_viewports
		);

		/// Fills in all fields in a \ref pass except for descriptors.
		[[nodiscard]] pass_resources create_pass_resources(
			std::span<const render_target_pass_options>, depth_stencil_pass_options
		);

		/// Calls \p ID3D12Device::CreateDescriptorHeap().
		[[nodiscard]] descriptor_pool create_descriptor_pool();

		/// Fills out a \ref shader object.
		[[nodiscard]] shader load_shader(std::span<const std::byte>);

		/// Calls \p ID3D12Device::CreateHeap().
		[[nodiscard]] device_heap create_device_heap(std::size_t size, heap_type);

		/// Calls \p ID3D12Device::CreateCommittedResource().
		[[nodiscard]] buffer create_committed_buffer(std::size_t size, heap_type, buffer_usage);
		/// Calls \p ID3D12Resource::Map().
		[[nodiscard]] void *map_buffer(buffer&, std::size_t begin, std::size_t length);
		/// Calls \p ID3D12Resource::Unmap().
		[[nodiscard]] void unmap_buffer(buffer&, std::size_t begin, std::size_t length);

		/// Fills out all fields in an \ref image2d_view.
		[[nodiscard]] image2d_view create_image2d_view_from(const image2d&, format, mip_levels);

		/// Fills out all fields in a \ref sampler.
		[[nodiscard]] sampler create_sampler(
			filtering minification, filtering magnification, filtering mipmapping,
			float mip_lod_bias, float min_lod, float max_lod, std::optional<float> max_anisotropy,
			sampler_address_mode addressing_u, sampler_address_mode addressing_v, sampler_address_mode addressing_w,
			linear_rgba_f border_color, std::optional<comparison_function> comparison
		);

		/// Fills out all fields in a \ref frame_buffer.
		[[nodiscard]] frame_buffer create_frame_buffer(
			std::span<const graphics::image2d_view*>, const image2d_view*, const pass_resources&
		);

		/// Calls \p ID3D12Device::CreateFence().
		[[nodiscard]] fence create_fence(synchronization_state);

		/// Calls \p ID3D12Fence::Signal() to reset the given fence to the unset state.
		void reset_fence(fence&);
		/// Calls \p ID3D12Fence::SetEventOnCompletion() to wait for the \ref fence to be set.
		void wait_for_fence(fence&);


		/// Calls \ref _set_debug_name().
		void set_debug_name(image &img, const char8_t *name) {
			_set_debug_name(*static_cast<_details::image*>(&img)->_image.Get(), name);
		}
	private:
		_details::com_ptr<ID3D12Device8> _device; ///< Pointer to the device.
		/// Heap used for allocating color descriptors.
		_details::descriptor_heap _rtv_descriptors = uninitialized;
		/// Heap used for allocating depth-stencil descriptors.
		_details::descriptor_heap _dsv_descriptors = uninitialized;

		/// Initializes this device.
		explicit device(_details::com_ptr<ID3D12Device8>);

		/// Calls \p ID3D12Object::SetPrivateData() to set the debug name of the given object.
		void _set_debug_name(ID3D12Object&, const char8_t*);
	};

	/// An adapter used for creating devices.
	class adapter {
		friend context;
	protected:
		/// Does not initialize \ref _adapter.
		adapter(std::nullptr_t) {
		}

		/// Calls \p D3D12CreateDevice().
		[[nodiscard]] device create_device();
		/// Returns the properties of this adapter.
		[[nodiscard]] adapter_properties get_properties() const;
	private:
		_details::com_ptr<IDXGIAdapter1> _adapter; ///< The adapter.
	};
}
