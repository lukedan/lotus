#include "lotus/av1/symbol_decoder.h"

/// \file
/// Implementation of the symbol decoder.

#include "lotus/av1/functions.h"
#include "lotus/av1/reader.h"

namespace lotus::av1 {
	symbol_decoder symbol_decoder::init_symbol(
		reader &r,
		const cdf::non_coeff &tile_non_coeff_cdf,
		const cdf::coeff &tile_coeff_cdf,
		bool disable_cdf_update,
		u64 sz
	) {
		symbol_decoder result(r, tile_non_coeff_cdf, tile_coeff_cdf, disable_cdf_update);

		const auto num_bits = static_cast<u32>(std::min(sz * 8, 15ULL));
		const u32 buf = result._reader.read_bits(num_bits);
		const u32 padded_buf = buf << (15u - num_bits);
		result._symbol_value = static_cast<cdf::value_t>(((1u << 15) - 1) ^ padded_buf);
		result._symbol_range = 1u << 15;
		result._symbol_max_bits = 8 * static_cast<i64>(sz) - 15;
		// TODO TileIntraFrameYModeCdf
		// TODO CDF array copies
		return result;
	}

	void symbol_decoder::exit_symbol() {
		crash_if(_symbol_max_bits < -14);
		const u64 trailing_bit_position =
			_reader.get_position() - std::min(15ULL, static_cast<u64>(_symbol_max_bits + 15));
		if (_symbol_max_bits > 0) {
			_reader.read_bits(_symbol_max_bits); // TODO skip_bits
		}
		const u64 padding_end_position = _reader.get_position();
		// TODO conformance check - bit at trailing bit position is 1, bits after are 0
		// TODO saved CDFs
	}

	bool symbol_decoder::read_bool() {
		constexpr cdf::value_t cdf[3] = { 1 << 14, 1 << 15, 0 };
		return std::to_underlying(_read_symbol_no_update(cdf)) == 1;
	}

	u32 symbol_decoder::read_literal(u32 n) {
		u32 x = 0;
		for (u32 i = 0; i < n; ++i) {
			x = 2 * x + read_bool();
		}
		return x;
	}

	symbol_t symbol_decoder::read_symbol(std::span<cdf::value_t> cdf) {
		const symbol_t symbol = _read_symbol_no_update(cdf);
		if (!_disable_cdf_update) {
			_update_cdf(cdf, symbol);
		}
		return symbol;
	}

	u32 symbol_decoder::read_ns(u32 n) {
		const u32 w = functions::floor_log2(n) + 1;
		const u32 m = (1u << w) - n;
		const u32 v = read_literal(w - 1);
		if (v < m) {
			return v;
		}
		const u32 extra_bit = read_literal(1);
		return (v << 1) - m + extra_bit;
	}

	bool symbol_decoder::read_use_intrabc() {
		return read_symbol(_tile_non_coeff_cdf.intrabc.value) != zero;
	}

	prediction_mode symbol_decoder::read_intra_frame_y_mode(
		const state::block &sb, const state::block_decoding &sbd
	) {
		const u32 abovemode = constants::intra_mode_context[std::to_underlying(
			sbd.avail_u ? sb.y_modes(sbd.mi_row - 1, sbd.mi_col) : prediction_mode::dc
		)];
		const u32 leftmode = constants::intra_mode_context[std::to_underlying(
			sbd.avail_l ? sb.y_modes(sbd.mi_row, sbd.mi_col - 1) : prediction_mode::dc
		)];
		return static_cast<prediction_mode>(read_symbol(_tile_intra_frame_y_mode_cdf.value[abovemode][leftmode]));
	}

	prediction_mode symbol_decoder::read_y_mode(block_size mi_size) {
		return static_cast<prediction_mode>(read_symbol(
			_tile_non_coeff_cdf.y_mode.value[constants::size_group[std::to_underlying(mi_size)]]
		));
	}

	prediction_mode symbol_decoder::read_uv_mode(
		const obu::sequence_header &seq_header, const obu::mode_info &mode_info, block_size mi_size
	) {
		std::span<cdf::value_t> cdf;
		if (
			mode_info.lossless &&
			functions::get_plane_residual_size(seq_header.color_config, mi_size, 1) == block_size::b4x4
		) {
			cdf = _tile_non_coeff_cdf.uv_mode_cfl_allowed.value[std::to_underlying(mode_info.y_mode)];
		} else if (
			!mode_info.lossless &&
			std::max(constants::block_width(mi_size), constants::block_height(mi_size)) <= 32
		) {
			cdf = _tile_non_coeff_cdf.uv_mode_cfl_allowed.value[std::to_underlying(mode_info.y_mode)];
		} else {
			cdf = _tile_non_coeff_cdf.uv_mode_cfl_not_allowed.value[std::to_underlying(mode_info.y_mode)];
		}
		return static_cast<prediction_mode>(read_symbol(cdf));
	}

	i8 symbol_decoder::read_angle_delta_y(prediction_mode y_mode) {
		return static_cast<i8>(read_symbol(
			_tile_non_coeff_cdf.angle_delta.values[std::to_underlying(y_mode) - std::to_underlying(prediction_mode::v)]
		));
	}

	i8 symbol_decoder::read_angle_delta_uv(prediction_mode uv_mode) {
		return static_cast<i8>(read_symbol(
			_tile_non_coeff_cdf.angle_delta.values[std::to_underlying(uv_mode) - std::to_underlying(prediction_mode::v)]
		));
	}

	/// Returns the CDF array used by the given partition configuration.
	static std::span<cdf::value_t> _get_partition_cdf(cdf::non_coeff &f, cdf::context::partition p) {
		switch (p.bsl) {
		case 1:
			return f.partition_w8.value[p.ctx];
		case 2:
			return f.partition_w16.value[p.ctx];
		case 3:
			return f.partition_w32.value[p.ctx];
		case 4:
			return f.partition_w64.value[p.ctx];
		default:
			return f.partition_w128.value[p.ctx];
		}
	}
	partition symbol_decoder::read_partition(cdf::context::partition p) {
		return static_cast<partition>(read_symbol(_get_partition_cdf(_tile_non_coeff_cdf, p)));
	}

	bool symbol_decoder::read_split_or_horz(cdf::context::partition p) {
		const std::span<const cdf::value_t> pf = _get_partition_cdf(_tile_non_coeff_cdf, p);
		crash_if(pf.size() <= std::to_underlying(partition::vert_b));
		u16 psum =
			(pf[std::to_underlying(partition::vert)]   - pf[std::to_underlying(partition::vert)   - 1]) +
			(pf[std::to_underlying(partition::split)]  - pf[std::to_underlying(partition::split)  - 1]) +
			(pf[std::to_underlying(partition::horz_a)] - pf[std::to_underlying(partition::horz_a) - 1]) +
			(pf[std::to_underlying(partition::vert_a)] - pf[std::to_underlying(partition::vert_a) - 1]) +
			(pf[std::to_underlying(partition::vert_b)] - pf[std::to_underlying(partition::vert_b) - 1]);
		if (p.b_size != block_size::b128x128) {
			psum += pf[std::to_underlying(partition::vert_4)] - pf[std::to_underlying(partition::vert_4) - 1];
		}
		const cdf::value_t cdf[3] = { static_cast<cdf::value_t>((1u << 15) - psum), 1u << 15, 0 };
		return _read_symbol_no_update(cdf) != zero;
	}

	bool symbol_decoder::read_split_or_vert(cdf::context::partition p) {
		const std::span<const cdf::value_t> pf = _get_partition_cdf(_tile_non_coeff_cdf, p);
		crash_if(pf.size() <= std::to_underlying(partition::vert_a));
		u16 psum =
			(pf[std::to_underlying(partition::horz)]   - pf[std::to_underlying(partition::horz)   - 1]) +
			(pf[std::to_underlying(partition::split)]  - pf[std::to_underlying(partition::split)  - 1]) +
			(pf[std::to_underlying(partition::horz_a)] - pf[std::to_underlying(partition::horz_a) - 1]) +
			(pf[std::to_underlying(partition::horz_b)] - pf[std::to_underlying(partition::horz_b) - 1]) +
			(pf[std::to_underlying(partition::vert_a)] - pf[std::to_underlying(partition::vert_a) - 1]);
		if (p.b_size != block_size::b128x128) {
			psum += pf[std::to_underlying(partition::horz_4)] - pf[std::to_underlying(partition::horz_4) - 1];
		}
		const cdf::value_t cdf[3] = { static_cast<cdf::value_t>((1u << 15) - psum), 1u << 15, 0 };
		return _read_symbol_no_update(cdf) != zero;
	}

	u8 symbol_decoder::read_tx_depth(u32 max_tx_depth, u32 ctx) {
		const std::span<cdf::value_t> cdfs[3] = {
			_tile_non_coeff_cdf.tx_16x16.value[ctx],
			_tile_non_coeff_cdf.tx_32x32.value[ctx],
			_tile_non_coeff_cdf.tx_64x64.value[ctx],
		};
		const std::span<cdf::value_t> cdf =
			max_tx_depth >= 2 && max_tx_depth <= 4 ?
			cdfs[max_tx_depth - 2] :
			_tile_non_coeff_cdf.tx_8x8.value[ctx];
		return std::to_underlying(read_symbol(cdf));
	}

	bool symbol_decoder::read_txfm_split(u32 ctx) {
		return read_symbol(_tile_non_coeff_cdf.txfm_split.value[ctx]) != zero;
	}

	segment_id_t symbol_decoder::read_segment_id(u32 ctx) {
		return static_cast<segment_id_t>(read_symbol(_tile_non_coeff_cdf.segment_id.value[ctx]));
	}

	bool symbol_decoder::read_seg_id_predicted(const state::block &sb, const state::block_decoding &sbd) {
		const u32 ctx =
			(sb.left_seg_pred_context[sbd.mi_row]  ? 1 : 0) +
			(sb.above_seg_pred_context[sbd.mi_col] ? 1 : 0);
		return read_symbol(_tile_non_coeff_cdf.segment_id_predicted.value[ctx]) != zero;
	}

	bool symbol_decoder::read_new_mv(u32 new_mv_ctx) {
		return read_symbol(_tile_non_coeff_cdf.new_mv.value[new_mv_ctx]) != zero;
	}

	bool symbol_decoder::read_zero_mv(u32 zero_mv_ctx) {
		return read_symbol(_tile_non_coeff_cdf.zero_mv.value[zero_mv_ctx]) != zero;
	}

	bool symbol_decoder::read_ref_mv(u32 ref_mv_ctx) {
		return read_symbol(_tile_non_coeff_cdf.ref_mv.value[ref_mv_ctx]) != zero;
	}

	bool symbol_decoder::read_drl_mode(u32 ctx) {
		return read_symbol(_tile_non_coeff_cdf.drl_mode.value[ctx]) != zero;
	}

	bool symbol_decoder::read_is_inter(u32 ctx) {
		return read_symbol(_tile_non_coeff_cdf.is_inter.value[ctx]) != zero;
	}

	bool symbol_decoder::read_skip_mode(u32 ctx) {
		return read_symbol(_tile_non_coeff_cdf.skip_mode.value[ctx]) != zero;
	}

	bool symbol_decoder::read_use_filter_intra(block_size mi_size) {
		return read_symbol(_tile_non_coeff_cdf.filter_intra.value[std::to_underlying(mi_size)]) != zero;
	}

	intra_filtering symbol_decoder::read_filter_intra_mode() {
		return static_cast<intra_filtering>(read_symbol(_tile_non_coeff_cdf.filter_intra_mode.value));
	}

	bool symbol_decoder::read_skip(const state::block &sb, const state::block_decoding &bs) {
		return read_symbol(_tile_non_coeff_cdf.skip.value[cdf::context::compute_skip(sb, bs)]) != zero;
	}

	u8 symbol_decoder::read_compound_mode(const state::motion_vector &mv) {
		const u32 ref_mv_ctx = mv.ref_mv_context >> 1;
		const u32 new_mv_ctx = std::min(mv.new_mv_context, constants::comp_newmv_ctxs - 1);
		const u32 ctx = constants::compound_mode_ctx_map[ref_mv_ctx][new_mv_ctx];
		return std::to_underlying(read_symbol(_tile_non_coeff_cdf.compound_mode.value[ctx]));
	}

	interpolation symbol_decoder::read_interp_filter(u32 ctx) {
		return static_cast<interpolation>(read_symbol(_tile_non_coeff_cdf.interp_filter.value[ctx]));
	}

	mv_joint symbol_decoder::read_mv_joint(u32 mv_ctx) {
		return static_cast<mv_joint>(read_symbol(_tile_non_coeff_cdf.mv_joint[mv_ctx].value));
	}

	bool symbol_decoder::read_mv_sign(u32 mv_ctx, u32 comp) {
		return read_symbol(_tile_non_coeff_cdf.mv_sign[mv_ctx][comp].value) != zero;
	}

	u8 symbol_decoder::read_mv_class(u32 mv_ctx, u32 comp) {
		return std::to_underlying(read_symbol(_tile_non_coeff_cdf.mv_class[mv_ctx].value[comp]));
	}

	bool symbol_decoder::read_mv_class0_bit(u32 mv_ctx, u32 comp) {
		return read_symbol(_tile_non_coeff_cdf.mv_class0_bit[mv_ctx][comp].value) != zero;
	}

	u8 symbol_decoder::read_mv_class0_fr(u32 mv_ctx, u32 comp, bool mv_class0_bit) {
		return std::to_underlying(read_symbol(
			_tile_non_coeff_cdf.mv_class0_fr[mv_ctx].value[comp][mv_class0_bit ? 1 : 0]
		));
	}

	bool symbol_decoder::read_mv_class0_hp(u32 mv_ctx, u32 comp) {
		return read_symbol(_tile_non_coeff_cdf.mv_class0_hp[mv_ctx][comp].value) != zero;
	}

	u8 symbol_decoder::read_mv_fr(u32 mv_ctx, u32 comp) {
		return std::to_underlying(read_symbol(_tile_non_coeff_cdf.mv_fr[mv_ctx].value[comp]));
	}

	bool symbol_decoder::read_mv_hp(u32 mv_ctx, u32 comp) {
		return read_symbol(_tile_non_coeff_cdf.mv_hp[mv_ctx][comp].value) != zero;
	}

	bool symbol_decoder::read_mv_bit(u32 mv_ctx, u32 comp, u32 i) {
		return read_symbol(_tile_non_coeff_cdf.mv_bit[mv_ctx][comp].value[i]) != zero;
	}

	bool symbol_decoder::read_all_zero(u32 tx_sz_ctx, u32 ctx) {
		return read_symbol(_tile_coeff_cdf.txb_skip.value[tx_sz_ctx][ctx]) != zero;
	}

	u8 symbol_decoder::read_eob_pt_16(u32 ptype, u32 ctx) {
		return std::to_underlying(read_symbol(_tile_coeff_cdf.eob_pt_16.value[ptype][ctx]));
	}

	u8 symbol_decoder::read_eob_pt_32(u32 ptype, u32 ctx) {
		return std::to_underlying(read_symbol(_tile_coeff_cdf.eob_pt_32.value[ptype][ctx]));
	}

	u8 symbol_decoder::read_eob_pt_64(u32 ptype, u32 ctx) {
		return std::to_underlying(read_symbol(_tile_coeff_cdf.eob_pt_64.value[ptype][ctx]));
	}

	u8 symbol_decoder::read_eob_pt_128(u32 ptype, u32 ctx) {
		return std::to_underlying(read_symbol(_tile_coeff_cdf.eob_pt_128.value[ptype][ctx]));
	}

	u8 symbol_decoder::read_eob_pt_256(u32 ptype, u32 ctx) {
		return std::to_underlying(read_symbol(_tile_coeff_cdf.eob_pt_256.value[ptype][ctx]));
	}

	u8 symbol_decoder::read_eob_pt_512(u32 ptype) {
		return std::to_underlying(read_symbol(_tile_coeff_cdf.eob_pt_512.value[ptype]));
	}

	u8 symbol_decoder::read_eob_pt_1024(u32 ptype) {
		return std::to_underlying(read_symbol(_tile_coeff_cdf.eob_pt_1024.value[ptype]));
	}

	bool symbol_decoder::read_eob_extra(u32 tx_sz_ctx, u32 ptype, u32 eob_pt) {
		return read_symbol(_tile_coeff_cdf.eob_extra.value[tx_sz_ctx][ptype][eob_pt - 3]) != zero;
	}

	u8 symbol_decoder::read_coeff_base(u32 tx_sz_ctx, u32 ptype, u32 ctx) {
		return std::to_underlying(read_symbol(_tile_coeff_cdf.coeff_base.value[tx_sz_ctx][ptype][ctx]));
	}

	u8 symbol_decoder::read_coeff_base_eob(u32 tx_sz_ctx, u32 ptype, u32 ctx) {
		return std::to_underlying(read_symbol(_tile_coeff_cdf.coeff_base_eob.value[tx_sz_ctx][ptype][ctx]));
	}

	bool symbol_decoder::read_dc_sign(u32 ptype, u32 ctx) {
		return read_symbol(_tile_coeff_cdf.dc_sign.value[ptype][ctx]) != zero;
	}

	u8 symbol_decoder::read_coeff_br(u32 tx_sz_ctx, u32 ptype, u32 ctx) {
		tx_sz_ctx = std::min<u32>(tx_sz_ctx, std::to_underlying(tx_size::size_32x32));
		return std::to_underlying(read_symbol(_tile_coeff_cdf.coeff_br.value[tx_sz_ctx][ptype][ctx]));
	}

	bool symbol_decoder::read_has_palette_y(u32 bsize_ctx, u32 ctx) {
		return read_symbol(_tile_non_coeff_cdf.palette_y_mode.value[bsize_ctx][ctx]) != zero;
	}

	bool symbol_decoder::read_has_palette_uv(u32 palette_size_y) {
		const u32 ctx = palette_size_y > 0 ? 1 : 0;
		return read_symbol(_tile_non_coeff_cdf.palette_uv_mode.value[ctx]) != zero;
	}

	u8 symbol_decoder::read_palette_size_y_minus_2(u32 bsize_ctx) {
		return std::to_underlying(read_symbol(_tile_non_coeff_cdf.palette_y_size.value[bsize_ctx]));
	}

	u8 symbol_decoder::read_palette_size_uv_minus_2(u32 bsize_ctx) {
		return std::to_underlying(read_symbol(_tile_non_coeff_cdf.palette_uv_size.value[bsize_ctx]));
	}

	u8 symbol_decoder::read_palette_color_idx_y(u8 palette_size_y, u32 color_context_hash) {
		const u32 ctx = constants::palette_color_context[color_context_hash];
		const std::span<cdf::value_t> cdfs[9] = {
			_tile_non_coeff_cdf.palette_size_2_y_color.value[ctx],
			_tile_non_coeff_cdf.palette_size_3_y_color.value[ctx],
			_tile_non_coeff_cdf.palette_size_4_y_color.value[ctx],
			_tile_non_coeff_cdf.palette_size_5_y_color.value[ctx],
			_tile_non_coeff_cdf.palette_size_6_y_color.value[ctx],
			_tile_non_coeff_cdf.palette_size_7_y_color.value[ctx],
			_tile_non_coeff_cdf.palette_size_8_y_color.value[ctx],
		};
		crash_if(palette_size_y < 2 || palette_size_y > 8);
		return std::to_underlying(read_symbol(cdfs[palette_size_y - 2]));
	}

	u8 symbol_decoder::read_palette_color_idx_uv(u8 palette_size_uv, u32 color_context_hash) {
		const u32 ctx = constants::palette_color_context[color_context_hash];
		const std::span<cdf::value_t> cdfs[9] = {
			_tile_non_coeff_cdf.palette_size_2_uv_color.value[ctx],
			_tile_non_coeff_cdf.palette_size_3_uv_color.value[ctx],
			_tile_non_coeff_cdf.palette_size_4_uv_color.value[ctx],
			_tile_non_coeff_cdf.palette_size_5_uv_color.value[ctx],
			_tile_non_coeff_cdf.palette_size_6_uv_color.value[ctx],
			_tile_non_coeff_cdf.palette_size_7_uv_color.value[ctx],
			_tile_non_coeff_cdf.palette_size_8_uv_color.value[ctx],
		};
		crash_if(palette_size_uv < 2 || palette_size_uv > 8);
		return std::to_underlying(read_symbol(cdfs[palette_size_uv - 2]));
	}

	u8 symbol_decoder::read_delta_q_abs() {
		return std::to_underlying(read_symbol(_tile_non_coeff_cdf.delta_q.value));
	}

	u8 symbol_decoder::read_delta_lf_abs(bool delta_lf_multi, u32 i) {
		if (delta_lf_multi) {
			return std::to_underlying(read_symbol(_tile_non_coeff_cdf.delta_lf_multi[i].value));
		} else {
			return std::to_underlying(read_symbol(_tile_non_coeff_cdf.delta_lf.value));
		}
	}

	u8 symbol_decoder::read_intra_tx_type(tx_set set, tx_size tx_sz, prediction_mode intra_dir) {
		const u32 tx_sz_sqr = std::to_underlying(constants::get_tx_size_sqr(tx_sz));
		if (set == tx_set::intra_1) {
			return std::to_underlying(read_symbol(
				_tile_non_coeff_cdf.intra_tx_type_set1.value[tx_sz_sqr][std::to_underlying(intra_dir)]
			));
		} else if (set == tx_set::intra_2) {
			return std::to_underlying(read_symbol(
				_tile_non_coeff_cdf.intra_tx_type_set2.value[tx_sz_sqr][std::to_underlying(intra_dir)]
			));
		} else {
			std::abort();
		}
	}

	u8 symbol_decoder::read_inter_tx_type(tx_set set, tx_size tx_sz) {
		const u32 tx_sz_sqr = std::to_underlying(constants::get_tx_size_sqr(tx_sz));
		if (set == tx_set::inter_1) {
			return std::to_underlying(read_symbol(_tile_non_coeff_cdf.inter_tx_type_set1.value[tx_sz_sqr]));
		} else if (set == tx_set::inter_2) {
			return std::to_underlying(read_symbol(_tile_non_coeff_cdf.inter_tx_type_set2.value));
		} else if (set == tx_set::inter_3) {
			return std::to_underlying(read_symbol(_tile_non_coeff_cdf.inter_tx_type_set3.value[tx_sz_sqr]));
		} else {
			std::abort();
		}
	}

	bool symbol_decoder::read_interintra(block_size mi_size) {
		const u32 ctx = constants::size_group[std::to_underlying(mi_size)] - 1;
		return read_symbol(_tile_non_coeff_cdf.inter_intra.value[ctx]) != zero;
	}

	intra_prediction symbol_decoder::read_interintra_mode(block_size mi_size) {
		const u32 ctx = constants::size_group[std::to_underlying(mi_size)] - 1;
		return static_cast<intra_prediction>(read_symbol(_tile_non_coeff_cdf.inter_intra_mode.value[ctx]));
	}

	u8 symbol_decoder::read_wedge_index(block_size mi_size) {
		return std::to_underlying(read_symbol(_tile_non_coeff_cdf.wedge_index.value[std::to_underlying(mi_size)]));
	}

	bool symbol_decoder::read_wedge_interintra(block_size mi_size) {
		return read_symbol(_tile_non_coeff_cdf.wedge_inter_intra.value[std::to_underlying(mi_size)]) != zero;
	}

	u8 symbol_decoder::read_cfl_alpha_signs() {
		return std::to_underlying(read_symbol(_tile_non_coeff_cdf.cfl_sign.value));
	}

	u8 symbol_decoder::read_cfl_alpha_u(u8 cfl_alpha_signs) {
		const u32 ctx = cfl_alpha_signs - 2;
		return std::to_underlying(read_symbol(_tile_non_coeff_cdf.cfl_alpha.value[ctx]));
	}

	u8 symbol_decoder::read_cfl_alpha_v(u8 cfl_alpha_signs) {
		constexpr u32 _ctx_map[] =
			{ 0, 3, std::numeric_limits<u32>::max(), 1, 4, std::numeric_limits<u32>::max(), 2, 5 };
		const u32 ctx = _ctx_map[cfl_alpha_signs];
		return std::to_underlying(read_symbol(_tile_non_coeff_cdf.cfl_alpha.value[ctx]));
	}

	bool symbol_decoder::read_use_wiener() {
		return read_symbol(_tile_non_coeff_cdf.use_wiener.value) != zero;
	}

	bool symbol_decoder::read_use_sgrproj() {
		return read_symbol(_tile_non_coeff_cdf.use_sgrproj.value) != zero;
	}

	frame_restoration symbol_decoder::read_restoration_type() {
		return static_cast<frame_restoration>(read_symbol(_tile_non_coeff_cdf.restoration_type.value));
	}

	i32 symbol_decoder::decode_signed_subexp_with_ref_bool(i32 low, i32 high, u32 k, i32 r) {
		const u32 x = decode_unsigned_subexp_with_ref_bool(static_cast<u32>(high - low), k, r - low);
		return static_cast<i32>(x) + low;
	}

	u32 symbol_decoder::decode_unsigned_subexp_with_ref_bool(u32 mx, u32 k, i32 r) {
		const u32 v = decode_subexp_bool(mx, k);
		if ((r << 1) <= static_cast<i32>(mx)) {
			return static_cast<u32>(functions::inverse_recenter(r, v));
		} else {
			return static_cast<u32>(
				static_cast<i32>(mx) - 1 - functions::inverse_recenter(static_cast<i32>(mx) - 1 - r, v)
			);
		}
	}

	u32 symbol_decoder::decode_subexp_bool(u32 num_syms, u32 k) {
		u32 i = 0;
		u32 mk = 0;
		while (true) {
			const u32 b2 = i != 0 ? k + i - 1 : k;
			const u32 a = 1u << b2;
			if (num_syms <= mk + 3 * a) {
				const u32 subexp_unif_bools = read_ns(num_syms - mk);
				return subexp_unif_bools + mk;
			} else {
				const bool subexp_more_bools = read_bool();
				if (subexp_more_bools) {
					++i;
					mk += a;
				} else {
					const u32 subexp_bools = read_literal(b2);
					return subexp_bools + mk;
				}
			}
		}
	}

	symbol_decoder::symbol_decoder(
		reader &r,
		const cdf::non_coeff &tile_non_coeff_cdf,
		const cdf::coeff &tile_coeff_cdf,
		bool disable_cdf_update
	) :
		_tile_non_coeff_cdf(tile_non_coeff_cdf),
		_tile_coeff_cdf(tile_coeff_cdf),
		_disable_cdf_update(disable_cdf_update),
		_reader(r) {
	}

	symbol_t symbol_decoder::_read_symbol_no_update(std::span<const cdf::value_t> cdf) {
		const usize n = cdf.size() - 1;

		// decode symbol
		cdf::value_t prev;
		cdf::value_t cur = _symbol_range;
		u8 symbol = static_cast<u8>(-1);
		do {
			++symbol;
			prev = cur;
			const cdf::value_t f = (1u << 15) - cdf[symbol];
			cur = static_cast<cdf::value_t>(
				(static_cast<u32>(_symbol_range >> 8) * (f >> constants::ec_prob_shift)) >>
				(7 - constants::ec_prob_shift)
			);
			cur += constants::ec_min_prob * (n - symbol - 1);
		} while (cur > _symbol_value);

		// prepare for next symbol
		_symbol_range = prev - cur;
		_symbol_value = _symbol_value - cur;
		if (const u32 bits = 15 - functions::floor_log2(_symbol_range); bits > 0) {
			_symbol_range <<= bits;
			const auto num_bits = static_cast<u32>(std::min<i64>(bits, std::max(0LL, _symbol_max_bits)));
			const u32 new_data = _reader.read_bits(num_bits);
			const auto padded_data = static_cast<u16>(new_data << (bits - num_bits));
			_symbol_value = static_cast<cdf::value_t>(((_symbol_value + 1) << bits) - 1) ^ padded_data;
			_symbol_max_bits -= static_cast<i32>(bits);
		}
		return static_cast<symbol_t>(symbol);
	}

	void symbol_decoder::_update_cdf(std::span<cdf::value_t> cdf, symbol_t symbol) {
		const u32 n = static_cast<u32>(cdf.size() - 1);
		const u32 rate = 3 + (cdf[n] > 15 ? 1 : 0) + (cdf[n] > 31 ? 1 : 0) + std::min(functions::floor_log2(n), 2u);
		cdf::value_t tmp = 0;
		for (u32 i = 0; i < n - 1; ++i) {
			tmp = i == std::to_underlying(symbol) ? (1u << 15) : tmp;
			if (tmp < cdf[i]) {
				cdf[i] -= (cdf[i] - tmp) >> rate;
			} else {
				cdf[i] += (tmp - cdf[i]) >> rate;
			}
		}
		cdf[n] += cdf[n] < 32 ? 1 : 0;
	}
}
