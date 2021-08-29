#pragma once

/// \file
/// Common graphics-related structures. This is the only file that can be included by backends.

#include <cassert>
#include <array>
#include <compare>
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
	class fence;
	class image2d_view;
	class sampler;


	/// Base class of all image types.
	class image {
	protected:
		/// Prevent objects of this type from being created directly.
		image() = default;
	};
	/// Base class of all image view types.
	class image_view {
	protected:
		/// Prevent objects of this type from being created directly.
		image_view() = default;
	};


	constexpr static std::size_t num_color_render_targets = 8; ///< The maximum number of color render targets.

	/// Data type.
	enum class data_type : std::uint8_t {
		float_bit      = 0, ///< Bit pattern that indicates that the type is floating point.
		int_bit        = 1, ///< Bit pattern that indicates that the type is integer.
		normalized_bit = 2, ///< Bit pattern that indicates this type is normalized.
		srgb_bit       = 3, ///< Bit pattern that indicates this type is unsigned normalized sRGB.
		data_type_mask = 0x3, ///< Mask for the data type.

		signed_bit = 1 << 2, ///< The bit that indicates that the type is signed.

		/// The bit that indicates that there's a depth channel, in which case the type bits indicate the type of
		/// that channel.
		depth_bit   = 1 << 3,
		stencil_bit = 1 << 4, ///< The bit that indicates that there's a stencil channel.


		none = 0, ///< No specific type.

		floating_point      = float_bit      | signed_bit, ///< Floating point number.
		unsigned_integer    = int_bit,                     ///< Unsigned integer.
		signed_integer      = int_bit        | signed_bit, ///< Signed integer.
		unsigned_normalized = normalized_bit,              ///< Unsigned value normalized to [0, 1].
		signed_normalized   = normalized_bit | signed_bit, ///< Signed value normalized to [0, 1].
		srgb                = srgb_bit,                    ///< Unsigned sRGB value normalized to [0, 1].

		depth_float         = float_bit      | signed_bit | depth_bit, ///< Floating-point depth.
		depth_unorm         = normalized_bit              | depth_bit, ///< Unsigned normalized depth.
		/// Floating-point depth with stencil.
		depth_float_stencil = float_bit      | signed_bit | depth_bit | stencil_bit,
		/// Unsigned normalized depth with stencil.
		depth_unorm_stencil = float_bit      | signed_bit | depth_bit | stencil_bit,
	};
}
namespace lotus {
	/// Enable bitwise operations for \ref graphics::data_type.
	template <> struct enable_enum_bitwise_operators<graphics::data_type> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref graphics::data_type.
	template <> struct enable_enum_is_empty<graphics::data_type> : public std::true_type {
	};
}

namespace lotus::graphics {
	namespace _details {
		/// Defines various constants and functions used by the regular \ref graphics::format class, in order
		/// to work around a few \p constexpr issues.
		class format {
		public:
			/// Pixel format related constants.
			class constants {
			public:
				/// The number of bits used to store the number of bits for a channel.
				constexpr static std::size_t channel_bit_count = 6;

				/// Bit offset of the red channel.
				constexpr static std::uint32_t red_offset   = 0;
				/// Bit offset of the green channel.
				constexpr static std::uint32_t green_offset = red_offset   + channel_bit_count;
				/// Bit offset of the blue channel.
				constexpr static std::uint32_t blue_offset  = green_offset + channel_bit_count;
				/// Bit offset of the alpha channel.
				constexpr static std::uint32_t alpha_offset = blue_offset  + channel_bit_count;
				/// Bit offset of the depth channel.
				constexpr static std::uint32_t depth_offset   = 0;
				/// Bit offset of the stencil channel.
				constexpr static std::uint32_t stencil_offset = depth_offset + channel_bit_count;
				/// Bit offset of the \ref data_type.
				constexpr static std::uint32_t data_type_offset = alpha_offset + channel_bit_count;

				constexpr static std::uint32_t channel_mask = (1u << channel_bit_count) - 1; ///< Mask for a single channel.
				constexpr static std::uint32_t red_mask   = channel_mask << red_offset;    ///< Mask for the red channel.
				constexpr static std::uint32_t green_mask = channel_mask << green_offset;  ///< Mask for the green channel.
				constexpr static std::uint32_t blue_mask  = channel_mask << blue_offset;   ///< Mask for the blue channel.
				constexpr static std::uint32_t alpha_mask = channel_mask << alpha_offset;  ///< Mask for the alpha channel.
				/// Mask for the depth channel.
				constexpr static std::uint32_t depth_mask   = channel_mask << depth_offset;
				/// Mask for the stencil channel.
				constexpr static std::uint32_t stencil_mask = channel_mask << stencil_offset;
				/// Mask for the \ref data_type.
				constexpr static std::uint32_t data_type_mask = 0xFFu << data_type_offset;
			};
		protected:
			/// Checks that the bit count fits inside a \ref format enum.
			constexpr inline static void _check_format_bit_count(std::uint8_t bits) {
				assert((bits & constants::channel_mask) == bits);
			}
			/// Creates an RGBA pixel format enum from the given parameters;
			[[nodiscard]] constexpr inline static std::uint32_t _create_rgba_format(
				std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha, data_type type
			) {
				_check_format_bit_count(red);
				_check_format_bit_count(green);
				_check_format_bit_count(blue);
				_check_format_bit_count(alpha);
				return
					(static_cast<std::uint32_t>(red)   << constants::red_offset)   |
					(static_cast<std::uint32_t>(green) << constants::green_offset) |
					(static_cast<std::uint32_t>(blue)  << constants::blue_offset)  |
					(static_cast<std::uint32_t>(alpha) << constants::alpha_offset) |
					(static_cast<std::uint32_t>(type)  << constants::data_type_offset);
			}
			/// Creates a depth-stencil pixel format enum from the given parameters.
			[[nodiscard]] constexpr inline static std::uint32_t _create_depth_stencil_format(
				std::uint8_t depth, std::uint8_t stencil, data_type type
			) {
				_check_format_bit_count(depth);
				_check_format_bit_count(stencil);
				return
					(static_cast<std::uint32_t>(depth)   << constants::depth_offset)   |
					(static_cast<std::uint32_t>(stencil) << constants::stencil_offset) |
					(static_cast<std::uint32_t>(type)    << constants::data_type_offset);
			}
		};
	}
	/// The format of a pixel.
	class format : public _details::format {
	public:
		/// Value type.
		enum value_type : std::uint32_t {
			none = 0, ///< No specific type.

			d32_float_s8 = _create_depth_stencil_format(32, 8, data_type::depth_float_stencil),
			d32_float    = _create_depth_stencil_format(32, 0, data_type::depth_float),
			d24_unorm_s8 = _create_depth_stencil_format(24, 8, data_type::depth_unorm_stencil),
			d16_unorm    = _create_depth_stencil_format(16, 0, data_type::depth_unorm),


			r8_unorm   = _create_rgba_format(8, 0, 0, 0, data_type::unsigned_normalized),
			r8_snorm   = _create_rgba_format(8, 0, 0, 0, data_type::signed_normalized),
			r8_srgb    = _create_rgba_format(8, 0, 0, 0, data_type::srgb),
			r8_uint    = _create_rgba_format(8, 0, 0, 0, data_type::unsigned_integer),
			r8_sint    = _create_rgba_format(8, 0, 0, 0, data_type::signed_integer),
			r8_unknown = _create_rgba_format(8, 0, 0, 0, data_type::none),

			r8g8_unorm   = _create_rgba_format(8, 8, 0, 0, data_type::unsigned_normalized),
			r8g8_snorm   = _create_rgba_format(8, 8, 0, 0, data_type::signed_normalized),
			r8g8_srgb    = _create_rgba_format(8, 8, 0, 0, data_type::srgb),
			r8g8_uint    = _create_rgba_format(8, 8, 0, 0, data_type::unsigned_integer),
			r8g8_sint    = _create_rgba_format(8, 8, 0, 0, data_type::signed_integer),
			r8g8_unknown = _create_rgba_format(8, 8, 0, 0, data_type::none),

			r8g8b8_unorm   = _create_rgba_format(8, 8, 8, 0, data_type::unsigned_normalized),
			r8g8b8_snorm   = _create_rgba_format(8, 8, 8, 0, data_type::signed_normalized),
			r8g8b8_srgb    = _create_rgba_format(8, 8, 8, 0, data_type::srgb),
			r8g8b8_uint    = _create_rgba_format(8, 8, 8, 0, data_type::unsigned_integer),
			r8g8b8_sint    = _create_rgba_format(8, 8, 8, 0, data_type::signed_integer),
			r8g8b8_unknown = _create_rgba_format(8, 8, 8, 0, data_type::none),

			r8g8b8a8_unorm   = _create_rgba_format(8, 8, 8, 8, data_type::unsigned_normalized),
			r8g8b8a8_snorm   = _create_rgba_format(8, 8, 8, 8, data_type::signed_normalized),
			r8g8b8a8_srgb    = _create_rgba_format(8, 8, 8, 8, data_type::srgb),
			r8g8b8a8_uint    = _create_rgba_format(8, 8, 8, 8, data_type::unsigned_integer),
			r8g8b8a8_sint    = _create_rgba_format(8, 8, 8, 8, data_type::signed_integer),
			r8g8b8a8_unknown = _create_rgba_format(8, 8, 8, 8, data_type::none),


			r16_unorm   = _create_rgba_format(16, 0, 0, 0, data_type::unsigned_normalized),
			r16_snorm   = _create_rgba_format(16, 0, 0, 0, data_type::signed_normalized),
			r16_srgb    = _create_rgba_format(16, 0, 0, 0, data_type::srgb),
			r16_uint    = _create_rgba_format(16, 0, 0, 0, data_type::unsigned_integer),
			r16_sint    = _create_rgba_format(16, 0, 0, 0, data_type::signed_integer),
			r16_float   = _create_rgba_format(16, 0, 0, 0, data_type::floating_point),
			r16_unknown = _create_rgba_format(16, 0, 0, 0, data_type::none),

			r16g16_unorm   = _create_rgba_format(16, 16, 0, 0, data_type::unsigned_normalized),
			r16g16_snorm   = _create_rgba_format(16, 16, 0, 0, data_type::signed_normalized),
			r16g16_srgb    = _create_rgba_format(16, 16, 0, 0, data_type::srgb),
			r16g16_uint    = _create_rgba_format(16, 16, 0, 0, data_type::unsigned_integer),
			r16g16_sint    = _create_rgba_format(16, 16, 0, 0, data_type::signed_integer),
			r16g16_float   = _create_rgba_format(16, 16, 0, 0, data_type::floating_point),
			r16g16_unknown = _create_rgba_format(16, 16, 0, 0, data_type::none),

			r16g16b16_unorm   = _create_rgba_format(16, 16, 16, 0, data_type::unsigned_normalized),
			r16g16b16_snorm   = _create_rgba_format(16, 16, 16, 0, data_type::signed_normalized),
			r16g16b16_srgb    = _create_rgba_format(16, 16, 16, 0, data_type::srgb),
			r16g16b16_uint    = _create_rgba_format(16, 16, 16, 0, data_type::unsigned_integer),
			r16g16b16_sint    = _create_rgba_format(16, 16, 16, 0, data_type::signed_integer),
			r16g16b16_float   = _create_rgba_format(16, 16, 16, 0, data_type::floating_point),
			r16g16b16_unknown = _create_rgba_format(16, 16, 16, 0, data_type::none),

			r16g16b16a16_unorm   = _create_rgba_format(16, 16, 16, 16, data_type::unsigned_normalized),
			r16g16b16a16_snorm   = _create_rgba_format(16, 16, 16, 16, data_type::signed_normalized),
			r16g16b16a16_srgb    = _create_rgba_format(16, 16, 16, 16, data_type::srgb),
			r16g16b16a16_uint    = _create_rgba_format(16, 16, 16, 16, data_type::unsigned_integer),
			r16g16b16a16_sint    = _create_rgba_format(16, 16, 16, 16, data_type::signed_integer),
			r16g16b16a16_float   = _create_rgba_format(16, 16, 16, 16, data_type::floating_point),
			r16g16b16a16_unknown = _create_rgba_format(16, 16, 16, 16, data_type::none),


			r32_unorm   = _create_rgba_format(32, 0, 0, 0, data_type::unsigned_normalized),
			r32_snorm   = _create_rgba_format(32, 0, 0, 0, data_type::signed_normalized),
			r32_srgb    = _create_rgba_format(32, 0, 0, 0, data_type::srgb),
			r32_uint    = _create_rgba_format(32, 0, 0, 0, data_type::unsigned_integer),
			r32_sint    = _create_rgba_format(32, 0, 0, 0, data_type::signed_integer),
			r32_float   = _create_rgba_format(32, 0, 0, 0, data_type::floating_point),
			r32_unknown = _create_rgba_format(32, 0, 0, 0, data_type::none),
	
			r32g32_unorm   = _create_rgba_format(32, 32, 0, 0, data_type::unsigned_normalized),
			r32g32_snorm   = _create_rgba_format(32, 32, 0, 0, data_type::signed_normalized),
			r32g32_srgb    = _create_rgba_format(32, 32, 0, 0, data_type::srgb),
			r32g32_uint    = _create_rgba_format(32, 32, 0, 0, data_type::unsigned_integer),
			r32g32_sint    = _create_rgba_format(32, 32, 0, 0, data_type::signed_integer),
			r32g32_float   = _create_rgba_format(32, 32, 0, 0, data_type::floating_point),
			r32g32_unknown = _create_rgba_format(32, 32, 0, 0, data_type::none),
	
			r32g32b32_unorm   = _create_rgba_format(32, 32, 32, 0, data_type::unsigned_normalized),
			r32g32b32_snorm   = _create_rgba_format(32, 32, 32, 0, data_type::signed_normalized),
			r32g32b32_srgb    = _create_rgba_format(32, 32, 32, 0, data_type::srgb),
			r32g32b32_uint    = _create_rgba_format(32, 32, 32, 0, data_type::unsigned_integer),
			r32g32b32_sint    = _create_rgba_format(32, 32, 32, 0, data_type::signed_integer),
			r32g32b32_float   = _create_rgba_format(32, 32, 32, 0, data_type::floating_point),
			r32g32b32_unknown = _create_rgba_format(32, 32, 32, 0, data_type::none),
	
			r32g32b32a32_unorm   = _create_rgba_format(32, 32, 32, 32, data_type::unsigned_normalized),
			r32g32b32a32_snorm   = _create_rgba_format(32, 32, 32, 32, data_type::signed_normalized),
			r32g32b32a32_srgb    = _create_rgba_format(32, 32, 32, 32, data_type::srgb),
			r32g32b32a32_uint    = _create_rgba_format(32, 32, 32, 32, data_type::unsigned_integer),
			r32g32b32a32_sint    = _create_rgba_format(32, 32, 32, 32, data_type::signed_integer),
			r32g32b32a32_float   = _create_rgba_format(32, 32, 32, 32, data_type::floating_point),
			r32g32b32a32_unknown = _create_rgba_format(32, 32, 32, 32, data_type::none),
		};

		/// No initialization.
		format(uninitialized_t) {
		}
		/// Initializes the enum to \ref none.
		constexpr format(zero_t) : _value(value_type::none) {
		}
		/// Initializes \ref _value with the given \ref value_type object.
		constexpr format(value_type v) : _value(v) {
		}
		/// Creates a \ref format object from the given parameters.
		[[nodiscard]] constexpr static inline format create_rgba(
			std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha, data_type type
		) {
			return format(_create_rgba_format(red, green, blue, alpha, type));
		}

		/// Returns the \ref data_type of this pixel format.
		[[nodiscard]] constexpr data_type get_data_type() const {
			return static_cast<data_type>((_value & constants::data_type_mask) >> constants::data_type_offset);
		}

		/// Comparison.
		[[nodiscard]] friend constexpr bool operator==(format lhs, format rhs) {
			return lhs._value == rhs._value;
		}
	protected:
		value_type _value; ///< The enum value.

		/// Initializes \ref _value.
		constexpr explicit format(std::uint32_t val) : _value(static_cast<value_type>(val)) {
		}
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
	/// A bitmask for shader stages.
	enum class shader_stage_mask : std::uint8_t {
		none            = 0,       ///< None.
		vertex_shader   = 1u << 0, ///< Vertex shader.
		// TODO tessellation shaders
		geometry_shader = 1u << 1, ///< Geometry shader.
		pixel_shader    = 1u << 2, ///< Pixel shader.
		compute_shader  = 1u << 3, ///< Compute shader.

		all = vertex_shader | geometry_shader | pixel_shader | compute_shader, ///< A mask for all stages.
		num_enumerators = 4, ///< The number of available stages.
	};
}
namespace lotus {
	/// Enable bitwise operations for \ref graphics::channels.
	template <> struct enable_enum_bitwise_operators<graphics::shader_stage_mask> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref graphics::channels.
	template <> struct enable_enum_is_empty<graphics::shader_stage_mask> : public std::true_type {
	};
}

namespace lotus::graphics {
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
		traignle_strip, ///< A strip of triangles - every three consecutive vertices define a triangle.
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

		num_enumerators ///< The total number of enumerators.
	};

	/// The state of an image resource.
	enum class image_usage : std::uint8_t {
		color_render_target, ///< The image can be used as a color render target.
		depth_stencil_render_target, ///< The image can be used as a depth-stencil render target.
		/// State indicating that the image has been used for presenting. Normally this state is not manually
		/// transitioned to.
		present,

		num_enumerators ///< The total number of enumerators.
	};
	/// The usage of a buffer resource.
	enum class buffer_usage : std::uint8_t {
		index_buffer, ///< Used as an index buffer.
		vertex_buffer, ///< Used as a vertex buffer.
		uniform_buffer, ///< Used as a uniform buffer.
		copy_source, ///< Source for copy operations.
		copy_destination, ///< Target for copy operations.

		num_enumerators ///< The total number of enumerators.
	};

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
		// TODO
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

	/// \ref render_target_blend_options for 8 render targets.
	struct blend_options {
	public:
		/// No initialization.
		blend_options(uninitialized_t) : render_target_options({
			uninitialized, uninitialized, uninitialized, uninitialized,
			uninitialized, uninitialized, uninitialized, uninitialized
		}) {
		}
		/// Creates a \ref blend_options object that has the given blend options for all render targets.
		[[nodiscard]] constexpr inline static blend_options create_for_render_targets(
			const render_target_blend_options &opt1,
			const render_target_blend_options &opt2 = render_target_blend_options::disabled(),
			const render_target_blend_options &opt3 = render_target_blend_options::disabled(),
			const render_target_blend_options &opt4 = render_target_blend_options::disabled(),
			const render_target_blend_options &opt5 = render_target_blend_options::disabled(),
			const render_target_blend_options &opt6 = render_target_blend_options::disabled(),
			const render_target_blend_options &opt7 = render_target_blend_options::disabled(),
			const render_target_blend_options &opt8 = render_target_blend_options::disabled()
		) {
			return blend_options({ opt1, opt2, opt3, opt4, opt5, opt6, opt7, opt8 });
		}
		/// \overload
		[[nodiscard]] constexpr inline static blend_options create_blend(
			std::initializer_list<render_target_blend_options> options
		) {
			return blend_options(options);
		}

		/// \ref render_target_blend_options for all render targets.
		std::array<render_target_blend_options, num_color_render_targets> render_target_options;
		// TODO logic operations
	protected:
		/// Initializes \ref render_target_options with the given list of options.
		constexpr blend_options(std::initializer_list<render_target_blend_options> options) : render_target_options({
			render_target_blend_options::disabled(), render_target_blend_options::disabled(),
			render_target_blend_options::disabled(), render_target_blend_options::disabled(),
			render_target_blend_options::disabled(), render_target_blend_options::disabled(),
			render_target_blend_options::disabled(), render_target_blend_options::disabled()
		}) {
			assert(options.size() <= num_color_render_targets);
			std::copy(options.begin(), options.end(), render_target_options.begin());
		}
	};

	/// Option used by the rasterizer to offset depth values.
	struct depth_bias_options {
	public:
		/// No initialization.
		depth_bias_options(uninitialized_t) {
		}
		/// Initializes all fields to zero, effectively having no bias.
		constexpr depth_bias_options(zero_t) : bias(0.0f), slope_scaled_bias(0.0f), clamp(0.0f) {
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
			LPCSTR sname, UINT sindex, format fmt, std::size_t off
		) {
			return input_buffer_element(sname, sindex, fmt, off);
		}

		// TODO replace these with reflection data. for Vulkan, these two would be replaced by a single location number
		LPCSTR semantic_name;
		UINT semantic_index;

		format element_format = uninitialized; ///< The format of this element.
		std::size_t byte_offset; ///< Byte offset of this element in a vertex.
	protected:
		/// Initializes all fields of this struct.
		constexpr input_buffer_element(LPCSTR sname, UINT sindex, format fmt, std::size_t off) :
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

		format pixel_format = uninitialized; ///< Expected pixel format for this attachment.
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

		format pixel_format = uninitialized; ///< Expected pixel format for this attachment.
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
		[[nodiscard]] constexpr inline static descriptor_range create(
			descriptor_type ty, std::size_t c, std::size_t r
		) {
			return descriptor_range(ty, c, r);
		}

		descriptor_type type; ///< The type of all descriptors in this range.
		std::size_t count; ///< The number of descriptors.
		std::size_t register_index; ///< Register index corresponding to this descriptor.
	protected:
		/// Initializes all fields of this struct.
		constexpr descriptor_range(
			descriptor_type ty, std::size_t c, std::size_t r
		) : type(ty), count(c), register_index(r) {
		}
	};

	/// An image resource barrier.
	struct image_barrier {
	public:
		/// No initialization.
		image_barrier(uninitialized_t) {
		}
		/// Creates a new \ref image_barrier object.
		[[nodiscard]] constexpr static inline image_barrier create(image &img, image_usage from, image_usage to) {
			return image_barrier(img, from, to);
		}

		image *target; ///< Target image.
		// TODO subresource
		image_usage from_state; ///< State to transition from.
		image_usage to_state; ///< State to transition to.
	protected:
		/// Initializes all fields of this struct.
		constexpr image_barrier(image &i, image_usage from, image_usage to) :
			target(&i), from_state(from), to_state(to) {
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
		buffer_usage from_state; ///< State to transition from.
		buffer_usage to_state; ///< State to transition to.
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
		[[nodiscard]] constexpr inline static vertex_buffer from_buffer_stride(buffer &b, std::size_t s) {
			return vertex_buffer(b, s);
		}

		buffer *data; ///< Data for the vertex buffer.
		std::size_t stride; ///< The stride of a single vertex.
	protected:
		/// Initializes all fields of this struct.
		constexpr vertex_buffer(buffer &b, std::size_t s) : data(&b), stride(s) {
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
}
