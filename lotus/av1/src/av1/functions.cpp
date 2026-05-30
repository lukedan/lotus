#include "lotus/av1/functions.h"

/// \file
/// Implementation of AV1 functions.

namespace lotus::av1::functions {
	[[nodiscard]] tx_size get_tx_size(
		const obu::color_config &color_config, const state::block_decoding &sbd, u32 plane, tx_size tx_sz
	) {
		if (plane == 0) {
			return tx_sz;
		}
		const tx_size uv_tx = constants::max_tx_size_rect[
			std::to_underlying(get_plane_residual_size(color_config, sbd.size, plane))
		];
		const u32 uv_tx_width = constants::get_tx_width(uv_tx);
		const u32 uv_tx_height = constants::get_tx_height(uv_tx);
		if (uv_tx_width == 64 || uv_tx_height == 64) {
			if (uv_tx_width == 16) {
				return tx_size::size_16x32;
			}
			if (uv_tx_height == 16) {
				return tx_size::size_32x16;
			}
			return tx_size::size_32x32;
		}
		return uv_tx;
	}

	inverse_transform compute_tx_type(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		const state::block &sb,
		const state::block_decoding &sbd,
		u32 plane, tx_size tx_sz, u32 block_x, u32 block_y
	) {
		const tx_size tx_sz_sqr_up = constants::get_tx_size_sqr_up(tx_sz);

		if (mode_info.lossless || tx_sz_sqr_up > tx_size::size_32x32) {
			return inverse_transform::dct_dct;
		}

		const tx_set tx_set = get_tx_set(header, mode_info, tx_sz);

		if (plane == 0) {
			return sb.tx_types(block_y, block_x);
		}

		if (mode_info.is_inter) {
			const u32 x4 = std::max(sbd.col, block_x << (seq_header.color_config.subsampling_x ? 1 : 0));
			const u32 y4 = std::max(sbd.row, block_y << (seq_header.color_config.subsampling_y ? 1 : 0));
			const inverse_transform tx_type = sb.tx_types(y4, x4);
			if (!is_tx_type_in_set(mode_info, tx_set, tx_type)) {
				return inverse_transform::dct_dct;
			}
			return tx_type;
		}

		const inverse_transform tx_type = constants::mode_to_txfm[std::to_underlying(mode_info.uv_mode)];
		if (!is_tx_type_in_set(mode_info, tx_set, tx_type)) {
			return inverse_transform::dct_dct;
		}
		return tx_type;
	}

	bool get_filter_type(
		const obu::sequence_header &seq_header,
		const state::block &sb,
		const state::block_decoding &sbd,
		u32 plane
	) {
		bool above_smooth = false;
		bool left_smooth = false;
		if (plane == 0 ? sbd.avail_u : sbd.avail_u_chroma) {
			u32 r = sbd.row - 1;
			u32 c = sbd.col;
			if (plane > 0) {
				if (seq_header.color_config.subsampling_x && !(sbd.col & 1)) {
					++c;
				}
				if (seq_header.color_config.subsampling_y && (sbd.row & 1)) {
					--r;
				}
			}
			above_smooth = is_smooth(sb, r, c, plane);
		}
		if (plane == 0 ? sbd.avail_l : sbd.avail_l_chroma) {
			u32 r = sbd.row;
			u32 c = sbd.col - 1;
			if (plane > 0) {
				if (seq_header.color_config.subsampling_x && (sbd.col & 1)) {
					--c;
				}
				if (seq_header.color_config.subsampling_y && !(sbd.row & 1)) {
					++r;
				}
			}
			left_smooth = is_smooth(sb, r, c, plane);
		}
		return above_smooth || left_smooth;
	}

	u32 get_coeff_base_ctx(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		const state::block &sb,
		const state::block_decoding &sbd,
		const state::coeffs &sc,
		tx_size tx_sz, u32 plane, u32 block_x, u32 block_y, u32 pos, u32 c, bool is_eob
	) {
		const tx_size adj_tx_sz = constants::adjusted_tx_size[std::to_underlying(tx_sz)];
		const u32 bwl = constants::get_tx_width_log2(adj_tx_sz);
		const u32 width = 1u << bwl;
		const u32 height = constants::get_tx_height(adj_tx_sz);
		// TODO same as plane_tx_type?
		const inverse_transform tx_type =
			compute_tx_type(seq_header, header, mode_info, sb, sbd, plane, tx_sz, block_x, block_y);
		if (is_eob) {
			if (c == 0) {
				return constants::sig_coef_contexts - 4;
			}
			if (c <= (height << bwl) / 8) {
				return constants::sig_coef_contexts - 3;
			}
			if (c <= (height << bwl) / 4) {
				return constants::sig_coef_contexts - 2;
			}
			return constants::sig_coef_contexts - 1;
		}
		const tx_class tx_class = get_tx_class(tx_type);
		const u32 row = pos >> bwl;
		const u32 col = pos - (row << bwl);
		u32 mag = 0;

		for (u32 idx = 0; idx < constants::sig_ref_diff_offset_num; ++idx) {
			const u32 ref_row = row + constants::sig_ref_diff_offset[std::to_underlying(tx_class)][idx][0];
			const u32 ref_col = col + constants::sig_ref_diff_offset[std::to_underlying(tx_class)][idx][1];
			// TODO can the coordinates be less than 0?
			if (/*ref_row >= 0 && ref_col >= 0 &&*/ ref_row < height && ref_col < width) {
				// TODO signed?
				mag += std::min(static_cast<u32>(std::abs(sc.quant[(ref_row << bwl) + ref_col])), 3u);
			}
		}

		u32 ctx = std::min((mag + 1) >> 1, 4u);
		if (tx_class == tx_class::both_2d) {
			if (row == 0 && col == 0) {
				return 0;
			}
			return
				ctx +
				constants::coeff_base_ctx_offset[std::to_underlying(tx_sz)][std::min(row, 4u)][std::min(col, 4u)];
		}
		const u32 idx = tx_class == tx_class::vertical ? row : col;
		return ctx + constants::coeff_base_pos_ctx_offset[std::min(idx, 2u)];
	}
}
