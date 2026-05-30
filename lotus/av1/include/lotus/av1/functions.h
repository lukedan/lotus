#pragma once

/// \file
/// Functions in the AV1 specification.

#include <bit>

#include "lotus/types.h"
#include "lotus/containers/short_vector.h"

#include "lotus/av1/common.h"
#include "lotus/av1/state.h"
#include "lotus/av1/tables.h"
#include "lotus/av1/obu.h"

namespace lotus::av1::functions {
	/// 4.7. Mathematical functions
	/// \p Clip1().
	template <typename T> [[nodiscard]] channel_t clip1(const obu::color_config &color_config, T x) {
		return static_cast<channel_t>(std::clamp<T>(x, zero, static_cast<T>(1u << color_config.bit_depth) - 1));
	}
	/// 4.7. Mathematical functions
	/// \p FloorLog2().
	[[nodiscard]] constexpr u32 floor_log2(u32 x) {
		return 31u - static_cast<u32>(std::countl_zero(x));
	}
	/// 4.7. Mathematical functions
	/// \p CeilLog2().
	[[nodiscard]] constexpr u32 ceil_log2(u32 x) {
		// TODO optimized impl
		if (x < 2) {
			return 0;
		}
		u32 i = 1;
		u32 p = 2;
		while (p < x) {
			++i;
			p = p << 1;
		}
		return i;
	}
	/// 4.7. Mathematical functions
	/// \p Round2().
	template <typename T> [[nodiscard]] constexpr T round2(T x, u32 n) {
		if (n == 0) {
			return x;
		}
		return (x + (1 << (n - 1))) >> n;
	}
	/// 4.7. Mathematical functions
	/// \p Round2Signed().
	template <typename T> [[nodiscard]] constexpr T round2_signed(T x, u32 n) {
		return x >= 0 ? round2(x, n) : -round2(-x, n);
	}

	/// 5.9.16. Tile size calculation function
	[[nodiscard]] constexpr u32 tile_log2(u32 blk_size, u32 target) {
		u32 k = 0;
		for (; (blk_size << k) < target; ++k) {
		}
		return k;
	}
	/// \ref tile_log2() with \p blk_size equal to 1.
	[[nodiscard]] constexpr u32 tile_log2_1(u32 target) {
		return tile_log2(1, target);
	}

	/// 5.9.29. Inverse recenter function
	[[nodiscard]] constexpr i32 inverse_recenter(i32 r, u32 v) {
		if (static_cast<i32>(v) > 2 * r) {
			return static_cast<i32>(v);
		} else if (v & 1) {
			return r - static_cast<i32>((v + 1) >> 1);
		} else {
			return r + static_cast<i32>(v >> 1);
		}
	}

	/// 5.11.3. Clear block decoded flags function
	inline void clear_block_decoded_flags(
		const obu::sequence_header &seq_header,
		state::block &sb,
		const state::block_range &sbr,
		u32 r, u32 c, u32 sb_size4
	) {
		for (u32 plane = 0; plane < seq_header.color_config.get_num_planes(); ++plane) {
			const u32 sub_x = plane > 0 ? (seq_header.color_config.subsampling_x ? 1 : 0) : 0;
			const u32 sub_y = plane > 0 ? (seq_header.color_config.subsampling_y ? 1 : 0) : 0;
			const u32 sb_width4 = (sbr.col_end - c) >> sub_x;
			const u32 sb_height4 = (sbr.row_end - r) >> sub_y;
			for (i32 y = -1; y <= static_cast<i32>(sb_size4 >> sub_y); ++y) {
				for (i32 x = -1; x <= static_cast<i32>(sb_size4 >> sub_x); ++x) {
					if (y < 0 && x < static_cast<i32>(sb_width4)) {
						sb.block_decoded(plane, y, x) = true;
					} else if (x < 0 && y < static_cast<i32>(sb_height4)) {
						sb.block_decoded(plane, y, x) = true;
					} else {
						sb.block_decoded(plane, y, x) = false;
					}
				}
			}
			sb.block_decoded(plane, static_cast<i32>(sb_size4 >> sub_y), -1) = false;
		}
	}

	/// 5.11.5. Decode block syntax
	/// \p reset_block_context().
	inline void reset_block_context(
		const obu::sequence_header &seq_header,
		state::block &sb,
		const state::block_decoding &sbd,
		u32 bw4, u32 bh4
	) {
		for (u32 plane = 0; plane < (sbd.has_chroma ? 3 : 1); ++plane) {
			const u32 sub_x = plane > 0 ? (seq_header.color_config.subsampling_x ? 1 : 0) : 0;
			const u32 sub_y = plane > 0 ? (seq_header.color_config.subsampling_y ? 1 : 0) : 0;
			for (u32 i = sbd.col >> sub_x; i < ((sbd.col + bw4) >> sub_x); ++i) {
				sb.above_level_context[plane][i] = 0;
				sb.above_dc_context[plane][i] = 0;
			}
			for (u32 i = sbd.row >> sub_y; i < ((sbd.row + bh4) >> sub_y); ++i) {
				sb.left_level_context[plane][i] = 0;
				sb.left_dc_context[plane][i] = 0;
			}
		}
	}

	/// 5.11.9. Read segment ID syntax
	/// \p neg_deinterleave().
	[[nodiscard]] constexpr u8 neg_deinterleave(u8 diff, u8 ref, u8 max) {
		if (ref == 0) {
			return diff;
		}
		if (ref >= (max - 1)) {
			return (max - 1) - diff;
		}
		if (2 * ref < max) {
			if (diff <= 2 * ref) {
				if (diff & 1) {
					return ref + ((diff + 1) >> 1);
				} else {
					return ref - (diff >> 1);
				}
			}
			return diff;
		} else {
			if (diff <= 2 * (max - ref - 1)) {
				if (diff & 1) {
					return ref + ((diff + 1) >> 1);
				} else {
					return ref - (diff >> 1);
				}
			}
			return max - (diff + 1);
		}
	}

	/// 5.11.21. Get segment ID function
	[[nodiscard]] constexpr segment_id_t get_segment_id(
		const state::block &sb, const state::block_decoding &sbd, u32 mi_rows, u32 mi_cols
	) {
		const u32 bw4 = constants::num_4x4_blocks_wide[std::to_underlying(sbd.size)];
		const u32 bh4 = constants::num_4x4_blocks_high[std::to_underlying(sbd.size)];
		const u32 x_mis = std::min(mi_cols - sbd.col, bw4);
		const u32 y_mis = std::min(mi_rows - sbd.row, bh4);
		auto seg = static_cast<segment_id_t>(7);
		for (u32 y = 0; y < y_mis; ++y) {
			for (u32 x = 0; x < x_mis; ++x) {
				seg = std::min(seg, sb.prev_segment_ids[sbd.row + y][sbd.col + x]);
			}
		}
		return seg;
	}

	/// 5.11.23. Inter block mode info syntax
	/// \p has_nearmv().
	[[nodiscard]] constexpr bool has_nearmv(prediction_mode y_mode) {
		return
			y_mode == prediction_mode::nearmv      ||
			y_mode == prediction_mode::near_nearmv ||
			y_mode == prediction_mode::near_newmv  ||
			y_mode == prediction_mode::new_nearmv;
	}

	/// 5.11.23. Inter block mode info syntax
	/// \p needs_interp_filter().
	[[nodiscard]] constexpr bool needs_interp_filter(
		const obu::global_motion_params &global_motion_params,
		const obu::mode_info &mode_info,
		const state::block_decoding &sbd
	) {
		const bool large = std::min(constants::block_width(sbd.size), constants::block_height(sbd.size)) >= 8;
		if (mode_info.skip_mode || mode_info.motion_mode == motion_compensation::localwarp) {
			return false;
		} else if (large && mode_info.y_mode == prediction_mode::globalmv) {
			const warp_model gm_type = global_motion_params.refs[std::to_underlying(mode_info.ref_frame[0])].type;
			return gm_type == warp_model::translation;
		} else if (large && mode_info.y_mode == prediction_mode::global_globalmv) {
			const warp_model gm_type0 = global_motion_params.refs[std::to_underlying(mode_info.ref_frame[0])].type;
			const warp_model gm_type1 = global_motion_params.refs[std::to_underlying(mode_info.ref_frame[1])].type;
			return gm_type0 == warp_model::translation || gm_type1 == warp_model::translation;
		} else {
			return true;
		}
	}

	/// 5.11.30. Get mode function
	[[nodiscard]] constexpr prediction_mode get_mode(const obu::mode_info &mode_info, u32 ref_list) {
		if (ref_list == 0) {
			if (mode_info.y_mode < prediction_mode::nearest_nearestmv) {
				return mode_info.y_mode;
			} else if (
				mode_info.y_mode == prediction_mode::new_newmv     ||
				mode_info.y_mode == prediction_mode::new_nearestmv ||
				mode_info.y_mode == prediction_mode::new_nearmv
			) {
				return prediction_mode::newmv;
			} else if (
				mode_info.y_mode == prediction_mode::nearest_nearestmv ||
				mode_info.y_mode == prediction_mode::nearest_newmv
			) {
				return prediction_mode::nearestmv;
			} else if (
				mode_info.y_mode == prediction_mode::near_nearmv ||
				mode_info.y_mode == prediction_mode::near_newmv
			) {
				return prediction_mode::nearmv;
			} else {
				return prediction_mode::globalmv;
			}
		} else {
			if (
				mode_info.y_mode == prediction_mode::new_newmv     ||
				mode_info.y_mode == prediction_mode::nearest_newmv ||
				mode_info.y_mode == prediction_mode::near_newmv
			) {
				return prediction_mode::newmv;
			} else if (
				mode_info.y_mode == prediction_mode::nearest_nearestmv ||
				mode_info.y_mode == prediction_mode::new_nearestmv
			) {
				return prediction_mode::nearestmv;
			} else if (
				mode_info.y_mode == prediction_mode::near_nearmv ||
				mode_info.y_mode == prediction_mode::new_nearmv
			) {
				return prediction_mode::nearmv;
			} else {
				return prediction_mode::globalmv;
			}
		}
	}

	/// 5.11.36. Transform tree syntax
	/// \p find_tx_size().
	[[nodiscard]] constexpr tx_size find_tx_size(u32 w, u32 h) {
		u32 tx_sz = 0;
		for (; tx_sz < constants::tx_sizes_all; ++tx_sz) {
			if (constants::tx_width[tx_sz] == w && constants::tx_height[tx_sz] == h) {
				break;
			}
		}
		return static_cast<tx_size>(tx_sz);
	}

	/// 5.11.37. Get TX size function
	[[nodiscard]] tx_size get_tx_size(
		const obu::color_config&, const state::block_decoding &sbd, u32 plane, tx_size tx_sz
	);

	/// 5.11.38. Get plane residual size function
	[[nodiscard]] constexpr block_size get_plane_residual_size(
		const obu::color_config &color_config, block_size subsize, u32 plane
	) {
		const bool subx = plane > 0 ? color_config.subsampling_x : false;
		const bool suby = plane > 0 ? color_config.subsampling_y : false;
		return constants::subsampled_size[std::to_underlying(subsize)][subx ? 1 : 0][suby ? 1 : 0];
	}

	/// 5.11.40. Compute transform type function
	[[nodiscard]] inverse_transform compute_tx_type(
		const obu::sequence_header&,
		const obu::uncompressed_header&,
		const obu::mode_info&,
		const state::block&,
		const state::block_decoding&,
		u32 plane, tx_size tx_sz, u32 block_x, u32 block_y
	);
	/// 5.11.40. Compute transform type function
	/// \p is_tx_type_in_set().
	[[nodiscard]] constexpr bool is_tx_type_in_set(
		const obu::mode_info &mode_info, tx_set tx_set, inverse_transform tx_type
	) {
		return
			mode_info.is_inter ?
			constants::tx_type_in_set_inter[std::to_underlying(tx_set)][std::to_underlying(tx_type)] :
			constants::tx_type_in_set_intra[std::to_underlying(tx_set)][std::to_underlying(tx_type)];
	}

	/// 5.11.41. Get scan function
	/// \p get_mrow_scan().
	[[nodiscard]] constexpr constants::scan::table_t get_mrow_scan(tx_size tx_sz) {
		switch (tx_sz) {
		case tx_size::size_4x4:
			return constants::scan::mrow_4x4;
		case tx_size::size_4x8:
			return constants::scan::mrow_4x8;
		case tx_size::size_8x4:
			return constants::scan::mrow_8x4;
		case tx_size::size_8x8:
			return constants::scan::mrow_8x8;
		case tx_size::size_8x16:
			return constants::scan::mrow_8x16;
		case tx_size::size_16x8:
			return constants::scan::mrow_16x8;
		case tx_size::size_16x16:
			return constants::scan::mrow_16x16;
		case tx_size::size_4x16:
			return constants::scan::mrow_4x16;
		default:
			return constants::scan::mrow_16x4;
		}
	}
	/// 5.11.41. Get scan function
	/// \p get_mcol_scan().
	[[nodiscard]] constexpr constants::scan::table_t get_mcol_scan(tx_size tx_sz) {
		switch (tx_sz) {
		case tx_size::size_4x4:
			return constants::scan::mcol_4x4;
		case tx_size::size_4x8:
			return constants::scan::mcol_4x8;
		case tx_size::size_8x4:
			return constants::scan::mcol_8x4;
		case tx_size::size_8x8:
			return constants::scan::mcol_8x8;
		case tx_size::size_8x16:
			return constants::scan::mcol_8x16;
		case tx_size::size_16x8:
			return constants::scan::mcol_16x8;
		case tx_size::size_16x16:
			return constants::scan::mcol_16x16;
		case tx_size::size_4x16:
			return constants::scan::mcol_4x16;
		default:
			return constants::scan::mcol_16x4;
		}
	}
	/// 5.11.41. Get scan function
	/// \p get_default_scan().
	[[nodiscard]] constexpr constants::scan::table_t get_default_scan(tx_size tx_sz) {
		switch (tx_sz) {
		case tx_size::size_4x4:
			return constants::scan::default_4x4;
		case tx_size::size_4x8:
			return constants::scan::default_4x8;
		case tx_size::size_8x4:
			return constants::scan::default_8x4;
		case tx_size::size_8x8:
			return constants::scan::default_8x8;
		case tx_size::size_8x16:
			return constants::scan::default_8x16;
		case tx_size::size_16x8:
			return constants::scan::default_16x8;
		case tx_size::size_16x16:
			return constants::scan::default_16x16;
		case tx_size::size_16x32:
			return constants::scan::default_16x32;
		case tx_size::size_32x16:
			return constants::scan::default_32x16;
		case tx_size::size_4x16:
			return constants::scan::default_4x16;
		case tx_size::size_16x4:
			return constants::scan::default_16x4;
		case tx_size::size_8x32:
			return constants::scan::default_8x32;
		case tx_size::size_32x8:
			return constants::scan::default_32x8;
		default:
			return constants::scan::default_32x32;
		}
	}
	/// 5.11.41. Get scan function
	/// \p get_scan().
	[[nodiscard]] constexpr constants::scan::table_t get_scan(const state::block &sb, tx_size tx_sz) {
		if (tx_sz == tx_size::size_16x64) {
			return constants::scan::default_16x32;
		}
		if (tx_sz == tx_size::size_64x16) {
			return constants::scan::default_32x16;
		}
		if (constants::get_tx_size_sqr_up(tx_sz) == tx_size::size_64x64) {
			return constants::scan::default_32x32;
		}

		if (sb.plane_tx_type == inverse_transform::idtx) {
			return get_default_scan(tx_sz);
		}

		const bool prefer_row =
			sb.plane_tx_type == inverse_transform::v_dct  ||
			sb.plane_tx_type == inverse_transform::v_adst ||
			sb.plane_tx_type == inverse_transform::v_flipadst;

		const bool prefer_col =
			sb.plane_tx_type == inverse_transform::h_dct  ||
			sb.plane_tx_type == inverse_transform::h_adst ||
			sb.plane_tx_type == inverse_transform::h_flipadst;

		if (prefer_row) {
			return get_mrow_scan(tx_sz);
		} else if (prefer_col) {
			return get_mcol_scan(tx_sz);
		}
		return get_default_scan(tx_sz);
	}

	/// 5.11.44. Is directional mode function
	[[nodiscard]] constexpr bool is_directional_mode(prediction_mode mode) {
		return mode >= prediction_mode::v && mode <= prediction_mode::d67;
	}

	/// \p PaletteSizes and its size \p n.
	using palette_cache_t = short_vector<channel_t, constants::palette_colors * 2>;
	/// 5.11.46. Palette mode info syntax
	/// \return An array \p PaletteCache with size \p n.
	[[nodiscard]] inline palette_cache_t get_palette_cache(
		const state::block &sb, const state::block_decoding &sbd, u32 plane
	) {
		u32 above_n = 0;
		if ((sbd.row * constants::mi_size) % 64 != 0) {
			above_n = sb.palette_sizes[plane](sbd.row - 1, sbd.col);
		}
		u32 left_n = 0;
		if (sbd.avail_l) {
			left_n = sb.palette_sizes[plane](sbd.row, sbd.col - 1);
		}
		u32 above_idx = 0;
		u32 left_idx = 0;
		palette_cache_t palette_cache;
		while (above_idx < above_n && left_idx < left_n) {
			const channel_t above_c = sb.palette_colors[plane](sbd.row - 1, sbd.col)[above_idx];
			const channel_t left_c = sb.palette_colors[plane](sbd.row, sbd.col - 1)[left_idx];
			if (left_c < above_c) {
				if (palette_cache.empty() || left_c != palette_cache.back()) {
					palette_cache.emplace_back(left_c);
				}
				++left_idx;
			} else {
				if (palette_cache.empty() || above_c != palette_cache.back()) {
					palette_cache.emplace_back(above_c);
				}
				++above_idx;
				if (left_c == above_c) {
					++left_idx;
				}
			}
		}
		while (above_idx < above_n) {
			const channel_t val = sb.palette_colors[plane](sbd.row - 1, sbd.col)[above_idx];
			++above_idx;
			if (palette_cache.empty() || val != palette_cache.back()) {
				palette_cache.emplace_back(val);
			}
		}
		while (left_idx < left_n) {
			const channel_t val = sb.palette_colors[plane](sbd.row, sbd.col - 1)[left_idx];
			++left_idx;
			if (palette_cache.empty() || val != palette_cache.back()) {
				palette_cache.emplace_back(val);
			}
		}
		return palette_cache;
	}

	/// 5.11.48. Get transform set function
	[[nodiscard]] constexpr tx_set get_tx_set(
		const obu::uncompressed_header &header, const obu::mode_info &mode_info, tx_size tx_sz
	) {
		const tx_size tx_sz_sqr = constants::get_tx_size_sqr(tx_sz);
		const tx_size tx_sz_sqr_up = constants::get_tx_size_sqr_up(tx_sz);
		if (tx_sz_sqr_up > tx_size::size_32x32) {
			return tx_set::dct_only;
		}
		if (mode_info.is_inter) {
			if (header.reduced_tx_set || tx_sz_sqr_up == tx_size::size_32x32) {
				return tx_set::inter_3;
			} else if (tx_sz_sqr == tx_size::size_16x16) {
				return tx_set::inter_2;
			} else {
				return tx_set::inter_1;
			}
		} else {
			if (tx_sz_sqr_up == tx_size::size_32x32) {
				return tx_set::dct_only;
			} else if (header.reduced_tx_set) {
				return tx_set::intra_2;
			} else if (tx_sz_sqr == tx_size::size_16x16) {
				return tx_set::intra_2;
			} else {
				return tx_set::intra_1;
			}
		}
	}

	/// 5.11.55. Clear CDEF function
	inline void clear_cdef(const obu::sequence_header &seq_header, state::block &sb, u32 r, u32 c) {
		sb.cdef_idx(r, c) = -1;
		if (seq_header.use_128x128_superblock) {
			const u32 cdef_size4 = constants::num_4x4_blocks_wide[std::to_underlying(block_size::b64x64)];
			sb.cdef_idx(r, c + cdef_size4) = -1;
			sb.cdef_idx(r + cdef_size4, c) = -1;
			sb.cdef_idx(r + cdef_size4, c + cdef_size4) = -1;
		}
	}

	/// 5.11.57. Read loop restoration syntax
	/// \p count_units_in_frame()
	[[nodiscard]] constexpr u32 count_units_in_frame(u32 unit_size, u32 frame_size) {
		return std::max((frame_size + (unit_size >> 1)) / unit_size, 1u);
	}

	/// 6.10.2. Decode tile semantics
	/// \p clear_left_context().
	inline void clear_left_context(state::block &sb) {
		for (u32 plane = 0; plane < 3; ++plane) {
			std::ranges::fill(sb.left_level_context[plane], 0);
			std::ranges::fill(sb.left_dc_context[plane], 0);
		}
		std::ranges::fill(sb.left_seg_pred_context, false);
	}
	/// 6.10.2. Decode tile semantics
	/// \p clear_above_context().
	inline void clear_above_context(state::block &sb) {
		for (u32 plane = 0; plane < 3; ++plane) {
			std::ranges::fill(sb.above_level_context[plane], 0);
			std::ranges::fill(sb.above_dc_context[plane], 0);
		}
		std::ranges::fill(sb.above_seg_pred_context, false);
	}

	/// 7.11.2.8. Intra filter type process
	[[nodiscard]] bool get_filter_type(
		const obu::sequence_header&,
		const state::block&,
		const state::block_decoding&,
		u32 plane
	);

	/// 7.11.2.8. Intra filter type process
	/// \p is_smooth().
	[[nodiscard]] inline bool is_smooth(const state::block &sb, u32 row, u32 col, u32 plane) {
		prediction_mode mode;
		if (plane == 0) {
			mode = sb.y_modes(row, col);
		} else {
			if (sb.ref_frames[0](row, col) > reference_frame::intra) {
				return false;
			}
			mode = sb.uv_modes(row, col);
		}
		return
			mode == prediction_mode::smooth   ||
			mode == prediction_mode::smooth_v ||
			mode == prediction_mode::smooth_h;
	}

	/// 7.12.2. Dequantization functions
	/// \p get_qindex().
	[[nodiscard]] inline u8 get_qindex(
		const obu::uncompressed_header &header,
		const state::block &sb,
		bool ignore_delta_q, segment_id_t segment_id
	) {
		if (header.segmentation_params.is_feature_active(segment_id, features::alt_q)) {
			const i16 data = header.segmentation_params.feature_data
				[std::to_underlying(segment_id)][std::to_underlying(features::alt_q)];
			i32 qindex = header.quantization_params.base_q_idx + data;
			if (!ignore_delta_q && header.delta_q_params.delta_q_present) {
				qindex = sb.current_q_index + data;
			}
			return static_cast<u8>(std::clamp(qindex, 0, 255));
		}
		if (!ignore_delta_q && header.delta_q_params.delta_q_present) {
			return sb.current_q_index;
		}
		return header.quantization_params.base_q_idx;
	}

	/// 7.13.2.1. Butterfly functions
	/// \p brev().
	[[nodiscard]] constexpr u32 brev(u32 num_bits, u32 x) {
		// TODO better impl
		u32 t = 0;
		for (u32 i = 0; i < num_bits; ++i) {
			const u32 bit = (x >> i) & 1;
			t += bit << (num_bits - 1 - i);
		}
		return t;
	}

	/// 7.13.2.1. Butterfly functions
	/// \p cos128().
	[[nodiscard]] constexpr i32 cos128(u32 angle) {
		// TODO better impl
		const u32 angle2 = angle & 255u;
		if (angle2 <= 64) {
			return constants::cos128_lookup[angle2];
		} else if (angle2 <= 128) {
			return -constants::cos128_lookup[128 - angle2];
		} else if (angle2 <= 192) {
			return -constants::cos128_lookup[angle2 - 128];
		}
		return constants::cos128_lookup[256 - angle2];
	}
	/// 7.13.2.1. Butterfly functions
	/// \p sin128().
	[[nodiscard]] constexpr i32 sin128(u32 angle) {
		return cos128(angle - 64);
	}

	/// Palette color context returned by \p get_palette_color_context().
	struct palette_color_context {
		/// No initialization.
		palette_color_context(uninitialized_t) {
		}

		u32 color_order[constants::palette_colors]; ///< \p ColorOrder.
		u32 hash; ///< \p ColorContextHash.

		/// 5.11.50. Palette color context function
		[[nodiscard]] static palette_color_context get(
			const state::color_map::channel &color_map, u32 r, u32 c, u32 n
		) {
			palette_color_context result = uninitialized;

			u32 scores[constants::palette_colors] = { 0 };
			for (u32 i = 0; i < constants::palette_colors; ++i) {
				result.color_order[i] = i;
			}
			u32 neighbor;
			if (c > 0) {
				neighbor = color_map[r][c - 1];
				scores[neighbor] += 2;
			}
			if (r > 0 && c > 0) {
				neighbor = color_map[r - 1][c - 1];
				scores[neighbor] += 1;
			}
			if (r > 0) {
				neighbor = color_map[r - 1][c];
				scores[neighbor] += 2;
			}
			for (u32 i = 0; i < constants::palette_num_neighbors; ++i) {
				u32 max_score = scores[i];
				u32 max_idx = i;
				for (u32 j = i + 1; j < n; ++j) {
					if (scores[j] > max_score) {
						max_score = scores[j];
						max_idx = j;
					}
				}
				if (max_idx != i) {
					max_score = scores[max_idx];
					const u32 max_color_order = result.color_order[max_idx];
					for (u32 k = max_idx; k > i; --k) {
						scores[k] = scores[k - 1];
						result.color_order[k] = result.color_order[k - 1];
					}
					scores[i] = max_score;
					result.color_order[i] = max_color_order;
				}
			}
			result.hash = 0;
			for (u32 i = 0; i < constants::palette_num_neighbors; ++i) {
				result.hash += scores[i] * constants::palette_color_hash_multipliers[i];
			}

			return result;
		}
	};

	// 365
	/// \p get_above_tx_width().
	[[nodiscard]] constexpr u32 get_above_tx_width(
		const state::block &sb, const state::block_decoding &sbd, u32 row, u32 col
	) {
		if (row == sbd.row) {
			if (!sbd.avail_u) {
				return 64;
			} else if (sb.skips(row - 1, col) && sb.is_inters(row - 1, col)) {
				return constants::block_width(sb.sizes(row - 1, col));
			}
		}
		return constants::get_tx_width(sb.inter_tx_sizes(row - 1, col));
	}

	/// \p get_left_tx_height().
	[[nodiscard]] constexpr u32 get_left_tx_height(
		const state::block &sb, const state::block_decoding &sbd, u32 row, u32 col
	) {
		if (col == sbd.col) {
			if (!sbd.avail_l) {
				return 64;
			} else if (sb.skips(row, col - 1) && sb.is_inters(row, col - 1)) {
				return constants::block_height(sb.sizes(row, col - 1));
			}
		}
		return constants::get_tx_height(sb.inter_tx_sizes(row, col - 1));
	}

	// 373
	/// \p get_coeff_base_ctx().
	[[nodiscard]] u32 get_coeff_base_ctx(
		const obu::sequence_header&,
		const obu::uncompressed_header&,
		const obu::mode_info&,
		const state::block&,
		const state::block_decoding&,
		const state::coeffs&,
		tx_size tx_sz, u32 plane, u32 block_x, u32 block_y, u32 pos, u32 c, bool is_eob
	);

	// 374
	/// \p get_tx_class().
	[[nodiscard]] constexpr tx_class get_tx_class(inverse_transform tx_type) {
		if (
			tx_type == inverse_transform::v_dct  ||
			tx_type == inverse_transform::v_adst ||
			tx_type == inverse_transform::v_flipadst
		) {
			return tx_class::vertical;
		} else if (
			tx_type == inverse_transform::h_dct  ||
			tx_type == inverse_transform::h_adst ||
			tx_type == inverse_transform::h_flipadst
		) {
			return tx_class::horizontal;
		} else {
			return tx_class::both_2d;
		}
	}
}
