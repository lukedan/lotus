#pragma once

/// \file
/// Common graphics-related structures. This is the only file that can be included by backends.

#include <cassert>
#include <array>
#include <compare>
#include <string_view>
#include <span>
#include <variant>

#include "lotus/common.h"
#include "lotus/math/aab.h"

namespace lotus::gpu {
	// one goal of these forward declarations are used to pass spans of pointers to derived classes to base class
	// functions. implementation of base class functions should be defined where the derived class has been defined
	// (i.e., the proper header has been included)
	class bottom_level_acceleration_structure_geometry;
	class bottom_level_acceleration_structure;
	class buffer;
	class command_list;
	class descriptor_set_layout;
	class descriptor_set;
	class fence;
	class image2d_view;
	class sampler;
	class shader_binary;
	class staging_buffer;
	class timeline_semaphore;
	class top_level_acceleration_structure;


	/// Base class of all image types.
	class image {
	protected:
		/// Prevent objects of this type from being created directly.
		~image() = default;
	};
	/// Base class of all image view types.
	class image_view {
	protected:
		/// Prevent objects of this type from being created directly.
		~image_view() = default;
	};


	/// Options for context creation.
	enum class context_options : std::uint8_t {
		none = 0, ///< None.
		enable_validation = 1 << 0, ///< Enables all validation.

		num_enumerators = 1 ///< Total number of enumerators.
	};
}
namespace lotus {
	/// Enable bitwise operations for \ref gpu::context_options.
	template <> struct enable_enum_bitwise_operators<gpu::context_options> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref gpu::context_options.
	template <> struct enable_enum_is_empty<gpu::context_options> : public std::true_type {
	};
}

namespace lotus::gpu {
	/// The format of a pixel.
	enum class format {
		none, ///< No specific type.

		d32_float_s8,
		d32_float,
		d24_unorm_s8,
		d16_unorm,


		r8_unorm,
		r8_snorm,
		r8_uint,
		r8_sint,

		r8g8_unorm,
		r8g8_snorm,
		r8g8_uint,
		r8g8_sint,

		r8g8b8a8_unorm,
		r8g8b8a8_snorm,
		r8g8b8a8_srgb,
		r8g8b8a8_uint,
		r8g8b8a8_sint,

		b8g8r8a8_unorm,
		b8g8r8a8_srgb,


		r16_unorm,
		r16_snorm,
		r16_uint,
		r16_sint,
		r16_float,

		r16g16_unorm,
		r16g16_snorm,
		r16g16_uint,
		r16g16_sint,
		r16g16_float,

		r16g16b16a16_unorm,
		r16g16b16a16_snorm,
		r16g16b16a16_uint,
		r16g16b16a16_sint,
		r16g16b16a16_float,


		r32_uint,
		r32_sint,
		r32_float,
	
		r32g32_uint,
		r32g32_sint,
		r32g32_float,
	
		r32g32b32_uint,
		r32g32b32_sint,
		r32g32b32_float,
	
		r32g32b32a32_uint,
		r32g32b32a32_sint,
		r32g32b32a32_float,

		num_enumerators ///< The total number of enumerators.
	};
	/// Properties of a format.
	struct format_properties {
	public:
		/// Data type used by the format.
		enum class data_type {
			unknown,        ///< Unknown.
			unsigned_norm,  ///< Unsigned value normalized in [0, 1].
			signed_norm,    ///< Signed value normalized in [-1, 1].
			srgb,           ///< sRGB values in [0, 1].
			unsigned_int,   ///< Unsigned integer.
			signed_int,     ///< Signed integer.
			floating_point, ///< Floating-point number.
		};
		/// The order of channels. One or more channels may not present in the format.
		enum class channel_order {
			unknown,       ///< Unknown.
			rgba,          ///< RGBA.
			bgra,          ///< BGRA.
			depth_stencil, ///< Depth-stencil.
		};

		/// No initialization.
		format_properties(uninitialized_t) {
		}
		/// Initializes all bit values to zero and \ref type to \ref data_type::unknown.
		constexpr format_properties(zero_t) :
			format_properties(0, 0, 0, 0, 0, 0, data_type::unknown, channel_order::unknown) {
		}
		/// Creates an object for a color format.
		[[nodiscard]] constexpr inline static format_properties create_color(
			std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a, data_type ty, channel_order o
		) {
			return format_properties(r, g, b, a, 0, 0, ty, o);
		}
		/// Creates an object for a depth-stencil format.
		[[nodiscard]] constexpr inline static format_properties create_depth_stencil(
			std::uint8_t d, std::uint8_t s, data_type ty
		) {
			return format_properties(0, 0, 0, 0, d, s, ty, channel_order::depth_stencil);
		}

		/// Retrieves the \ref format_properties for the given \ref format.
		[[nodiscard]] static const format_properties &get(format);

		/// Finds the pixel format that has the exact parameters.
		[[nodiscard]] static format find_exact_rgba(
			std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a, data_type type
		);

		/// Bits per pixel.
		[[nodiscard]] constexpr std::uint8_t bits_per_pixel() const {
			return _bits_per_pixel;
		}
		/// Bytes per pixel.
		[[nodiscard]] constexpr std::uint8_t bytes_per_pixel() const {
			return _bytes_per_pixel;
		}

		/// Returns whether this format has any color components.
		[[nodiscard]] constexpr bool has_color() const {
			return red_bits > 0 || green_bits > 0 || blue_bits > 0 || alpha_bits > 0;
		}
		/// Returns whether this format has any depth or stencil components.
		[[nodiscard]] constexpr bool has_depth_stencil() const {
			return depth_bits > 0 || stencil_bits > 0;
		}

		std::uint8_t red_bits;     ///< Number of bits for the red channel.
		std::uint8_t green_bits;   ///< Number of bits for the green channel.
		std::uint8_t blue_bits;    ///< Number of bits for the blue channel.
		std::uint8_t alpha_bits;   ///< Number of bits for the alpha channel.
		std::uint8_t depth_bits;   ///< Number of bits for the depth channel.
		std::uint8_t stencil_bits; ///< Number of bits for the stencil channel.
		data_type type; ///< Data type for all the channels except for stencil.
		channel_order order; ///< Order of the channels.
	protected:
		/// Initializes all fields.
		constexpr format_properties(
			std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a,
			std::uint8_t d, std::uint8_t s, data_type ty, channel_order o
		) :
			red_bits(r), green_bits(g), blue_bits(b), alpha_bits(a),
			depth_bits(d), stencil_bits(s), type(ty), order(o),
			_bits_per_pixel(r + g + b + a + d + s), _bytes_per_pixel((_bits_per_pixel + 7) / 8) {
		}

		std::uint8_t _bits_per_pixel;
		std::uint8_t _bytes_per_pixel;
	};

	/// Format used by the index buffer.
	enum class index_format {
		uint16, ///< 16-bit unsigned integers.
		uint32, ///< 32-bit unsigned integers.

		num_enumerators ///< The number of enumerators.
	};

	/// Specifies the tiling of an image.
	enum class image_tiling {
		/// The image is stored as a row-major matrix of pixels, with potential padding between rows and array/depth
		/// slices.
		row_major,
		optimal, ///< The image is stored in an undefined tiling that's optimal for rendering.

		num_enumerators ///< The number of enumerators.
	};


	/// A bitmask for the four color channels.
	enum class channel_mask : std::uint8_t {
		none  = 0,       ///< Empty value.
		red   = 1u << 0, ///< The red channel.
		green = 1u << 1, ///< The green channel.
		blue  = 1u << 2, ///< The blue channel.
		alpha = 1u << 3, ///< The alpha channel.

		all = red | green | blue | alpha, ///< All channels.
		num_enumerators = 4 ///< The number of channels.
	};
}
namespace lotus {
	/// Enable bitwise operations for \ref gpu::channel_mask.
	template <> struct enable_enum_bitwise_operators<gpu::channel_mask> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref gpu::channel_mask.
	template <> struct enable_enum_is_empty<gpu::channel_mask> : public std::true_type {
	};
}

namespace lotus::gpu {
	/// Aspects of an image such as color, depth, or stencil.
	enum class image_aspect_mask : std::uint8_t {
		none    = 0,       ///< None.
		color   = 1u << 0, ///< Color aspect.
		depth   = 1u << 1, ///< Depth aspect.
		stencil = 1u << 2, ///< Stencil aspect.

		num_enumerators = 3 ///< The total number of aspects.
	};
}
namespace lotus {
	/// Enable bitwise operations for \ref gpu::image_aspect_mask.
	template <> struct enable_enum_bitwise_operators<gpu::image_aspect_mask> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref gpu::image_aspect_mask.
	template <> struct enable_enum_is_empty<gpu::image_aspect_mask> : public std::true_type {
	};
}

namespace lotus::gpu {
	/// A specific shader stage or all shader stages.
	enum class shader_stage {
		all,                   ///< All used stages.
		vertex_shader,         ///< Vertex shader.
		// TODO tessellation shaders
		geometry_shader,       ///< Geometry shader.
		pixel_shader,          ///< Pixel shader.
		compute_shader,        ///< Compute shader.

		callable_shader,       ///< Callable shader.
		ray_generation_shader, ///< Ray-tracing ray generation shader.
		intersection_shader,   ///< Ray-tracing intersection shader.
		any_hit_shader,        ///< Ray-tracing any hit shader.
		closest_hit_shader,    ///< Ray-tracing closest hit shader.
		miss_shader,           ///< Ray-tracing miss shader.

		num_enumerators ///< The number of available stages.
	};

	/// A factor used for blending.
	enum class blend_factor {
		zero,                        ///< Zero.
		one,                         ///< One.
		source_color,                ///< Output color RGB.
		one_minus_source_color,      ///< One minus output color RGB.
		destination_color,           ///< Color RGB on the destination surface.
		one_minus_destination_color, ///< One minus the color RGB on the destination surface.
		source_alpha,                ///< Output color alpha.
		one_minus_source_alpha,      ///< One minus output color alpha.
		destination_alpha,           ///< Color alpha on the destination surface.
		one_minus_destination_alpha, ///< One minus color alpha on the destination surface.
		// TODO other modes

		num_enumerators ///< The total number of enumerators.
	};

	/// Dictates how colors are blended onto the destination surface.
	enum class blend_operation {
		add,              ///< The result is the sum of the two operands.
		subtract,         ///< The result is the first operand minus the second operand.
		reverse_subtract, ///< The result is the second operand minus the first operand.
		// TODO are VK_BLEND_OP_MIN and D3D12_BLEND_OP_MIN equivalent?
		min,              ///< The minimum of the two operands.
		max,              ///< The maximum of the two operands.

		num_enumerators ///< The total number of enumerators.
	};

	/// Used to decide if a triangle is front-facing.
	enum class front_facing_mode {
		/// The triangle will be considered front-facing if the vertices are ordered clockwise.
		clockwise,
		/// The triangle will be considered front-facing if the vertices are ordered counter-clockwise.
		counter_clockwise,

		num_enumerators ///< The total number of enumerators.
	};

	/// Specifies if and how triangles are culled based on the direction they're facing. The facing direction of a
	/// triangle is determined by \ref front_facing_mode.
	enum class cull_mode {
		none,       ///< No culling.
		cull_front, ///< Cull all front-facing triangles.
		cull_back,  ///< Cull all back-facing triangles.

		num_enumerators ///< The total number of enumerators.
	};

	/// Specifies what stencil operation is used.
	enum class stencil_operation {
		keep,                ///< Keep the original value.
		zero,                ///< Reset the value to zero.
		replace,             ///< Replace the value with the specified reference value.
		increment_and_clamp, ///< Increment the value by 1, and clamp it to the maximum value.
		decrement_and_clamp, ///< Decrement the value by 1, and clamp it to the minimum value.
		bitwise_invert,      ///< Bitwise invert the value.
		increment_and_wrap,  ///< Increment the value by 1, wrapping around to 0 if necessary.
		decrement_and_wrap,  ///< Decrement the value by 1, wrapping around to the maximum value if necessary.

		num_enumerators ///< The total number of enumerators.
	};

	/// Indicates how data is used for an input buffer.
	enum class input_buffer_rate {
		per_vertex,   ///< Indicates that the buffer data is per-vertex.
		per_instance, ///< Indicates that the buffer data is per-instance.

		num_enumerators ///< The total number of enumerators.
	};

	/// Primitive topology.
	enum class primitive_topology {
		point_list,     ///< A list of points.
		line_list,      ///< A list of lines - every two vertices define a line.
		line_strip,     ///< A line strip - there's a line between each vertex and the previous vertex.
		triangle_list,  ///< A list of triangles - every other three vertices define a triangle.
		triangle_strip, ///< A strip of triangles - every three consecutive vertices define a triangle.
		/// Like \ref line_list, but with additional vertices only accessible by the geometry shader.
		line_list_with_adjacency,
		/// Like \ref line_strip, but with additional vertices only accessible by the geometry shader.
		line_strip_with_adjacency,
		/// Like \ref triangle_list, but with additional vertices only accessible by the geometry shader.
		triangle_list_with_adjacency,
		/// Like \ref triangle_strip, but with additional vertices only accessible by the geometry shader.
		triangle_strip_with_adjacency,
		// TODO patch_list

		num_enumerators ///< The total number of enumerators.
	};


	/// Determines what kind of filtering is applied when sampling an image.
	enum class filtering : std::uint8_t {
		nearest, ///< The nearest texel or mip level is used.
		linear,  ///< Linearly interpolates neighboring texels or mip levels.

		num_enumerators ///< The total number of enumerators.
	};

	/// Determines how the sampling coordinates are transformed before fetching texels and filtering.
	enum class sampler_address_mode : std::uint8_t {
		repeat, ///< The texture repeats beyond its borders.
		mirror, ///< The texture mirrors beyond its borders.
		/// The coordinate is clamped to the border, meaning that the border texel will be used for all values out of
		/// range.
		clamp,
		border, ///< A specified border color is used for coordinates out of range.
		// TODO dx12 mirror_once and vulkan clamp_to_edge

		num_enumerators ///< The total number of enumerators.
	};

	/// Determines when a comparison returns \p true.
	enum class comparison_function : std::uint8_t {
		never,            ///< Comparison result is always \p false.
		less,             ///< Returns \p true if source data is less than destination data.
		equal,            ///< Returns \p true if the two values are equal.
		less_or_equal,    ///< Returns \p true if the source data is less than or equal to the destination data.
		greater,          ///< Returns \p true if the source data is greater than the destination data.
		not_equal,        ///< Returns \p true if the two values are not equal.
		greater_or_equal, ///< Returns \p true if the source data is greater than or equal to the destination data.
		always,           ///< Comparison result is always \p true.

		num_enumerators ///< The total number of enumerators.
	};


	/// Enum indicating how values are loaded from a resource during a render pass.
	enum class pass_load_operation : std::uint8_t {
		discard, ///< Indicates that a render pass does not depend on the previous value of a resource.
		preserve, ///< Indicates that a render pass reads values from a resource that has been previously written to.
		clear, ///< Indicates that the resource will be cleared using a value specified when starting a render pass.

		num_enumerators ///< The total number of enumerators.
	};

	/// Enum indicating how values are stored into a resource during a render pass.
	enum class pass_store_operation : std::uint8_t {
		discard, ///< Indicates that the written values will not be needed in the future.
		preserve, ///< Indicates that the written values will be read in the future.

		num_enumerators ///< The total number of enumerators.
	};

	/// The state of a synchronization object.
	enum class synchronization_state : std::uint8_t {
		unset = 0, ///< The synchronization object has not been set.
		set = 1    ///< The synchronization object has been set.
	};
	/// Flips the given \ref synchronization_state.
	[[nodiscard]] constexpr inline synchronization_state operator!(synchronization_state s) {
		return s == synchronization_state::unset ? synchronization_state::set : synchronization_state::unset;
	}

	/// The type of a descriptor.
	enum class descriptor_type : std::uint8_t {
		sampler,                ///< A sampler.
		read_only_image,        ///< An image that can only be read.
		read_write_image,       ///< An image that can be read from or written to.
		read_only_buffer,       ///< A structured buffer that can only be read.
		read_write_buffer,      ///< A structured buffer that can be read from or written to.
		constant_buffer,        ///< A small buffer containing constants.
		acceleration_structure, ///< A ray-tracing acceleration structure.

		num_enumerators ///< The total number of enumerators.
	};

	/// The usage/state of a buffer resource.
	struct buffer_usage {
		/// Values indicating a single usage.
		enum values {
			index_buffer,           ///< Used as an index buffer.
			vertex_buffer,          ///< Used as a vertex buffer.
			read_only_buffer,       ///< Used as a read-only uniform buffer.
			read_write_buffer,      ///< Used as a read-write buffer.
			copy_source,            ///< Source for copy operations.
			copy_destination,       ///< Target for copy operations.
			acceleration_structure, ///< Acceleration structure.

			num_enumerators ///< The total number of enumerators.
		};
		/// Used for specifying multiple usages. These values correspond directly to those in \ref values.
		enum class mask : std::uint8_t {
			none = 0, ///< No usage.

			index_buffer           = 1 << values::index_buffer,
			vertex_buffer          = 1 << values::vertex_buffer,
			read_only_buffer       = 1 << values::read_only_buffer,
			read_write_buffer      = 1 << values::read_write_buffer,
			copy_source            = 1 << values::copy_source,
			copy_destination       = 1 << values::copy_destination,
			acceleration_structure = 1 << values::acceleration_structure,
		};

		/// No initialization.
		buffer_usage(uninitialized_t) {
		}
		/// Initializes \ref _value.
		constexpr buffer_usage(values val) : _value(val) {
		}

		/// Allow explicit conversion to integers.
		template <
			typename Number, std::enable_if_t<std::is_integral_v<Number>, int> = 0
		> constexpr explicit operator Number() const {
			static_assert(
				std::numeric_limits<Number>::max() >= values::num_enumerators,
				"Potential invalid conversion to raw number"
			);
			return static_cast<Number>(_value);
		}

		/// Default comparisons.
		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(buffer_usage, buffer_usage) = default;

		/// Converts the value into a bit in the mask.
		[[nodiscard]] constexpr mask as_mask() const {
			return static_cast<mask>(1 << _value);
		}
	protected:
		values _value; ///< The value.
	};
}
namespace lotus {
	/// Enable bitwise operations for \ref gpu::buffer_usage::mask.
	template <> struct enable_enum_bitwise_operators<gpu::buffer_usage::mask> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref gpu::buffer_usage::mask.
	template <> struct enable_enum_is_empty<gpu::buffer_usage::mask> : public std::true_type {
	};
}

namespace lotus::gpu {
	/// The usage/state of an image resource.
	struct image_usage {
	public:
		/// Values indicating a single usage.
		enum values {
			color_render_target,         ///< The image can be used as a color render target.
			depth_stencil_render_target, ///< The image can be used as a depth-stencil render target.
			read_only_texture,           ///< A color/depth-stencil texture that can be read in shaders.
			read_write_color_texture,    ///< A color texture that can be written to.
			present,                     ///< State indicating that the image has been used for presenting.
			copy_source,                 ///< Source for copy operations.
			copy_destination,            ///< Destination for copy operations.

			initial, ///< Special value indicating that this image is just created.

			num_enumerators ///< The total number of enumerators.
		};
		/// Used for specifying multiple usages. These values correspond directly to those in \ref values.
		enum class mask : std::uint8_t {
			none = 0, ///< No usage.

			color_render_target         = 1 << values::color_render_target,
			depth_stencil_render_target = 1 << values::depth_stencil_render_target,
			read_only_texture           = 1 << values::read_only_texture,
			read_write_color_texture    = 1 << values::read_write_color_texture,
			present                     = 1 << values::present,
			copy_source                 = 1 << values::copy_source,
			copy_destination            = 1 << values::copy_destination,
		};

		/// No initialization.
		image_usage(uninitialized_t) {
		}
		/// Initializes \ref _value.
		constexpr image_usage(values val) : _value(val) {
		}

		/// Allow explicit conversion to integers.
		template <
			typename Number, std::enable_if_t<std::is_integral_v<Number>, int> = 0
		> constexpr explicit operator Number() const {
			static_assert(
				std::numeric_limits<Number>::max() >= values::num_enumerators,
				"Potential invalid conversion to raw number"
			);
			return static_cast<Number>(_value);
		}

		/// Equality.
		[[nodiscard]] friend constexpr bool operator==(image_usage, image_usage) = default;

		/// Converts the value into a bit in the mask.
		[[nodiscard]] constexpr mask as_mask() const {
			return static_cast<mask>(1 << _value);
		}
	protected:
		values _value; ///< The value.
	};
}
namespace lotus {
	/// Enable bitwise operations for \ref gpu::image_usage::mask.
	template <> struct enable_enum_bitwise_operators<gpu::image_usage::mask> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref gpu::image_usage::mask.
	template <> struct enable_enum_is_empty<gpu::image_usage::mask> : public std::true_type {
	};
}


namespace lotus::gpu {
	/// Opaque type that holds the index of a type of memory. This can hold backend-specific, potentially
	/// runtime-generated values.
	enum class memory_type_index : std::uint8_t {
		invalid = 0xFF, ///< Invalid value.
	};

	/// Properties of a memory block.
	enum class memory_properties : std::uint8_t {
		none = 0, ///< Empty properties.

		device_local = 1 << 0, ///< The memory is located near the graphics device.
		host_visible = 1 << 1, ///< The memory can be mapped and written to / read from by the host.
		host_cached  = 1 << 2, ///< Host reads of the memory is cached.

		num_enumerators = 3, ///< Number of enumerators.
	};
}
namespace lotus {
	/// Enable bitwise operations for \ref gpu::image_aspect_mask.
	template <> struct enable_enum_bitwise_operators<gpu::memory_properties> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref gpu::image_aspect_mask.
	template <> struct enable_enum_is_empty<gpu::memory_properties> : public std::true_type {
	};
}

namespace lotus::gpu {
	/// The status of a swap chain.
	enum class swap_chain_status {
		ok,          ///< The swap chain is functioning properly.
		suboptimal,  ///< The swap chain does not match the surface properties exactly but still works.
		unavailable, ///< The swap chain is no longer usable.
	};


	/// Converts the given \ref descriptor_type to a \ref image_usage. Returns \ref image_usage::num_enumerators
	/// for invalid descriptor types.
	[[nodiscard]] constexpr image_usage to_image_usage(descriptor_type ty) {
		switch (ty) {
		case descriptor_type::read_only_image:
			return image_usage::read_only_texture;
		case descriptor_type::read_write_image:
			return image_usage::read_write_color_texture;
		default:
			return image_usage::num_enumerators;
		}
	}


	/// Clear value for a color render target.
	struct color_clear_value {
	public:
		/// Initializes the value to integer zero.
		color_clear_value(zero_t) : value(std::in_place_type<cvec4<std::uint64_t>>, 0, 0, 0, 0) {
		}
		/// Initializes this struct with the given integral value.
		color_clear_value(cvec4<std::uint64_t> v) : value(v) {
		}
		/// Initializes this struct with the given floating-point value.
		color_clear_value(cvec4d v) : value(v) {
		}

		/// The clear color value that can be either floating-point or integral.
		std::variant<cvec4<std::uint64_t>, cvec4d> value;
	};


	/// Properties of an adapter.
	struct adapter_properties {
		/// No initialization.
		adapter_properties(uninitialized_t) {
		}

		// TODO allocator
		std::u8string name; ///< The name of this device.
		bool is_software; ///< Whether this is a software adapter.
		bool is_discrete; ///< Whether this is a discrete adapter.

		std::size_t constant_buffer_alignment;           ///< Alignment required for constant buffers.
		std::size_t acceleration_structure_alignment;    ///< Alignment required for acceleration structures.
		std::size_t shader_group_handle_size;            ///< Size of a shader record.
		std::size_t shader_group_handle_alignment;       ///< Alignment required for a single shader record.
		std::size_t shader_group_handle_table_alignment; ///< Alignment required for a table of shader records.
	};

	/// Describes how color blending is carried out for a single render target.
	struct render_target_blend_options {
	public:
		/// No initialization.
		render_target_blend_options(uninitialized_t) {
		}
		/// Initializes \ref enabled to \p false, and other fields to as if no blending is applied.
		[[nodiscard]] constexpr static render_target_blend_options disabled() {
			return zero;
		}
		/// Initializes \ref enabled to \p true and other fields with the given values.
		[[nodiscard]] constexpr inline static render_target_blend_options create_custom(
			blend_factor src_color, blend_factor dst_color, blend_operation color_op,
			blend_factor src_alpha, blend_factor dst_alpha, blend_operation alpha_op,
			channel_mask mask
		) {
			render_target_blend_options result = zero;
			result.enabled           = true;
			result.source_color      = src_color;
			result.destination_color = dst_color;
			result.color_operation   = color_op;
			result.source_alpha      = src_alpha;
			result.destination_alpha = dst_alpha;
			result.alpha_operation   = alpha_op;
			result.write_mask        = mask;
			return result;
		}

		/// Default equality and inequality.
		[[nodiscard]] friend bool operator==(
			const render_target_blend_options&, const render_target_blend_options&
		) = default;


		bool enabled = false; ///< Whether or not blend is enabled for this render target.
		
		/// \ref blend_factor to be multiplied with the output color RGB.
		blend_factor source_color = blend_factor::one;
		/// \ref blend_factor to be multiplied with the color RGB on the destination surface.
		blend_factor destination_color = blend_factor::zero;
		blend_operation color_operation = blend_operation::add; ///< \ref blend_operation for color RGB.

		/// \ref blend_factor to be multiplied with the output alpha.
		blend_factor source_alpha = blend_factor::one;
		/// \ref blend_factor to be multiplied with the color alpha on the destination surface.
		blend_factor destination_alpha = blend_factor::zero;
		blend_operation alpha_operation = blend_operation::add; ///< \ref blend_operation for color alpha.

		channel_mask write_mask = channel_mask::all; ///< Indicates which channels to write to.
	protected:
		/// Initializes \ref enabled to \p false, and other fields to as if no blending is applied.
		constexpr render_target_blend_options(zero_t) {
		}
	};
}
namespace std {
	/// Hash for \ref lotus::gpu::render_target_blend_options.
	template <> struct hash<lotus::gpu::render_target_blend_options> {
		/// Hashes the given object.
		[[nodiscard]] constexpr size_t operator()(const lotus::gpu::render_target_blend_options &opt) const {
			size_t result = lotus::compute_hash(opt.enabled);
			if (opt.enabled) {
				result = lotus::hash_combine({
					result,
					lotus::compute_hash(opt.source_color),
					lotus::compute_hash(opt.destination_color),
					lotus::compute_hash(opt.color_operation),
					lotus::compute_hash(opt.source_alpha),
					lotus::compute_hash(opt.destination_alpha),
					lotus::compute_hash(opt.alpha_operation),
					lotus::compute_hash(opt.write_mask),
				});
			}
			return result;
		}
	};
}

namespace lotus::gpu {
	/// Option used by the rasterizer to offset depth values.
	struct depth_bias_options {
	public:
		/// No initialization.
		depth_bias_options(std::nullptr_t) {
		}
		/// Initializes all fields to zero, effectively having no bias.
		[[nodiscard]] constexpr inline static depth_bias_options disabled() {
			return depth_bias_options(0.0f, 0.0f, 0.0f);
		}
		/// Creates a depth bias state that does not contain clamping for the bias.
		[[nodiscard]] constexpr inline static depth_bias_options create_unclamped(float bias, float slope_bias) {
			return depth_bias_options(bias, slope_bias, 0.0f);
		}
		/// Creates a new object containing the specified values.
		[[nodiscard]] constexpr inline static depth_bias_options create_clamped(
			float bias, float slope_bias, float clamp
		) {
			return depth_bias_options(bias, slope_bias, clamp);
		}

		/// Default equality and inequality.
		[[nodiscard]] friend bool operator==(const depth_bias_options&, const depth_bias_options&) = default;

		float bias = 0.0f; ///< Uniform depth bias based on the floating-point precision at the triangle.
		float slope_scaled_bias = 0.0f; ///< Slope (and implicitly texel size) scaled depth bias.
		/// The value that the resulting bias is clamped to. If this is greater than zero, it specifies the maximum
		/// bias value; otherwise, if this is less than zero, it specifies the negative minimum bias value;
		/// otherwise, the bias value is not altered.
		float clamp = 0.0f;
	protected:
		/// Initializes all fields of this struct.
		constexpr depth_bias_options(float constant, float slope, float c) :
			bias(constant), slope_scaled_bias(slope), clamp(c) {
		}
	};
}
namespace std {
	/// Hash for \ref lotus::gpu::depth_bias_options.
	template <> struct hash<lotus::gpu::depth_bias_options> {
		/// Hashes the given object.
		[[nodiscard]] constexpr size_t operator()(const lotus::gpu::depth_bias_options &opt) const {
			return lotus::hash_combine({
				lotus::compute_hash(opt.bias),
				lotus::compute_hash(opt.slope_scaled_bias),
				lotus::compute_hash(opt.clamp),
			});
		}
	};
}

namespace lotus::gpu {
	/// Options for the rasterizer.
	struct rasterizer_options {
		/// No initialization.
		rasterizer_options(std::nullptr_t) : depth_bias(nullptr) {
		}
		/// Initializes all fields of this struct.
		constexpr rasterizer_options(depth_bias_options db, front_facing_mode front, cull_mode cull, bool wf) :
			depth_bias(db), front_facing(front), culling(cull), is_wireframe(wf) {
		}

		/// Default equality and inequality.
		[[nodiscard]] friend bool operator==(const rasterizer_options&, const rasterizer_options&) = default;

		depth_bias_options depth_bias; ///< \ref depth_bias_options.
		/// Indicates how front-facing triangles are determined.
		front_facing_mode front_facing = front_facing_mode::clockwise;
		cull_mode culling = cull_mode::none; ///< The \ref cull_mode.
		bool is_wireframe = false; ///< Whether or not to render in wireframe mode.
	};
}
namespace std {
	/// Hash for \ref lotus::gpu::rasterizer_options.
	template <> struct hash<lotus::gpu::rasterizer_options> {
		/// Hashes the given object.
		[[nodiscard]] constexpr size_t operator()(const lotus::gpu::rasterizer_options &opt) const {
			return lotus::hash_combine({
				lotus::compute_hash(opt.depth_bias),
				lotus::compute_hash(opt.front_facing),
				lotus::compute_hash(opt.culling),
				lotus::compute_hash(opt.is_wireframe),
			});
		}
	};
}

namespace lotus::gpu {
	/// Describes how stencil values should be tested and updated.
	struct stencil_options {
	public:
		/// No initialization.
		stencil_options(std::nullptr_t) {
		}
		/// Creates an object indicating that stencil test should always pass, and no modifications should be made to
		/// the stencil buffer.
		[[nodiscard]] constexpr inline static stencil_options always_pass_no_op() {
			return stencil_options(
				comparison_function::always,
				stencil_operation::keep, stencil_operation::keep, stencil_operation::keep
			);
		}
		/// Creates a new object with the given parameters.
		[[nodiscard]] constexpr inline static stencil_options create(
			comparison_function cmp, stencil_operation fail, stencil_operation depth_fail, stencil_operation pass
		) {
			return stencil_options(cmp, fail, depth_fail, pass);
		}

		/// Default equality and inequality.
		[[nodiscard]] friend bool operator==(const stencil_options&, const stencil_options&) = default;

		comparison_function comparison = comparison_function::always; ///< Comparison function for stencil testing.
		/// The operation to perform when stencil testing fails.
		stencil_operation fail       = stencil_operation::keep;
		/// The operation to perform when stencil testing passes but depth testing fails.
		stencil_operation depth_fail = stencil_operation::keep;
		/// The operation to perform when both stencil testing and depth testing passes.
		stencil_operation pass       = stencil_operation::keep;
	protected:
		/// Initializes all fields of this struct.
		constexpr stencil_options(
			comparison_function cmp, stencil_operation f, stencil_operation df, stencil_operation p
		) : comparison(cmp), fail(f), depth_fail(df), pass(p) {
		}
	};
}
namespace std {
	/// Hash for \ref lotus::gpu::stencil_options.
	template <> struct hash<lotus::gpu::stencil_options> {
		/// Hashes the given object.
		[[nodiscard]] constexpr size_t operator()(const lotus::gpu::stencil_options &opt) const {
			return lotus::hash_combine({
				lotus::compute_hash(opt.comparison),
				lotus::compute_hash(opt.fail),
				lotus::compute_hash(opt.depth_fail),
				lotus::compute_hash(opt.pass),
			});
		}
	};
}

namespace lotus::gpu {
	/// Options for depth stencil operations.
	struct depth_stencil_options {
	public:
		/// Initializes the options to a default value.
		depth_stencil_options(std::nullptr_t) : stencil_front_face(nullptr), stencil_back_face(nullptr) {
		}
		/// Initializes all fields of this struct.
		constexpr depth_stencil_options(
			bool depth_test, bool depth_write, comparison_function depth_comp,
			bool stencil_test, std::uint8_t sread_mask, std::uint8_t swrite_mask,
			stencil_options front_op, stencil_options back_op
		) :
			enable_depth_testing(depth_test), write_depth(depth_write), depth_comparison(depth_comp),
			enable_stencil_testing(stencil_test), stencil_read_mask(sread_mask), stencil_write_mask(swrite_mask),
			stencil_front_face(front_op), stencil_back_face(back_op) {
		}
		/// Creates an object indicating that all tests are disabled.
		[[nodiscard]] constexpr inline static depth_stencil_options all_disabled() {
			return depth_stencil_options(
				false, false, comparison_function::always,
				false, 0, 0, stencil_options::always_pass_no_op(), stencil_options::always_pass_no_op()
			);
		}

		/// Default equality and inequality.
		[[nodiscard]] friend bool operator==(const depth_stencil_options&, const depth_stencil_options&) = default;

		bool enable_depth_testing = false; ///< Whether depth testing is enabled.
		bool write_depth = false; ///< Whether to write depth values.
		/// Comparison function used for depth testing.
		comparison_function depth_comparison = comparison_function::always;

		bool enable_stencil_testing = false; ///< Whether stencil testing is enabled.
		std::uint8_t stencil_read_mask  = 0; ///< Stencil read mask.
		std::uint8_t stencil_write_mask = 0; ///< Stencil write mask.
		stencil_options stencil_front_face; ///< Stencil operation for front-facing triangles.
		stencil_options stencil_back_face;  ///< Stencil operation for back-facing triangles.
	};
}
namespace std {
	/// Hash for \ref lotus::gpu::depth_stencil_options.
	template <> struct hash<lotus::gpu::depth_stencil_options> {
		/// Hashes the given object.
		[[nodiscard]] constexpr size_t operator()(const lotus::gpu::depth_stencil_options &opt) const {
			return lotus::hash_combine({
				lotus::compute_hash(opt.enable_depth_testing),
				lotus::compute_hash(opt.write_depth),
				lotus::compute_hash(opt.depth_comparison),
				lotus::compute_hash(opt.enable_stencil_testing),
				lotus::compute_hash(opt.stencil_read_mask),
				lotus::compute_hash(opt.stencil_write_mask),
				lotus::compute_hash(opt.stencil_front_face),
				lotus::compute_hash(opt.stencil_back_face),
			});
		}
	};
}

namespace lotus::gpu {
	/// An element used for vertex/instance input.
	struct input_buffer_element {
		/// Initializes this element to empty.
		input_buffer_element(std::nullptr_t) {
		}
		/// Initializes all fields of this struct.
		constexpr input_buffer_element(const char8_t *sname, std::uint32_t sindex, format fmt, std::size_t off) :
			semantic_name(sname), semantic_index(sindex), element_format(fmt), byte_offset(off) {
		}

		/// Default equality and inequality.
		[[nodiscard]] friend bool operator==(const input_buffer_element&, const input_buffer_element&) = default;

		const char8_t *semantic_name = nullptr;
		std::uint32_t semantic_index = 0;

		format element_format = format::none; ///< The format of this element.
		std::size_t byte_offset = 0; ///< Byte offset of this element in a vertex.
	};
}
namespace std {
	/// Hash for \ref lotus::gpu::input_buffer_element.
	template <> struct hash<lotus::gpu::input_buffer_element> {
		/// Hashes the given object.
		[[nodiscard]] constexpr size_t operator()(const lotus::gpu::input_buffer_element &elem) const {
			return lotus::hash_combine({
				lotus::compute_hash(std::u8string_view(elem.semantic_name)),
				lotus::compute_hash(elem.semantic_index),
				lotus::compute_hash(elem.element_format),
				lotus::compute_hash(elem.byte_offset),
			});
		}
	};
}

namespace lotus::gpu {
	/// Information about an input (vertex/instance) buffer.
	struct input_buffer_layout {
		/// Initializes this layout to empty.
		input_buffer_layout(std::nullptr_t) {
		}
		/// Initializes all fields of this struct.
		constexpr input_buffer_layout(
			std::span<const input_buffer_element> elems,
			std::size_t s, input_buffer_rate rate, std::size_t buf_id
		) : elements(elems), stride(s), input_rate(rate), buffer_index(buf_id) {
		}
		/// Creates a new layout for vertex buffers with the given arguments.
		[[nodiscard]] constexpr inline static input_buffer_layout create_vertex_buffer(
			std::span<const input_buffer_element> elems, std::size_t s, std::size_t buf_id
		) {
			return input_buffer_layout(elems, s, input_buffer_rate::per_vertex, buf_id);
		}
		/// Creates a new layout for instance buffers with the given arguments.
		[[nodiscard]] constexpr inline static input_buffer_layout create_instance_buffer(
			std::span<const input_buffer_element> elems, std::size_t s, std::size_t buf_id
		) {
			return input_buffer_layout(elems, s, input_buffer_rate::per_instance, buf_id);
		}
		/// Creates a new layout for vertex buffers with the given arguments, using the size of the vertex as
		/// \ref stride.
		template <
			typename Vertex
		> [[nodiscard]] constexpr inline static input_buffer_layout create_vertex_buffer(
			std::span<const input_buffer_element> elems, std::size_t buf_id
		) {
			return create_vertex_buffer(elems, sizeof(Vertex), buf_id);
		}
		/// Creates a new layout for instance buffers with the given arguments, using the size of the element as
		/// \ref stride.
		template <
			typename Inst
		> [[nodiscard]] constexpr inline static input_buffer_layout create_instance_buffer(
			std::span<const input_buffer_element> elems, std::size_t buf_id
		) {
			return create_instance_buffer(elems, sizeof(Inst), buf_id);
		}

		std::span<const input_buffer_element> elements; ///< Elements in this vertex buffer.
		std::size_t stride = 0; ///< The size of one vertex.
		std::size_t buffer_index = 0; ///< Index of the vertex buffer.
		input_buffer_rate input_rate = input_buffer_rate::per_vertex; ///< Specifies how the buffer data is used.
	};


	/// Describes a render target attachment used in a render pass.
	struct render_target_pass_options {
	public:
		/// No initialization.
		render_target_pass_options(uninitialized_t) {
		}
		/// Initializes this struct to refer to an empty render target.
		render_target_pass_options(std::nullptr_t) :
			pixel_format(format::none),
			load_operation(pass_load_operation::discard),
			store_operation(pass_store_operation::discard) {
		}
		/// Creates a new \ref render_target_pass_options object.
		[[nodiscard]] inline static render_target_pass_options create(
			format fmt, pass_load_operation load_op, pass_store_operation store_op
		) {
			return render_target_pass_options(fmt, load_op, store_op);
		}

		format pixel_format; ///< Expected pixel format for this attachment.
		pass_load_operation load_operation; ///< Determines the behavior when the pass loads from this attachment.
		pass_store_operation store_operation; ///< Determines the behavior when the pass stores to the attachment.
	protected:
		/// Initializes all fields of this struct.
		render_target_pass_options(
			format fmt, pass_load_operation load_op, pass_store_operation store_op
		) : pixel_format(fmt), load_operation(load_op), store_operation(store_op) {
		}
	};
	/// Describes a depth stencil attachment used in a render pass.
	struct depth_stencil_pass_options {
	public:
		/// No initialization.
		depth_stencil_pass_options(uninitialized_t) {
		}
		/// Initializes this struct to refer to an empty render target.
		depth_stencil_pass_options(std::nullptr_t) :
			pixel_format(format::none),
			depth_load_operation(pass_load_operation::discard),
			depth_store_operation(pass_store_operation::discard),
			stencil_load_operation(pass_load_operation::discard),
			stencil_store_operation(pass_store_operation::discard) {
		}
		/// Creates a new \ref depth_stencil_pass_options object.
		[[nodiscard]] inline static depth_stencil_pass_options create(
			format fmt,
			pass_load_operation depth_load_op, pass_store_operation depth_store_op,
			pass_load_operation stencil_load_op, pass_store_operation stencil_store_op
		) {
			return depth_stencil_pass_options(fmt, depth_load_op, depth_store_op, stencil_load_op, stencil_store_op);
		}

		format pixel_format; ///< Expected pixel format for this attachment.
		pass_load_operation depth_load_operation; ///< \ref pass_load_operation for depth.
		pass_store_operation depth_store_operation; ///< \ref pass_store_operation for depth.
		pass_load_operation stencil_load_operation; ///< \ref pass_load_operation for stencil.
		pass_store_operation stencil_store_operation; ///< \ref pass_store_operation for stencil.
	protected:
		/// Initializes all fields of this struct.
		depth_stencil_pass_options(
			format fmt,
			pass_load_operation depth_load_op, pass_store_operation depth_store_op,
			pass_load_operation stencil_load_op, pass_store_operation stencil_store_op
		) :
			pixel_format(fmt),
			depth_load_operation(depth_load_op), depth_store_operation(depth_store_op),
			stencil_load_operation(stencil_load_op), stencil_store_operation(stencil_store_op) {

			if constexpr (is_debugging) {
				// checks that this is not a color render target and we don't have redundant load/store operations
				const auto &fmt_props = format_properties::get(pixel_format);
				if (fmt_props.depth_bits == 0) {
					assert(
						depth_load_operation == pass_load_operation::discard &&
						depth_store_operation == pass_store_operation::discard
					);
				}
				if (fmt_props.stencil_bits == 0) {
					assert(
						stencil_load_operation == pass_load_operation::discard &&
						stencil_store_operation == pass_store_operation::discard
					);
				}
			}
		}
	};

	/// Describes a subresource index.
	struct subresource_index {
	public:
		/// No initialization.
		subresource_index(uninitialized_t) {
		}
		/// Initializes all members of this struct.
		constexpr subresource_index(std::uint16_t mip, std::uint16_t arr, image_aspect_mask asp) :
			mip_level(mip), array_slice(arr), aspects(asp) {
		}
		/// Creates an index pointing to the color aspect of the first subresource.
		[[nodiscard]] constexpr inline static subresource_index first_color() {
			return subresource_index(0, 0, image_aspect_mask::color);
		}
		/// Creates an index pointing to the depth aspect of the first subresource.
		[[nodiscard]] constexpr inline static subresource_index first_depth() {
			return subresource_index(0, 0, image_aspect_mask::depth);
		}
		/// Creates an index pointing to the depth and stencil aspect of the first subresource.
		[[nodiscard]] constexpr inline static subresource_index first_depth_stencil() {
			return subresource_index(0, 0, image_aspect_mask::depth | image_aspect_mask::stencil);
		}
		/// Creates an index pointing to the color aspect of the specified subresource.
		[[nodiscard]] constexpr inline static subresource_index create_color(std::uint16_t mip, std::uint16_t arr) {
			return subresource_index(mip, arr, image_aspect_mask::color);
		}
		/// Creates an index pointing to the color aspect of the specified subresource.
		[[nodiscard]] constexpr inline static subresource_index create_depth(std::uint16_t mip, std::uint16_t arr) {
			return subresource_index(mip, arr, image_aspect_mask::depth);
		}
		/// Creates an index pointing to the color aspect of the specified subresource.
		[[nodiscard]] constexpr inline static subresource_index create_stencil(
			std::uint16_t mip, std::uint16_t arr
		) {
			return subresource_index(mip, arr, image_aspect_mask::stencil);
		}

		std::uint16_t mip_level; ///< Mip level.
		std::uint16_t array_slice; ///< Array slice.
		image_aspect_mask aspects; ///< The aspects of the subresource.

		/// Default equality and inequality comparison.
		[[nodiscard]] friend bool operator==(const subresource_index&, const subresource_index&) = default;
	};
	/// Describes a range of mip levels.
	struct mip_levels {
	public:
		/// Use this for \ref num_levels to indicate that all levels below \ref minimum can be used.
		constexpr static std::uint16_t all_mip_levels = 0;

		/// No initialization.
		mip_levels(uninitialized_t) {
		}
		/// Returns zero mip levels.
		[[nodiscard]] constexpr inline static mip_levels empty() {
			return mip_levels(0, 0);
		}
		/// Indicates that all mip levels can be used.
		[[nodiscard]] constexpr inline static mip_levels all() {
			return mip_levels(0, all_mip_levels);
		}
		/// Indicates that all mip levels below the given layer can be used.
		[[nodiscard]] constexpr inline static mip_levels all_below(std::uint16_t layer) {
			return mip_levels(layer, all_mip_levels);
		}
		/// Indicates that only the given layer can be used.
		[[nodiscard]] constexpr inline static mip_levels only(std::uint16_t layer) {
			return mip_levels(layer, 1);
		}
		/// Indicates that only the highest layer can be used.
		[[nodiscard]] constexpr inline static mip_levels only_highest() {
			return mip_levels(0, 1);
		}
		/// Creates an object indicating that mip levels in the given range can be used.
		[[nodiscard]] constexpr inline static mip_levels create(std::uint16_t min, std::uint16_t num) {
			return mip_levels(min, num);
		}

		/// Returns whether this struct represents all mip levels about the specified one.
		[[nodiscard]] constexpr bool is_all() const {
			return num_levels == all_mip_levels;
		}
		/// Returns whether this contains no levels.
		[[nodiscard]] constexpr bool is_empty() const {
			return num_levels == 0;
		}

		/// Finds the intersection of two mip ranges.
		[[nodiscard]] constexpr inline static mip_levels intersection(mip_levels lhs, mip_levels rhs) {
			std::uint16_t min = std::max(lhs.minimum, rhs.minimum);
			if (lhs.is_all() && rhs.is_all()) {
				return mip_levels::all_below(min);
			}
			auto last_level = static_cast<std::uint16_t>(std::min(
				lhs.minimum + static_cast<std::uint32_t>(lhs.num_levels),
				rhs.minimum + static_cast<std::uint32_t>(rhs.num_levels)
			));
			return mip_levels(min, last_level - min);
		}

		std::uint16_t minimum; ///< Minimum mip level.
		std::uint16_t num_levels; ///< Number of mip levels.
	protected:
		/// Initializes all fields of this struct.
		constexpr mip_levels(std::uint16_t min, std::uint16_t num) : minimum(min), num_levels(num) {
		}
	};


	/// Synchronization values used by a timeline semaphore.
	struct timeline_semaphore_synchronization {
		/// Initializes this struct to empty.
		timeline_semaphore_synchronization(std::nullptr_t) {
		}
		/// Initializes all fields of this struct.
		timeline_semaphore_synchronization(timeline_semaphore &sem, std::uint64_t v) : semaphore(&sem), value(v) {
		}

		std::uint64_t value = 0; ///< The value of the semaphore.
		timeline_semaphore *semaphore = nullptr; ///< The semaphore.
	};

	/// Synchronization primitives that will be waited for and/or notified when commands are submitted to a queue.
	struct queue_synchronization {
	public:
		/// Initializes all fields of this struct.
		queue_synchronization(
			fence *f,
			std::span<const timeline_semaphore_synchronization> wait   = {},
			std::span<const timeline_semaphore_synchronization> notify = {}
		) : wait_semaphores(wait), notify_semaphores(notify), notify_fence(f) {
		}

		std::span<const timeline_semaphore_synchronization> wait_semaphores;   ///< Semaphores to wait for.
		std::span<const timeline_semaphore_synchronization> notify_semaphores; ///< Semaphores to notify.
		fence *notify_fence = nullptr; ///< Fence to notify.
	};

	/// Synchronization primitives that will be notified when a frame has finished presenting.
	struct back_buffer_synchronization {
	public:
		/// Initializes all fields to \p nullptr.
		back_buffer_synchronization(std::nullptr_t) {
		}
		/// Creates a new object with the specified parameters.
		[[nodiscard]] inline static back_buffer_synchronization create(fence *f) {
			return back_buffer_synchronization(f);
		}
		/// Creates an object indicating that only the given fence should be used for synchronization.
		[[nodiscard]] inline static back_buffer_synchronization with_fence(fence &f) {
			return back_buffer_synchronization(&f);
		}

		fence *notify_fence = nullptr; ///< Fence to notify.
	protected:
		/// Initializes all fields of this struct.
		back_buffer_synchronization(fence *f) : notify_fence(f) {
		}
	};
	/// Information used when presenting a back buffer.
	struct back_buffer_info {
		/// No initialization.
		back_buffer_info(uninitialized_t) {
		}
		/// Initializes the \ref fence to \p nullptr and the status to \ref swap_chain_status::unavailable.
		back_buffer_info(std::nullptr_t) : index(0), on_presented(nullptr), status(swap_chain_status::unavailable) {
		}

		std::size_t index; ///< Index of the back buffer.
		/// Fence that will be triggered when this has finished presenting the previous frame. This can be empty.
		fence *on_presented;
		swap_chain_status status; ///< The status of this swapchain.
	};


	/// A range of descriptors of the same type.
	struct descriptor_range {
	public:
		/// Indicates that the number of descriptors is unbounded.
		constexpr static std::size_t unbounded_count = std::numeric_limits<std::size_t>::max();

		/// No initialization.
		descriptor_range(uninitialized_t) {
		}
		/// Creates a new \ref descriptor_range object.
		[[nodiscard]] constexpr inline static descriptor_range create(descriptor_type ty, std::size_t c) {
			return descriptor_range(ty, c);
		}
		/// Creates a \ref descriptor_range with unbounded descriptor count.
		[[nodiscard]] constexpr inline static descriptor_range create_unbounded(descriptor_type ty) {
			return descriptor_range(ty, unbounded_count);
		}

		/// Equality and inequality.
		[[nodiscard]] friend bool operator==(const descriptor_range&, const descriptor_range&) = default;

		descriptor_type type; ///< The type of the descriptors.
		std::size_t count; ///< The number of descriptors.
	protected:
		/// Initializes all fields.
		constexpr descriptor_range(descriptor_type ty, std::size_t c) : type(ty), count(c) {
		}
	};
	/// A range of descriptors and its register binding.
	struct descriptor_range_binding {
	public:
		/// No initialization.
		descriptor_range_binding(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		constexpr descriptor_range_binding(descriptor_range rng, std::size_t reg) : range(rng), register_index(reg) {
		}
		/// \overload
		[[nodiscard]] constexpr inline static descriptor_range_binding create(
			descriptor_type ty, std::size_t count, std::size_t reg
		) {
			return descriptor_range_binding(descriptor_range::create(ty, count), reg);
		}
		/// Creates a new \ref descriptor_range_binding object with unbounded size.
		[[nodiscard]] constexpr inline static descriptor_range_binding create_unbounded(
			descriptor_type ty, std::size_t reg
		) {
			return descriptor_range_binding(descriptor_range::create_unbounded(ty), reg);
		}

		/// Equality and inequality.
		[[nodiscard]] friend bool operator==(
			const descriptor_range_binding&, const descriptor_range_binding&
		) = default;

		/// Returns the register index of the last binding in this range.
		[[nodiscard]] constexpr std::size_t get_last_register_index() const {
			return register_index + range.count - 1;
		}

		/// Given an array of descriptor range bindings that has been sorted based on \ref register_index, merges all
		/// neighboring ranges that contain the same type of registers. If it detects overlapping ranges, the
		/// callback (if supplied) will be called, in which the caller can modify the overlapping ranges.
		///
		/// \return Iterator past the last valid element after merging.
		template <
			typename It, typename OverlapCallback
		> [[nodiscard]] inline static It merge_sorted_descriptor_ranges(
			It begin, It end, OverlapCallback &&callback = nullptr
		) {
			It last = begin;
			for (It cur = begin; cur != end; ++cur) {
				if (last != begin) {
					It prev = last - 1;
					std::size_t last_index = prev->get_last_register_index();
					if constexpr (!std::is_same_v<std::decay_t<OverlapCallback>, std::nullptr_t>) {
						if (last_index >= cur->register_index) {
							callback(prev, cur);
						}
					}
					if (prev->range.type == cur->range.type && last_index + 1 >= cur->register_index) {
						// merge!
						prev->range.count = cur->get_last_register_index() + 1 - prev->register_index;
						continue;
					}
				}
				// if it can't be merged, add the new range to the array
				*last = *cur;
				++last;
			}
			return last;
		}

		descriptor_range range = uninitialized; ///< The type and number of descriptors.
		std::size_t register_index; ///< Register index corresponding to the first descriptor.
	};

	/// An image resource barrier.
	struct image_barrier {
		/// No initialization.
		image_barrier(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		constexpr image_barrier(subresource_index sub, image &i, image_usage from, image_usage to) :
			subresource(sub), target(&i), from_state(from), to_state(to) {
		}

		subresource_index subresource = uninitialized; ///< Subresource.
		image *target; ///< Target image.
		image_usage from_state = uninitialized; ///< State to transition from.
		image_usage to_state   = uninitialized; ///< State to transition to.
	};
	/// A buffer resource barrier.
	struct buffer_barrier {
		/// No initialization.
		buffer_barrier(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		constexpr buffer_barrier(buffer &b, buffer_usage from, buffer_usage to) :
			target(&b), from_state(from), to_state(to) {
		}

		buffer *target; ///< Target buffer.
		// TODO subresource
		buffer_usage from_state = uninitialized; ///< State to transition from.
		buffer_usage to_state   = uninitialized; ///< State to transition to.
	};

	/// Information about a vertex buffer.
	struct vertex_buffer {
	public:
		/// Initializes this struct to empty.
		constexpr vertex_buffer(std::nullptr_t) {
		}
		/// Initializes all fields of this struct.
		constexpr vertex_buffer(const buffer &b, std::size_t off, std::size_t s) : data(&b), offset(off), stride(s) {
		}

		const buffer *data = nullptr; ///< Data for the vertex buffer.
		std::size_t offset = 0; ///< Offset from the start of the buffer.
		std::size_t stride = 0; ///< The stride of a single vertex.
	};
	/// A view into a structured buffer.
	struct structured_buffer_view {
	public:
		/// No initialization.
		structured_buffer_view(uninitialized_t) {
		}
		/// Creates a new view with the given values.
		[[nodiscard]] constexpr inline static structured_buffer_view create(
			buffer &b, std::size_t f, std::size_t c, std::size_t str
		) {
			return structured_buffer_view(b, f, c, str);
		}

		buffer *data; ///< Data for the buffer.
		std::size_t first; ///< Index of the first buffer element.
		std::size_t count; ///< Size of the buffer in elements.
		std::size_t stride; ///< Stride between two consecutive buffer elements.
	protected:
		/// Initializes all fields of this struct.
		constexpr structured_buffer_view(buffer &b, std::size_t f, std::size_t c, std::size_t str) :
			data(&b), first(f), count(c), stride(str) {
		}
	};
	/// A view into a constant buffer.
	struct constant_buffer_view {
		/// No initialization.
		constant_buffer_view(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		constexpr constant_buffer_view(const buffer &b, std::size_t off, std::size_t sz) :
			data(&b), offset(off), size(sz) {
		}

		const buffer *data; ///< Data for the buffer.
		std::size_t offset; ///< Offset to the range to be used as constants.
		std::size_t size; ///< Size of the range in bytes.
	};


	/// Describes the layout of a frame buffer.
	struct frame_buffer_layout {
		/// Initializes this layout to empty.
		frame_buffer_layout(std::nullptr_t) {
		}
		/// Initializes all fields of this struct.
		constexpr frame_buffer_layout(std::span<const format> crts, format dsrt) :
			color_render_target_formats(crts), depth_stencil_render_target_format(dsrt) {
		}

		std::span<const format> color_render_target_formats; ///< Format of all color render targets.
		/// Format of the depth-stencil render target, or \ref format::none.
		format depth_stencil_render_target_format = format::none;
	};

	/// Describes how a render target is accessed during a render pass.
	template <typename ClearValue> struct render_target_access {
	public:
		/// Initializes clear value to zero, and both \ref load_operation and \ref store_operation to \p discard.
		constexpr render_target_access(std::nullptr_t) :
			clear_value(zero),
			load_operation(pass_load_operation::discard),
			store_operation(pass_store_operation::discard) {
		}
		/// Returns a struct with the specified access.
		[[nodiscard]] constexpr inline static render_target_access create_custom(
			ClearValue clear, pass_load_operation load, pass_store_operation store
		) {
			return render_target_access(clear, load, store);
		}
		/// Returns a struct indicating that the contents of the render target is irrelevant both before and after
		/// the pass.
		[[nodiscard]] constexpr inline static render_target_access create_discard() {
			return create_custom(zero, pass_load_operation::discard, pass_store_operation::discard);
		}
		/// Returns a struct indicating that the render target is cleared before the pass and the contents produced
		/// by the pass are preserved.
		[[nodiscard]] constexpr inline static render_target_access create_clear(ClearValue clear) {
			return create_custom(clear, pass_load_operation::clear, pass_store_operation::preserve);
		}
		/// Returns a struct indicating that the original contents of the render target should be ignored and newly
		/// rendered contents should be written back to the render target.
		[[nodiscard]] constexpr inline static render_target_access create_discard_then_write() {
			return create_custom(zero, pass_load_operation::discard, pass_store_operation::preserve);
		}

		ClearValue           clear_value;     ///< Clear value.
		pass_load_operation  load_operation;  ///< Load opepration.
		pass_store_operation store_operation; ///< Store operation.
	protected:
		/// Initializes all fields of this struct.
		constexpr render_target_access(
			ClearValue clear, pass_load_operation load, pass_store_operation store
		) : clear_value(clear), load_operation(load), store_operation(store) {
		}
	};
	/// Access of a color render target by a pass.
	using color_render_target_access = render_target_access<color_clear_value>;
	/// Access of a depth render target by a pass.
	using depth_render_target_access = render_target_access<double>;
	/// Access of a stencil render target by a pass.
	using stencil_render_target_access = render_target_access<std::uint32_t>;
	/// Describes how a frame buffer is accessed during a render pass.
	struct frame_buffer_access {
	public:
		/// Does not initialize any of the fields.
		frame_buffer_access(std::nullptr_t) {
		}
		/// Initializes all fields of the struct.
		constexpr frame_buffer_access(
			std::span<const color_render_target_access> color_rts,
			depth_render_target_access depth_rt, stencil_render_target_access stencil_rt
		) : color_render_targets(color_rts), depth_render_target(depth_rt), stencil_render_target(stencil_rt) {
		}

		std::span<const color_render_target_access> color_render_targets;  ///< Access of the color render targets.
		depth_render_target_access   depth_render_target   = nullptr; ///< Access of the depth render target.
		stencil_render_target_access stencil_render_target = nullptr; ///< Access of the stencil render target.
	};

	/// A viewport.
	struct viewport {
	public:
		/// No initialization.
		viewport(uninitialized_t) {
		}
		/// Creates a \ref viewport with the given arguments.
		[[nodiscard]] constexpr inline static viewport create(aab2f plane, float mind, float maxd) {
			return viewport(plane, mind, maxd);
		}

		aab2f xy = uninitialized; ///< The dimensions of this viewport on X and Y.
		float minimum_depth; ///< Minimum depth.
		float maximum_depth; ///< Maximum depth.
	protected:
		/// Initializes all fields of this struct.
		constexpr viewport(aab2f plane, float mind, float maxd) :
			xy(plane), minimum_depth(mind), maximum_depth(maxd) {
		}
	};


	/// A generic shader function.
	struct shader_function {
	public:
		/// Initializes an empty object.
		shader_function(std::nullptr_t) : code(nullptr), entry_point(nullptr) {
		}
		/// Initializes all fields of this struct.
		shader_function(shader_binary *c, const char8_t *entry, shader_stage s) :
			code(c), entry_point(entry), stage(s) {
		}

		shader_binary *code; ///< Binary shader code.
		const char8_t *entry_point; ///< Entry point.
		shader_stage stage; ///< Shader stage.
	};

	/// Describes a resource binding in a shader.
	struct shader_resource_binding {
	public:
		/// No initialization.
		shader_resource_binding(uninitialized_t) {
		}

		std::size_t first_register; ///< Index of the first register.
		std::size_t register_count; ///< The number of registers.
		std::size_t register_space; ///< Register space.
		descriptor_type type; ///< The type of this descriptor binding.
		// TODO allocator
		std::u8string name; ///< Variable name of this binding.
		// TODO more fields
	};

	/// An output variable of a shader.
	struct shader_output_variable {
	public:
		/// No initialization.
		shader_output_variable(uninitialized_t) {
		}

		// TODO allocator
		std::u8string semantic_name; ///< Upper case semantic name without the index.
		std::size_t semantic_index; ///< Semantic index.
	};


	// raytracing related
	/// Computed sizes of an acceleration structure.
	struct acceleration_structure_build_sizes {
		/// No initialization.
		acceleration_structure_build_sizes(uninitialized_t) {
		}

		std::size_t acceleration_structure_size; ///< Size of the acceleration structure itself.
		/// Required size of the scratch buffer when building the acceleration structure.
		std::size_t build_scratch_size;
		/// Required size of the scratch buffer when updating the acceleration structure.
		std::size_t update_scratch_size;
	};

	/// A view into a vertex buffer with a specific offset and stride.
	struct vertex_buffer_view {
	public:
		/// Initializes this view to empty.
		constexpr vertex_buffer_view(std::nullptr_t) {
		}
		/// Initializes all fields of this struct.
		constexpr vertex_buffer_view(const buffer &d, format f, std::size_t off, std::size_t s, std::size_t c) :
			data(&d), vertex_format(f), offset(off), stride(s), count(c) {
		}

		const buffer *data = nullptr; ///< Data of the vertex buffer.
		format vertex_format = format::none; ///< Format used for a single element.
		std::size_t offset = 0; ///< Offset in bytes to the first element.
		std::size_t stride = 0; ///< Stride between two consecutive elements.
		std::size_t count  = 0; ///< Total number of elements.
	};
	/// A view into an index buffer.
	struct index_buffer_view {
	public:
		/// Initializes this view to empty.
		constexpr index_buffer_view(std::nullptr_t) {
		}
		/// Initializes all fields of this struct.
		constexpr index_buffer_view(const buffer &d, index_format f, std::size_t off, std::size_t c) :
			data(&d), element_format(f), offset(off), count(c) {
		}

		const buffer *data = nullptr; ///< Data of the index buffer.
		index_format element_format = index_format::uint16; ///< Index format.
		std::size_t offset = 0; ///< Offset in bytes to the first index.
		std::size_t count  = 0; ///< The number of indices.
	};
	/// A view into an array of an array of shader records.
	struct shader_record_view {
	public:
		/// Initializes this view to empty.
		constexpr shader_record_view(std::nullptr_t) : data(nullptr) {
		}
		/// Creates a new object from the given arguments.
		[[nodiscard]] inline static constexpr shader_record_view create(
			buffer &d, std::size_t off, std::size_t c, std::size_t str
		) {
			return shader_record_view(d, off, c, str);
		}

		buffer *data; ///< Data of the shader records.
		std::size_t offset; ///< Offset of the first entry in bytes.
		std::size_t count; ///< Size of the buffer in elements.
		std::size_t stride; ///< Stride of an element.
	protected:
		/// Initializes all fields of this struct.
		constexpr shader_record_view(buffer &d, std::size_t off, std::size_t c, std::size_t str) :
			data(&d), offset(off), count(c), stride(str) {
		}
	};

	/// A group of shaders responsible for handling ray intersections.
	struct hit_shader_group {
	public:
		/// Index indicating that no shader is associated and that the default behavior should be used.
		constexpr static std::size_t no_shader = std::numeric_limits<std::size_t>::max();

		/// No initialization.
		hit_shader_group(uninitialized_t) {
		}
		/// Creates a group with no associated shaders.
		constexpr hit_shader_group(std::nullptr_t) : closest_hit_shader_index(no_shader), any_hit_shader_index(no_shader) {
		}
		/// Creates a new object from the given parameters.
		[[nodiscard]] inline static constexpr hit_shader_group create(std::size_t closest_hit, std::size_t any_hit) {
			return hit_shader_group(closest_hit, any_hit);
		}
		/// Creates a shader group with only a closest hit shader.
		[[nodiscard]] inline static constexpr hit_shader_group create_closest_hit(std::size_t closest_hit) {
			return hit_shader_group(closest_hit, no_shader);
		}

		std::size_t closest_hit_shader_index; ///< Index of the closest hit shader.
		std::size_t any_hit_shader_index; ///< Index of the any hit shader.
	protected:
		/// Initializes all fields of this struct.
		constexpr hit_shader_group(std::size_t closest_hit, std::size_t any_hit) :
			closest_hit_shader_index(closest_hit), any_hit_shader_index(any_hit) {
		}
	};
}
