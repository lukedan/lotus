#pragma once

/// \file
/// DDS file loader.

#include <cstdint>
#include <type_traits>
#include <optional>
#include <span>

#include "lotus/utils/misc.h"
#include "lotus/utils/dds.h"
#include "lotus/gpu/common.h"

namespace lotus {
	namespace dds {
		/// Loader for a single DDS file.
		class loader {
		public:
			/// Converts a four-character code or an enum value to a \ref gpu::format.
			[[nodiscard]] static gpu::format four_cc_to_format(std::uint32_t);
			/// Infers the pixel format from a \ref pixel_format object.
			[[nodiscard]] static gpu::format infer_format_from(const pixel_format&);

			/// Initializes the loader to empty.
			loader(std::nullptr_t) {
			}
			/// Creates a loader for the given binary data.
			[[nodiscard]] static std::optional<loader> create(std::span<const std::byte>);

			/// Returns the header of this file.
			[[nodiscard]] const header &get_header() const {
				return *_data_at_offset<header>(sizeof(std::uint32_t));
			}
			/// Returns whether this file contains a DX10 header.
			[[nodiscard]] bool has_dx10_header() const {
				return _has_dx10_header;
			}
			/// Returns the DX10 header of this file.
			[[nodiscard]] const header_dx10 *get_dx10_header() const {
				return
					_has_dx10_header ?
					_data_at_offset<header_dx10>(sizeof(std::uint32_t) + sizeof(header)) :
					nullptr;
			}

			/// Returns raw image data, excluding headers.
			[[nodiscard]] std::span<const std::byte> get_raw_data() const {
				const auto *start = _data_at_offset(
					sizeof(std::uint32_t) +
					sizeof(header) +
					(_has_dx10_header ? sizeof(header_dx10) : 0)
				);
				return { start, _data + _size };
			}
			/// Returns whether the image is a cubemap.
			[[nodiscard]] bool is_cubemap() const {
				return _is_cubemap;
			}
			/// Returns the width of the first mipmap.
			[[nodiscard]] std::uint32_t get_width() const {
				return _width;
			}
			/// Returns the height of the first mipmap.
			[[nodiscard]] std::uint32_t get_height() const {
				return _height;
			}
			/// Returns the depth of the first mipmap.
			[[nodiscard]] std::uint32_t get_depth() const {
				return _depth;
			}
			/// Returns the number of array slices.
			[[nodiscard]] std::uint32_t get_array_size() const {
				return _array_size;
			}
			/// Returns the number of mips.
			[[nodiscard]] std::uint32_t get_num_mips() const {
				return _num_mips;
			}
			/// Returns the pixel format of the image.
			[[nodiscard]] gpu::format get_format() const {
				return _format;
			}
		private:
			const std::byte *_data = nullptr; ///< Binary data.
			std::size_t _size = 0; ///< The size of the binary.

			bool _has_dx10_header = false; ///< Whether a DX10 header is available.

			bool _is_cubemap = false; ///< Whether this is a cubemap texture.
			std::uint32_t _width      = 0; ///< The width of the texture.
			std::uint32_t _height     = 0; ///< The height of the texture.
			std::uint32_t _depth      = 0; ///< The depth of the texture.
			std::uint32_t _array_size = 0; ///< Number of array slices in the texture.
			std::uint32_t _num_mips   = 0; ///< Number of mips.
			gpu::format _format = gpu::format::none; ///< Pixel format.

			/// Offsets the data pointer by the given amount.
			///
			/// \tparam T Expected return type. If the file is not large enough to contain the value, \p nullptr will
			///           be returned.
			template <typename T = std::byte> const T *_data_at_offset(std::size_t off) const {
				if constexpr (!std::is_same_v<T, std::byte>) {
					if (off + sizeof(T) > _size) {
						return nullptr;
					}
				}
				return reinterpret_cast<const T*>(_data + off);
			}
		};
	}
}
