#pragma once

/// \file
/// Common graphics-related structures. This is the only file that can be included by backends.

#include <cassert>
#include <array>
#include <compare>
#include <string_view>
#include <span>

#include "lotus/common.h"
#include "lotus/math/aab.h"

namespace lotus::graphics {
	// one goal of these forward declarations are used to pass spans of pointers to derived classes to base class
	// functions. implementation of base class functions should be defined where the derived class has been defined
	// (i.e., the proper header has been included)
	class buffer;
	class command_list;
	class descriptor_set_layout;
	class descriptor_set;
	class fence;
	class image2d_view;
	class sampler;
	class staging_buffer;


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
			floatint_point, ///< Floating-point number.
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
		[[nodiscard]] std::uint8_t bits_per_pixel() const {
			return _bits_per_pixel;
		}
		/// Bytes per pixel.
		[[nodiscard]] std::uint8_t bytes_per_pixel() const {
			return _bytes_per_pixel;
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
	/// Enable bitwise operations for \ref graphics::channel_mask.
	template <> struct enable_enum_bitwise_operators<graphics::channel_mask> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref graphics::channel_mask.
	template <> struct enable_enum_is_empty<graphics::channel_mask> : public std::true_type {
	};
}

namespace lotus::graphics {
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
	/// Enable bitwise operations for \ref graphics::image_aspect_mask.
	template <> struct enable_enum_bitwise_operators<graphics::image_aspect_mask> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref graphics::image_aspect_mask.
	template <> struct enable_enum_is_empty<graphics::image_aspect_mask> : public std::true_type {
	};
}

namespace lotus::graphics {
	/// A specific shader stage or all shader stages.
	enum class shader_stage {
		all,             ///< All used stages.
		vertex_shader,   ///< Vertex shader.
		// TODO tessellation shaders
		geometry_shader, ///< Geometry shader.
		pixel_shader,    ///< Pixel shader.
		compute_shader,  ///< Compute shader.

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
		sampler,           ///< A sampler.
		read_only_image,   ///< An image that can only be read.
		read_write_image,  ///< An image that can be read from or written to.
		read_only_buffer,  ///< A buffer that can only be read.
		read_write_buffer, ///< A buffer that can be read from or written to.
		constant_buffer,   ///< A small buffer containing constants.

		num_enumerators ///< The total number of enumerators.
	};

	/// The usage/state of a buffer resource.
	struct buffer_usage {
		/// Values indicating a single usage.
		enum values {
			index_buffer,      ///< Used as an index buffer.
			vertex_buffer,     ///< Used as a vertex buffer.
			read_only_buffer,  ///< Used as a read-only uniform buffer.
			read_write_buffer, ///< Used as a read-write buffer.
			copy_source,       ///< Source for copy operations.
			copy_destination,  ///< Target for copy operations.

			num_enumerators ///< The total number of enumerators.
		};
		/// Used for specifying multiple usages. These values correspond directly to those in \ref values.
		enum class mask : std::uint8_t {
			none = 0, ///< No usage.

			index_buffer      = 1 << values::index_buffer,
			vertex_buffer     = 1 << values::vertex_buffer,
			read_only_buffer  = 1 << values::read_only_buffer,
			read_write_buffer = 1 << values::read_write_buffer,
			copy_source       = 1 << values::copy_source,
			copy_destination  = 1 << values::copy_destination,
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

		/// Equality.
		[[nodiscard]] friend constexpr bool operator==(buffer_usage, buffer_usage) = default;

		/// Converts the value into a bit in the mask.
		[[nodiscard]] constexpr mask as_mask() const {
			return static_cast<mask>(1 << _value);
		}
	protected:
		values _value; ///< The value.
	};
}
namespace lotus {
	/// Enable bitwise operations for \ref graphics::buffer_usage::mask.
	template <> struct enable_enum_bitwise_operators<graphics::buffer_usage::mask> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref graphics::buffer_usage::mask.
	template <> struct enable_enum_is_empty<graphics::buffer_usage::mask> : public std::true_type {
	};
}

namespace lotus::graphics {
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
	/// Enable bitwise operations for \ref graphics::image_usage::mask.
	template <> struct enable_enum_bitwise_operators<graphics::image_usage::mask> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref graphics::image_usage::mask.
	template <> struct enable_enum_is_empty<graphics::image_usage::mask> : public std::true_type {
	};
}


namespace lotus::graphics {
	/// The type of a heap.
	enum class heap_type : std::uint8_t {
		device_only, ///< A heap that can only be accessed from the device.
		/// A heap used for uploading data to the device. Heaps of this type cannot be written to by the device.
		upload,
		readback, ///< A heap used for transferring data back to the CPU.

		num_enumerators ///< The total numbers of enumerators.
	};


	/// Properties of an adapter.
	struct adapter_properties {
		/// No initialization.
		adapter_properties(uninitialized_t) {
		}

		// TODO allocator
		std::u8string name; ///< The name of this device.
		/// Alignment required for multiple regions in a buffer to be used as constant buffers.
		std::size_t constant_buffer_alignment;
		bool is_software; ///< Whether this is a software adapter.
		bool is_discrete; ///< Whether this is a discrete adapter.
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
			result.enabled = true;
			result.source_color = src_color;
			result.destination_color = dst_color;
			result.color_operation = color_op;
			result.source_alpha = src_alpha;
			result.destination_alpha = dst_alpha;
			result.alpha_operation = alpha_op;
			result.write_mask = mask;
			return result;
		}

		bool enabled; ///< Whether or not blend is enabled for this render target.
		
		/// \ref blend_factor to be multiplied with the output color RGB.
		blend_factor source_color;
		/// \ref blend_factor to be multiplied with the color RGB on the destination surface.
		blend_factor destination_color;
		blend_operation color_operation; ///< \ref blend_operation for color RGB.

		/// \ref blend_factor to be multiplied with the output alpha.
		blend_factor source_alpha;
		/// \ref blend_factor to be multiplied with the color alpha on the destination surface.
		blend_factor destination_alpha;
		blend_operation alpha_operation; ///< \ref blend_operation for color alpha.

		channel_mask write_mask; ///< Indicates which channels to write to.
	protected:
		/// Initializes \ref enabled to \p false, and other fields to as if no blending is applied.
		constexpr render_target_blend_options(zero_t) :
			enabled(false),
			source_color(blend_factor::one),
			destination_color(blend_factor::zero),
			color_operation(blend_operation::add),
			source_alpha(blend_factor::one),
			destination_alpha(blend_factor::zero),
			alpha_operation(blend_operation::add),
			write_mask(channel_mask::all) {
		}
	};

	/// Option used by the rasterizer to offset depth values.
	struct depth_bias_options {
	public:
		/// No initialization.
		depth_bias_options(uninitialized_t) {
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

		float bias; ///< Uniform depth bias based on the floating-point precision at the triangle.
		float slope_scaled_bias; ///< Slope (and implicitly texel size) scaled depth bias.
		/// The value that the resulting bias is clamped to. If this is greater than zero, it specifies the maximum
		/// bias value; otherwise, if this is less than zero, it specifies the negative minimum bias value;
		/// otherwise, the bias value is not altered.
		float clamp;
	protected:
		/// Initializes all fields of this struct.
		constexpr depth_bias_options(float constant, float slope, float c) :
			bias(constant), slope_scaled_bias(slope), clamp(c) {
		}
	};

	/// Options for the rasterizer.
	struct rasterizer_options {
	public:
		/// No initialization.
		rasterizer_options(uninitialized_t) {
		}
		/// Creates a \ref rasterizer_options object using the given parameters.
		[[nodiscard]] constexpr inline static rasterizer_options create(
			depth_bias_options db, front_facing_mode front, cull_mode cull, bool wf = false
		) {
			return rasterizer_options(db, front, cull, wf);
		}

		depth_bias_options depth_bias = uninitialized; ///< \ref depth_bias_options.
		front_facing_mode front_facing; ///< Indicates how front-facing triangles are determined.
		cull_mode culling; ///< The \ref cull_mode.
		bool is_wireframe; ///< Whether or not to render in wireframe mode.
	protected:
		/// Initializes all fields of this struct.
		constexpr rasterizer_options(depth_bias_options db, front_facing_mode front, cull_mode cull, bool wf) :
			depth_bias(db), front_facing(front), culling(cull), is_wireframe(wf) {
		}
	};

	/// Describes how stencil values should be tested and updated.
	struct stencil_options {
	public:
		/// No initialization.
		stencil_options(uninitialized_t) {
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

		comparison_function comparison; ///< Comparison function for stencil testing.
		stencil_operation fail; ///< The operation to perform when stencil testing fails.
		/// The operation to perform when stencil testing passes but depth testing fails.
		stencil_operation depth_fail;
		stencil_operation pass; ///< The operation to perform when both stencil testing and depth testing passes.
	protected:
		/// Initializes all fields of this struct.
		constexpr stencil_options(
			comparison_function cmp, stencil_operation f, stencil_operation df, stencil_operation p
		) : comparison(cmp), fail(f), depth_fail(df), pass(p) {
		}
	};

	/// Options for depth stencil operations.
	struct depth_stencil_options {
	public:
		/// No initialization.
		depth_stencil_options(uninitialized_t) {
		}
		/// Creates an object indicating that all tests are disabled.
		[[nodiscard]] constexpr inline static depth_stencil_options all_disabled() {
			return depth_stencil_options(
				false, false, comparison_function::always,
				false, 0, 0, stencil_options::always_pass_no_op(), stencil_options::always_pass_no_op()
			);
		}
		/// Creates an object with the given parameters.
		[[nodiscard]] constexpr inline static depth_stencil_options create(
			bool enable_depth_testing, bool write_depth, comparison_function depth_comparison,
			bool enable_stencil_testing, std::uint8_t stencil_read_mask, std::uint8_t stencil_write_mask,
			stencil_options stencil_front_face, stencil_options stencil_back_face
		) {
			return depth_stencil_options(
				enable_depth_testing, write_depth, depth_comparison,
				enable_stencil_testing, stencil_read_mask, stencil_write_mask,
				stencil_front_face, stencil_back_face
			);
		}

		bool enable_depth_testing; ///< Whether depth testing is enabled.
		bool write_depth; ///< Whether to write depth values.
		comparison_function depth_comparison; ///< Comparison function used for depth testing.

		bool enable_stencil_testing; ///< Whether stencil testing is enabled.
		std::uint8_t stencil_read_mask;  ///< Stencil read mask.
		std::uint8_t stencil_write_mask; ///< Stencil write mask.
		stencil_options stencil_front_face = uninitialized; ///< Stencil operation for front-facing triangles.
		stencil_options stencil_back_face  = uninitialized; ///< Stencil operation for back-facing triangles.
	protected:
		/// Initializes all fields of this struct.
		constexpr depth_stencil_options(
			bool depth_test, bool depth_write, comparison_function depth_comp,
			bool stencil_test, std::uint8_t sread_mask, std::uint8_t swrite_mask,
			stencil_options front_op, stencil_options back_op
		) :
			enable_depth_testing(depth_test), write_depth(depth_write), depth_comparison(depth_comp),
			enable_stencil_testing(stencil_test), stencil_read_mask(sread_mask), stencil_write_mask(swrite_mask),
			stencil_front_face(front_op), stencil_back_face(back_op)
		{
		}
	};

	/// An element used for vertex/instance input.
	struct input_buffer_element {
	public:
		/// No initialization.
		input_buffer_element(uninitialized_t) {
		}
		/// Creates a new object with the given arguments.
		[[nodiscard]] constexpr inline static input_buffer_element create(
			const char8_t *sname, std::uint32_t sindex, format fmt, std::size_t off
		) {
			return input_buffer_element(sname, sindex, fmt, off);
		}

		const char8_t *semantic_name;
		std::uint32_t semantic_index;

		format element_format; ///< The format of this element.
		std::size_t byte_offset; ///< Byte offset of this element in a vertex.
	protected:
		/// Initializes all fields of this struct.
		constexpr input_buffer_element(const char8_t *sname, std::uint32_t sindex, format fmt, std::size_t off) :
			semantic_name(sname), semantic_index(sindex), element_format(fmt), byte_offset(off) {
		}
	};

	/// Information about an input (vertex/instance) buffer.
	struct input_buffer_layout {
	public:
		/// No initialization.
		input_buffer_layout(uninitialized_t) {
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
		std::size_t stride; ///< The size of one vertex.
		std::size_t buffer_index; ///< Index of the vertex buffer.
		input_buffer_rate input_rate; ///< Specifies how the buffer data is used.
	protected:
		/// Initializes all fields of this struct.
		constexpr input_buffer_layout(
			std::span<const input_buffer_element> elems,
			std::size_t s, input_buffer_rate rate, std::size_t buf_id
		) : elements(elems), stride(s), input_rate(rate), buffer_index(buf_id) {
		}
	};


	/// Describes a render target attachment used in a render pass.
	struct render_target_pass_options {
	public:
		/// No initialization.
		render_target_pass_options(uninitialized_t) {
		}
		/// Creates a new \ref render_target_pass_options object.
		[[nodiscard]] constexpr inline static render_target_pass_options create(
			format fmt, pass_load_operation load_op, pass_store_operation store_op
		) {
			return render_target_pass_options(fmt, load_op, store_op);
		}

		format pixel_format; ///< Expected pixel format for this attachment.
		pass_load_operation load_operation; ///< Determines the behavior when the pass loads from this attachment.
		pass_store_operation store_operation; ///< Determines the behavior when the pass stores to the attachment.
	protected:
		/// Initializes all fields of this struct.
		constexpr render_target_pass_options(
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
		/// Creates a new \ref depth_stencil_pass_options object.
		[[nodiscard]] constexpr inline static depth_stencil_pass_options create(
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
		constexpr depth_stencil_pass_options(
			format fmt,
			pass_load_operation depth_load_op, pass_store_operation depth_store_op,
			pass_load_operation stencil_load_op, pass_store_operation stencil_store_op
		) :
			pixel_format(fmt),
			depth_load_operation(depth_load_op), depth_store_operation(depth_store_op),
			stencil_load_operation(stencil_load_op), stencil_store_operation(stencil_store_op) {
		}
	};

	/// Describes a subresource index.
	struct subresource_index {
	public:
		/// No initialization.
		subresource_index(uninitialized_t) {
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
		/// Creates an index with the specified arguments.
		[[nodiscard]] constexpr inline static subresource_index create(
			std::uint16_t mip, std::uint16_t arr, image_aspect_mask asp
		) {
			return subresource_index(mip, arr, asp);
		}

		std::uint16_t mip_level; ///< Mip level.
		std::uint16_t array_slice; ///< Array slice.
		image_aspect_mask aspects; ///< The aspects of the subresource.
	protected:
		/// Initializes all members of this struct.
		constexpr subresource_index(std::uint16_t mip, std::uint16_t arr, image_aspect_mask asp) :
			mip_level(mip), array_slice(arr), aspects(asp) {
		}
	};
	/// Describes a range of mip levels.
	struct mip_levels {
	public:
		/// Use this for \ref num_levels to indicate that all levels below \ref minimum can be used.
		constexpr static std::uint16_t all_mip_levels = 0;

		/// No initialization.
		mip_levels(uninitialized_t) {
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

		std::uint16_t minimum; ///< Minimum mip level.
		std::uint16_t num_levels; ///< Number of mip levels.
	protected:
		/// Initializes all fields of this struct.
		constexpr mip_levels(std::uint16_t min, std::uint16_t num) : minimum(min), num_levels(num) {
		}
	};

	/// Synchronization primitives that will be notified when a frame has finished presenting.
	struct back_buffer_synchronization {
	public:
		/// Initializes all fields to \p nullptr.
		back_buffer_synchronization(std::nullptr_t) : notify_fence(nullptr) {
		}
		/// Creates a new object with the specified parameters.
		[[nodiscard]] inline static back_buffer_synchronization create(fence *f) {
			return back_buffer_synchronization(f);
		}
		/// Creates an object indicating that only the given fence should be used for synchronization.
		[[nodiscard]] inline static back_buffer_synchronization with_fence(fence &f) {
			return back_buffer_synchronization(&f);
		}

		fence *notify_fence; ///< Fence to notify.
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

		std::size_t index; ///< Index of the back buffer.
		/// Fence that will be triggered when this has finished presenting the previous frame. This can be empty.
		fence *on_presented;
	};


	/// A range of descriptors of the same type.
	struct descriptor_range {
	public:
		/// No initialization.
		descriptor_range(uninitialized_t) {
		}
		/// Creates a new \ref descriptor_range object.
		[[nodiscard]] constexpr inline static descriptor_range create(descriptor_type ty, std::size_t c) {
			return descriptor_range(ty, c);
		}

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
		/// Creates a new \ref descriptor_range_binding object.
		[[nodiscard]] constexpr inline static descriptor_range_binding create(descriptor_range r, std::size_t reg) {
			return descriptor_range_binding(r, reg);
		}
		/// \overload
		[[nodiscard]] constexpr inline static descriptor_range_binding create(
			descriptor_type ty, std::size_t count, std::size_t reg
		) {
			return descriptor_range_binding(descriptor_range::create(ty, count), reg);
		}

		descriptor_range range = uninitialized; ///< The type and number of descriptors.
		std::size_t register_index; ///< Register index corresponding to this descriptor.
	protected:
		/// Initializes all fields of this struct.
		constexpr descriptor_range_binding(descriptor_range rng, std::size_t reg) : range(rng), register_index(reg) {
		}
	};

	/// An image resource barrier.
	struct image_barrier {
	public:
		/// No initialization.
		image_barrier(uninitialized_t) {
		}
		/// Creates a new \ref image_barrier object.
		[[nodiscard]] constexpr static inline image_barrier create(
			subresource_index sub, image &img, image_usage from, image_usage to
		) {
			return image_barrier(sub, img, from, to);
		}

		subresource_index subresource = uninitialized; ///< Subresource.
		image *target; ///< Target image.
		image_usage from_state = uninitialized; ///< State to transition from.
		image_usage to_state   = uninitialized; ///< State to transition to.
	protected:
		/// Initializes all fields of this struct.
		constexpr image_barrier(subresource_index sub, image &i, image_usage from, image_usage to) :
			subresource(sub), target(&i), from_state(from), to_state(to) {
		}
	};
	/// A buffer resource barrier.
	struct buffer_barrier {
	public:
		/// No initialization.
		buffer_barrier(uninitialized_t) {
		}
		/// Creates a new \ref buffer_barrier object.
		[[nodiscard]] constexpr static inline buffer_barrier create(buffer &b, buffer_usage from, buffer_usage to) {
			return buffer_barrier(b, from, to);
		}

		buffer *target; ///< Target buffer.
		// TODO subresource
		buffer_usage from_state = uninitialized; ///< State to transition from.
		buffer_usage to_state   = uninitialized; ///< State to transition to.
	protected:
		/// Initializes all fields of this struct.
		constexpr buffer_barrier(buffer &b, buffer_usage from, buffer_usage to) :
			target(&b), from_state(from), to_state(to) {
		}
	};

	/// Information about a vertex buffer.
	struct vertex_buffer {
	public:
		/// No initialization.
		vertex_buffer(uninitialized_t) {
		}
		/// Creates a new object with the given values.
		[[nodiscard]] constexpr inline static vertex_buffer from_buffer_offset_stride(
			buffer &b, std::size_t off, std::size_t s
		) {
			return vertex_buffer(b, off, s);
		}

		buffer *data; ///< Data for the vertex buffer.
		std::size_t offset; ///< Offset from the start of the buffer.
		std::size_t stride; ///< The stride of a single vertex.
	protected:
		/// Initializes all fields of this struct.
		constexpr vertex_buffer(buffer &b, std::size_t off, std::size_t s) : data(&b), offset(off), stride(s) {
		}
	};
	/// A view into a structured buffer.
	struct buffer_view {
	public:
		/// No initialization.
		buffer_view(uninitialized_t) {
		}
		/// Creates a new view with the given values.
		[[nodiscard]] constexpr inline static buffer_view create(
			buffer &b, std::size_t f, std::size_t sz, std::size_t str
		) {
			return buffer_view(b, f, sz, str);
		}

		buffer *data; ///< Data for the buffer.
		std::size_t first; ///< Index of the first buffer element.
		std::size_t size; ///< Size of the buffer in elements.
		std::size_t stride; ///< Stride between two consecutive buffer elements.
	protected:
		/// Initializes all fields of this struct.
		constexpr buffer_view(buffer &b, std::size_t f, std::size_t sz, std::size_t str) :
			data(&b), first(f), size(sz), stride(str) {
		}
	};
	/// A view into a constant buffer.
	struct constant_buffer_view {
	public:
		/// No initialization.
		constant_buffer_view(uninitialized_t) {
		}
		/// Creates a new view with the given values.
		[[nodiscard]] constexpr inline static constant_buffer_view create(
			buffer &b, std::size_t off, std::size_t sz
		) {
			return constant_buffer_view(b, off, sz);
		}

		buffer *data; ///< Data for the buffer.
		std::size_t offset; ///< Offset to the range to be used as constants.
		std::size_t size; ///< Size of the range in bytes.
	protected:
		/// Initializes all fields of this struct.
		constexpr constant_buffer_view(buffer &b, std::size_t off, std::size_t sz) :
			data(&b), offset(off), size(sz) {
		}
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
}
