#pragma once

/// \file
/// Metal command buffers.

#include "lotus/color.h"
#include "lotus/gpu/common.h"
#include "details.h"
#include "acceleration_structure.h"
#include "frame_buffer.h"
#include "pipeline.h"
#include "resources.h"
#include "synchronization.h"
#include "descriptors.h"

namespace lotus::gpu::backends::metal {
	class adapter;
	class command_list;
	class command_queue;
	class device;

	/// Metal 4 command allocators can only service one command list at a time, so this class only contains a
	/// \p MTL4::CommandQueue, and the \p MTL4::CommandAllocator is bundled with the command list.
	class command_allocator {
		friend command_list;
		friend device;
	protected:
		/// Initializes this object to empty.
		command_allocator(std::nullptr_t) {
		}

		/// Does nothing.
		void reset(device&);
	private:
		MTL4::CommandQueue *_q = nullptr; ///< The command queue.

		/// Initializes \ref _q.
		explicit command_allocator(MTL4::CommandQueue *q) : _q(q) {
		}
	};

	/// Holds a \p MTL4::CommandBuffer and the corresponding \p MTL4::CommandAllocator.
	class command_list {
		friend command_queue;
		friend device;
	protected:
		/// Initializes this object to empty.
		command_list(std::nullptr_t) {
		}

		/// Creates a new \p MTL4::RenderPassDescriptor that corresponds to the given input and creates a
		/// \p MTL4::RenderCommandEncoder with it.
		void begin_pass(const frame_buffer&, const frame_buffer_access&);

		/// Binds all graphics pipeline related state to \ref _pass_encoder.
		void bind_pipeline_state(const graphics_pipeline_state&);
		/// Records the given pipeline state object to be bound during dispatches.
		void bind_pipeline_state(const compute_pipeline_state&);
		/// Updates the vertex buffer bindings in \ref _graphics_bindings.
		void bind_vertex_buffers(usize start, std::span<const vertex_buffer>);
		/// Updates \ref _index_buffer, \ref _index_offset_bytes, and \ref _index_format.
		void bind_index_buffer(const buffer&, usize offset_bytes, index_format);
		/// Binds descriptor sets to \ref _pass_encoder.
		void bind_graphics_descriptor_sets(
			const pipeline_resources&, u32 first, std::span<const gpu::descriptor_set *const>
		);
		/// Records the given descriptor sets to be bound during dispatches.
		void bind_compute_descriptor_sets(
			const pipeline_resources&, u32 first, std::span<const gpu::descriptor_set *const>
		);

		/// Calls \p MTL4::RenderCommandEncoder::setViewports().
		void set_viewports(std::span<const viewport>);
		/// Calls \p MTL4::RenderCommandEncoder::setScissorRects().
		void set_scissor_rectangles(std::span<const aab2u32>);

		/// Creates a \p MTL4::ComputeCommandEncoder to encode a copy command.
		void copy_buffer(const buffer &from, usize off1, buffer &to, usize off2, usize size);
		/// Creates a \p MTL4::ComputeCommandEncoder to encode a copy command.
		void copy_image2d(
			image2d &from, subresource_index sub1, aab2u32 region, image2d &to, subresource_index sub2, cvec2u32 off
		);
		/// Creates a \p MTL4::ComputeCommandEncoder to encode a copy command.
		void copy_buffer_to_image(
			const buffer &from,
			usize byte_offset,
			staging_buffer_metadata,
			image2d &to,
			subresource_index subresource,
			cvec2u32 off
		);

		/// Calls \p MTL4::RenderCommandEncoder::drawPrimitives().
		void draw_instanced(u32 first_vertex, u32 vertex_count, u32 first_instance, u32 instance_count);
		/// Calls \p MTL4::RenderCommandEncoder::drawIndexedPrimitives().
		void draw_indexed_instanced(
			u32 first_index, u32 index_count, i32 first_vertex, u32 first_instance, u32 instance_count
		);
		/// Creates a new \p MTL4::ComputeCommandEncoder, binds resources, and calls \p dispatchThreadgroups().
		void run_compute_shader(u32 x, u32 y, u32 z);

		/// Updates \ref _pending_graphics_barriers and \ref _pending_compute_barriers based on the given resource
		/// barriers.
		void resource_barrier(std::span<const image_barrier>, std::span<const buffer_barrier>);

		/// Calls \p MTL4::RenderCommandEncoder::endEncoding() and releases the encoder.
		void end_pass();

		/// Calls \p MTL4::CommandBuffer::writeTimestampIntoHeap().
		void query_timestamp(timestamp_query_heap&, u32 index);
		/// Does nothing. Metal supports resolving these queries either on the GPU timeline or on the CPU timeline;
		/// we opt to do the latter.
		void resolve_queries(timestamp_query_heap&, u32 first, u32 count);

		void insert_marker(const char8_t*, linear_rgba_u8 color); // TODO
		/// Calls \p MTL4::CommandBuffer::pushDebugGroup().
		void begin_marker_scope(const char8_t*, linear_rgba_u8 color);
		/// Calls \p MTL4::CommandBuffer::popDebugGroup().
		void end_marker_scope();

		/// Calls \p MTL4::CommandBuffer::endCommandBuffer().
		void finish();


		// ray-tracing related
		/// Creates a \p MTL4::ComputeCommandEncoder and calls \p buildAccelerationStructure().
		void build_acceleration_structure(
			const bottom_level_acceleration_structure_geometry&,
			bottom_level_acceleration_structure &output,
			buffer &scratch,
			usize scratch_offset
		);
		/// Creates a \p MTL::IndirectAccelerationStructureInstanceDescriptor and builds an acceleration structure
		/// with it.
		void build_acceleration_structure(
			const buffer &instances,
			usize offset,
			usize count,
			top_level_acceleration_structure &output,
			buffer &scratch,
			usize scratch_offset
		);

		void bind_pipeline_state(const raytracing_pipeline_state&); // TODO
		/// Records the given descriptor sets to be bound during ray tracingdispatches.
		void bind_ray_tracing_descriptor_sets(
			const pipeline_resources&, u32 first, std::span<const gpu::descriptor_set *const>
		);
		void trace_rays(constant_buffer_view ray_generation, shader_record_view miss_shaders, shader_record_view hit_groups, usize width, usize height, usize depth); // TODO


		/// Checks whether this object is valid.
		[[nodiscard]] bool is_valid() const {
			return !!_buf;
		}
	private:
		/// A scoped \p MTL4::ComputeCommandEncoder that ends encoding on destruction.
		struct _scoped_compute_encoder {
			/// Initializes this object to empty.
			_scoped_compute_encoder(std::nullptr_t) {
			}
			/// Initializes \ref _encoder.
			explicit _scoped_compute_encoder(MTL4::ComputeCommandEncoder *e) : _encoder(e) {
			}
			/// Move construction.
			_scoped_compute_encoder(_scoped_compute_encoder &&src) noexcept :
				_encoder(std::exchange(src._encoder, nullptr)) {
			}
			/// No copy construction.
			_scoped_compute_encoder(const _scoped_compute_encoder&) = delete;
			/// Calls \ref reset().
			~_scoped_compute_encoder() {
				reset();
			}

			/// Returns the \p MTL4::ComputeCommandEncoder object.
			[[nodiscard]] MTL4::ComputeCommandEncoder *get() const {
				return _encoder;
			}
			/// \overload
			[[nodiscard]] MTL4::ComputeCommandEncoder *operator->() const {
				return _encoder;
			}

			/// Returns whether this object contains a valid encoder.
			[[nodiscard]] bool is_valid() const {
				return _encoder;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
			/// Resets this encoder to null, ending encoding if necessary.
			void reset() {
				if (_encoder) {
					_encoder->endEncoding();
					_encoder = nullptr;
				}
			}
		private:
			MTL4::ComputeCommandEncoder *_encoder = nullptr; ///< The encoder.
		};


		NS::SharedPtr<MTL4::CommandBuffer> _buf; ///< The command buffer.
		/// The command allocator. This is bundled with the command buffer because it can only service one command
		/// buffer at a time.
		NS::SharedPtr<MTL4::CommandAllocator> _alloc;
		NS::SharedPtr<MTL::ResidencySet> _residency_set; ///< Residency set for temporary allocations.

		MTL4::RenderCommandEncoder *_pass_encoder = nullptr; ///< Encoder for the render pass.
		NS::SharedPtr<MTL4::ArgumentTable> _graphics_bindings; ///< Current bindings for the graphics stages.
		std::vector<u64> _graphics_sets; ///< Current bindings for the graphics stages.
		bool _graphics_sets_bound = false; ///< Whether \ref _graphics_bindings contains fresh data.
		MTL::GPUAddress _index_addr = 0; ///< Currently bound index buffer address.
		index_format _index_format = index_format::num_enumerators; ///< Currently bound index buffer format.
		/// Primitive topology of the last bound graphics pipeline.
		primitive_topology _topology = primitive_topology::num_enumerators;

		/// Current bindings for the compute and raytracing stages.
		NS::SharedPtr<MTL4::ArgumentTable> _compute_bindings;
		NS::SharedPtr<MTL::ComputePipelineState> _compute_pipeline; ///< Currently bound compute pipeline state.
		cvec3u32 _compute_thread_group_size = zero; ///< Thread group size of the currently bound compute pipeline.
		std::vector<u64> _compute_sets; ///< Currently bound compute descriptor sets.
		bool _compute_sets_bound = false; ///< Whether \ref _compute_bindings contains fresh data.

		/// Resource IDs of swap chain images that have barriers in this command list.
		std::vector<MTL::ResourceID> _used_swapchain_images;
		MTL::Stages _pending_graphics_barriers = 0; ///< Pending barriers for the next graphics command encoder.
		MTL::Stages _pending_compute_barriers = 0; ///< Pending barriers for the next compute command encoder.
		std::vector<NS::SharedPtr<MTL::Buffer>> _binding_buffers; ///< Buffers for argument table buffers.


		/// Updates the given array of GPU addresses with the addresses of the given descriptor sets.
		static void _update_descriptor_set_bindings(
			std::vector<u64> &bindings, u32 first, std::span<const gpu::descriptor_set *const>
		);
		/// Returns \ref _graphics_bindings, initializing it if necessary.
		[[nodiscard]] MTL4::ArgumentTable *_get_graphics_argument_table();
		/// Returns \ref _compute_bindings, initializing it if necessary.
		[[nodiscard]] MTL4::ArgumentTable *_get_compute_argument_table();
		/// Allocates GPU memory for this command buffer and fills it with the given data.
		[[nodiscard]] MTL::GPUAddress _allocate_temporary_buffer(std::span<const std::byte>);
		/// Refreshes graphics descriptor bindings to \ref _graphics_bindings if necessary.
		void _maybe_refresh_graphics_descriptor_set_bindings();
		/// Refreshes compute descriptor bindings to \ref _compute_bindings if necessary.
		void _maybe_refresh_compute_descriptor_set_bindings();

		/// Starts a new compute pass and flushes pending compute barriers to the given command encoder.
		[[nodiscard]] _scoped_compute_encoder _start_compute_pass();


		/// Initializes \ref _buf and \ref _alloc.
		command_list(NS::SharedPtr<MTL4::CommandBuffer> buf, NS::SharedPtr<MTL4::CommandAllocator> alloc) :
			_buf(std::move(buf)), _alloc(std::move(alloc)) {

			{
				auto set_desc = NS::TransferPtr(MTL::ResidencySetDescriptor::alloc()->init());
				NS::Error *err = nullptr;
				_residency_set = NS::TransferPtr(_buf->device()->newResidencySet(set_desc.get(), &err));
				if (err) {
					log().error("Failed to create residency set: {}", err->localizedDescription()->utf8String());
				}
			}

			_buf->useResidencySet(_residency_set.get());
		}
	};

	/// Holds a \p MTL::CommandQueue.
	class command_queue {
		friend adapter;
		friend device;
	protected:
		/// Initializes this object to empty.
		command_queue(std::nullptr_t) {
		}

		/// Returns the result of \p MTL::Device::queryTimestampFrequency().
		[[nodiscard]] f64 get_timestamp_frequency();

		/// Creates command lists to handle synchronization, and calls \p MTL::CommandBuffer::commit() to submit all
		/// the command lists.
		void submit_command_lists(std::span<const gpu::command_list *const>, queue_synchronization);
		/// Creates a new command list, calls \p MTL::CommandBuffer::presentDrawable(), then submits it.
		[[nodiscard]] swap_chain_status present(swap_chain&);

		/// Creates a new command buffer and signals the given fence.
		void signal(fence&);
		/// Creates a new command buffer and signals the given semaphore.
		void signal(timeline_semaphore&, gpu::_details::timeline_semaphore_value_type);

		/// All Metal command queues support timestamp queries.
		[[nodiscard]] queue_capabilities get_capabilities() const;

		/// Checks if this object is valid.
		[[nodiscard]] bool is_valid() const {
			return !!_q;
		}
	private:
		NS::SharedPtr<MTL4::CommandQueue> _q; ///< The command queue.
		/// Mapping from drawable resource IDs to the drawables themselves.
		_details::drawable_mapping *_drawable_mapping = nullptr;

		/// Initializes \ref _q and \ref _drawable_mapping.
		command_queue(NS::SharedPtr<MTL4::CommandQueue> q, _details::drawable_mapping &drawable_mapping) :
			_q(std::move(q)), _drawable_mapping(&drawable_mapping) {
		}
	};
}
