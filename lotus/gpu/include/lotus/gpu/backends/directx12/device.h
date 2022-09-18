#pragma once

/// \file
/// DirectX 12 devices and adapters.

#include <utility>
#include <vector>
#include <optional>

#include "details.h"

#include "lotus/gpu/common.h"
#include "acceleration_structure.h"
#include "commands.h"
#include "descriptors.h"
#include "frame_buffer.h"
#include "pipeline.h"
#include "resources.h"
#include "synchronization.h"

namespace lotus::gpu::backends::directx12 {
	class adapter;
	class context;
	class device;

	/// DirectX 12 device implementation.
	class device {
		friend adapter;
		friend context;
		friend void frame_buffer::_free();
		friend void descriptor_set::_free();
	protected:
		/// The capacity of \ref _rtv_descriptors and \ref _dsv_descriptors.
		constexpr static std::size_t descriptor_heap_size = 65535;
		constexpr static std::size_t sampler_heap_size = 2048;

		/// Does not create a device.
		device(std::nullptr_t) {
		}

		/// Calls \p IDXGISwapChain3::GetCurrentBackBufferIndex().
		[[nodiscard]] back_buffer_info acquire_back_buffer(swap_chain&);
		/// Calls \p IDXGISwapChain3::ResizeBuffers().
		void resize_swap_chain_buffers(swap_chain&, cvec2s);

		/// Calls \p ID3D12Device::CreateCommandQueue().
		[[nodiscard]] command_queue create_command_queue();
		/// Calls \p ID3D12Device::CreateCommandAllocator().
		[[nodiscard]] command_allocator create_command_allocator();
		/// Calls \p ID3D12Device::CreateCommandList().
		[[nodiscard]] command_list create_and_start_command_list(command_allocator&);

		/// Fills out a \ref descriptor_set_layout object.
		[[nodiscard]] descriptor_set_layout create_descriptor_set_layout(
			std::span<const descriptor_range_binding>, shader_stage visible_stages
		);

		/// Calls \p D3D12SerializeVersionedRootSignature() to serialize the root signature, then calls
		/// \p ID3D12Device::CreateRootSignature() to create a \p ID3D12RootSignature.
		[[nodiscard]] pipeline_resources create_pipeline_resources(
			std::span<const gpu::descriptor_set_layout *const>
		);
		/// Calls \p ID3D12Device::CreateGraphicsPipelineState().
		[[nodiscard]] graphics_pipeline_state create_graphics_pipeline_state(
			const pipeline_resources&,
			const shader_binary *vertex_shader,
			const shader_binary *pixel_shader,
			const shader_binary *domain_shader,
			const shader_binary *hull_shader,
			const shader_binary *geometry_shader,
			std::span<const render_target_blend_options>,
			const rasterizer_options&,
			const depth_stencil_options&,
			std::span<const input_buffer_layout>,
			primitive_topology,
			const frame_buffer_layout&,
			std::size_t num_viewports
		);
		/// Calls \p ID3D12Device::CreateComputePipelineState().
		[[nodiscard]] compute_pipeline_state create_compute_pipeline_state(const pipeline_resources&, const shader_binary&);

		/// Calls \p ID3D12Device::CreateDescriptorHeap().
		[[nodiscard]] descriptor_pool create_descriptor_pool(
			std::span<const descriptor_range>, std::size_t max_num_sets
		);
		/// Allocates descriptors from the given descriptor pool.
		[[nodiscard]] descriptor_set create_descriptor_set(descriptor_pool&, const descriptor_set_layout&);
		/// Allocates descriptors from the given descriptor pool, for a descriptor set where one descriptor range has
		/// a dynamically determined (unbounded) size.
		[[nodiscard]] descriptor_set create_descriptor_set(
			descriptor_pool&, const descriptor_set_layout&, std::size_t dynamic_count
		);

		/// Calls \p ID3D12Device::CreateShaderResourceView().
		void write_descriptor_set_read_only_images(
			descriptor_set&, const descriptor_set_layout&, std::size_t, std::span<const image_view *const>
		);
		/// Calls \p ID3D12Device::CreateUnorderedAccessView().
		void write_descriptor_set_read_write_images(
			descriptor_set&, const descriptor_set_layout&, std::size_t, std::span<const image_view *const>
		);
		/// Calls \p ID3D12Device::CreateShaderResourceView().
		void write_descriptor_set_read_only_structured_buffers(
			descriptor_set&, const descriptor_set_layout&, std::size_t, std::span<const structured_buffer_view>
		);
		/// Calls \p ID3D12Device::CreateUnorderedAccessView().
		void write_descriptor_set_read_write_structured_buffers(
			descriptor_set&, const descriptor_set_layout&, std::size_t, std::span<const structured_buffer_view>
		);
		/// Calls \p ID3D12Device::CreateConstantBufferView().
		void write_descriptor_set_constant_buffers(
			descriptor_set&, const descriptor_set_layout&, std::size_t, std::span<const constant_buffer_view>
		);
		/// Calls \p ID3D12Device::CreateSampler().
		void write_descriptor_set_samplers(
			descriptor_set&, const descriptor_set_layout&, std::size_t, std::span<const gpu::sampler *const>
		);

		/// Fills out a \ref shader_binary object.
		[[nodiscard]] shader_binary load_shader(std::span<const std::byte>);

		/// Returns the three default memory types supported by DirectX12.
		[[nodiscard]] std::span<
			const std::pair<memory_type_index, memory_properties>
		> enumerate_memory_types() const;
		/// Calls \p ID3D12Device::CreateHeap().
		[[nodiscard]] memory_block allocate_memory(std::size_t size, memory_type_index);

		/// Calls \p ID3D12Device::CreateCommittedResource3().
		[[nodiscard]] buffer create_committed_buffer(std::size_t size, memory_type_index, buffer_usage_mask);
		/// Calls \p ID3D12Device::CreateCommittedResource3().
		[[nodiscard]] image2d create_committed_image2d(
			std::size_t width, std::size_t height, std::size_t array_slices, std::size_t mip_levels,
			format, image_tiling, image_usage_mask
		);
		/// Computes the layout of the image using \p ID3D12Device::GetCopyableFootprints(), then creates a buffer
		/// that can hold it.
		[[nodiscard]] std::tuple<buffer, staging_buffer_pitch, std::size_t> create_committed_staging_buffer(
			std::size_t width, std::size_t height, format, memory_type_index, buffer_usage_mask
		);

		/// Calls \p ID3D12Resource::Map().
		[[nodiscard]] void *map_buffer(buffer&, std::size_t begin, std::size_t length);
		/// Calls \p ID3D12Resource::Unmap().
		void unmap_buffer(buffer&, std::size_t begin, std::size_t length);
		/// Calls \p ID3D12Resource::Map().
		[[nodiscard]] void *map_image2d(image2d&, subresource_index, std::size_t begin, std::size_t length);
		/// Calls \p ID3D12Resource::Unmap().
		void unmap_image2d(image2d&, subresource_index, std::size_t begin, std::size_t length);

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
			std::span<const gpu::image2d_view *const>, const image2d_view*, cvec2s
		);

		/// Calls \p ID3D12Device::CreateFence().
		[[nodiscard]] fence create_fence(synchronization_state);
		/// Calls \p ID3D12Device::CreateFence().
		[[nodiscard]] timeline_semaphore create_timeline_semaphore(std::uint64_t);

		/// Calls \p ID3D12Fence::Signal() to reset the given fence to the unset state.
		void reset_fence(fence&);
		/// Calls \p ID3D12Fence::SetEventOnCompletion() to wait for the \ref fence to be set.
		void wait_for_fence(fence&);

		/// Calls \p ID3D12Fence::Signal() to update the value of the semaphore.
		void signal_timeline_semaphore(timeline_semaphore&, std::uint64_t);
		/// Calls \p ID3D12Fence::GetCompletedValue() to retrieve the current value of the semaphore.
		[[nodiscard]] std::uint64_t query_timeline_semaphore(timeline_semaphore&);
		/// Calls \p ID3D12Fence::SetEventOnCompletion() to wait for the value of the \ref timeline_semaphore to
		/// reach the given value.
		void wait_for_timeline_semaphore(timeline_semaphore &sem, std::uint64_t);


		/// Calls \ref _set_debug_name().
		void set_debug_name(image &img, const char8_t *name) {
			_set_debug_name(*static_cast<_details::image*>(&img)->_image.Get(), name);
		}
		/// Calls \ref _set_debug_name().
		void set_debug_name(buffer &buf, const char8_t *name) {
			_set_debug_name(*buf._buffer.Get(), name);
		}
		/// Calls \ref _set_debug_name().
		void set_debug_name(image_view &buf, const char8_t *name) {
			_set_debug_name(*static_cast<_details::image_view*>(&buf)->_image.Get(), name);
		}


		// ray-tracing related
		/// Fills in the \p D3D12_RAYTRACING_GEOMETRY_DESC structures.
		[[nodiscard]] bottom_level_acceleration_structure_geometry
			create_bottom_level_acceleration_structure_geometry(
				std::span<const std::pair<vertex_buffer_view, index_buffer_view>>
			);

		/// Fills out the \p D3D12_RAYTRACING_INSTANCE_DESC structure.
		[[nodiscard]] instance_description get_bottom_level_acceleration_structure_description(
			bottom_level_acceleration_structure&,
			mat44f trans, std::uint32_t id, std::uint8_t mask, std::uint32_t hit_group_offset // TODO options
		) const;

		/// Calls \p ID3D12Device5::GetRaytracingAccelerationStructurePrebuildInfo().
		[[nodiscard]] acceleration_structure_build_sizes get_bottom_level_acceleration_structure_build_sizes(
			const bottom_level_acceleration_structure_geometry&
		);
		/// Calls \p ID3D12Device5::GetRaytracingAccelerationStructurePrebuildInfo().
		[[nodiscard]] acceleration_structure_build_sizes get_top_level_acceleration_structure_build_sizes(
			const buffer&, std::size_t offset, std::size_t count
		);
		/// Fills in the \ref bottom_level_acceleration_structure.
		[[nodiscard]] bottom_level_acceleration_structure create_bottom_level_acceleration_structure(
			buffer&, std::size_t off, std::size_t size
		);
		/// Fills in the \ref top_level_acceleration_structure.
		[[nodiscard]] top_level_acceleration_structure create_top_level_acceleration_structure(
			buffer&, std::size_t off, std::size_t size
		);

		/// Calls \p ID3D12Device::CreateShaderResourceView().
		void write_descriptor_set_acceleration_structures(
			descriptor_set&, const descriptor_set_layout&, std::size_t,
			std::span<gpu::top_level_acceleration_structure *const>
		);

		/// Calls \p ID3D12StateObjectProperties::GetShaderIdentifier().
		[[nodiscard]] shader_group_handle get_shader_group_handle(raytracing_pipeline_state&, std::size_t index);

		/// Calls \p ID3D12Device5::CreateStateObject().
		[[nodiscard]] raytracing_pipeline_state create_raytracing_pipeline_state(
			std::span<const shader_function> hit_group_shaders, std::span<const hit_shader_group>,
			std::span<const shader_function> general_shaders,
			std::size_t max_recursion, std::size_t max_payload_size, std::size_t max_attribute_size,
			const pipeline_resources&
		);
	private:
		_details::com_ptr<ID3D12Device10> _device; ///< Pointer to the device.
		/// Heap used for allocating color descriptors.
		_details::descriptor_heap<1, 8> _rtv_descriptors = nullptr;
		/// Heap used for allocating depth-stencil descriptors.
		_details::descriptor_heap<1, 1> _dsv_descriptors = nullptr;
		/// Heap used for allocating shader resource descriptors.
		_details::descriptor_heap<4, 5> _srv_descriptors = nullptr;
		/// Heap used for allocating sampler descriptors.
		_details::descriptor_heap<1, 4> _sampler_descriptors = nullptr;

		/// Initializes this device.
		explicit device(_details::com_ptr<ID3D12Device10>);

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
