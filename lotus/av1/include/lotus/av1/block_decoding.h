#pragma once

/// \file
/// Block decoding routines.

#include "lotus/av1/tables.h"
#include "lotus/av1/state.h"
#include "lotus/av1/obu.h"
#include "lotus/av1/functions.h"

namespace lotus::av1::block_decoding {
	/// 5.11.33. Compute prediction syntax
	void compute_prediction(
		const obu::sequence_header&,
		const obu::uncompressed_header&,
		const obu::mode_info&,
		state::block&,
		const state::block_decoding&
	);

	/// 7.4. Decode frame wrapup process
	void decode_frame_wrapup(
		const obu::uncompressed_header&,
		const state::block&
	);

	/// 7.11.2. Intra prediction process
	/// 7.11.2.1. General
	void predict_intra(
		const obu::sequence_header&,
		const obu::uncompressed_header&,
		const obu::mode_info&,
		state::block&,
		const state::block_decoding&,
		u32 plane, u32 x, u32 y,
		bool have_left, bool have_above, bool have_above_right, bool have_below_left,
		prediction_mode mode, u32 log2w, u32 log2h
	);
	/// 7.11.2. Intra prediction process
	struct prediction {
		/// No initialization.
		prediction(uninitialized_t) {
		}

		u32 above_row_val[257]; ///< \p AboveRow with each element offset by 1.
		u32 left_col_val[257]; ///< \p LeftCol with each element offset by 1.
		u16 pred[64][64]; ///< \p pred.

		/// \p AboveRow.
		[[nodiscard]] u32 &above_row(std::integral auto i) {
			return above_row_val[i + 2];
		}
		/// \p AboveRow.
		[[nodiscard]] const u32 &above_row(std::integral auto i) const {
			return above_row_val[i + 2];
		}
		/// \p LeftCol.
		[[nodiscard]] u32 &left_col(std::integral auto i) {
			return left_col_val[i + 2];
		}
		/// \p LeftCol.
		[[nodiscard]] const u32 &left_col(std::integral auto i) const {
			return left_col_val[i + 2];
		}

		/// 7.11.2.2. Basic intra prediction process
		void predict_intra_basic(u32 w, u32 h);
		/// 7.11.2.3. Recursive intra prediction process
		void predict_intra_recursive(const obu::sequence_header&, const obu::mode_info&, u32 w, u32 h);
		/// 7.11.2.4. Directional intra prediction process
		void predict_intra_directional(
			const obu::sequence_header&,
			const obu::mode_info&,
			const state::block&,
			const state::block_decoding&,
			u32 plane, u32 x, u32 y,
			bool have_left, bool have_above, prediction_mode mode,
			u32 w, u32 h, u32 max_x, u32 max_y
		);
		/// 7.11.2.5. DC intra prediction process
		void predict_intra_dc(
			const obu::sequence_header&, bool have_left, bool have_above, u32 log2w, u32 log2h, u32 w, u32 h
		);
		/// 7.11.2.6. Smooth intra prediction process
		void predict_intra_smooth(prediction_mode mode, u32 log2w, u32 log2h, u32 w, u32 h);

		/// 7.11.2.7. Filter corner process
		[[nodiscard]] u32 filter_corner() const {
			const u32 s = left_col(0) * 5 + above_row(-1) * 6 + above_row(0) * 5;
			return functions::round2(s, 4);
		}

		/// 7.11.2.11. Intra edge upsample process
		void intra_edge_upsample(const obu::color_config&, u32 num_px, bool dir);
		/// 7.11.2.12. Intra edge filter process
		void intra_edge_filter(u32 sz, u32 strength, bool left);
	};

	/// 7.11.2.9. Intra edge filter strength selection process
	[[nodiscard]] inline u32 select_intra_edge_filter_strength(u32 w, u32 h, bool filter_type, i32 delta) {
		const i32 d = std::abs(delta);
		const u32 blk_wh = w + h;
		u32 strength = 0;
		if (!filter_type) {
			if (blk_wh <= 8) {
				if (d >= 56) {
					strength = 1;
				}
			} else if (blk_wh <= 12) {
				if (d >= 40) {
					strength = 1;
				}
			} else if (blk_wh <= 16) {
				if (d >= 40) {
					strength = 1;
				}
			} else if (blk_wh <= 24) {
				if (d >= 32) {
					strength = 3;
				} else if (d >= 16) {
					strength = 2;
				} else if (d >= 8) {
					strength = 1;
				}
			} else if (blk_wh <= 32) {
				if (d >= 32) {
					strength = 3;
				} else if (d >= 4) {
					strength = 2;
				} else {
					strength = 1;
				}
			} else {
				strength = 3;
			}
		} else {
			if (blk_wh <= 8) {
				if (d >= 64) {
					strength = 2;
				} else if (d >= 40) {
					strength = 1;
				}
			} else if (blk_wh <= 16) {
				if (d >= 48) {
					strength = 2;
				} else if (d >= 20) {
					strength = 1;
				}
			} else if (blk_wh <= 24) {
				if (d >= 4) {
					strength = 3;
				}
			} else {
				strength = 3;
			}
		}
		return strength;
	}
	/// 7.11.2.10. Intra edge upsample selection process
	[[nodiscard]] inline bool select_intra_edge_upsample(u32 w, u32 h, bool filter_type, i32 delta) {
		const i32 d = std::abs(delta);
		const u32 blk_wh = w + h;
		if (d <= 0 || d >= 40) {
			return false;
		} else if (!filter_type) {
			return blk_wh <= 16;
		} else {
			return blk_wh <= 8;
		}
	}

	/// 7.11.4. Palette prediction process
	void predict_palette(
		const obu::mode_info&,
		state::block&,
		const state::color_map&,
		u32 plane, u32 start_x, u32 start_y, u32 x, u32 y, tx_size tx_sz
	);

	/// 7.11.5. Predict chroma from luma process
	void predict_chroma_from_luma(
		const obu::sequence_header&,
		const obu::mode_info&,
		state::block&,
		u32 plane, u32 start_x, u32 start_y, tx_size tx_sz
	);

	/// 7.12.2. Dequantization functions
	/// \p dc_q().
	[[nodiscard]] constexpr u16 dc_q(const obu::sequence_header &seq_header, i32 b) {
		return constants::dc_q_lookup[(seq_header.color_config.bit_depth - 8) >> 1][std::clamp(b, 0, 255)];
	}
	/// 7.12.2. Dequantization functions
	/// \p ac_q().
	[[nodiscard]] constexpr u16 ac_q(const obu::sequence_header &seq_header, i32 b) {
		return constants::ac_q_lookup[(seq_header.color_config.bit_depth - 8) >> 1][std::clamp(b, 0, 255)];
	}
	/// 7.12.2. Dequantization functions
	/// \p get_dc_quant().
	[[nodiscard]] constexpr u16 get_dc_quant(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		const state::block &sb,
		u32 plane
	) {
		obu::delta_q delta_q;
		if (plane == 0) {
			delta_q = header.quantization_params.delta_q_y_dc;
		} else if (plane == 1) {
			delta_q = header.quantization_params.delta_q_u_dc;
		} else {
			crash_if(plane != 2);
			delta_q = header.quantization_params.delta_q_v_dc;
		}
		return dc_q(
			seq_header,
			static_cast<i32>(functions::get_qindex(header, sb, false, mode_info.segment_id)) + delta_q
		);
	}
	/// 7.12.2. Dequantization functions
	/// \p get_ac_quant().
	[[nodiscard]] constexpr u16 get_ac_quant(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		const state::block &sb,
		u32 plane
	) {
		obu::delta_q delta_q;
		if (plane == 0) {
			delta_q = 0;
		} else if (plane == 1) {
			delta_q = header.quantization_params.delta_q_u_ac;
		} else {
			crash_if(plane != 2);
			delta_q = header.quantization_params.delta_q_v_ac;
		}
		return ac_q(
			seq_header,
			static_cast<i32>(functions::get_qindex(header, sb, false, mode_info.segment_id)) + delta_q
		);
	}

	/// 7.12.3. Reconstruct process
	void reconstruct(
		const obu::sequence_header&,
		const obu::uncompressed_header&,
		const obu::mode_info&,
		state::block&,
		state::coeffs&,
		u32 plane, u32 x, u32 y, tx_size tx_sz
	);

	/// 7.13.2. 1D transforms
	/// Struct holding 1D inverse transform data.
	struct inverse_transform_1d {
		using value_t = i32; ///< Value type.
		using array_t = std::array<value_t, 64>; ///< Array type.

		/// No initialization.
		inverse_transform_1d(uninitialized_t) {
		}

		array_t t; ///< \p T.

		/// 7.13.2.1. Butterfly functions
		/// \p B().
		void butterfly_rotation(u32 a, u32 b, u32 angle, bool flip);
		/// 7.13.2.1. Butterfly functions
		/// \p H().
		void hadamard_rotation(u32 a, u32 b, bool flip, u32 r);

		/// 7.13.2.2. Inverse DCT array permutation process
		void permute_dct(u32 n);
		/// 7.13.2.3. Inverse DCT process
		void inverse_dct(u32 n, u32 r);

		/// 7.13.2.4. Inverse ADST input array permutation process
		void permute_adst_input(u32 n);
		/// 7.13.2.5. Inverse ADST output array permutation process
		void permute_adst_output(u32 n);
		/// 7.13.2.6. Inverse ADST4 process
		void inverse_adst4(u32 r);
		/// 7.13.2.7. Inverse ADST8 process
		void inverse_adst8(u32 r);
		/// 7.13.2.8. Inverse ADST16 process
		void inverse_adst16(u32 r);
		/// 7.13.2.9. Inverse ADST process
		void inverse_adst(u32 n, u32 r);

		/// 7.13.2.11. Inverse identity transform 4 process
		void inverse_idtx4();
		/// 7.13.2.12. Inverse identity transform 8 process
		void inverse_idtx8();
		/// 7.13.2.13. Inverse identity transform 16 process
		void inverse_idtx16();
		/// 7.13.2.14. Inverse identity transform 32 process
		void inverse_idtx32();
		/// 7.13.2.15. Inverse identity transform process
		void inverse_idtx(u32 n);
	};

	/// 7.13.3. 2D inverse transform process
	void inverse_transform_2d(
		const obu::sequence_header&,
		const obu::mode_info&,
		const state::block&,
		state::coeffs&,
		tx_size tx_sz
	);
}
