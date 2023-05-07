#pragma once

/// \file
/// Context commands.

#include <vector>

#include "lotus/containers/static_optional.h"
#include "lotus/gpu/common.h"
#include "lotus/renderer/common.h"
#include "misc.h"
#include "assets.h"
#include "execution.h"

namespace lotus::renderer::commands {
	/// Static properties of a command type.
	enum class flags : std::uint32_t {
		none = 0, ///< No flags.

		advances_timer   = 1 << 0, ///< The command advances device timers.
		pass_command     = 1 << 1, ///< This command is only valid within a render pass.
		non_pass_command = 1 << 2, ///< This command is only valid outside a render pass.
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
		enum class timer_index : std::uint32_t {
			invalid = std::numeric_limits<std::underlying_type_t<timer_index>>::max() ///< Invalid timer index.
		};


		/// Placeholder for an invalid command.
		struct invalid {
			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::none;
			}
		};

		/// Uploads contents from the given staging buffer to the highest visible mip of the target image.
		struct upload_image {
			/// Initializes all fields of this struct.
			upload_image(gpu::staging_buffer src, recorded_resources::image2d_view dst) :
				source(std::move(src)), destination(dst) {
			}

			gpu::staging_buffer source; ///< Image data.
			recorded_resources::image2d_view destination; ///< Image to upload to.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Uploads contents from the given staging buffer to the target buffer.
		struct upload_buffer {
			/// Initializes all fields of this struct.
			upload_buffer(
				gpu::buffer &src, std::uint32_t src_off, upload_buffers::allocation_type ty,
				recorded_resources::buffer dst, std::uint32_t off, std::uint32_t sz
			) : source(&src), source_offset(src_off), destination(dst), destination_offset(off), size(sz), type(ty) {
			}

			gpu::buffer *source = nullptr; ///< Buffer data.
			std::uint32_t source_offset = 0; ///< Offset in the source buffer in bytes.
			recorded_resources::buffer destination; ///< Buffer to upload to.
			std::uint32_t destination_offset = 0; ///< Offset of the region to upload to in the destination buffer.
			std::uint32_t size = 0; ///< Size of the region to upload.
			/// The type of \ref source.
			upload_buffers::allocation_type type = upload_buffers::allocation_type::invalid;

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Copies data from one buffer to another.
		struct copy_buffer {
			/// Initializes all fields of this struct.
			copy_buffer(
				recorded_resources::buffer src, recorded_resources::buffer dst,
				std::uint32_t src_off, std::uint32_t dst_off, std::uint32_t sz
			) : source(src), destination(dst), source_offset(src_off), destination_offset(dst_off), size(sz) {
			}

			recorded_resources::buffer source;      ///< The source buffer.
			recorded_resources::buffer destination; ///< The destination buffer.
			std::uint32_t source_offset = 0;      ///< Offset in the source buffer in bytes.
			std::uint32_t destination_offset = 0; ///< Offset in the destination buffer in bytes.
			std::uint32_t size = 0;               ///< Number of bytes to copy.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Builds a bottom level acceleration structure.
		struct build_blas {
			/// Initializes all fields of this struct.
			explicit build_blas(recorded_resources::blas t) : target(std::move(t)) {
			}

			recorded_resources::blas target; ///< The BLAS to build.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Builds a top level acceleration structure.
		struct build_tlas {
			/// Initializes all fields of this struct.
			explicit build_tlas(recorded_resources::tlas t) : target(std::move(t)) {
			}

			recorded_resources::tlas target; ///< The TLAS to build.

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
				std::uint32_t num_instances,
				std::vector<input_buffer_binding> in, std::uint32_t num_verts,
				index_buffer_binding indices, std::uint32_t num_indices,
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

			std::uint32_t instance_count = 0; ///< Number of instances to draw.
			std::uint32_t vertex_count   = 0; ///< Number of vertices.
			std::uint32_t index_count    = 0; ///< Number of indices.
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
				std::uint32_t rg_group_idx,
				std::vector<std::uint32_t> miss_group_idx,
				std::vector<std::uint32_t> hit_group_idx,
				std::uint32_t rec_depth,
				std::uint32_t payload_size,
				std::uint32_t attr_size,
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

			std::uint32_t raygen_shader_group_index = 0; ///< Index of the ray generation shader group.
			std::vector<std::uint32_t> miss_group_indices; ///< Indices of the miss shader groups.
			std::vector<std::uint32_t> hit_group_indices;  ///< Indices of the hit shader groups.

			std::uint32_t max_recursion_depth = 0; ///< Maximum recursion depth for the rays.
			std::uint32_t max_payload_size = 0;    ///< Maximum payload size.
			std::uint32_t max_attribute_size = 0;  ///< Maximum attribute size.

			cvec3u32 num_threads; ///< Number of threads to spawn.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
				return flags::advances_timer | flags::non_pass_command;
			}
		};

		/// Presents the given swap chain.
		struct present {
			/// Initializes the target.
			explicit present(swap_chain t) : target(std::move(t)) {
			}

			swap_chain target; ///< The swap chain to present.

			/// Returns the properties of this command.
			[[nodiscard]] constexpr inline static flags get_flags() {
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
				return flags::none;
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
				return flags::none;
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
		/// Initializes this command.
		template <typename ...Args> explicit command(std::u8string_view desc, Args &&...args) :
			value(std::forward<Args>(args)...), description(desc) {
		}
		/// \overload
		template <typename ...Args> explicit command(const char8_t (&desc)[], Args &&...args) :
			command(std::u8string_view(desc), std::forward<Args>(args)...) {
		}
		/// \overload
		template <typename ...Args> explicit command(
			static_optional<std::u8string, should_register_debug_names> desc, Args &&...args
		) : value(std::forward<Args>(args)...), description(std::move(desc)) {
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

			commands::upload_image, // TODO: get rid of these two
			commands::upload_buffer,
			commands::copy_buffer,
			commands::build_blas,
			commands::build_tlas,

			commands::begin_pass,
			commands::draw_instanced, // TODO: very large
			commands::end_pass,

			commands::dispatch_compute,
			commands::trace_rays, // TODO: 176 bytes

			commands::present,

			commands::start_timer,
			commands::end_timer,
			commands::pause_for_debugging
		> value; ///< The value of this command.
		/// Debug description of this command.
		[[no_unique_address]] static_optional<std::u8string, should_register_debug_names> description;
	};
}
