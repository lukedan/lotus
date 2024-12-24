#include "lotus/gpu/backends/common/dxgi_format.h"

/// \file
/// Lookup tables for format conversions.

#include <vector>

#include <Unknwn.h>
#include <directx/d3d12.h>

#include "lotus/common.h"

namespace lotus::gpu::backends::common::_details::conversions {
	static_assert(
		std::is_same_v<std::underlying_type_t<DXGI_FORMAT>, std::underlying_type_t<dxgi_format>>,
		"Format type mismatch"
	);

	/// Lookup table of all available formats.
	constexpr static enums::sequential_mapping<format, DXGI_FORMAT> _lookup_table{
		std::pair(format::none,               DXGI_FORMAT_UNKNOWN             ),
		std::pair(format::d32_float_s8,       DXGI_FORMAT_D32_FLOAT_S8X24_UINT),
		std::pair(format::d32_float,          DXGI_FORMAT_D32_FLOAT           ),
		std::pair(format::d24_unorm_s8,       DXGI_FORMAT_D24_UNORM_S8_UINT   ),
		std::pair(format::d16_unorm,          DXGI_FORMAT_D16_UNORM           ),
		std::pair(format::r8_unorm,           DXGI_FORMAT_R8_UNORM            ),
		std::pair(format::r8_snorm,           DXGI_FORMAT_R8_SNORM            ),
		std::pair(format::r8_uint,            DXGI_FORMAT_R8_UINT             ),
		std::pair(format::r8_sint,            DXGI_FORMAT_R8_SINT             ),
		std::pair(format::r8g8_unorm,         DXGI_FORMAT_R8G8_UNORM          ),
		std::pair(format::r8g8_snorm,         DXGI_FORMAT_R8G8_SNORM          ),
		std::pair(format::r8g8_uint,          DXGI_FORMAT_R8G8_UINT           ),
		std::pair(format::r8g8_sint,          DXGI_FORMAT_R8G8_SINT           ),
		std::pair(format::r8g8b8a8_unorm,     DXGI_FORMAT_R8G8B8A8_UNORM      ),
		std::pair(format::r8g8b8a8_snorm,     DXGI_FORMAT_R8G8B8A8_SNORM      ),
		std::pair(format::r8g8b8a8_srgb,      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ),
		std::pair(format::r8g8b8a8_uint,      DXGI_FORMAT_R8G8B8A8_UINT       ),
		std::pair(format::r8g8b8a8_sint,      DXGI_FORMAT_R8G8B8A8_SINT       ),
		std::pair(format::b8g8r8a8_unorm,     DXGI_FORMAT_B8G8R8A8_UNORM      ),
		std::pair(format::b8g8r8a8_srgb,      DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ),
		std::pair(format::r16_unorm,          DXGI_FORMAT_R16_UNORM           ),
		std::pair(format::r16_snorm,          DXGI_FORMAT_R16_SNORM           ),
		std::pair(format::r16_uint,           DXGI_FORMAT_R16_UINT            ),
		std::pair(format::r16_sint,           DXGI_FORMAT_R16_SINT            ),
		std::pair(format::r16_float,          DXGI_FORMAT_R16_FLOAT           ),
		std::pair(format::r16g16_unorm,       DXGI_FORMAT_R16G16_UNORM        ),
		std::pair(format::r16g16_snorm,       DXGI_FORMAT_R16G16_SNORM        ),
		std::pair(format::r16g16_uint,        DXGI_FORMAT_R16G16_UINT         ),
		std::pair(format::r16g16_sint,        DXGI_FORMAT_R16G16_SINT         ),
		std::pair(format::r16g16_float,       DXGI_FORMAT_R16G16_FLOAT        ),
		std::pair(format::r16g16b16a16_unorm, DXGI_FORMAT_R16G16B16A16_UNORM  ),
		std::pair(format::r16g16b16a16_snorm, DXGI_FORMAT_R16G16B16A16_SNORM  ),
		std::pair(format::r16g16b16a16_uint,  DXGI_FORMAT_R16G16B16A16_UINT   ),
		std::pair(format::r16g16b16a16_sint,  DXGI_FORMAT_R16G16B16A16_SINT   ),
		std::pair(format::r16g16b16a16_float, DXGI_FORMAT_R16G16B16A16_FLOAT  ),
		std::pair(format::r32_uint,           DXGI_FORMAT_R32_UINT            ),
		std::pair(format::r32_sint,           DXGI_FORMAT_R32_SINT            ),
		std::pair(format::r32_float,          DXGI_FORMAT_R32_FLOAT           ),
		std::pair(format::r32g32_uint,        DXGI_FORMAT_R32G32_UINT         ),
		std::pair(format::r32g32_sint,        DXGI_FORMAT_R32G32_SINT         ),
		std::pair(format::r32g32_float,       DXGI_FORMAT_R32G32_FLOAT        ),
		std::pair(format::r32g32b32_uint,     DXGI_FORMAT_R32G32B32_UINT      ),
		std::pair(format::r32g32b32_sint,     DXGI_FORMAT_R32G32B32_SINT      ),
		std::pair(format::r32g32b32_float,    DXGI_FORMAT_R32G32B32_FLOAT     ),
		std::pair(format::r32g32b32a32_uint,  DXGI_FORMAT_R32G32B32A32_UINT   ),
		std::pair(format::r32g32b32a32_sint,  DXGI_FORMAT_R32G32B32A32_SINT   ),
		std::pair(format::r32g32b32a32_float, DXGI_FORMAT_R32G32B32A32_FLOAT  ),
		std::pair(format::bc1_unorm,          DXGI_FORMAT_BC1_UNORM           ),
		std::pair(format::bc1_srgb,           DXGI_FORMAT_BC1_UNORM_SRGB      ),
		std::pair(format::bc2_unorm,          DXGI_FORMAT_BC2_UNORM           ),
		std::pair(format::bc2_srgb,           DXGI_FORMAT_BC2_UNORM_SRGB      ),
		std::pair(format::bc3_unorm,          DXGI_FORMAT_BC3_UNORM           ),
		std::pair(format::bc3_srgb,           DXGI_FORMAT_BC3_UNORM_SRGB      ),
		std::pair(format::bc4_unorm,          DXGI_FORMAT_BC4_UNORM           ),
		std::pair(format::bc4_snorm,          DXGI_FORMAT_BC4_SNORM           ),
		std::pair(format::bc5_unorm,          DXGI_FORMAT_BC5_UNORM           ),
		std::pair(format::bc5_snorm,          DXGI_FORMAT_BC5_SNORM           ),
		std::pair(format::bc6h_f16,           DXGI_FORMAT_BC6H_SF16           ),
		std::pair(format::bc6h_uf16,          DXGI_FORMAT_BC6H_UF16           ),
		std::pair(format::bc7_unorm,          DXGI_FORMAT_BC7_UNORM           ),
		std::pair(format::bc7_srgb,           DXGI_FORMAT_BC7_UNORM_SRGB      ),
	};

	dxgi_format to_format(format fmt) {
		return static_cast<dxgi_format>(_lookup_table[fmt]);
	}

	[[nodiscard]] constexpr static std::array<
		std::pair<format, DXGI_FORMAT>, static_cast<std::size_t>(format::num_enumerators)
	> _get_sorted_format_table() {
		std::array table = _lookup_table.get_raw_table();
		std::sort(table.begin(), table.end(), [](const auto &lhs, const auto &rhs) {
			return lhs.second < rhs.second;
		});
		return table;
	}
	format back_to_format(dxgi_format raw_fmt) {
		constexpr static std::array table = _get_sorted_format_table();

		const auto fmt = static_cast<DXGI_FORMAT>(raw_fmt);
		auto it = std::lower_bound(table.begin(), table.end(), fmt, [](const auto &pair, auto fmt) {
			return pair.second < fmt;
		});
		if (it != table.end() && it->second == fmt) {
			return it->first;
		}
		return format::none;
	}
}
