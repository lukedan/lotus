#pragma once

/// \file
/// Context information for selecting CDF tables.

#include "lotus/common.h"

#include "lotus/av1/common.h"
#include "lotus/av1/state.h"
#include "lotus/av1/obu.h"
#include "lotus/av1/functions.h"

namespace lotus::av1::cdf::context {
	/// Used to compute CDF for partition decoding info.
	struct partition {
		/// Zero initialization.
		partition(zero_t) {
		}

		/// Initializes this struct.
		[[nodiscard]] static partition compute(
			const state::block &mi, block_size b_size, bool avail_u, bool avail_l, u32 r, u32 c
		) {
			partition result = zero;
			result.b_size = b_size;
			result.bsl = constants::mi_width_log2[std::to_underlying(b_size)];
			const bool above =
				avail_u && constants::mi_width_log2 [std::to_underlying(mi.sizes(r - 1, c))] < result.bsl;
			const bool left =
				avail_l && constants::mi_height_log2[std::to_underlying(mi.sizes(r, c - 1))] < result.bsl;
			result.ctx = (left ? 1 : 0) * 2 + (above ? 1 : 0);
			return result;
		}

		block_size b_size = zero; ///< \p bSize.
		u32 bsl = 0; ///< \p bsl.
		u32 ctx = 0; ///< \p ctx.
	};

	// 364
	/// Compute \p ctx for \p tx_depth.
	[[nodiscard]] inline u32 compute_tx_depth(
		const state::block &sb, const state::block_decoding &sbd, tx_size max_rect_tx_size
	) {
		const u32 max_tx_width = constants::get_tx_width(max_rect_tx_size);
		const u32 max_tx_height = constants::get_tx_height(max_rect_tx_size);

		u32 above_w;
		if (sbd.avail_u && sb.is_inters(sbd.row - 1, sbd.col)) {
			above_w = constants::block_width(sb.sizes(sbd.row - 1, sbd.col));
		} else if (sbd.avail_u) {
			above_w = functions::get_above_tx_width(sb, sbd, sbd.row, sbd.col);
		} else {
			above_w = 0;
		}

		u32 left_h;
		if (sbd.avail_l && sb.is_inters(sbd.row, sbd.col - 1)) {
			left_h = constants::block_height(sb.sizes(sbd.row, sbd.col - 1));
		} else if (sbd.avail_l) {
			left_h = functions::get_left_tx_height(sb, sbd, sbd.row, sbd.col);
		} else {
			left_h = 0;
		}

		return (above_w >= max_tx_width ? 1 : 0) + (left_h >= max_tx_height ? 1 : 0);
	}

	/// Compute \p ctx for \p TileTxfmSplitCdf.
	[[nodiscard]] inline u32 compute_txfm_split(
		const state::block &sb, const state::block_decoding &sbd, u32 row, u32 col, tx_size tx_sz
	) {
		const bool above = functions::get_above_tx_width(sb, sbd, row, col) < constants::get_tx_width(tx_sz);
		const bool left = functions::get_left_tx_height(sb, sbd, row, col) < constants::get_tx_height(tx_sz);
		const u32 size =
			std::min(64u, std::max(constants::block_width(sbd.size), constants::block_height(sbd.size)));
		const tx_size max_tx_sz = functions::find_tx_size(size, size);
		const tx_size tx_sz_sqr_up = constants::get_tx_size_sqr_up(tx_sz);
		return
			(tx_sz_sqr_up != max_tx_sz ? 1 : 0) * 3 +
			(constants::tx_sizes - 1 - std::to_underlying(max_tx_sz)) * 6 +
			(above ? 1 : 0) +
			(left ? 1 : 0);
	}

	// 365
	/// Compute \p ctx for \p TileSegmentIdCdf.
	[[nodiscard]] inline u32 compute_segment_id(
		std::optional<av1::segment_id_t> prev_ul,
		std::optional<av1::segment_id_t> prev_u,
		std::optional<av1::segment_id_t> prev_l
	) {
		if (!prev_ul) {
			return 0;
		} else if (prev_ul == prev_u && prev_ul == prev_l) {
			return 2;
		} else if (prev_ul == prev_u || prev_ul == prev_l || prev_u == prev_l) {
			return 1;
		} else {
			return 0;
		}
	}

	// 366
	/// Compute \p ctx for \p TileIsInterCdf.
	[[nodiscard]] inline u32 compute_is_inter(
		const obu::mode_info &mode_info, const state::block_decoding &sbd
	) {
		if (sbd.avail_u && sbd.avail_l) {
			if (mode_info.left_intra && mode_info.above_intra) {
				return 3;
			}
			return mode_info.left_intra || mode_info.above_intra ? 1 : 0;
		} else if (sbd.avail_u || sbd.avail_l) {
			return 2 * ((sbd.avail_u ? mode_info.above_intra : mode_info.left_intra) ? 1 : 0);
		} else {
			return 0;
		}
	}

	// 367
	/// Compute \p ctx for \p TileSkipModeCdf.
	[[nodiscard]] inline u32 compute_skip_mode(const state::block &sb, const state::block_decoding &sbd) {
		u32 ctx = 0;
		if (sbd.avail_u) {
			ctx += sb.skip_modes(sbd.row - 1, sbd.col);
		}
		if (sbd.avail_l) {
			ctx += sb.skip_modes(sbd.row, sbd.col - 1);
		}
		return ctx;
	}

	/// Compute \p ctx for \p TileSkipCdf.
	[[nodiscard]] inline u32 compute_skip(const state::block &sb, const state::block_decoding &bs) {
		u32 ctx = 0;
		if (bs.avail_u) {
			ctx += sb.skips(bs.row - 1, bs.col) ? 1 : 0;
		}
		if (bs.avail_l) {
			ctx += sb.skips(bs.row, bs.col - 1) ? 1 : 0;
		}
		return ctx;
	}

	// 370
	/// Compute \p ctx for \p TileInterpFilterCdf.
	[[nodiscard]] inline u32 compute_interp_filter(
		const obu::mode_info &mode_info,
		const state::block &sb,
		const state::block_decoding &sbd,
		u32 dir
	) {
		u32 ctx = ((dir & 1u) * 2 + (mode_info.ref_frame[1] > reference_frame::intra ? 1 : 0)) * 4;
		auto left_type = interpolation::bilinear;
		auto above_type = interpolation::bilinear;

		if (sbd.avail_l) {
			if (
				sb.ref_frames[0](sbd.row, sbd.col - 1) == mode_info.ref_frame[0] ||
				sb.ref_frames[1](sbd.row, sbd.col - 1) == mode_info.ref_frame[0]
			) {
				left_type = sb.interp_filters[sbd.row][sbd.col - 1][dir];
			}
		}

		if (sbd.avail_u) {
			if (
				sb.ref_frames[0](sbd.row - 1, sbd.col) == mode_info.ref_frame[0] ||
				sb.ref_frames[1](sbd.row - 1, sbd.col) == mode_info.ref_frame[0]
			) {
				above_type = sb.interp_filters[sbd.row - 1][sbd.col][dir];
			}
		}

		if (left_type == above_type) {
			ctx += std::to_underlying(left_type);
		} else if (left_type == interpolation::bilinear) {
			ctx += std::to_underlying(above_type);
		} else if (above_type == interpolation::bilinear) {
			ctx += std::to_underlying(left_type);
		} else {
			ctx += 3;
		}

		return ctx;
	}

	// 371
	/// Compute \p ctx for \p all_zero.
	[[nodiscard]] inline u32 compute_all_zero(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const state::block &sb,
		const state::block_decoding &sbd,
		u32 plane, tx_size tx_sz, u32 x4, u32 y4, u32 w4, u32 h4
	) {
		u32 max_x4 = header.frame_size.get_mi_cols();
		u32 max_y4 = header.frame_size.get_mi_rows();
		if (plane > 0) {
			max_x4 = max_x4 >> (seq_header.color_config.subsampling_x ? 1 : 0);
			max_y4 = max_y4 >> (seq_header.color_config.subsampling_y ? 1 : 0);
		}

		const u32 w = constants::get_tx_width(tx_sz);
		const u32 h = constants::get_tx_height(tx_sz);

		const block_size bsize = functions::get_plane_residual_size(seq_header.color_config, sbd.size, plane);
		const u32 bw = constants::block_width(bsize);
		const u32 bh = constants::block_height(bsize);

		if (plane == 0) {
			u8 top = 0;
			u8 left = 0;
			for (u32 k = 0; k < w4; ++k) {
				if (x4 + k < max_x4) {
					top = std::max(top, sb.above_level_context[plane][x4 + k]);
				}
			}
			for (u32 k = 0; k < h4; ++k) {
				if (y4 + k < max_y4) {
					left = std::max(left, sb.left_level_context[plane][y4 + k]);
				}
			}
			/*top = std::min(top, 255u);
			left = std::min(left, 255u);*/ // unnecessary?
			if (bw == w && bh == h) {
				return 0;
			} else if (top == 0 && left == 0) {
				return 1;
			} else if (top == 0 || left == 0) {
				return 2 + (std::max(top, left) > 3 ? 1 : 0);
			} else if (std::max(top, left) <= 3) {
				return 4;
			} else if (std::min(top, left) <= 3) {
				return 5;
			} else {
				return 6;
			}
		} else {
			u8 above = 0;
			u8 left = 0;
			for (u32 i = 0; i < w4; ++i) {
				if (x4 + i < max_x4) {
					above |= sb.above_level_context[plane][x4 + i];
					above |= sb.above_dc_context[plane][x4 + i];
				}
			}
			for (u32 i = 0; i < h4; ++i) {
				if (y4 + i < max_y4) {
					left |= sb.left_level_context[plane][y4 + i];
					left |= sb.left_dc_context[plane][y4 + i];
				}
			}
			u32 ctx = (above != 0 ? 1 : 0) + (left != 0 ? 1 : 0);
			ctx += 7;
			if (bw * bh > w * h) {
				ctx += 3;
			}
			return ctx;
		}
	}

	// 372
	/// Compute \p ctx for \p eob_pt_16, \p eob_pt_32, \p eob_pt_64, \p eob_pt_128, and \p eob_pt_256.
	[[nodiscard]] inline u32 compute_eob_pt(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		const state::block &sb,
		const state::block_decoding &sbd,
		u32 plane,tx_size tx_sz, u32 x4, u32 y4
	) {
		// TODO is this plane_tx_type?
		const inverse_transform tx_type =
			functions::compute_tx_type(seq_header, header, mode_info, sb, sbd, plane, tx_sz, x4, y4);
		return functions::get_tx_class(tx_type) == tx_class::both_2d ? 0 : 1;
	}

	// 378
	/// Compute \p ctx for \p dc_sign.
	[[nodiscard]] inline u32 compute_dc_sign(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const state::block &sb,
		u32 plane, u32 x4, u32 y4, u32 w4, u32 h4
	) {
		u32 max_x4 = header.frame_size.get_mi_cols();
		u32 max_y4 = header.frame_size.get_mi_rows();
		if (plane > 0) {
			max_x4 = max_x4 >> (seq_header.color_config.subsampling_x ? 1 : 0);
			max_y4 = max_y4 >> (seq_header.color_config.subsampling_y ? 1 : 0);
		}

		i32 dc_sign = 0;
		for (u32 k = 0; k < w4; ++k) {
			if (x4 + k < max_x4) {
				const u8 sign = sb.above_dc_context[plane][x4 + k];
				if (sign == 1) {
					--dc_sign;
				} else if (sign == 2) {
					++dc_sign;
				}
			}
		}
		for (u32 k = 0; k < h4; ++k) {
			if (y4 + k < max_y4) {
				const u8 sign = sb.left_dc_context[plane][y4 + k];
				if (sign == 1) {
					--dc_sign;
				} else if (sign == 2) {
					++dc_sign;
				}
			}
		}
		if (dc_sign < 0) {
			return 1;
		} else if (dc_sign > 0) {
			return 2;
		} else {
			return 0;
		}
	}

	// 379
	/// Compute \p ctx for \p coeff_br.
	[[nodiscard]] inline u32 compute_coeff_br(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		const state::block &sb,
		const state::block_decoding &sbd,
		const state::coeffs &sc,
		tx_size tx_sz, u32 plane, u32 x4, u32 y4, u32 pos
	) {
		const tx_size adj_tx_sz = constants::adjusted_tx_size[std::to_underlying(tx_sz)];
		const u32 bwl = constants::get_tx_width_log2(adj_tx_sz);
		const u32 txw = constants::get_tx_width(adj_tx_sz);
		const u32 txh = constants::get_tx_height(adj_tx_sz);
		const u32 row = pos >> bwl;
		const u32 col = pos - (row << bwl);

		u32 mag = 0;

		const inverse_transform tx_type =
			functions::compute_tx_type(seq_header, header, mode_info, sb, sbd, plane, tx_sz, x4, y4);
		const tx_class tx_class = functions::get_tx_class(tx_type);

		for (u32 idx = 0; idx < 3; ++idx) {
			const u32 ref_row = row + constants::mag_ref_offset_with_tx_class[std::to_underlying(tx_class)][idx][0];
			const u32 ref_col = col + constants::mag_ref_offset_with_tx_class[std::to_underlying(tx_class)][idx][1];
			// TODO can the values be negative?
			if (/*ref_row >= 0 && ref_col >= 0 &&*/ ref_row < txh && ref_col < (1u << bwl)) {
				mag += std::min(
					static_cast<u32>(sc.quant[ref_row * txw + ref_col]),
					constants::coeff_base_range + constants::num_base_levels + 1
				);
			}
		}

		mag = std::min((mag + 1) >> 1, 6u);
		if (pos == 0) {
			return mag;
		} else if (std::to_underlying(tx_class) == 0) {
			if (row < 2 && col < 2) {
				return mag + 7;
			} else {
				return mag + 14;
			}
		} else {
			if (std::to_underlying(tx_class) == 1) {
				if (col == 0) {
					return mag + 7;
				} else {
					return mag + 14;
				}
			} else {
				if (row == 0) {
					return mag + 7;
				} else {
					return mag + 14;
				}
			}
		}
	}

	// 380
	/// Compute \p ctx for \p has_palette_y.
	[[nodiscard]] inline u32 compute_has_palette_y(const state::block &sb, const state::block_decoding &sbd) {
		u32 ctx = 0;
		if (sbd.avail_u && sb.palette_sizes[0](sbd.row - 1, sbd.col) > 0) {
			++ctx;
		}
		if (sbd.avail_l && sb.palette_sizes[0](sbd.row, sbd.col - 1) > 0) {
			++ctx;
		}
		return ctx;
	}

	// 381
	/// Compute \p intraDir for \p intra_tx_type.
	[[nodiscard]] inline prediction_mode compute_intra_tx_type(const obu::mode_info &mode_info) {
		if (mode_info.use_filter_intra) {
			return constants::filter_intra_mode_to_intra_dir[std::to_underlying(mode_info.filter_intra_mode)];
		} else {
			return mode_info.y_mode;
		}
	}
}
