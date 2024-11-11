#pragma once

/// \file
/// Common typedefs and functions for DirectX 12.

#include <cstdlib>
#include <vector>
#include <deque>
#include <map>

#include <wrl/client.h>
#include <directx/d3d12.h>
#include <directx/d3d12shader.h>

#include "lotus/containers/static_optional.h"
#include "lotus/utils/static_function.h"
#include "lotus/gpu/common.h"

namespace lotus::gpu::backends::directx12 {
	class device;
}

namespace lotus::gpu::backends::directx12::_details {
	template <typename T> using com_ptr = Microsoft::WRL::ComPtr<T>; ///< Reference-counted pointer to a COM object.
	/// ID type used to identify debug messages.
	using debug_message_id = D3D12_MESSAGE_ID;
	/// Debug message callback type.
	using debug_message_callback =
		static_function<void(debug_message_severity, debug_message_id, std::u8string_view)>;

	/// Aborts if the given \p HRESULT does not indicate success. Specify the \p ID3D12Device parameter to also
	/// obtain device removal reason if applicable.
	void assert_dx(HRESULT, ID3D12Device* = nullptr);

	/// Converts generic types into DX12 types.
	namespace conversions {
		// simple enum conversions
		/// Calls \ref backends::common::_details::conversions::to_format().
		[[nodiscard]] DXGI_FORMAT to_format(format);
		/// Converts a \ref index_format to a \p DXGI_FORMAT.
		[[nodiscard]] DXGI_FORMAT to_format(index_format);
		/// Converts a \ref image_tiling to a \p D3D12_TEXTURE_LAYOUT.
		[[nodiscard]] D3D12_TEXTURE_LAYOUT to_texture_layout(image_tiling);
		/// Converts a \ref blend_factor to a \p D3D12_BLEND.
		[[nodiscard]] D3D12_BLEND to_blend_factor(blend_factor);
		/// Converts a \ref blend_operation to a \p D3D12_BLEND_OP.
		[[nodiscard]] D3D12_BLEND_OP to_blend_operation(blend_operation);
		/// Converts a \ref cull_mode to a \p D3D12_CULL_MODE.
		[[nodiscard]] D3D12_CULL_MODE to_cull_mode(cull_mode);
		/// Converts a \ref stencil_operation to a \p D3D12_STENCIL_OP.
		[[nodiscard]] D3D12_STENCIL_OP to_stencil_operation(stencil_operation);
		/// Converts a \ref input_buffer_rate to a \p D3D12_INPUT_CLASSIFICATION.
		[[nodiscard]] D3D12_INPUT_CLASSIFICATION to_input_classification(input_buffer_rate);
		/// Converts a \ref primitive_topology to a \p D3D12_PRIMITIVE_TOPOLOGY_TYPE.
		[[nodiscard]] D3D12_PRIMITIVE_TOPOLOGY_TYPE to_primitive_topology_type(primitive_topology);
		/// Converts a \ref primitive_topology to a \p D3D_PRIMITIVE_TOPOLOGY.
		[[nodiscard]] D3D_PRIMITIVE_TOPOLOGY to_primitive_topology(primitive_topology);
		/// Converts a \ref pass_load_operation to a \p D3D12_RENDER_PASS_BEGINNING_ACCESS.
		[[nodiscard]] D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE to_render_pass_beginning_access_type(pass_load_operation);
		/// Converts a \ref pass_store_operation to a \p D3D12_RENDER_PASS_ENDING_ACCESS.
		[[nodiscard]] D3D12_RENDER_PASS_ENDING_ACCESS_TYPE to_render_pass_ending_access_type(pass_store_operation);
		/// Converts a \ref descriptor_type to a \p D3D12_DESCRIPTOR_RANGE_TYPE.
		[[nodiscard]] D3D12_DESCRIPTOR_RANGE_TYPE to_descriptor_range_type(descriptor_type);
		/// Converts a \ref synchronization_point_mask to a \p D3D12_BARRIER_SYNC.
		[[nodiscard]] D3D12_BARRIER_SYNC to_barrier_sync(synchronization_point_mask);
		/// Converts a \ref image_usage_mask to a \p D3D12_RESOURCE_FLAGS.
		[[nodiscard]] D3D12_RESOURCE_FLAGS to_resource_flags(image_usage_mask);
		/// Converts a \ref buffer_usage_mask to a \p D3D12_RESOURCE_FLAGS.
		[[nodiscard]] D3D12_RESOURCE_FLAGS to_resource_flags(buffer_usage_mask);
		/// Converts a \ref image_access_mask to a \p D3D12_BARRIER_ACCESS.
		[[nodiscard]] D3D12_BARRIER_ACCESS to_barrier_access(image_access_mask);
		/// Converts a \ref buffer_access_mask to a \p D3D12_BARRIER_ACCESS.
		[[nodiscard]] D3D12_BARRIER_ACCESS to_barrier_access(buffer_access_mask);
		/// Converts a \ref image_layout to a \p D3D12_BARRIER_LAYOUT.
		[[nodiscard]] D3D12_BARRIER_LAYOUT to_barrier_layout(image_layout);
		/// Converts a \ref sampler_address_mode to a \p D3D12_TEXTURE_ADDRESS_MODE.
		[[nodiscard]] D3D12_TEXTURE_ADDRESS_MODE to_texture_address_mode(sampler_address_mode);
		/// Converts a \ref comparison_function to a \p D3D12_COMPARISON_FUNC.
		[[nodiscard]] D3D12_COMPARISON_FUNC to_comparison_function(comparison_function);
		/// Converts a \ref shader_stage to a \p D3D12_SHADER_VERSION_TYPE.
		[[nodiscard]] D3D12_SHADER_VERSION_TYPE to_shader_version_type(shader_stage);
		/// Converts a \ref queue_family to a \p D3D12_COMMAND_LIST_TYPE.
		[[nodiscard]] D3D12_COMMAND_LIST_TYPE to_command_list_type(queue_family);
		/// Converts a \ref raytracing_instance_flags to a \p D3D12_RAYTRACING_INSTANCE_FLAGS.
		[[nodiscard]] D3D12_RAYTRACING_INSTANCE_FLAGS to_raytracing_instance_flags(raytracing_instance_flags);
		/// Converts a \ref raytracing_geometry_flags to a \p D3D12_RAYTRACING_GEOMETRY_FLAGS.
		[[nodiscard]] D3D12_RAYTRACING_GEOMETRY_FLAGS to_raytracing_geometry_flags(raytracing_geometry_flags);

		// bitmask conversions
		/// Converts a \ref channel_mask to a \p D3D12_COLOR_WRITE_ENABLE.
		[[nodiscard]] D3D12_COLOR_WRITE_ENABLE to_color_write_mask(channel_mask);
		/// Converts a \ref shader_stage to a \p D3D12_SHADER_VISIBILITY.
		[[nodiscard]] D3D12_SHADER_VISIBILITY to_shader_visibility(shader_stage);

		// complex enum conversions
		/// Converts filter parameters to a \p D3D12_FILTER.
		[[nodiscard]] D3D12_FILTER to_filter(
			filtering minification, filtering magnification, filtering mipmapping, bool anisotropic, bool comparison
		);

		// struct conversions
		/// Converts a \ref viewport to a \p D3D12_VIEWPORT.
		[[nodiscard]] D3D12_VIEWPORT to_viewport(const viewport&);
		/// Converts a \ref aab2i to a \p D3D12_RECT.
		[[nodiscard]] D3D12_RECT to_rect(const aab2i&);

		/// Converts a \ref render_target_blend_options to a \p D3D12_RENDER_TARGET_BLEND_DESC.
		[[nodiscard]] D3D12_RENDER_TARGET_BLEND_DESC to_render_target_blend_description(
			const render_target_blend_options&
		);
		/// Converts a \ref blend_options to a \p D3D12_BLEND_DESC.
		[[nodiscard]] D3D12_BLEND_DESC to_blend_description(std::span<const render_target_blend_options>);
		/// Converts a \ref rasterizer_options to a \p D3D12_RASTERIZER_DESC.
		[[nodiscard]] D3D12_RASTERIZER_DESC to_rasterizer_description(const rasterizer_options&);
		/// Converts a \ref stencil_options to a \p D3D12_DEPTH_STENCILOP_DESC.
		[[nodiscard]] D3D12_DEPTH_STENCILOP_DESC to_depth_stencil_operation_description(const stencil_options&);
		/// Converts a \ref depth_stencil_options to a \p D3D12_DEPTH_STENCIL_DESC1.
		[[nodiscard]] D3D12_DEPTH_STENCIL_DESC to_depth_stencil_description(const depth_stencil_options&);

		/// Converts a \ref render_target_pass_options to a \p D3D12_RENDER_PASS_RENDER_TARGET_DESC.
		[[nodiscard]] D3D12_RENDER_PASS_RENDER_TARGET_DESC to_render_pass_render_target_description(
			const render_target_pass_options&
		);
		/// Converts a \ref depth_stencil_pass_options to a \p D3D12_DEPTH_STENCIL_RENDER_TARGET_DESC.
		[[nodiscard]] D3D12_RENDER_PASS_DEPTH_STENCIL_DESC to_render_pass_depth_stencil_description(
			const depth_stencil_pass_options&
		);

		/// Converts a \ref subresource_range to a \p D3D12_BARRIER_SUBRESOURCE_RANGE.
		[[nodiscard]] D3D12_BARRIER_SUBRESOURCE_RANGE to_barrier_subresource_range(const subresource_range&);


		/// Converts a \p D3D12_MESSAGE_SEVERITY back to a \ref debug_message_severitiy.
		[[nodiscard]] debug_message_severity back_to_debug_message_severity(D3D12_MESSAGE_SEVERITY);

		/// Converts a \p D3D12_SHADER_INPUT_BIND_DESC back to a \ref shader_resource_binding.
		[[nodiscard]] shader_resource_binding back_to_shader_resource_binding(const D3D12_SHADER_INPUT_BIND_DESC&);
		/// Converts a \p D3D12_SIGNATURE_PARAMETER_DESC back to a \ref shader_output_variable.
		[[nodiscard]] shader_output_variable back_to_shader_output_variable(const D3D12_SIGNATURE_PARAMETER_DESC&);
	}


	/// Wrapper around a \p D3D12_CPU_DESCRIPTOR_HANDLE.
	struct descriptor_range {
		template <std::size_t, std::size_t> friend class descriptor_heap;
	public:
		using index_t = std::uint32_t; ///< Index of a descriptor.

		/// Initializes this descriptor to empty.
		descriptor_range(std::nullptr_t) {
		}
		/// Move constructor.
		descriptor_range(descriptor_range &&src) noexcept :
			_cpu(src._cpu), _gpu(src._gpu), _increment(src._increment),
			_index(src._index), _count(std::exchange(src._count, index_t(0))) {
		}
		/// No copy construction.
		descriptor_range(const descriptor_range&) = delete;
		/// Move assignment.
		descriptor_range &operator=(descriptor_range &&src) noexcept {
			assert(is_empty());
			_cpu       = src._cpu;
			_gpu       = src._gpu;
			_increment = src._increment;
			_index     = src._index;
			_count     = std::exchange(src._count, index_t(0));
			return *this;
		}
		/// No copy assignment.
		descriptor_range &operator=(const descriptor_range&) = delete;
		/// Checks that this \ref descriptor_range has been freed by a \ref descriptor_range_heap.
		~descriptor_range() {
			assert(is_empty());
		}

		/// Returns the CPU descriptor at the given index.
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_cpu(index_t id) const {
			assert(id < _count);
			D3D12_CPU_DESCRIPTOR_HANDLE result = _cpu;
			result.ptr += id * _increment;
			return result;
		}
		/// Returns the GPU descriptor at the given index.
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE get_gpu(index_t id) const {
			assert(id < _count);
			D3D12_GPU_DESCRIPTOR_HANDLE result = _gpu;
			result.ptr += id * _increment;
			return result;
		}

		/// Returns the number of descriptors.
		[[nodiscard]] index_t get_count() const {
			return _count;
		}

		/// Returns whether this descriptor is empty.
		[[nodiscard]] bool is_empty() const {
			return _count == 0;
		}
	private:
		D3D12_CPU_DESCRIPTOR_HANDLE _cpu = {}; ///< The CPU descriptor.
		D3D12_GPU_DESCRIPTOR_HANDLE _gpu = {}; ///< The GPU descriptor.
		UINT _increment = 0; ///< The size of a single descriptor.
		index_t _index = 0; ///< Index of the descriptor in the heap.
		/// The number of descriptors. If this is 0, it indicates that the range is empty or has been freed.
		index_t _count = 0;

		/// Initializes all members. Note that the descriptor handles point to the start of the heap, instead of the
		/// start of the range, to simplify construction.
		descriptor_range(
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_start, D3D12_GPU_DESCRIPTOR_HANDLE gpu_start, UINT incr,
			index_t id, index_t c
		) : _cpu(cpu_start), _gpu(gpu_start), _increment(incr), _index(id), _count(c) {
			_cpu.ptr += _increment * _index;
			_gpu.ptr += _increment * _index;
		}
	};
	/// Manages a series of descriptors.
	template <std::size_t Gap, std::size_t Levels> class descriptor_heap {
	public:
		/// No initialization.
		descriptor_heap(std::nullptr_t) {
		}
		/// Creates the descriptor heap.
		descriptor_heap(ID3D12Device &device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity) {
			initialize(device, type, capacity);
		}
		/// Move constructor.
		descriptor_heap(descriptor_heap &&src) noexcept :
			_heap(std::exchange(src._heap, nullptr)),
			_free(std::move(src._free)),
			_sized_free(std::move(src._sized_free)),
			_increment(src._increment),
			_capacity(src._capacity) {
		}
		/// No copy construction.
		descriptor_heap(const descriptor_heap&) = delete;
		/// Move assignment.
		descriptor_heap &operator=(descriptor_heap &&src) noexcept {
			_heap       = std::exchange(src._heap, nullptr);
			_free       = std::move(src._free);
			_sized_free = std::move(src._sized_free);
			_increment  = src._increment;
			_capacity   = src._capacity;
			return *this;
		}
		/// No copy assignment.
		descriptor_heap &operator=(const descriptor_heap&) = delete;

		/// Initializes the descriptor heap. This descriptor heap must not have been initialized or must have been
		/// explicitly de-initialized.
		void initialize(ID3D12Device &device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity) {
			assert(_heap == nullptr);
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type           = type;
			desc.NumDescriptors = capacity;
			desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NodeMask       = 0;
			if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) {
				desc.Flags |= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			}
			_details::assert_dx(device.CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_heap)));

			_increment = device.GetDescriptorHandleIncrementSize(type);

			auto entry = _free.emplace(
				descriptor_range::index_t(0), _range_data(static_cast<descriptor_range::index_t>(capacity), 0)
			);
			_sized_free.add_range(entry.first);

			_capacity = debug_value<std::size_t>(capacity);
		}

		/// Allocates a range of descritors.
		[[nodiscard]] descriptor_range allocate(descriptor_range::index_t count) {
			auto range = _sized_free.allocate_range(count);
			auto node = _free.extract(range);
			D3D12_GPU_DESCRIPTOR_HANDLE gpu_addr = { 0 };
			if (_heap->GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
				gpu_addr = _heap->GetGPUDescriptorHandleForHeapStart();
			}
			descriptor_range result(
				_heap->GetCPUDescriptorHandleForHeapStart(), gpu_addr, _increment, node.key(), count
			);
			if (node.mapped().count != count) { // not an exact fit
				node.key() += count;
				node.mapped().count -= count;
				auto entry = _free.insert(std::move(node));
				_sized_free.add_range(entry.position);
			}
			return result;
		}
		/// Frees the given range of descriptors.
		void free(descriptor_range desc) {
			assert(!desc.is_empty());
			auto it = _free.lower_bound(desc._index);
			if (it != _free.begin()) { // check range before
				auto prev = it;
				--prev;
				assert(prev->first + prev->second.count <= desc._index);
				if (prev->first + prev->second.count == desc._index) { // merge
					desc._index = prev->first;
					desc._count += prev->second.count;
					_sized_free.remove_range(prev);
					_free.erase(prev);
				}
			}
			if (it != _free.end()) { // check range after
				assert(it->first >= desc._index + desc.get_count());
				if (it->first == desc._index + desc.get_count()) { // merge
					desc._count += it->second.count;
					_sized_free.remove_range(it);
					_free.erase(it);
				}
			}
			auto entry = _free.emplace(desc._index, _range_data(desc._count, 0));
			_sized_free.add_range(entry.first);
			_dispose(std::move(desc));
		}

		/// Returns the underlying descriptor heap.
		[[nodiscard]] ID3D12DescriptorHeap *get_heap() const {
			return _heap.Get();
		}
	protected:
		/// Data of a range.
		struct _range_data {
			/// No initialization.
			_range_data(uninitialized_t) {
			}
			/// Initializes all fields of this struct.
			_range_data(descriptor_range::index_t len, descriptor_range::index_t i) :
				count(len), size_list_index(i) {
			}

			descriptor_range::index_t count; ///< The length of this range.
			descriptor_range::index_t size_list_index; ///< Index of this range in \ref _sized_free.
		};
		// TODO allocator
		/// List of free ranges and their indices in \ref _sized_free ordered by index.
		using _free_list = std::map<descriptor_range::index_t, _range_data>;

		/// Categorizes descriptor ranges by size.
		struct _size_list {
		public:
			using entry_t = typename _free_list::iterator; ///< Entry type.

			/// Adds a range to this list.
			void add_range(entry_t r) {
				std::size_t level = get_list_level(r->second.count);
				r->second.size_list_index = static_cast<descriptor_range::index_t>(_lists[level].size());
				_lists[level].emplace_back(r);
			}
			/// Removes the given range.
			void remove_range(entry_t it) {
				std::size_t level = get_list_level(it->second.count);
				_remove_range(level, it->second.size_list_index);
			}
			/// Allocates a range and removes it from the list.
			[[nodiscard]] entry_t allocate_range(descriptor_range::index_t count) {
				std::size_t level = get_allocate_level(count);
				std::size_t allocate_index = 0;
				// if the current level is empty, look in higher levels
				for (; level < Levels && _lists[level].empty(); ++level) {
				}
				assert(level < Levels); // out of space!
				if (get_level_min_bound(level) < count) { // also make sure the range is large enough
					assert(level + 1 == Levels); // this should only happen at the last level
					for (allocate_index = 0; allocate_index < _lists[level].size(); ++allocate_index) {
						if (_lists[level][allocate_index]->second.count >= count) {
							break; // found one
						}
					}
					assert(allocate_index < _lists[level].size()); // out of space!
				}
				auto result = _lists[level][allocate_index];
				_remove_range(level, allocate_index);
				return result;
			}

			/// Computes the minimum size for the given level. First level has a minimum of 1, and then each level
			/// has \p Gap more.
			[[nodiscard]] constexpr static std::size_t get_level_min_bound(std::size_t level) {
				return 1 + level * Gap;
			}
			/// Computes a level to allocate from based on the given size.
			[[nodiscard]] constexpr static std::size_t get_allocate_level(descriptor_range::index_t count) {
				return std::min((count + Gap - 2) / Gap, Levels - 1);
			}
			/// Computes the level that the given range is in.
			[[nodiscard]] constexpr static std::size_t get_list_level(descriptor_range::index_t count) {
				return std::min((count - 1) / Gap, Levels - 1);
			}
		protected:
			// TODO allocator
			std::deque<entry_t> _lists[Levels]; ///< Lists of descriptor ranges.

			/// Removes the specified range.
			void _remove_range(std::size_t level, std::size_t index) {
				std::swap(_lists[level][index], _lists[level].back());
				_lists[level][index]->second.size_list_index = static_cast<descriptor_range::index_t>(index);
				_lists[level].pop_back();
			}
		};

		com_ptr<ID3D12DescriptorHeap> _heap; ///< The heap of descriptors.
		_free_list _free; ///< An array containing descriptors that are not in use.
		_size_list _sized_free; ///< Free descriptor lists categorized by size.
		UINT _increment; ///< Size of a single descriptor.
		[[no_unique_address]] debug_value<std::size_t> _capacity; ///< Capacity.

		/// Safely disposes of the given range.
		void _dispose(descriptor_range range) {
			range._count = 0;
		}
	};


	/// Convenience function that returns a default \p D3D12_HEAP_PROPERTIES corresponding to the given
	/// \p D3D12_HEAP_TYPE.
	[[nodiscard]] D3D12_HEAP_PROPERTIES default_heap_properties(D3D12_HEAP_TYPE);

	/// Computes the index of the given subresource.
	[[nodiscard]] UINT compute_subresource_index(const subresource_index&, ID3D12Resource*);

	/// Returns a unique shader name corresponding to the given index.
	[[nodiscard]] LPCWSTR shader_name(std::size_t index);
	/// Returns a unique shader record name corresponding to the given index.
	[[nodiscard]] LPCWSTR shader_record_name(std::size_t index);

	/// Used to create \p D3D12_RESOURCE_DESC objects for various types of resources.
	namespace resource_desc {
		/// Description for a buffer with the specified size.
		[[nodiscard]] D3D12_RESOURCE_DESC1 for_buffer(std::size_t size, buffer_usage_mask);
		/// Adjusts various flags of buffer properties.
		void adjust_resource_flags_for_buffer(D3D12_HEAP_TYPE, buffer_usage_mask, D3D12_HEAP_FLAGS* = nullptr);

		/// Description for a 2D image.
		[[nodiscard]] D3D12_RESOURCE_DESC1 for_image2d(
			cvec2u32 size, std::uint32_t mip_levels, format, image_tiling, image_usage_mask
		);
		/// Description for a 3D image.
		[[nodiscard]] D3D12_RESOURCE_DESC1 for_image3d(
			cvec3u32 size, std::uint32_t mip_levels, format, image_tiling, image_usage_mask
		);
		/// Adjusts various flags of image properties.
		void adjust_resource_flags_for_image(format, image_usage_mask, D3D12_HEAP_FLAGS* = nullptr);
	}
}
