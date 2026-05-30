#include "lotus/av1/block_decoding.h"

/// \file
/// Implementation of block decoding routines.

#include "lotus/av1/functions.h"
#include "lotus/av1/quantizer_matrix.h"

namespace lotus::av1::block_decoding {
	void compute_prediction(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		state::block &sb,
		const state::block_decoding &sbd
	) {
		const u32 sb_mask = seq_header.use_128x128_superblock ? 31 : 15;
		const u32 sub_block_mi_row = sbd.row & sb_mask;
		const u32 sub_block_mi_col = sbd.col & sb_mask;
		for (u32 plane = 0; plane < 1 + (sbd.has_chroma ? 1 : 0) * 2; ++plane) {
			const block_size plane_sz = functions::get_plane_residual_size(seq_header.color_config, sbd.size, plane);
			const u32 num_4x4_w = constants::num_4x4_blocks_wide[std::to_underlying(plane_sz)];
			const u32 num_4x4_h = constants::num_4x4_blocks_high[std::to_underlying(plane_sz)];
			const u32 log2w = constants::mi_size_log2 + constants::mi_width_log2[std::to_underlying(plane_sz)];
			const u32 log2h = constants::mi_size_log2 + constants::mi_height_log2[std::to_underlying(plane_sz)];
			const u32 sub_x = plane > 0 ? (seq_header.color_config.subsampling_x ? 1 : 0) : 0;
			const u32 sub_y = plane > 0 ? (seq_header.color_config.subsampling_y ? 1 : 0) : 0;
			const u32 base_x = (sbd.col >> sub_x) * constants::mi_size;
			const u32 base_y = (sbd.row >> sub_y) * constants::mi_size;
			const u32 cand_row = (sbd.row >> sub_y) << sub_y;
			const u32 cand_col = (sbd.col >> sub_x) << sub_x;

			const bool is_inter_intra = mode_info.is_inter && mode_info.ref_frame[1] == reference_frame::intra;
			if (is_inter_intra) {
				prediction_mode mode;
				if (mode_info.interintra_mode == intra_prediction::dc) {
					mode = prediction_mode::dc;
				} else if (mode_info.interintra_mode == intra_prediction::v) {
					mode = prediction_mode::v;
				} else if (mode_info.interintra_mode == intra_prediction::h) {
					mode = prediction_mode::h;
				} else {
					mode = prediction_mode::smooth;
				}
				predict_intra(
					seq_header,
					header,
					mode_info,
					sb,
					sbd,
					plane,
					base_x,
					base_y,
					plane == 0 ? sbd.avail_l : sbd.avail_l_chroma,
					plane == 0 ? sbd.avail_u : sbd.avail_u_chroma,
					sb.block_decoded(
						plane,
						static_cast<i32>(sub_block_mi_row >> sub_y) - 1,
						static_cast<i32>((sub_block_mi_col >> sub_x) + num_4x4_w)
					),
					sb.block_decoded(
						plane,
						static_cast<i32>((sub_block_mi_row >> sub_y) + num_4x4_h),
						static_cast<i32>(sub_block_mi_col >> sub_x) - 1
					),
					mode,
					log2w,
					log2h
				);
			}
			if (mode_info.is_inter) {
				// TODO
			}
		}
	}

	void decode_frame_wrapup(const obu::uncompressed_header &header, const state::block &sb) {
		if (!header.show_existing_frame) {
			std::abort(); // TODO
		} else {
			// TODO
		}
		// 1.
		// TODO reference frame update
		// 2.
	}

	void predict_intra(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		state::block &sb,
		const state::block_decoding &sbd,
		u32 plane, u32 x, u32 y,
		bool have_left, bool have_above, bool have_above_right, bool have_below_left,
		prediction_mode mode, u32 log2w, u32 log2h
	) {
		const u32 w = 1u << log2w;
		const u32 h = 1u << log2h;
		u32 max_x = header.frame_size.get_mi_cols() * constants::mi_size - 1;
		u32 max_y = header.frame_size.get_mi_rows() * constants::mi_size - 1;
		if (plane > 0) {
			const u32 subsampling_x = seq_header.color_config.subsampling_x ? 1 : 0;
			const u32 subsampling_y = seq_header.color_config.subsampling_y ? 1 : 0;
			max_x = ((header.frame_size.get_mi_cols() * constants::mi_size) >> subsampling_x) - 1;
			max_y = ((header.frame_size.get_mi_rows() * constants::mi_size) >> subsampling_y) - 1;
		}
		prediction p = uninitialized;
		for (u32 i = 0; i < w + h; ++i) {
			if (!have_above && have_left) {
				p.above_row(i) = sb.curr_frame[plane](y, x - 1);
			} else if (!have_above && !have_left) {
				p.above_row(i) = (1 << (seq_header.color_config.bit_depth - 1)) - 1;
			} else {
				const u32 above_limit = std::min(max_x, x + (have_above_right ? 2 * w : w) - 1);
				p.above_row(i) = sb.curr_frame[plane](y - 1, std::min(above_limit, x + i));
			}
		}
		for (u32 i = 0; i < w + h; ++i) {
			if (!have_left && have_above) {
				p.left_col(i) = sb.curr_frame[plane](y - 1, x);
			} else if (!have_left && !have_above) {
				p.left_col(i) = (1 << (seq_header.color_config.bit_depth - 1)) + 1;
			} else {
				const u32 left_limit = std::min(max_y, y + (have_below_left ? 2 * h : h) - 1);
				p.left_col(i) = sb.curr_frame[plane](std::min(left_limit, y + i), x - 1);
			}
		}
		if (have_above && have_left) {
			p.above_row(-1) = sb.curr_frame[plane](y - 1, x - 1);
		} else if (have_above) {
			p.above_row(-1) = sb.curr_frame[plane](y - 1, x);
		} else if (have_left) {
			p.above_row(-1) = sb.curr_frame[plane](y, x - 1);
		} else {
			p.above_row(-1) = 1 << (seq_header.color_config.bit_depth - 1);
		}
		p.left_col(-1) = p.above_row(-1);
		if (plane == 0 && mode_info.use_filter_intra) {
			p.predict_intra_recursive(seq_header, mode_info, w, h);
		} else if (functions::is_directional_mode(mode)) {
			p.predict_intra_directional(
				seq_header, mode_info, sb, sbd, plane, x, y, have_left, have_above, mode, w, h, max_x, max_y
			);
		} else {
			switch (mode) {
			case prediction_mode::smooth:   [[fallthrough]];
			case prediction_mode::smooth_v: [[fallthrough]];
			case prediction_mode::smooth_h:
				p.predict_intra_smooth(mode, log2w, log2h, w, h);
				break;
			case prediction_mode::dc:
				p.predict_intra_dc(seq_header, have_left, have_above, log2w, log2h, w, h);
				break;
			case prediction_mode::paeth:
				p.predict_intra_basic(w, h);
				break;
			default:
				crash_if(true);
				break;
			}
		}
		for (u32 i = 0; i < h; ++i) {
			for (u32 j = 0; j < w; ++j) {
				sb.curr_frame[plane](y + i, x + j) = p.pred[i][j];
			}
		}
	}

	void prediction::predict_intra_basic(u32 w, u32 h) {
		for (u32 i = 0; i < h; ++i) {
			for (u32 j = 0; j < w; ++j) {
				const i32 base = static_cast<i32>(above_row(j) + left_col(i)) - static_cast<i32>(above_row(-1));
				const i32 p_left = std::abs(base - static_cast<i32>(left_col(i)));
				const i32 p_top = std::abs(base - static_cast<i32>(above_row(j)));
				const i32 p_top_left = std::abs(base - static_cast<i32>(above_row(-1)));
				if (p_left <= p_top && p_left <= p_top_left) {
					pred[i][j] = static_cast<u16>(left_col(i));
				} else if (p_top <= p_top_left) {
					pred[i][j] = static_cast<u16>(above_row(j));
				} else {
					pred[i][j] = static_cast<u16>(above_row(-1));
				}
			}
		}
	}

	void prediction::predict_intra_recursive(
		const obu::sequence_header &seq_header,
		const obu::mode_info &mode_info,
		u32 w, u32 h
	) {
		const u32 w4 = w >> 2;
		const u32 h2 = h >> 1;
		for (u32 i2 = 0; i2 < h2; ++i2) {
			for (u32 j4 = 0; j4 < w4; ++j4) {
				std::array<u32, 7> p;
				for (u32 i = 0; i < 7; ++i) {
					if (i < 5) {
						if (i2 == 0) {
							p[i] = above_row((j4 << 2) + i - 1);
						} else if (j4 == 0 && i == 0) {
							p[i] = left_col((i2 << 1) - 1);
						} else {
							p[i] = pred[(i2 << 1) - 1][(j4 << 2) + i - 1];
						}
					} else {
						if (j4 == 0) {
							p[i] = left_col((i2 << 1) + i - 5);
						} else {
							p[i] = pred[(i2 << 1) + i - 5][(j4 << 2) - 1];
						}
					}
				}
				for (u32 i1 = 0; i1 < 2; ++i1) {
					for (u32 j1 = 0; j1 < 4; ++j1) {
						i32 pr = 0;
						for (u32 i = 0; i < 7; ++i) {
							const u32 filter_intra_mode = std::to_underlying(mode_info.filter_intra_mode);
							pr +=
								constants::intra_filter_taps[filter_intra_mode][(i1 << 2) + j1][i] *
								static_cast<i32>(p[i]);
						}
						pred[(i2 << 1) + i1][(j4 << 2) + j1] = functions::clip1(
							seq_header.color_config,
							functions::round2_signed(pr, constants::intra_filter_scale_bits)
						);
					}
				}
			}
		}
	}

	void prediction::predict_intra_directional(
		const obu::sequence_header &seq_header,
		const obu::mode_info &mode_info,
		const state::block &sb,
		const state::block_decoding &sbd,
		u32 plane, u32 x, u32 y,
		bool have_left, bool have_above, prediction_mode mode,
		u32 w, u32 h, u32 max_x, u32 max_y
	) {
		// 1.
		const i32 angle_delta = plane == 0 ? mode_info.angle_delta_y : mode_info.angle_delta_uv;
		// 2.
		const i32 p_angle =
			static_cast<i32>(constants::mode_to_angle[std::to_underlying(mode)]) +
			angle_delta * static_cast<i32>(constants::angle_step);
		// 3.
		i32 upsample_above = 0;
		i32 upsample_left = 0;
		// 4.
		if (seq_header.enable_intra_edge_filter) {
			const bool filter_type = functions::get_filter_type(seq_header, sb, sbd, plane);
			if (p_angle != 90 && p_angle != 180) {
				if (p_angle > 90 && p_angle < 180 && w + h >= 24) {
					left_col(-1) = above_row(-1) = filter_corner();
				}
				if (have_above) {
					const u32 strength = select_intra_edge_filter_strength(w, h, filter_type, p_angle - 90);
					const u32 num_px = std::min(w, max_x - x + 1) + (p_angle < 90 ? h : 0) + 1;
					intra_edge_filter(num_px, strength, false);
				}
				if (have_left) {
					const u32 strength = select_intra_edge_filter_strength(w, h, filter_type, p_angle - 180);
					const u32 num_px = std::min(h, max_y - y + 1) + (p_angle > 180 ? w : 0) + 1;
					intra_edge_filter(num_px, strength, true);
				}
			}
			upsample_above = select_intra_edge_upsample(w, h, filter_type, p_angle - 90) ? 1 : 0;
			if (upsample_above) {
				const u32 num_px = w + (p_angle < 90 ? h : 0);
				intra_edge_upsample(seq_header.color_config, num_px, false);
			}
			upsample_left = select_intra_edge_upsample(w, h, filter_type, p_angle - 180) ? 1 : 0;
			if (upsample_left) {
				const u32 num_px = h + (p_angle > 180 ? w : 0);
				intra_edge_upsample(seq_header.color_config, num_px, true);
			}
		}
		// 5.
		const u32 dx =
			p_angle < 90 ?
			constants::dr_intra_derivative[p_angle] :
			(p_angle > 90 && p_angle < 180 ? constants::dr_intra_derivative[180 - p_angle] : 0);
		// 6.
		const u32 dy =
			p_angle > 90 && p_angle < 180 ?
			constants::dr_intra_derivative[p_angle - 90] :
			(p_angle > 180 ? constants::dr_intra_derivative[270 - p_angle] : 0);
		// 7.
		if (p_angle < 90) {
			const u32 max_base_x = (w + h - 1) << upsample_above;
			for (u32 i = 0; i < h; ++i) {
				const u32 idx = (i + 1) * dx;
				const u32 shift = ((idx << upsample_above) >> 1) & 0x1F;
				for (u32 j = 0; j < w; ++j) {
					const u32 base = (idx >> (6 - upsample_above)) + (j << upsample_above);
					if (base < max_base_x) {
						pred[i][j] = static_cast<u16>(functions::round2(
							above_row(base) * (32 - shift) + above_row(base + 1) * shift, 5
						));
					} else {
						pred[i][j] = static_cast<u16>(above_row(max_base_x));
					}
				}
			}
		}
		// 8.
		if (p_angle > 90 && p_angle < 180) {
			for (u32 i = 0; i < h; ++i) {
				for (u32 j = 0; j < w; ++j) {
					i32 idx = static_cast<i32>(j << 6) - static_cast<i32>((i + 1) * dx);
					i32 base = idx >> (6 - upsample_above);
					if (base >= -(1 << upsample_above)) {
						const auto shift = static_cast<u32>(((idx << upsample_above) >> 1) & 0x1F);
						pred[i][j] = static_cast<u16>(functions::round2(
							above_row(base) * (32 - shift) + above_row(base + 1) * shift, 5
						));
					} else {
						idx = static_cast<i32>(i << 6) - static_cast<i32>((j + 1) * dy);
						base = idx >> (6 - upsample_left);
						const auto shift = static_cast<u32>(((idx << upsample_left) >> 1) & 0x1F);
						pred[i][j] = static_cast<u16>(functions::round2(
							left_col(base) * (32 - shift) + left_col(base + 1) * shift, 5
						));
					}
				}
			}
		}
		// 9.
		if (p_angle > 180) {
			for (u32 i = 0; i < h; ++i) {
				for (u32 j = 0; j < w; ++j) {
					const u32 idx = (j + 1) * dy;
					const u32 base = (idx >> (6 - upsample_left)) + (i << upsample_left);
					const u32 shift = ((idx << upsample_left) >> 1) & 0x1F;
					pred[i][j] = static_cast<u16>(functions::round2(
						left_col(base) * (32 - shift) + left_col(base + 1) * shift, 5
					));
				}
			}
		}
		// 10.
		if (p_angle == 90) {
			for (u32 i = 0; i < h; ++i) {
				for (u32 j = 0; j < w; ++j) {
					pred[i][j] = static_cast<u16>(above_row(j));
				}
			}
		}
		// 11.
		if (p_angle == 180) {
			for (u32 i = 0; i < h; ++i) {
				for (u32 j = 0; j < w; ++j) {
					pred[i][j] = static_cast<u16>(left_col(i));
				}
			}
		}
	}

	void prediction::predict_intra_dc(
		const obu::sequence_header &seq_header,
		bool have_left, bool have_above, u32 log2w, u32 log2h, u32 w, u32 h
	) {
		u32 avg;
		if (have_left && have_above) {
			u32 sum = 0;
			for (u32 k = 0; k < h; ++k) {
				sum += left_col(k);
			}
			for (u32 k = 0; k < w; ++k) {
				sum += above_row(k);
			}
			sum += (w + h) >> 1;
			avg = sum / (w + h);
		} else if (have_left) {
			u32 sum = 0;
			for (u32 k = 0; k < h; ++k) {
				sum += left_col(k);
			}
			avg = functions::clip1(seq_header.color_config, (sum + (h >> 1)) >> log2h);
		} else if (have_above) {
			u32 sum = 0;
			for (u32 k = 0; k < w; ++k) {
				sum += above_row(k);
			}
			avg = functions::clip1(seq_header.color_config, (sum + (w >> 1)) >> log2w);
		} else {
			avg = 1 << (seq_header.color_config.bit_depth - 1);
		}
		for (u32 i = 0; i < h; ++i) {
			for (u32 j = 0; j < w; ++j) {
				pred[i][j] = static_cast<u16>(avg);
			}
		}
	}

	void prediction::predict_intra_smooth(prediction_mode mode, u32 log2w, u32 log2h, u32 w, u32 h) {
		const auto get_weights_lut = [](u32 log2s) -> std::span<const u8> {
			switch (log2s) {
			case 2: return constants::sm_weights_tx_4x4;
			case 3: return constants::sm_weights_tx_8x8;
			case 4: return constants::sm_weights_tx_16x16;
			case 5: return constants::sm_weights_tx_32x32;
			case 6: return constants::sm_weights_tx_64x64;
			default: return {};
			}
		};

		const std::span<const u8> sm_weights_x = get_weights_lut(log2w);
		const std::span<const u8> sm_weights_y = get_weights_lut(log2h);
		if (mode == prediction_mode::smooth) {
			for (u32 i = 0; i < h; ++i) {
				for (u32 j = 0; j < w; ++j) {
					const u32 smooth_pred =
						sm_weights_y[i] * above_row(j) + (256 - sm_weights_y[i]) * left_col(h - 1) +
						sm_weights_x[j] * left_col(i)  + (256 - sm_weights_x[j]) * above_row(w - 1);
					pred[i][j] = static_cast<u16>(functions::round2(smooth_pred, 9));
				}
			}
		} else if (mode == prediction_mode::smooth_v) {
			for (u32 i = 0; i < h; ++i) {
				for (u32 j = 0; j < w; ++j) {
					const u32 smooth_pred =
						sm_weights_y[i] * above_row(j) + (256 - sm_weights_y[i]) * left_col(h - 1);
					pred[i][j] = static_cast<u16>(functions::round2(smooth_pred, 8));
				}
			}
		} else {
			crash_if(mode != prediction_mode::smooth_h);
			for (u32 i = 0; i < h; ++i) {
				for (u32 j = 0; j < w; ++j) {
					const u32 smooth_pred =
						sm_weights_x[j] * left_col(i) + (256 - sm_weights_x[j]) * above_row(w - 1);
					pred[i][j] = static_cast<u16>(functions::round2(smooth_pred, 8));
				}
			}
		}
	}

	void prediction::intra_edge_upsample(const obu::color_config &color_config, u32 num_px, bool dir) {
		u32 *buf = dir ? &left_col(0) : &above_row(0);

		// create array dup
		std::array<u32, 67> dup;
		dup[0] = buf[-1];
		for (i32 i = -1; i < static_cast<i32>(num_px); ++i) {
			dup[static_cast<usize>(i + 2)] = buf[i];
		}
		dup[num_px + 2] = buf[num_px - 1];

		// upsample
		buf[-2] = dup[0];
		for (u32 i = 0; i < num_px; ++i) {
			i32 s =
				-static_cast<i32>(dup[i]) +
				9 * static_cast<i32>(dup[i + 1]) +
				9 * static_cast<i32>(dup[i + 2]) -
				static_cast<i32>(dup[i + 3]);
			s = functions::clip1(color_config, functions::round2(s, 4));
			buf[static_cast<i32>(2 * i) - 1] = static_cast<u32>(s);
			buf[2 * i] = dup[i + 2];
		}
	}

	void prediction::intra_edge_filter(u32 sz, u32 strength, bool left) {
		if (strength == 0) {
			return;
		}
		std::array<u32, 129> edge;
		for (i32 i = 0; i < static_cast<i32>(sz); ++i) {
			edge[static_cast<usize>(i)] = left ? left_col(i - 1) : above_row(i - 1);
		}
		for (i32 i = 1; i < static_cast<i32>(sz); ++i) {
			u32 s = 0;
			for (u32 j = 0; j < constants::intra_edge_taps; ++j) {
				const i32 k = std::clamp(i - 2 + static_cast<i32>(j), 0, static_cast<i32>(sz) - 1);
				s += constants::intra_edge_kernel[strength - 1][j] * edge[static_cast<usize>(k)];
			}
			(left ? left_col(i - 1) : above_row(i - 1)) = (s + 8) >> 4;
		}
	}

	void predict_palette(
		const obu::mode_info &mode_info,
		state::block &sb,
		const state::color_map &scm,
		u32 plane, u32 start_x, u32 start_y, u32 x, u32 y, tx_size tx_sz
	) {
		const u32 w = constants::get_tx_width(tx_sz);
		const u32 h = constants::get_tx_height(tx_sz);
		std::span<const channel_t> palette;
		if (plane == 0) {
			palette = mode_info.palette_colors_y;
		} else if (plane == 1) {
			palette = mode_info.palette_colors_u;
		} else {
			crash_if(plane != 2);
			palette = mode_info.palette_colors_v;
		}
		const state::color_map::channel &map = plane == 0 ? scm.y : scm.uv;
		for (u32 i = 0; i < h; ++i) {
			for (u32 j = 0; j < w; ++j) {
				sb.curr_frame[plane](start_y + i, start_x + j) = palette[map[y * 4 + i][x * 4 + j]];
			}
		}
	}

	void predict_chroma_from_luma(
		const obu::sequence_header &seq_header,
		const obu::mode_info &mode_info,
		state::block &sb,
		u32 plane, u32 start_x, u32 start_y, tx_size tx_sz
	) {
		const u32 w = constants::get_tx_width(tx_sz);
		const u32 h = constants::get_tx_height(tx_sz);
		const u32 sub_x = seq_header.color_config.subsampling_x ? 1 : 0;
		const u32 sub_y = seq_header.color_config.subsampling_y ? 1 : 0;
		const i8 alpha = plane == 1 ? mode_info.cfl_alphas.u : mode_info.cfl_alphas.v;

		u32 luma_avg = 0;
		u32 l[64][64];
		for (u32 i = 0; i < h; ++i) {
			u32 luma_y = (start_y + i) << sub_y;
			luma_y = std::min(luma_y, sb.max_luma_h - (1u << sub_y));
			for (u32 j = 0; j < w; ++j) {
				u32 luma_x = (start_x + j) << sub_x;
				luma_x = std::min(luma_x, sb.max_luma_w - (1u << sub_x));
				u32 t = 0;
				for (u32 dy = 0; dy <= sub_y; ++dy) {
					for (u32 dx = 0; dx <= sub_x; ++dx) {
						t += sb.curr_frame[0](luma_y + dy, luma_x + dx);
					}
				}
				const u32 v = t << (3 - sub_x - sub_y);
				l[i][j] = v;
				luma_avg += v;
			}
		}
		luma_avg = functions::round2(
			luma_avg, constants::get_tx_width_log2(tx_sz) + constants::get_tx_height_log2(tx_sz)
		);

		for (u32 i = 0; i < h; ++i) {
			for (u32 j = 0; j < w; ++j) {
				const u32 dc = sb.curr_frame[plane](start_y + i, start_x + j);
				const i32 scaled_luma =
					functions::round2_signed(alpha * (static_cast<i32>(l[i][j]) - static_cast<i32>(luma_avg)), 6);
				sb.curr_frame[plane](start_y + i, start_x + j) =
					functions::clip1(seq_header.color_config, static_cast<i32>(dc) + scaled_luma);
			}
		}
	}

	void reconstruct(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		state::block &sb,
		state::coeffs &sc,
		u32 plane, u32 x, u32 y, tx_size tx_sz
	) {
		u32 dq_denom;
		if (
			tx_sz == tx_size::size_32x32 ||
			tx_sz == tx_size::size_16x32 ||
			tx_sz == tx_size::size_32x16 ||
			tx_sz == tx_size::size_16x64 ||
			tx_sz == tx_size::size_64x16
		) {
			dq_denom = 2;
		} else if (
			tx_sz == tx_size::size_64x64 ||
			tx_sz == tx_size::size_32x64 ||
			tx_sz == tx_size::size_64x32
		) {
			dq_denom = 4;
		} else {
			dq_denom = 1;
		}
		const u32 log2w = constants::get_tx_width_log2(tx_sz);
		const u32 log2h = constants::get_tx_height_log2(tx_sz);
		const u32 w = 1u << log2w;
		const u32 h = 1u << log2h;
		const u32 tw = std::min(32u, w);
		const u32 th = std::min(32u, h);
		const bool flip_ud =
			sb.plane_tx_type == inverse_transform::flipadst_dct  ||
			sb.plane_tx_type == inverse_transform::flipadst_adst ||
			sb.plane_tx_type == inverse_transform::v_flipadst    ||
			sb.plane_tx_type == inverse_transform::flipadst_flipadst;
		const bool flip_lr =
			sb.plane_tx_type == inverse_transform::dct_flipadst  ||
			sb.plane_tx_type == inverse_transform::adst_flipadst ||
			sb.plane_tx_type == inverse_transform::h_flipadst    ||
			sb.plane_tx_type == inverse_transform::flipadst_flipadst;
		for (u32 i = 0; i < th; ++i) {
			for (u32 j = 0; j < tw; ++j) {
				const u32 q =
					i == 0 && j == 0 ?
					get_dc_quant(seq_header, header, mode_info, sb, plane) :
					get_ac_quant(seq_header, header, mode_info, sb, plane);
				u32 q2 = q;
				const u32 seg_q_m_level = header.seg_q_m_level[plane][std::to_underlying(mode_info.segment_id)];
				if (
					header.quantization_params.using_qmatrix &&
					sb.plane_tx_type < inverse_transform::idtx &&
					seg_q_m_level < 15
				) {
					const u32 qm_index = constants::qm_offset[std::to_underlying(tx_sz)] + i * tw + j;
					const u32 qm_value = constants::quantizer_matrix[seg_q_m_level][plane > 0 ? 1 : 0][qm_index];
					q2 = functions::round2(q * qm_value, 5);
				}
				const i32 dq = sc.quant[i * tw + j] * static_cast<i32>(q2);
				const i32 sign = dq < 0 ? -1 : 1;
				const i32 dq2 = sign * (std::abs(dq) & 0xFFFFFF) / static_cast<i32>(dq_denom);
				const u32 bit_depth = seq_header.color_config.bit_depth;
				sc.dequant[i][j] = std::clamp(dq2, -(1 << (7 + bit_depth)), (1 << (7 + bit_depth)) - 1);
			}
		}
		inverse_transform_2d(seq_header, mode_info, sb, sc, tx_sz);
		for (u32 i = 0; i < h; ++i) {
			for (u32 j = 0; j < w; ++j) {
				const u32 xx = flip_lr ? w - j - 1 : j;
				const u32 yy = flip_ud ? h - i - 1 : i;
				sb.curr_frame[plane](y + yy, x + xx) = functions::clip1(
					seq_header.color_config,
					static_cast<i16>(sb.curr_frame[plane](y + yy, x + xx)) + sc.residual[i][j]
				);
			}
		}
	}

	void inverse_transform_1d::butterfly_rotation(u32 a, u32 b, u32 angle, bool flip) {
		const i32 x = t[a] * functions::cos128(angle) - t[b] * functions::sin128(angle);
		const i32 y = t[a] * functions::sin128(angle) + t[b] * functions::cos128(angle);
		t[a] = functions::round2(x, 12);
		t[b] = functions::round2(y, 12);
		if (flip) {
			std::swap(t[a], t[b]);
		}
	}

	void inverse_transform_1d::hadamard_rotation(u32 a, u32 b, bool flip, u32 r) {
		if (flip) {
			std::swap(a, b);
		}
		const i32 x = t[a];
		const i32 y = t[b];
		t[a] = std::clamp(x + y, -(1 << (r - 1)), (1 << (r - 1)) - 1);
		t[b] = std::clamp(x - y, -(1 << (r - 1)), (1 << (r - 1)) - 1);
	}

	void inverse_transform_1d::permute_dct(u32 n) {
		const array_t copy_t = t;
		for (u32 i = 0; i < (1u << n) - 1; ++i) {
			t[i] = copy_t[functions::brev(n, i)];
		}
	}

	void inverse_transform_1d::inverse_dct(u32 n, u32 r) {
		// 1.
		permute_dct(n);
		// 2.
		if (n == 6) {
			for (u32 i = 0; i < 16; ++i) {
				butterfly_rotation(32 + i, 63 - i, 63 - 4 * functions::brev(4, i), false);
			}
		}
		// 3.
		if (n >= 5) {
			for (u32 i = 0; i < 8; ++i) {
				butterfly_rotation(16 + i, 31 - i, 6 + (functions::brev(3, 7 - i) << 3), false);
			}
		}
		// 4.
		if (n == 6) {
			for (u32 i = 0; i < 16; ++i) {
				hadamard_rotation(32 + i * 2, 33 + i * 2, i & 1, r);
			}
		}
		// 5.
		if (n >= 4) {
			for (u32 i = 0; i < 4; ++i) {
				butterfly_rotation(8 + i, 15 - i, 12 + (functions::brev(2, 3 - i) << 4), false);
			}
		}
		// 6.
		if (n >= 5) {
			for (u32 i = 0; i < 8; ++i) {
				hadamard_rotation(16 + 2 * i, 17 + 2 * i, i & 1, r);
			}
		}
		// 7.
		if (n == 6) {
			for (u32 i = 0; i < 4; ++i) {
				for (u32 j = 0; j < 2; ++j) {
					butterfly_rotation(
						62 - i * 4 - j,
						33 + i * 4 + j,
						60 - 16 * functions::brev(2, i) + 64 * j,
						true
					);
				}
			}
		}
		// 8.
		if (n >= 3) {
			for (u32 i = 0; i < 2; ++i) {
				butterfly_rotation(4 + i, 7 - i, 56 - 32 * i, false);
			}
		}
		// 9.
		if (n >= 4) {
			for (u32 i = 0; i < 4; ++i) {
				hadamard_rotation(8 + 2 * i, 9 + 2 * i, i & 1, r);
			}
		}
		// 10.
		if (n >= 5) {
			for (u32 i = 0; i < 2; ++i) {
				for (u32 j = 0; j < 2; ++j) {
					butterfly_rotation(30 - 4 * i - j, 17 + 4 * i + j, 24 + (j << 6) + ((1 - i) << 5), true);
				}
			}
		}
		// 11.
		if (n == 6) {
			for (u32 i = 0; i < 8; ++i) {
				for (u32 j = 0; j < 2; ++j) {
					hadamard_rotation(32 + i * 4 + j, 35 + i * 4 - j, i & 1, r);
				}
			}
		}
		// 12.
		for (u32 i = 0; i < 2; ++i) {
			butterfly_rotation(2 * i, 2 * i + 1, 32 + 16 * i, 1 - i);
		}
		// 13.
		if (n >= 3) {
			for (u32 i = 0; i < 2; ++i) {
				hadamard_rotation(4 + 2 * i, 5 + 2 * i, i, r);
			}
		}
		// 14.
		if (n >= 4) {
			for (u32 i = 0; i < 2; ++i) {
				butterfly_rotation(14 - i, 9 + i, 48 + 64 * i, true);
			}
		}
		// 15.
		if (n >= 5) {
			for (u32 i = 0; i < 4; ++i) {
				for (u32 j = 0; j < 2; ++j) {
					hadamard_rotation(16 + 4 * i + j, 19 + 4 * i - j, i & 1, r);
				}
			}
		}
		// 16.
		if (n == 6) {
			for (u32 i = 0; i < 2; ++i) {
				for (u32 j = 0; j < 4; ++j) {
					butterfly_rotation(61 - i * 8 - j, 34 + i * 8 + j, 56 - i * 32 + (j >> 1) * 64, true);
				}
			}
		}
		// 17.
		for (u32 i = 0; i < 2; ++i) {
			hadamard_rotation(i, 3 - i, false, r);
		}
		// 18.
		if (n >= 3) {
			butterfly_rotation(6, 5, 32, true);
		}
		// 19.
		if (n >= 4) {
			for (u32 i = 0; i < 2; ++i) {
				for (u32 j = 0; j < 2; ++j) {
					hadamard_rotation(8 + 4 * i + j, 11 + 4 * i - j, i, r);
				}
			}
		}
		// 20.
		if (n >= 5) {
			for (u32 i = 0; i < 4; ++i) {
				butterfly_rotation(29 - i, 18 + i, 48 + (i >> 1) * 64, true);
			}
		}
		// 21.
		if (n == 6) {
			for (u32 i = 0; i < 4; ++i) {
				for (u32 j = 0; j < 4; ++j) {
					hadamard_rotation(32 + 8 * i + j, 39 + 8 * i - j, i & 1, r);
				}
			}
		}
		// 22.
		if (n >= 3) {
			for (u32 i = 0; i < 4; ++i) {
				hadamard_rotation(i, 7 - i, false, r);
			}
		}
		// 23.
		if (n >= 4) {
			for (u32 i = 0; i < 2; ++i) {
				butterfly_rotation(13 - i, 10 + i, 32, true);
			}
		}
		// 24.
		if (n >= 5) {
			for (u32 i = 0; i < 2; ++i) {
				for (u32 j = 0; j < 4; ++j) {
					hadamard_rotation(16 + i * 8 + j, 23 + i * 8 - j, i, r);
				}
			}
		}
		// 25.
		if (n == 6) {
			for (u32 i = 0; i < 8; ++i) {
				butterfly_rotation(59 - i, 36 + i, i < 4 ? 48 : 112, true);
			}
		}
		// 26.
		if (n >= 4) {
			for (u32 i = 0; i < 8; ++i) {
				hadamard_rotation(i, 15 - i, false, r);
			}
		}
		// 27.
		if (n >= 5) {
			for (u32 i = 0; i < 4; ++i) {
				butterfly_rotation(27 - i, 20 + i, 32, true);
			}
		}
		// 28.
		if (n == 6) {
			for (u32 i = 0; i < 8; ++i) {
				hadamard_rotation(32 + i, 47 - i, false, r);
				hadamard_rotation(48 + i, 63 - i, true, r);
			}
		}
		// 29.
		if (n >= 5) {
			for (u32 i = 0; i < 16; ++i) {
				hadamard_rotation(i, 31 - i, false, r);
			}
		}
		// 30.
		if (n == 6) {
			for (u32 i = 0; i < 8; ++i) {
				butterfly_rotation(55 - i, 40 + i, 32, true);
			}
		}
		// 31.
		if (n == 6) {
			for (u32 i = 0; i < 32; ++i) {
				hadamard_rotation(i, 63 - i, false, r);
			}
		}
	}

	void inverse_transform_1d::permute_adst_input(u32 n) {
		const u32 n0 = 1u << n;
		const array_t copy_t = t;
		for (u32 i = 0; i < n0; ++i) {
			const u32 idx = (i & 1) ? i - 1 : n0 - i - 1;
			t[i] = copy_t[idx];
		}
	}

	void inverse_transform_1d::permute_adst_output(u32 n) {
		const u32 n0 = 1u << n;
		const array_t copy_t = t;
		for (u32 i = 0; i < n0; ++i) {
			const u32 a =  (i >> 3) & 1;
			const u32 b = ((i >> 2) & 1) ^ ((i >> 3) & 1);
			const u32 c = ((i >> 1) & 1) ^ ((i >> 2) & 1);
			const u32 d = ( i       & 1) ^ ((i >> 1) & 1);
			const u32 idx = ((d << 3) | (c << 2) | (b << 1) | a) >> (4 - n);
			t[i] = (i & 1) ? -copy_t[idx] : copy_t[idx];
		}
	}

	void inverse_transform_1d::inverse_adst4(u32 r) {
		i32 s[7];
		i32 x[4];

		s[0] = constants::sinpi_1_9 * t[0];
		s[1] = constants::sinpi_2_9 * t[0];
		s[2] = constants::sinpi_3_9 * t[1];
		s[3] = constants::sinpi_4_9 * t[2];
		s[4] = constants::sinpi_1_9 * t[2];
		s[5] = constants::sinpi_2_9 * t[3];
		s[6] = constants::sinpi_4_9 * t[3];
		const i32 a7 = t[0] - t[2];
		const i32 b7 = a7 + t[3];

		s[0] = s[0] + s[3];
		s[1] = s[1] - s[4];
		s[3] = s[2];
		s[2] = constants::sinpi_3_9 * b7;

		s[0] = s[0] + s[5];
		s[1] = s[1] - s[6];

		x[0] = s[0] + s[3];
		x[1] = s[1] + s[3];
		x[2] = s[2];
		x[3] = s[0] + s[1];

		x[3] = x[3] - s[3];

		t[0] = functions::round2(x[0], 12);
		t[1] = functions::round2(x[1], 12);
		t[2] = functions::round2(x[2], 12);
		t[3] = functions::round2(x[3], 12);
	}

	void inverse_transform_1d::inverse_adst8(u32 r) {
		// 1.
		permute_adst_input(3);
		// 2.
		for (u32 i = 0; i < 4; ++i) {
			butterfly_rotation(2 * i, 2 * i + 1, 60 - 16 * i, true);
		}
		// 3.
		for (u32 i = 0; i < 4; ++i) {
			hadamard_rotation(i, 4 + i, false, r);
		}
		// 4.
		for (u32 i = 0; i < 2; ++i) {
			butterfly_rotation(4 + 3 * i, 5 + i, 48 - 32 * i, true);
		}
		// 5.
		for (u32 i = 0; i < 2; ++i) {
			for (u32 j = 0; j < 2; ++j) {
				hadamard_rotation(4 * j + i, 2 + 4 * j + i, false, r);
			}
		}
		// 6.
		for (u32 i = 0; i < 2; ++i) {
			butterfly_rotation(2 + 4 * i, 3 + 4 * i, 32, true);
		}
		// 7.
		permute_adst_output(3);
	}

	void inverse_transform_1d::inverse_adst16(u32 r) {
		// 1.
		permute_adst_input(4);
		// 2.
		for (u32 i = 0; i < 8; ++i) {
			butterfly_rotation(2 * i, 2 * i + 1, 62 - 8 * i, true);
		}
		// 3.
		for (u32 i = 0; i < 8; ++i) {
			hadamard_rotation(i, 8 + i, false, r);
		}
		// 4.
		for (u32 i = 0; i < 2; ++i) {
			butterfly_rotation(8 + 2 * i, 9 + 2 * i, 56 - 32 * i, true);
			butterfly_rotation(13 + 2 * i, 12 + 2 * i, 8 + 32 * i, true);
		}
		// 5.
		for (u32 i = 0; i < 4; ++i) {
			for (u32 j = 0; j < 2; ++j) {
				hadamard_rotation(8 * j + i, 4 + 8 * j + i, false, r);
			}
		}
		// 6.
		for (u32 i = 0; i < 2; ++i) {
			for (u32 j = 0; j < 2; ++j) {
				butterfly_rotation(4 + 8 * j + 3 * i, 5 + 8 * j + i, 48 - 32 * i, true);
			}
		}
		// 7.
		for (u32 i = 0; i < 2; ++i) {
			for (u32 j = 0; j < 4; ++j) {
				hadamard_rotation(4 * j + i, 2 + 4 * j + i, false, r);
			}
		}
		// 8.
		for (u32 i = 0; i < 4; ++i) {
			butterfly_rotation(2 + 4 * i, 3 + 4 * i, 32, true);
		}
		// 9.
		permute_adst_output(4);
	}

	void inverse_transform_1d::inverse_adst(u32 n, u32 r) {
		if (n == 2) {
			inverse_adst4(r);
		} else if (n == 3) {
			inverse_adst8(r);
		} else {
			crash_if(n != 4);
			inverse_adst16(r);
		}
	}

	void inverse_transform_1d::inverse_idtx4() {
		for (u32 i = 0; i < 4; ++i) {
			t[i] = functions::round2(t[i] * 5793, 12);
		}
	}

	void inverse_transform_1d::inverse_idtx8() {
		for (u32 i = 0; i < 8; ++i) {
			t[i] = t[i] * 2;
		}
	}

	void inverse_transform_1d::inverse_idtx16() {
		for (u32 i = 0; i < 16; ++i) {
			t[i] = functions::round2(t[i] * 11586, 12);
		}
	}

	void inverse_transform_1d::inverse_idtx32() {
		for (u32 i = 0; i < 32; ++i) {
			t[i] = t[i] * 4;
		}
	}

	void inverse_transform_1d::inverse_idtx(u32 n) {
		if (n == 2) {
			inverse_idtx4();
		} else if (n == 3) {
			inverse_idtx8();
		} else if (n == 4) {
			inverse_idtx16();
		} else {
			crash_if(n != 5);
			inverse_idtx32();
		}
	}

	void inverse_transform_2d(
		const obu::sequence_header &seq_header,
		const obu::mode_info &mode_info,
		const state::block &sb,
		state::coeffs &sc,
		tx_size tx_sz
	) {
		const u32 log2w = constants::get_tx_width_log2(tx_sz);
		const u32 log2h = constants::get_tx_height_log2(tx_sz);
		const u32 w = 1u << log2w;
		const u32 h = 1u << log2h;
		const u32 row_shift = mode_info.lossless ? 0 : constants::transform_row_shift[std::to_underlying(tx_sz)];
		const u32 col_shift = mode_info.lossless ? 0 : 4;
		const u32 row_clamp_range = seq_header.color_config.bit_depth + 8;
		const u32 col_clamp_range = std::max(seq_header.color_config.bit_depth + 6u, 16u);

		// row transforms
		for (u32 i = 0; i < h; ++i) {
			inverse_transform_1d t = uninitialized;
			for (u32 j = 0; j < w; ++j) {
				t.t[j] = i < 32 && j < 32 ? sc.dequant[i][j] : 0;
			}
			if (std::abs(static_cast<i32>(log2w) - static_cast<i32>(log2h)) == 1) {
				for (u32 j = 0; j < w; ++j) {
					t.t[j] = functions::round2(t.t[j] * 2896, 12);
				}
			}
			if (mode_info.lossless) {
				std::abort(); // TODO
			} else {
				switch (sb.plane_tx_type) {
				case inverse_transform::dct_dct:      [[fallthrough]];
				case inverse_transform::adst_dct:     [[fallthrough]];
				case inverse_transform::flipadst_dct: [[fallthrough]];
				case inverse_transform::h_dct:
					t.inverse_dct(log2w, row_clamp_range);
					break;
				case inverse_transform::dct_adst:          [[fallthrough]];
				case inverse_transform::adst_adst:         [[fallthrough]];
				case inverse_transform::dct_flipadst:      [[fallthrough]];
				case inverse_transform::flipadst_flipadst: [[fallthrough]];
				case inverse_transform::adst_flipadst:     [[fallthrough]];
				case inverse_transform::flipadst_adst:     [[fallthrough]];
				case inverse_transform::h_adst:            [[fallthrough]];
				case inverse_transform::h_flipadst:
					t.inverse_adst(log2w, row_clamp_range);
					break;
				default:
					t.inverse_idtx(log2w);
					break;
				}
			}
			for (u32 j = 0; j < w; ++j) {
				sc.residual[i][j] = functions::round2(t.t[j], row_shift);
			}
		}

		for (u32 i = 0; i < h; ++i) {
			for (u32 j = 0; j < w; ++j) {
				sc.residual[i][j] = std::clamp(
					sc.residual[i][j],
					-(1 << (col_clamp_range - 1)),
					(1 << (col_clamp_range - 1)) - 1
				);
			}
		}

		// column transforms
		for (u32 j = 0; j < w; ++j) {
			inverse_transform_1d t = uninitialized;
			for (u32 i = 0; i < h; ++i) {
				t.t[i] = sc.residual[i][j];
			}
			if (mode_info.lossless) {
				std::abort(); // TODO
			} else {
				switch (sb.plane_tx_type) {
				case inverse_transform::dct_dct:      [[fallthrough]];
				case inverse_transform::dct_adst:     [[fallthrough]];
				case inverse_transform::dct_flipadst: [[fallthrough]];
				case inverse_transform::v_dct:
					t.inverse_dct(log2h, col_clamp_range);
					break;
				case inverse_transform::adst_dct:          [[fallthrough]];
				case inverse_transform::adst_adst:         [[fallthrough]];
				case inverse_transform::flipadst_dct:      [[fallthrough]];
				case inverse_transform::flipadst_flipadst: [[fallthrough]];
				case inverse_transform::adst_flipadst:     [[fallthrough]];
				case inverse_transform::flipadst_adst:     [[fallthrough]];
				case inverse_transform::v_adst:            [[fallthrough]];
				case inverse_transform::v_flipadst:
					t.inverse_adst(log2h, col_clamp_range);
					break;
				default:
					t.inverse_idtx(log2h);
					break;
				}
			}
			for (u32 i = 0; i < h; ++i) {
				sc.residual[i][j] = functions::round2(t.t[i], col_shift);
			}
		}
	}
}
