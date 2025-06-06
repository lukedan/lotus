#pragma once

/// \file
/// DirectX 12 devices and adapters.

#include <utility>
#include <vector>
#include <optional>

#include "details.h"

#include "lotus/memory/common.h"
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
		constexpr static usize descriptor_heap_size = 524288;
		constexpr static usize sampler_heap_size = 2048;

		/// Does not create a device.
		device(std::nullptr_t) {
		}

		/// Calls \p IDXGISwapChain3::GetCurrentBackBufferIndex().
		[[nodiscard]] back_buffer_info acquire_back_buffer(swap_chain&);
		/// Calls \p IDXGISwapChain3::ResizeBuffers().
		void resize_swap_chain_buffers(swap_chain&, cvec2u32);

		/// Calls \p ID3D12Device::CreateCommandAllocator().
		[[nodiscard]] command_allocator create_command_allocator(command_queue&);
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
			u32 num_viewports
		);
		/// Calls \p ID3D12Device::CreateComputePipelineState().
		[[nodiscard]] compute_pipeline_state create_compute_pipeline_state(
			const pipeline_resources&, const shader_binary&
		);

		/// Calls \p ID3D12Device::CreateDescriptorHeap().
		[[nodiscard]] descriptor_pool create_descriptor_pool(
			std::span<const descriptor_range> capacity, u32 max_num_sets
		);
		/// Allocates descriptors from the given descriptor pool.
		[[nodiscard]] descriptor_set create_descriptor_set(descriptor_pool&, const descriptor_set_layout&);
		/// Allocates descriptors from the given descriptor pool, for a descriptor set where one descriptor range has
		/// a dynamically determined (unbounded) size.
		[[nodiscard]] descriptor_set create_descriptor_set(
			descriptor_pool&, const descriptor_set_layout&, u32 dynamic_count
		);

		/// Calls \p ID3D12Device::CreateShaderResourceView().
		void write_descriptor_set_read_only_images(
			descriptor_set&, const descriptor_set_layout&, u32, std::span<const image_view_base *const>
		);
		/// Calls \p ID3D12Device::CreateUnorderedAccessView().
		void write_descriptor_set_read_write_images(
			descriptor_set&, const descriptor_set_layout&, u32, std::span<const image_view_base *const>
		);
		/// Calls \p ID3D12Device::CreateShaderResourceView().
		void write_descriptor_set_read_only_structured_buffers(
			descriptor_set&, const descriptor_set_layout&, u32, std::span<const structured_buffer_view>
		);
		/// Calls \p ID3D12Device::CreateUnorderedAccessView().
		void write_descriptor_set_read_write_structured_buffers(
			descriptor_set&, const descriptor_set_layout&, u32, std::span<const structured_buffer_view>
		);
		/// Calls \p ID3D12Device::CreateConstantBufferView().
		void write_descriptor_set_constant_buffers(
			descriptor_set&, const descriptor_set_layout&, u32, std::span<const constant_buffer_view>
		);
		/// Calls \p ID3D12Device::CreateSampler().
		void write_descriptor_set_samplers(
			descriptor_set&, const descriptor_set_layout&, u32, std::span<const gpu::sampler *const>
		);

		/// Fills out a \ref shader_binary object.
		[[nodiscard]] shader_binary load_shader(std::span<const std::byte>);

		/// Returns the three default memory types supported by DirectX12.
		[[nodiscard]] std::span<
			const std::pair<memory_type_index, memory_properties>
		> enumerate_memory_types() const;
		/// Calls \p ID3D12Device::CreateHeap().
		[[nodiscard]] memory_block allocate_memory(usize size, memory_type_index);

		/// Calls \p ID3D12Device::CreateCommittedResource3().
		[[nodiscard]] buffer create_committed_buffer(usize size, memory_type_index, buffer_usage_mask);
		/// Calls \p ID3D12Device::CreateCommittedResource3().
		[[nodiscard]] image2d create_committed_image2d(
			cvec2u32 size, u32 mip_levels, format, image_tiling, image_usage_mask
		);
		/// Calls \p ID3D12Device::CreateCommittedResource3().
		[[nodiscard]] image3d create_committed_image3d(
			cvec3u32 size, u32 mip_levels, format, image_tiling, image_usage_mask
		);
		/// Computes the layout of the image using \p ID3D12Device::GetCopyableFootprints(), then creates a buffer
		/// that can hold it.
		[[nodiscard]] std::tuple<buffer, staging_buffer_metadata, usize> create_committed_staging_buffer(
			cvec2u32 size, format, memory_type_index, buffer_usage_mask
		);

		/// Calls \p ID3D12Device::GetResourceAllocationInfo2().
		[[nodiscard]] memory::size_alignment get_image2d_memory_requirements(
			cvec2u32 size, u32 mip_levels, format, image_tiling, image_usage_mask
		);
		/// Calls \p ID3D12Device::GetResourceAllocationInfo2().
		[[nodiscard]] memory::size_alignment get_image3d_memory_requirements(
			cvec3u32 size, u32 mip_levels, format, image_tiling, image_usage_mask
		);
		/// Calls \p ID3D12Device::GetResourceAllocationInfo2().
		[[nodiscard]] memory::size_alignment get_buffer_memory_requirements(usize size, buffer_usage_mask);
		/// Calls \p ID3D12Device::CreatePlacedResource2().
		[[nodiscard]] buffer create_placed_buffer(
			usize size, buffer_usage_mask, const memory_block&, usize offset
		);
		/// Calls \p ID3D12Device::CreatePlacedResource2().
		[[nodiscard]] image2d create_placed_image2d(
			cvec2u32 size, u32 mip_levels,
			format, image_tiling, image_usage_mask, const memory_block&, usize offset
		);
		/// Calls \p ID3D12Device::CreatePlacedResource2().
		[[nodiscard]] image3d create_placed_image3d(
			cvec3u32 size, u32 mip_levels,
			format, image_tiling, image_usage_mask, const memory_block&, usize offset
		);

		/// Calls \p ID3D12Resource::Map().
		[[nodiscard]] std::byte *map_buffer(buffer&);
		/// Cleans up any outstanding unmap calls due to flushes, then calls \p ID3D12Resource::Unmap().
		void unmap_buffer(buffer&);
		/// Calls \p ID3D12Resource::Map() and records an outstanding unmap operation.
		void flush_mapped_buffer_to_host(buffer&, usize begin, usize length);
		/// Consumes an unmap operation if possible or calls \p ID3D12Resource::Map(), then calls
		/// \p ID3D12Resource::Unmap().
		void flush_mapped_buffer_to_device(buffer&, usize begin, usize length);

		/// Fills out all fields in an \ref image2d_view.
		[[nodiscard]] image2d_view create_image2d_view_from(const image2d&, format, mip_levels);
		/// Fills out all fields in an \ref image3d_view.
		[[nodiscard]] image3d_view create_image3d_view_from(const image3d&, format, mip_levels);

		/// Fills out all fields in a \ref sampler.
		[[nodiscard]] sampler create_sampler(
			filtering minification, filtering magnification, filtering mipmapping,
			float mip_lod_bias, float min_lod, float max_lod, std::optional<float> max_anisotropy,
			sampler_address_mode addressing_u, sampler_address_mode addressing_v, sampler_address_mode addressing_w,
			linear_rgba_f border_color, comparison_function comparison
		);

		/// Fills out all fields in a \ref frame_buffer.
		[[nodiscard]] frame_buffer create_frame_buffer(
			std::span<const gpu::image2d_view *const>, const image2d_view*, cvec2u32
		);

		/// Calls \p ID3D12Device::CreateFence().
		[[nodiscard]] fence create_fence(synchronization_state);
		/// Calls \p ID3D12Device::CreateFence().
		[[nodiscard]] timeline_semaphore create_timeline_semaphore(gpu::_details::timeline_semaphore_value_type);

		/// Calls \p ID3D12Fence::Signal() to reset the given fence to the unset state.
		void reset_fence(fence&);
		/// Calls \p ID3D12Fence::SetEventOnCompletion() to wait for the \ref fence to be set.
		void wait_for_fence(fence&);

		/// Calls \p ID3D12Fence::Signal() to update the value of the semaphore.
		void signal_timeline_semaphore(timeline_semaphore&, gpu::_details::timeline_semaphore_value_type);
		/// Calls \p ID3D12Fence::GetCompletedValue() to retrieve the current value of the semaphore.
		[[nodiscard]] gpu::_details::timeline_semaphore_value_type query_timeline_semaphore(timeline_semaphore&);
		/// Calls \p ID3D12Fence::SetEventOnCompletion() to wait for the value of the \ref timeline_semaphore to
		/// reach the given value.
		void wait_for_timeline_semaphore(timeline_semaphore &sem, gpu::_details::timeline_semaphore_value_type);

		/// Calls \p ID3D12Device::CreateQueryHeap() to create the heap, then creates a buffer for receiving query
		/// results.
		[[nodiscard]] timestamp_query_heap create_timestamp_query_heap(u32 size);
		/// Maps the buffer and copies over the results of the queries.
		void fetch_query_results(timestamp_query_heap&, u32 first, std::span<u64>);


		/// Calls \ref _set_debug_name().
		void set_debug_name(image_base &img, const char8_t *name) {
			_set_debug_name(*static_cast<_details::image_base*>(&img)->_image.Get(), name);
		}
		/// Calls \ref _set_debug_name().
		void set_debug_name(buffer &buf, const char8_t *name) {
			_set_debug_name(*buf._buffer.Get(), name);
		}
		/// Calls \ref _set_debug_name().
		void set_debug_name(image_view_base &buf, const char8_t *name) {
			_set_debug_name(*static_cast<_details::image_view_base*>(&buf)->_image.Get(), name);
		}


		// ray-tracing related
		/// Fills in the \p D3D12_RAYTRACING_GEOMETRY_DESC structures.
		[[nodiscard]] bottom_level_acceleration_structure_geometry
			create_bottom_level_acceleration_structure_geometry(std::span<const raytracing_geometry_view>);

		/// Fills out the \p D3D12_RAYTRACING_INSTANCE_DESC structure.
		[[nodiscard]] instance_description get_bottom_level_acceleration_structure_description(
			bottom_level_acceleration_structure&,
			mat44f trans, u32 id, u8 mask, u32 hit_group_offset,
			raytracing_instance_flags
		) const;

		/// Calls \p ID3D12Device5::GetRaytracingAccelerationStructurePrebuildInfo().
		[[nodiscard]] acceleration_structure_build_sizes get_bottom_level_acceleration_structure_build_sizes(
			const bottom_level_acceleration_structure_geometry&
		);
		/// Calls \p ID3D12Device5::GetRaytracingAccelerationStructurePrebuildInfo().
		[[nodiscard]] acceleration_structure_build_sizes get_top_level_acceleration_structure_build_sizes(
			usize instance_count
		);
		/// Fills in the \ref bottom_level_acceleration_structure.
		[[nodiscard]] bottom_level_acceleration_structure create_bottom_level_acceleration_structure(
			buffer&, usize off, usize size
		);
		/// Fills in the \ref top_level_acceleration_structure.
		[[nodiscard]] top_level_acceleration_structure create_top_level_acceleration_structure(
			buffer&, usize off, usize size
		);

		/// Calls \p ID3D12Device::CreateShaderResourceView().
		void write_descriptor_set_acceleration_structures(
			descriptor_set&, const descriptor_set_layout&, u32,
			std::span<gpu::top_level_acceleration_structure *const>
		);

		/// Calls \p ID3D12StateObjectProperties::GetShaderIdentifier().
		[[nodiscard]] shader_group_handle get_shader_group_handle(
			const raytracing_pipeline_state&, usize index
		);

		/// Calls \p ID3D12Device5::CreateStateObject().
		[[nodiscard]] raytracing_pipeline_state create_raytracing_pipeline_state(
			std::span<const shader_function> hit_group_shaders, std::span<const hit_shader_group>,
			std::span<const shader_function> general_shaders,
			usize max_recursion, usize max_payload_size, usize max_attribute_size,
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

		/// Calls \p D3D12CreateDevice(), then calls \p ID3D12Device::CreateCommandQueue() to create requested
		/// command queues.
		[[nodiscard]] std::pair<device, std::vector<command_queue>> create_device(std::span<const queue_family>);
		/// Returns the properties of this adapter.
		[[nodiscard]] adapter_properties get_properties() const;

		/// Checks if the adapter is empty.
		[[nodiscard]] bool is_valid() const {
			return _adapter;
		}
	private:
		_details::com_ptr<IDXGIAdapter1> _adapter; ///< The adapter.
		_details::debug_message_callback *_debug_callback = nullptr; ///< The debug callback.
	};
}
