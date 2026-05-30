#pragma once

/// \file
/// 8.2 Parsing process for symbol decoder

#include "lotus/types.h"

#include "lotus/av1/cdf.h"
#include "lotus/av1/cdf_context.h"

namespace lotus::av1 {
	struct reader;

	/// 8.2. Parsing process for symbol decoder
	struct symbol_decoder {
	public:
		/// 8.2.2. Initialization process for symbol decoder
		static symbol_decoder init_symbol(
			reader&, const cdf::non_coeff&, const cdf::coeff&, bool disable_cdf_update, u64 sz
		);
		/// 8.2.4. Exit process for symbol decoder
		void exit_symbol();

		/// 8.2.3. Boolean decoding process
		[[nodiscard]] bool read_bool();
		/// 4.10.8. L(n)
		/// 8.2.5. Parsing process for read_literal
		[[nodiscard]] u32 read_literal(u32 n);
		/// 4.10.9. S()
		/// 8.2.6. Symbol decoding process
		[[nodiscard]] symbol_t read_symbol(std::span<cdf::value_t>);
		/// 4.10.10. NS(n)
		[[nodiscard]] u32 read_ns(u32 n);

		// 361
		/// \p use_intrabc.
		[[nodiscard]] bool read_use_intrabc();
		/// \p intra_frame_y_mode.
		[[nodiscard]] prediction_mode read_intra_frame_y_mode(const state::block&, const state::block_decoding&);
		// 362
		/// \p y_mode.
		[[nodiscard]] prediction_mode read_y_mode(block_size mi_size);
		/// \p uv_mode.
		[[nodiscard]] prediction_mode read_uv_mode(const obu::sequence_header&, const obu::mode_info&, block_size);
		/// \p angle_delta_y.
		[[nodiscard]] i8 read_angle_delta_y(prediction_mode y_mode);
		/// \p angle_delta_uv.
		[[nodiscard]] i8 read_angle_delta_uv(prediction_mode uv_mode);
		/// \p partition.
		[[nodiscard]] partition read_partition(cdf::context::partition);
		/// \p split_or_horz.
		[[nodiscard]] bool read_split_or_horz(cdf::context::partition);
		// 363
		/// \p split_or_vert.
		[[nodiscard]] bool read_split_or_vert(cdf::context::partition);
		/// \p tx_depth.
		[[nodiscard]] u8 read_tx_depth(u32 max_tx_depth, u32 ctx);
		// 364
		/// \p txfm_split.
		[[nodiscard]] bool read_txfm_split(u32 ctx);
		// 365
		/// \p segment_id.
		[[nodiscard]] segment_id_t read_segment_id(u32 ctx);
		/// \p seg_id_predicted.
		[[nodiscard]] bool read_seg_id_predicted(const state::block&, const state::block_decoding&);
		/// \p new_mv.
		[[nodiscard]] bool read_new_mv(u32 new_mv_ctx);
		/// \p zero_mv.
		[[nodiscard]] bool read_zero_mv(u32 zero_mv_ctx);
		/// \p ref_mv.
		[[nodiscard]] bool read_ref_mv(u32 ref_mv_ctx);
		/// \p drl_mode.
		[[nodiscard]] bool read_drl_mode(u32 ctx);
		/// \p is_inter.
		[[nodiscard]] bool read_is_inter(u32 ctx);
		/// \p skip_mode.
		[[nodiscard]] bool read_skip_mode(u32 ctx);
		// 366
		/// \p use_filter_intra.
		[[nodiscard]] bool read_use_filter_intra(block_size mi_size);
		/// \p filter_intra_mode.
		[[nodiscard]] intra_filtering read_filter_intra_mode();
		// 367
		/// \p skip.
		[[nodiscard]] bool read_skip(const state::block&, const state::block_decoding&);
		// 369
		/// \p compound_mode.
		[[nodiscard]] u8 read_compound_mode(const state::motion_vector&);
		/// \p interp_filter.
		[[nodiscard]] interpolation read_interp_filter(u32 ctx);
		// 370
		/// \p mv_joint.
		[[nodiscard]] mv_joint read_mv_joint(u32 mv_ctx);
		/// \p mv_sign.
		[[nodiscard]] bool read_mv_sign(u32 mv_ctx, u32 comp);
		/// \p mv_class.
		[[nodiscard]] u8 read_mv_class(u32 mv_ctx, u32 comp);
		/// \p mv_class0_bit.
		[[nodiscard]] bool read_mv_class0_bit(u32 mv_ctx, u32 comp);
		/// \p mv_class0_fr.
		[[nodiscard]] u8 read_mv_class0_fr(u32 mv_ctx, u32 comp, bool mv_class0_bit);
		/// \p mv_class0_hp.
		[[nodiscard]] bool read_mv_class0_hp(u32 mv_ctx, u32 comp);
		/// \p mv_fr.
		[[nodiscard]] u8 read_mv_fr(u32 mv_ctx, u32 comp);
		/// \p mv_hp.
		[[nodiscard]] bool read_mv_hp(u32 mv_ctx, u32 comp);
		/// \p mv_bit.
		[[nodiscard]] bool read_mv_bit(u32 mv_ctx, u32 comp, u32 i);
		/// \p all_zero.
		[[nodiscard]] bool read_all_zero(u32 tx_sz_ctx, u32 ctx);
		// 372
		/// \p eob_pt_16.
		[[nodiscard]] u8 read_eob_pt_16(u32 ptype, u32 ctx);
		/// \p eob_pt_32.
		[[nodiscard]] u8 read_eob_pt_32(u32 ptype, u32 ctx);
		/// \p eob_pt_64.
		[[nodiscard]] u8 read_eob_pt_64(u32 ptype, u32 ctx);
		/// \p eob_pt_128.
		[[nodiscard]] u8 read_eob_pt_128(u32 ptype, u32 ctx);
		/// \p eob_pt_256.
		[[nodiscard]] u8 read_eob_pt_256(u32 ptype, u32 ctx);
		/// \p eob_pt_512.
		[[nodiscard]] u8 read_eob_pt_512(u32 ptype);
		/// \p eob_pt_1024.
		[[nodiscard]] u8 read_eob_pt_1024(u32 ptype);
		/// \p eob_extra.
		[[nodiscard]] bool read_eob_extra(u32 tx_sz_ctx, u32 ptype, u32 eob_pt);
		/// \p coeff_base.
		[[nodiscard]] u8 read_coeff_base(u32 tx_sz_ctx, u32 ptype, u32 ctx);
		// 377
		/// \p coeff_base_eob.
		[[nodiscard]] u8 read_coeff_base_eob(u32 tx_sz_ctx, u32 ptype, u32 ctx);
		// 378
		/// \p dc_sign.
		[[nodiscard]] bool read_dc_sign(u32 ptype, u32 ctx);
		/// \p coeff_br.
		[[nodiscard]] u8 read_coeff_br(u32 tx_sz_ctx, u32 ptype, u32 ctx);
		// 380
		/// \p has_palette_y.
		[[nodiscard]] bool read_has_palette_y(u32 bsize_ctx, u32 ctx);
		/// \p has_palette_uv.
		[[nodiscard]] bool read_has_palette_uv(u32 palette_size_y);
		/// \p palette_size_y_minus_2.
		[[nodiscard]] u8 read_palette_size_y_minus_2(u32 bsize_ctx);
		/// \p palette_size_uv_minus_2.
		[[nodiscard]] u8 read_palette_size_uv_minus_2(u32 bsize_ctx);
		/// \p palette_color_idx_y.
		[[nodiscard]] u8 read_palette_color_idx_y(u8 palette_size_y, u32 color_context_hash);
		/// \p palette_color_idx_uv.
		[[nodiscard]] u8 read_palette_color_idx_uv(u8 palette_size_uv, u32 color_context_hash);
		// 381
		/// \p delta_q_abs.
		[[nodiscard]] u8 read_delta_q_abs();
		/// \p delta_lf_abs.
		[[nodiscard]] u8 read_delta_lf_abs(bool delta_lf_multi, u32 i);
		/// \p intra_tx_type.
		[[nodiscard]] u8 read_intra_tx_type(tx_set set, tx_size tx_sz, prediction_mode intra_dir);
		// 382
		/// \p inter_tx_type.
		[[nodiscard]] u8 read_inter_tx_type(tx_set set, tx_size tx_sz);
		// 385
		/// \p interintra.
		[[nodiscard]] bool read_interintra(block_size mi_size);
		/// \p interintra_mode.
		[[nodiscard]] intra_prediction read_interintra_mode(block_size mi_size);
		/// \p wedge_index.
		[[nodiscard]] u8 read_wedge_index(block_size mi_size);
		/// \p wedge_interintra.
		[[nodiscard]] bool read_wedge_interintra(block_size mi_size);
		/// \p cfl_alpha_signs.
		[[nodiscard]] u8 read_cfl_alpha_signs();
		/// \p cfl_alpha_u.
		[[nodiscard]] u8 read_cfl_alpha_u(u8 cfl_alpha_signs);
		// 386
		/// \p cfl_alpha_v.
		[[nodiscard]] u8 read_cfl_alpha_v(u8 cfl_alpha_signs);
		// 387
		/// \p use_wiener.
		[[nodiscard]] bool read_use_wiener();
		/// \p use_sgrproj.
		[[nodiscard]] bool read_use_sgrproj();
		/// \p restoration_type.
		[[nodiscard]] frame_restoration read_restoration_type();

		/// 5.11.58. Read loop restoration unit syntax
		/// \p decode_signed_subexp_with_ref_bool().
		[[nodiscard]] i32 decode_signed_subexp_with_ref_bool(i32 low, i32 high, u32 k, i32 r);
		/// 5.11.58. Read loop restoration unit syntax
		/// \p decode_unsigned_subexp_with_ref_bool().
		[[nodiscard]] u32 decode_unsigned_subexp_with_ref_bool(u32 mx, u32 k, i32 r);
		/// 5.11.58. Read loop restoration unit syntax
		/// \p decode_subexp_bool().
		[[nodiscard]] u32 decode_subexp_bool(u32 num_syms, u32 k);
	private:
		/// Initializes \ref _reader.
		symbol_decoder(reader&, const cdf::non_coeff&, const cdf::coeff&, bool disable_cdf_update);

		/// \p TileIntraFrameYModeCdf.
		cdf::intra_frame_y_mode_t _tile_intra_frame_y_mode_cdf = cdf::defaults::intra_frame_y_mode;
		cdf::non_coeff _tile_non_coeff_cdf; ///< CDF arrays initialized by \p init_non_coeff_cdfs.
		cdf::coeff _tile_coeff_cdf; ///< CDF arrays initialized by \p init_coeff_cdfs.
		const bool _disable_cdf_update = false; ///< \p disable_cdf_update.

		reader &_reader; ///< The bitstream reader.

		i64 _symbol_max_bits = 0; ///< \p SymbolMaxBits.
		cdf::value_t _symbol_value = 0; ///< \p SymbolValue.
		cdf::value_t _symbol_range = 0; ///< \p SymbolRange.

		/// 8.2.6. Symbol decoding process
		/// Symbol decoding without CDF update.
		[[nodiscard]] symbol_t _read_symbol_no_update(std::span<const cdf::value_t>);
		/// 8.2.6. Symbol decoding process
		/// CDF update.
		static void _update_cdf(std::span<cdf::value_t>, symbol_t symbol);
	};
}
