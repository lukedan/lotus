#pragma once

/// \file
/// Common DDS enums and structures.

#include <cstdint>
#include <type_traits>

#include "lotus/common.h"
#include "misc.h"

namespace lotus {
	namespace dds {
		/// Magic number at the beginning of a DDS file.
		constexpr std::uint32_t magic = make_four_character_code(u8"DDS ");

		/// Pixel format flags.
		enum class pixel_format_flags : std::uint32_t {
			alpha_pixels = 1 << 0,  ///< Texture contains alpha data.
			alpha        = 1 << 1,  ///< Alpha-only uncompressed data.
			four_cc      = 1 << 2,  ///< Compressed RGB data, see also \ref pixel_format::four_cc.
			rgb          = 1 << 6,  ///< Uncompressed RGB data.
			yuv          = 1 << 9,  ///< Uncompressed YUV data.
			luminance    = 1 << 17, ///< Uncompressed single channel data.
			bump_dudv    = 1 << 19, ///< Uncompressed signed data?
		};
	}
	/// Enable bitwise operations for \ref dds::pixel_format_flags.
	template <> struct enable_enum_bitwise_operators<dds::pixel_format_flags> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref dds::pixel_format_flags.
	template <> struct enable_enum_is_empty<dds::pixel_format_flags> : public std::true_type {
	};

	namespace dds {
		/// Flags that indicate which members of \ref header contain valid data.
		enum class header_flags : std::uint32_t {
			caps         = 1 << 0,  ///< Required.
			height       = 1 << 1,  ///< Required.
			width        = 1 << 2,  ///< Required.
			pitch        = 1 << 3,  ///< Required when pitch is provided for an uncompressed texture.
			pixel_format = 1 << 12, ///< Required.
			mipmap_count = 1 << 17, ///< Required in a mipmapped texture.
			linear_size  = 1 << 19, ///< Required when pitch is provided for a compressed texture.
			depth        = 1 << 23, ///< Required in a depth texture.

			required_flags = caps | height | width | pixel_format, ///< All required flags.
		};
	}
	/// Enable bitwise operations for \ref dds::header_flags.
	template <> struct enable_enum_bitwise_operators<dds::header_flags> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref dds::header_flags.
	template <> struct enable_enum_is_empty<dds::header_flags> : public std::true_type {
	};

	namespace dds {
		/// Information about a DDS file.
		enum class capabilities : std::uint32_t {
			complex = 1 << 3,  ///< Used on any file that contains more than one texture (mipmaps or cubemaps).
			texture = 1 << 12, ///< Required.
			mipmap  = 1 << 22, ///< Used for a mipmap.
		};
	}
	/// Enable bitwise operations for \ref dds::capabilities.
	template <> struct enable_enum_bitwise_operators<dds::capabilities> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref dds::capabilities.
	template <> struct enable_enum_is_empty<dds::capabilities> : public std::true_type {
	};

	namespace dds {
		/// Additional information about a DDS file.
		enum class capabilities2 : std::uint32_t {
			none = 0, ///< None.

			cubemap            = 1 << 9,  ///< Required for a cubemap.
			cubemap_positive_x = 1 << 10, ///< The file contains the positive X face of a cubemap.
			cubemap_negative_x = 1 << 11, ///< The file contains the negative X face of a cubemap.
			cubemap_positive_y = 1 << 12, ///< The file contains the positive Y face of a cubemap.
			cubemap_negative_y = 1 << 13, ///< The file contains the negative Y face of a cubemap.
			cubemap_positive_z = 1 << 14, ///< The file contains the positive Z face of a cubemap.
			cubemap_negative_z = 1 << 15, ///< The file contains the negative Z face of a cubemap.
			volume             = 1 << 21, ///< Required for a volume texture.

			/// Mask of all cubemap faces.
			cubemap_all_faces =
				cubemap_positive_x | cubemap_negative_x |
				cubemap_positive_y | cubemap_negative_y |
				cubemap_positive_z | cubemap_negative_z,
		};
	}
	/// Enable bitwise operations for \ref dds::capabilities2.
	template <> struct enable_enum_bitwise_operators<dds::capabilities2> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref dds::capabilities2.
	template <> struct enable_enum_is_empty<dds::capabilities2> : public std::true_type {
	};

	namespace dds {
		/// \p D3D10_RESOURCE_DIMENSION.
		enum class resource_dimension : std::uint32_t {
			unknown   = 0, ///< Unknown.
			buffer    = 1, ///< Resource is a buffer.
			texture1d = 2, ///< Resource is a 1D texture.
			texture2d = 3, ///< Resource is a 2D texture.
			texture3d = 4, ///< Resource is a 3D texture.
		};

		/// Miscellaneous resource flags.
		enum class miscellaneous_flags : std::uint32_t {
			none = 0, ///< None.

			texture_cube = 1 << 2, ///< This 2D texture is a cubemap.
		};
	}
	/// Enable bitwise operations for \ref dds::miscellaneous_flags.
	template <> struct enable_enum_bitwise_operators<dds::miscellaneous_flags> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref dds::miscellaneous_flags.
	template <> struct enable_enum_is_empty<dds::miscellaneous_flags> : public std::true_type {
	};

	namespace dds {
		/// Additional miscellaneous flags.
		enum class miscellaneous_flags2 : std::uint32_t {
			alpha_mode_unknown       = 0,      ///< Unknown alpha mode.
			alpha_mode_straight      = 1 << 0, ///< Alpha channel is handled normally.
			alpha_mode_premultiplied = 1 << 1, ///< RGB channels are premultiplied with alpha.
			alpha_mode_opaque        = 1 << 2, ///< Alpha is fully opaque.
			alpha_mode_custom        = 1 << 3, ///< Alpha channel does not indicate transparency.
		};
	}
	/// Enable bitwise operations for \ref dds::miscellaneous_flags2.
	template <> struct enable_enum_bitwise_operators<dds::miscellaneous_flags2> : public std::true_type {
	};
	/// Enable \ref is_empty for \ref dds::miscellaneous_flags2.
	template <> struct enable_enum_is_empty<dds::miscellaneous_flags2> : public std::true_type {
	};


	namespace dds {
		/// Pixel format.
		struct pixel_format {
			std::uint32_t      size;          ///< Structure size - 32.
			pixel_format_flags flags;         ///< Flags.
			std::uint32_t      four_cc;       ///< Four-character code specifying compressed or custom formats.
			std::uint32_t      rgb_bit_count; ///< Number of bits in an RGB format.
			std::uint32_t      r_bit_mask;    ///< R or Y channel mask.
			std::uint32_t      g_bit_mask;    ///< G or U channel mask.
			std::uint32_t      b_bit_mask;    ///< B or V channel mask.
			std::uint32_t      a_bit_mask;    ///< A channel mask.

			/// Checks if the bit masks contain exactly the supplied values.
			[[nodiscard]] bool is_bit_mask(
				std::uint32_t r, std::uint32_t g, std::uint32_t b, std::uint32_t a
			) const {
				return r_bit_mask == r && g_bit_mask == g && b_bit_mask == b && a_bit_mask == a;
			}
		};
		static_assert(std::is_standard_layout_v<pixel_format>, "Expected dds::pixel_format to be a POD type.");

		/// DDS header definition.
		struct header {
			std::uint32_t size;          ///< Structure size - 124.
			header_flags  flags;         ///< Indicates which members contain valid data.
			std::uint32_t height;        ///< Height in pixels.
			std::uint32_t width;         ///< Width in pixels.
			/// The number of bytes per scan line in an uncompressed texture, or the total number of bytes in the top
			/// level texture for a compressed texture.
			std::uint32_t pitch_or_linear_size;
			std::uint32_t depth;         ///< Depth of a volume texture.
			std::uint32_t mipmap_count;  ///< Number of mipmap levels.
			std::uint32_t reserved1[11]; ///< Unused.
			pixel_format  pixel_format;  ///< Pixel format description.
			capabilities  caps;          ///< Information about the file.
			capabilities2 caps2;         ///< Additional information about the file.
			std::uint32_t caps3;         ///< Unused.
			std::uint32_t caps4;         ///< Unused.
			std::uint32_t reserved2;     ///< Unused.
		};
		static_assert(std::is_standard_layout_v<header>, "Expected dds::header to be a POD type.");

		/// Additional DDS header information.
		struct header_dx10 {
			std::uint32_t        dxgi_format; ///< DXGI format.
			resource_dimension   dimension;   ///< The dimension of this resource.
			miscellaneous_flags  flags;       ///< Miscellaneous flags.
			std::uint32_t        array_size;  ///< Number of array elements.
			miscellaneous_flags2 flags2;      ///< Additional miscellaneous flags.
		};
		static_assert(std::is_standard_layout_v<header_dx10>, "Expected dds::header to be a POD type.");
	}
}
