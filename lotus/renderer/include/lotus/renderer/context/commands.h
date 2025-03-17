#pragma once

/// \file
/// Context commands.

#include <vector>

#include "lotus/containers/static_optional.h"
#include "lotus/gpu/common.h"
#include "lotus/renderer/common.h"
#include "misc.h"
#include "assets.h"

namespace lotus::renderer::commands {
	/// Static properties of a command type.
	enum class flags : u32 {
		none = 0, ///< No flags.

		advances_timer               = 1 << 0, ///< The command advances device timers.
		pass_command                 = 1 << 1, ///< This command is only valid within a render pass.
		non_pass_command             = 1 << 2, ///< This command is only valid outside a render pass.
		/// Timeline semaphore release events before or after this command make no practical difference.
		dependency_release_unordered = 1 << 3,
	};
}
namespace lotus::enums {
	/// \ref renderer::commands::flags is a bit mask.
	template <> struct is_bit_mask<renderer::commands::flags> : public std::true_type {
	};
}

namespace lotus::renderer {
	namespace commands {
		/// Opaque index type for timers.
		enum class timer_index : u32 {
			invalid = std::numeric_limits<std::underlying_type_t<timer_index>>::max() ///< Invalid timer index.
		};


		/// Placeholder for an invalid command.
		struct invalid {
			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::none;
			}
		};

		/// Special command indicating the start of a batch. This command is inserted to the command queue of all
		/// queues, and cannot be inserted any other way.
		struct start_of_batch {
			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::none;
			}
		};

		/// Copies data from one buffer to another.
		struct copy_buffer {
			/// Initializes all fields of this struct.
			copy_buffer(
				recorded_resources::buffer src, recorded_resources::buffer dst,
				usize src_off, usize dst_off, usize sz
			) : source(src), destination(dst), source_offset(src_off), destination_offset(dst_off), size(sz) {
			}

			recorded_resources::buffer source;      ///< The source buffer.
			recorded_resources::buffer destination; ///< The destination buffer.
			usize source_offset      = 0; ///< Offset in the source buffer in bytes.
			usize destination_offset = 0; ///< Offset in the destination buffer in bytes.
			usize size               = 0; ///< Number of bytes to copy.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Copies data from a buffer to an image.
		struct copy_buffer_to_image {
			/// Initializes all fields of this struct.
			copy_buffer_to_image(
				recorded_resources::buffer src, recorded_resources::image2d_view dst,
				gpu::staging_buffer::metadata meta, usize src_off, cvec2u32 dst_off
			) :
				source(src),
				destination(dst),
				staging_buffer_meta(std::move(meta)),
				source_offset(src_off),
				destination_offset(dst_off) {
			}

			recorded_resources::buffer       source;      ///< The source buffer.
			recorded_resources::image2d_view destination; ///< The destination image.
			gpu::staging_buffer::metadata staging_buffer_meta; ///< Metadata of the staging buffer.
			usize    source_offset      = 0;    ///< Offset in the source buffer in bytes.
			cvec2u32 destination_offset = zero; ///< Offset in the destination image in pixels.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Builds a bottom level acceleration structure.
		struct build_blas {
			/// Initializes all fields of this struct.
			build_blas(recorded_resources::blas t, std::vector<geometry_buffers_view> geom) :
				target(std::move(t)), geometry(std::move(geom)) {
			}

			recorded_resources::blas target; ///< The BLAS to save build results into.
			std::vector<geometry_buffers_view> geometry; ///< All geometry for the BLAS.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Builds a top level acceleration structure.
		struct build_tlas {
			/// Initializes all fields of this struct.
			build_tlas(recorded_resources::tlas t, std::vector<blas_instance> insts) :
				target(std::move(t)), instances(std::move(insts)) {
			}

			recorded_resources::tlas target; ///< The TLAS to build.
			std::vector<blas_instance> instances; ///< All BLAS instances for this TLAS.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Starts a render pass.
		struct begin_pass {
			/// Initializes the render target(s).
			begin_pass(std::vector<image2d_color> color_rts, image2d_depth_stencil ds_rt, cvec2u32 sz) :
				color_render_targets(std::move(color_rts)), depth_stencil_target(std::move(ds_rt)),
				render_target_size(sz) {
			}

			std::vector<image2d_color> color_render_targets; ///< Color render targets.
			image2d_depth_stencil depth_stencil_target; ///< Depth stencil render target.
			cvec2u32 render_target_size; ///< The size of the render target.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				// passes can only be started when no other pass is running
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Draws a number of instances of a given mesh.
		struct draw_instanced {
			/// Initializes all fields of this struct.
			draw_instanced(
				u32 num_instances,
				std::vector<input_buffer_binding> in, u32 num_verts,
				index_buffer_binding indices, u32 num_indices,
				gpu::primitive_topology t,
				_details::numbered_bindings resources,
				assets::handle<assets::shader> vs,
				assets::handle<assets::shader> ps,
				graphics_pipeline_state s
			) :
				inputs(std::move(in)), index_buffer(std::move(indices)), resource_bindings(std::move(resources)),
				vertex_shader(std::move(vs)), pixel_shader(std::move(ps)), state(std::move(s)),
				instance_count(num_instances), vertex_count(num_verts), index_count(num_indices), topology(t) {
			}

			std::vector<input_buffer_binding> inputs;       ///< Input buffers.
			index_buffer_binding              index_buffer; ///< Index buffer, if applicable.

			_details::numbered_bindings resource_bindings; ///< Resource bindings.
			// TODO more shaders
			assets::handle<assets::shader> vertex_shader; ///< Vertex shader.
			assets::handle<assets::shader> pixel_shader;  ///< Pixel shader.
			graphics_pipeline_state state; ///< Render pipeline state.

			u32 instance_count = 0; ///< Number of instances to draw.
			u32 vertex_count   = 0; ///< Number of vertices.
			u32 index_count    = 0; ///< Number of indices.
			/// Primitive topology.
			gpu::primitive_topology topology = gpu::primitive_topology::point_list;

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::pass_command;
			}
		};

		/// Ends the current render pass.
		struct end_pass {
			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				// passes can only be ended when a pass is running
				return flags::advances_timer | flags::pass_command;
			}
		};

		/// Compute shader dispatch.
		struct dispatch_compute {
			/// Initializes all fields of this struct.
			dispatch_compute(
				_details::numbered_bindings rsrc,
				assets::handle<assets::shader> compute_shader,
				cvec3u32 numgroups
			) : resources(std::move(rsrc)),
				shader(std::move(compute_shader)),
				num_thread_groups(numgroups) {
			}

			_details::numbered_bindings resources; ///< All resource bindings.
			assets::handle<assets::shader> shader; ///< The shader.
			cvec3u32 num_thread_groups = uninitialized; ///< Number of thread groups.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Generates and traces rays.
		struct trace_rays {
			/// Initializes all fields of this struct.
			trace_rays(
				_details::numbered_bindings b,
				std::vector<shader_function> hg_shaders,
				std::vector<gpu::hit_shader_group> groups,
				std::vector<shader_function> gen_shaders,
				u32 rg_group_idx,
				std::vector<u32> miss_group_idx,
				std::vector<u32> hit_group_idx,
				u32 rec_depth,
				u32 payload_size,
				u32 attr_size,
				cvec3u32 threads
			) :
				resource_bindings(std::move(b)),
				hit_group_shaders(std::move(hg_shaders)),
				hit_groups(std::move(groups)),
				general_shaders(std::move(gen_shaders)),
				raygen_shader_group_index(rg_group_idx),
				miss_group_indices(std::move(miss_group_idx)),
				hit_group_indices(std::move(hit_group_idx)),
				max_recursion_depth(rec_depth),
				max_payload_size(payload_size),
				max_attribute_size(attr_size),
				num_threads(threads) {
			}

			_details::numbered_bindings resource_bindings; ///< All resource bindings.

			std::vector<shader_function> hit_group_shaders; ///< Ray tracing shaders.
			std::vector<gpu::hit_shader_group> hit_groups;  ///< Hit groups.
			std::vector<shader_function> general_shaders;   ///< General callable shaders.

			u32 raygen_shader_group_index = 0; ///< Index of the ray generation shader group.
			std::vector<u32> miss_group_indices; ///< Indices of the miss shader groups.
			std::vector<u32> hit_group_indices;  ///< Indices of the hit shader groups.

			u32 max_recursion_depth = 0; ///< Maximum recursion depth for the rays.
			u32 max_payload_size = 0;    ///< Maximum payload size.
			u32 max_attribute_size = 0;  ///< Maximum attribute size.

			cvec3u32 num_threads; ///< Number of threads to spawn.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Presents the given swap chain.
		struct present {
			/// Initializes the target.
			explicit present(recorded_resources::swap_chain t) : target(std::move(t)) {
			}

			recorded_resources::swap_chain target; ///< The swap chain to present.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Signals that a dependency has been released.
		struct release_dependency {
			/// Initializes the dependency.
			explicit release_dependency(recorded_resources::dependency t) : target(std::move(t)) {
			}

			recorded_resources::dependency target; ///< The dependency handle.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::non_pass_command | flags::dependency_release_unordered;
			}
		};

		/// Signals that a dependency has been acquired.
		struct acquire_dependency {
			/// Initializes the dependency.
			explicit acquire_dependency(recorded_resources::dependency t) : target(std::move(t)) {
			}

			recorded_resources::dependency target; ///< The dependency handle.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				// TODO: can semaphores be waited on within passes?
				return flags::advances_timer | flags::non_pass_command;
			}
		};


		// utililty commands
		/// Starts a GPU timer.
		struct start_timer {
			/// Initializes all fields of this struct.
			start_timer(std::u8string n, timer_index i) : name(std::move(n)), index(i) {
			}

			std::u8string name; ///< The name of this timer.
			timer_index index; ///< The index of this timer.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::dependency_release_unordered;
			}
		};

		/// Ends a GPU timer.
		struct end_timer {
			/// Initializes all fields of this struct.
			explicit end_timer(timer_index i) : index(i) {
			}

			timer_index index; ///< The index of the timer.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::dependency_release_unordered;
			}
		};

		/// Pause execution into the debugger on the CPU when the command is being handled.
		struct pause_for_debugging {
			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::none;
			}
		};
	}
	/// A union of all renderer context command types.
	struct command {
		/// Initializes this command to empty.
		command(std::u8string_view desc, global_submission_index i) : index(i), description(desc) {
		}

		/// Returns the flags of the command.
		[[nodiscard]] commands::flags get_flags() const {
			return std::visit(
				[]<typename Cmd>(const Cmd&) -> commands::flags {
					return Cmd::get_flags();
				},
				value
			);
		}

		std::variant<
			commands::invalid,

			commands::start_of_batch,

			commands::copy_buffer,
			commands::copy_buffer_to_image,
			commands::build_blas,
			commands::build_tlas,

			commands::begin_pass, // TODO: somewhat large
			commands::draw_instanced, // TODO: very large
			commands::end_pass,

			commands::dispatch_compute,
			commands::trace_rays, // TODO: large

			commands::present,

			commands::release_dependency,
			commands::acquire_dependency,

			commands::start_timer,
			commands::end_timer,
			commands::pause_for_debugging
		> value; ///< The value of this command.
		/// Denotes the order in which these commands are submitted from the CPU.
		[[no_unique_address]] global_submission_index index = global_submission_index::zero;
		/// Debug description of this command.
		[[no_unique_address]] static_optional<std::u8string, should_register_debug_names> description;
	};
}
