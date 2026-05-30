#pragma once

/// \file
/// Common AV1 definitions.

#include "lotus/types.h"

namespace lotus::av1 {
	/// Symbol type.
	enum class symbol_t : u8 {
	};
	/// Segment ID type.
	enum class segment_id_t : u8 {
	};
	using channel_t = u16; ///< Channel/plane type.

	namespace constants {
		// 7
		constexpr u32 refs_per_frame       = 7;           ///< \p REFS_PER_FRAME.
		constexpr u32 total_refs_per_frame = 8;           ///< \p TOTAL_REFS_PER_FRAME.
		constexpr u32 block_size_groups    = 4;           ///< \p BLOCK_SIZE_GROUPS.
		constexpr u32 block_sizes          = 22;          ///< \p BLOCK_SIZES.
		constexpr u32 max_sb_size          = 128;         ///< \p MAX_SB_SIZE.
		constexpr u32 mi_size              = 4;           ///< \p MI_SIZE.
		constexpr u32 mi_size_log2         = 2;           ///< \p MI_SIZE_LOG2.
		constexpr u32 max_tile_width       = 4096;        ///< \p MAX_TILE_WIDTH.
		constexpr u32 max_tile_area        = 4096 * 2304; ///< \p MAX_TILE_AREA.
		constexpr u32 max_tile_rows        = 64;          ///< \p MAX_TILE_ROWS.
		constexpr u32 max_tile_cols        = 64;          ///< \p MAX_TILE_COLS.
		// 8
		constexpr u32 intrabc_delay_pixels   = 256; ///< \p INTRABC_DELAY_PIXELS.
		constexpr u32 intrabc_delay_sb64     = 4;   ///< \p INTRABC_DELAY_SB64.
		constexpr u32 num_ref_frames         = 8;   ///< \p NUM_REF_FRAMES.
		constexpr u32 is_inter_contexts      = 4;   ///< \p IS_INTER_CONTEXTS.
		constexpr u32 ref_contexts           = 3;   ///< \p REF_CONTEXTS.
		constexpr u32 max_segments           = 8;   ///< \p MAX_SEGMENTS.
		constexpr u32 segment_id_contexts    = 3;   ///< \p SEGMENT_ID_CONTEXTS.
		constexpr u32 plane_types            = 2;   ///< \p PLANE_TYPES.
		constexpr u32 tx_size_contexts       = 3;   ///< \p TX_SIZE_CONTEXTS.
		constexpr u32 interp_filters         = 3;   ///< \p INTERP_FILTERS.
		constexpr u32 interp_filter_contexts = 16;  ///< \p INTERP_FILTER_CONTEXTS.
		constexpr u32 skip_mode_contexts     = 3;   ///< \p SKIP_MODE_CONTEXTS.
		constexpr u32 skip_contexts          = 3;   ///< \p SKIP_CONTEXTS.
		constexpr u32 partition_contexts     = 4;   ///< \p PARTITION_CONTEXTS.
		constexpr u32 tx_sizes               = 5;   ///< \p TX_SIZES.
		constexpr u32 tx_sizes_all           = 19;  ///< \p TX_SIZES_ALL.
		// 9
		constexpr u32 tx_types    = 16; ///< \p TX_TYPES.
		constexpr u32 intra_modes = 13; ///< \p INTRA_MODES.
		// 10
		constexpr u32 uv_intra_modes_cfl_not_allowed = 13; ///< \p UV_INTRA_MODES_CFL_NOT_ALLOWED.
		constexpr u32 uv_intra_modes_cfl_allowed     = 14; ///< \p UV_INTRA_MODES_CFL_ALLOWED.
		constexpr u32 compound_modes                 = 8;  ///< \p COMPOUND_MODES.
		constexpr u32 compound_mode_contexts         = 8;  ///< \p COMPOUND_MODE_CONTEXTS.
		constexpr u32 comp_newmv_ctxs                = 5;  ///< \p COMP_NEWMV_CTXS.
		constexpr u32 new_mv_contexts                = 6;  ///< \p NEW_MV_CONTEXTS.
		constexpr u32 zero_mv_contexts               = 2;  ///< \p ZERO_MV_CONTEXTS.
		constexpr u32 ref_mv_contexts                = 6;  ///< \p REF_MV_CONTEXTS.
		constexpr u32 drl_mode_contexts              = 3;  ///< \p DRL_MODE_CONTEXTS.
		constexpr u32 mv_contexts                    = 2;  ///< \p MV_CONTEXTS.
		constexpr u32 mv_intrabc_context             = 1;  ///< \p MV_INTRABC_CONTEXT.
		constexpr u32 mv_joints                      = 4;  ///< \p MV_JOINTS.
		constexpr u32 mv_classes                     = 11; ///< \p MV_CLASSES.
		constexpr u32 class0_size                    = 2;  ///< \p CLASS0_SIZE.
		constexpr u32 mv_offset_bits                 = 10; ///< \p MV_OFFSET_BITS.
		constexpr u32 max_loop_filter                = 63; ///< \p MAX_LOOP_FILTER.
		constexpr u32 palette_color_contexts         = 5;  ///< \p PALETTE_COLOR_CONTEXTS.
		// 11
		constexpr u32 palette_max_color_context_hash = 8;    ///< \p PALETTE_MAX_COLOR_CONTEXT_HASH.
		constexpr u32 palette_block_size_contexts    = 7;    ///< \p PALETTE_BLOCK_SIZE_CONTEXTS.
		constexpr u32 palette_y_mode_contexts        = 3;    ///< \p PALETTE_Y_MODE_CONTEXTS.
		constexpr u32 palette_uv_mode_contexts       = 2;    ///< \p PALETTE_UV_MODE_CONTEXTS.
		constexpr u32 palette_sizes                  = 7;    ///< \p PALETTE_SIZES.
		constexpr u32 palette_colors                 = 8;    ///< \p PALETTE_COLORS.
		constexpr u32 palette_num_neighbors          = 3;    ///< \p PALETTE_NUM_NEIGHBORS.
		constexpr u32 delta_q_small                  = 3;    ///< \p DELTA_Q_SMALL.
		constexpr u32 delta_lf_small                 = 3;    ///< \p DELTA_LF_SMALL.
		constexpr u32 qm_total_size                  = 3344; ///< \p QM_TOTAL_SIZE.
		constexpr u32 max_angle_delta                = 3;    ///< \p MAX_ANGLE_DELTA.
		constexpr u32 directional_modes              = 8;    ///< \p DIRECTIONAL_MODES.
		constexpr u32 angle_step                     = 3;    ///< \p ANGLE_STEP.
		constexpr u32 tx_set_types_intra             = 3;    ///< \p TX_SET_TYPES_INTRA.
		constexpr u32 tx_set_types_inter             = 4;    ///< \p TX_SET_TYPES_INTER.
		constexpr u32 warpedmodel_prec_bits          = 16;   ///< \p WARPEDMODEL_PREC_BITS.
		constexpr u32 gm_abs_trans_bits              = 12;   ///< \p GM_ABS_TRANS_BITS.
		// 12
		constexpr u32 gm_abs_trans_only_bits         = 9;  ///< \p GM_ABS_TRANS_ONLY_BITS.
		constexpr u32 gm_abs_alpha_bits              = 12; ///< \p GM_ABS_ALPHA_BITS.
		constexpr u32 gm_alpha_prec_bits             = 15; ///< \p GM_ALPHA_PREC_BITS.
		constexpr u32 gm_trans_prec_bits             = 6;  ///< \p GM_TRANS_PREC_BITS.
		constexpr u32 gm_trans_only_prec_bits        = 3;  ///< \p GM_TRANS_ONLY_PREC_BITS.
		constexpr u32 interintra_modes               = 4;  ///< \p INTERINTRA_MODES.
		// 13
		constexpr u32 segment_id_predicted_contexts = 3;  ///< \p SEGMENT_ID_PREDICTED_CONTEXTS.
		constexpr u32 cfl_joint_signs               = 8;  ///< \p CFL_JOINT_SIGNS.
		constexpr u32 cfl_alphabet_size             = 16; ///< \p CFL_ALPHABET_SIZE.
		constexpr u32 cfl_alpha_contexts            = 6;  ///< \p CFL_ALPHA_CONTEXTS.
		constexpr u32 intra_mode_contexts           = 5;  ///< \p INTRA_MODE_CONTEXTS.
		constexpr u32 intra_edge_kernels            = 3;  ///< \p INTRA_EDGE_KERNELS.
		constexpr u32 intra_edge_taps               = 5;  ///< \p INTRA_EDGE_TAPS.
		constexpr u32 frame_lf_count                = 4;  ///< \p FRAME_LF_COUNT.
		constexpr u32 max_vartx_depth               = 2;  ///< \p MAX_VARTX_DEPTH.
		constexpr u32 txfm_partition_contexts       = 21; ///< \p TXFM_PARTITION_CONTEXTS.
		constexpr u32 max_ref_mv_stack_size         = 8;  ///< \p MAX_REF_MV_STACK_SIZE.
		// 14
		constexpr u32 max_tx_depth                = 2;   ///< \p MAX_TX_DEPTH.
		constexpr u32 wiener_coeffs               = 3;   ///< \p WIENER_COEFFS.
		constexpr u32 sgrproj_params_bits         = 4;   ///< \p SGRPROJ_PARAMS_BITS.
		constexpr u32 sgrproj_prj_subexp_k        = 4;   ///< \p SGRPROJ_PRJ_SUBEXP_K.
		constexpr u32 sgrproj_prj_bits            = 7;   ///< \p SGRPROJ_PRJ_BITS.
		constexpr u32 ec_prob_shift               = 6;   ///< \p EC_PROB_SHIFT.
		constexpr u32 ec_min_prob                 = 4;   ///< \p EC_MIN_PROB.
		constexpr u32 select_screen_content_tools = 2;   ///< \p SELECT_SCREEN_CONTENT_TOOLS.
		constexpr u32 select_integer_mv           = 2;   ///< \p SELECT_INTEGER_MV.
		constexpr u32 restoration_tilesize_max    = 256; ///< \p RESTORATION_TILESIZE_MAX.
		// 15
		constexpr u32 num_base_levels         = 2;           ///< \p NUM_BASE_LEVELS.
		constexpr u32 coeff_base_range        = 12;          ///< \p COEFF_BASE_RANGE.
		constexpr u32 br_cdf_size             = 4;           ///< \p BR_CDF_SIZE.
		constexpr u32 sig_coef_contexts_eob   = 4;           ///< \p SIG_COEF_CONTEXTS_EOB.
		constexpr u32 sig_coef_contexts_2d    = 26;          ///< \p SIG_COEF_CONTEXTS_2D.
		constexpr u32 sig_coef_contexts       = 42;          ///< \p SIG_COEF_CONTEXTS.
		constexpr u32 sig_ref_diff_offset_num = 5;           ///< \p SIG_REF_DIFF_OFFSET_NUM.
		constexpr u32 superres_num            = 8;           ///< \p SUPERRES_NUM.
		constexpr u32 superres_denom_min      = 9;           ///< \p SUPERRES_DENOM_MIN.
		constexpr u32 superres_denom_bits     = 3;           ///< \p SUPERRES_DENOM_BITS.
		constexpr u32 txb_skip_contexts       = 13;          ///< \p TXB_SKIP_CONTEXTS.
		constexpr u32 eob_coef_contexts       = 9;           ///< \p EOB_COEF_CONTEXTS.
		constexpr u32 dc_sign_contexts        = 3;           ///< \p DC_SIGN_CONTEXTS.
		constexpr u32 level_contexts          = 21;          ///< \p LEVEL_CONTEXTS.
		// 16
		constexpr u32 intra_filter_scale_bits = 4;           ///< \p INTRA_FILTER_SCALE_BITS.
		constexpr u32 intra_filter_modes      = 5;           ///< \p INTRA_FILTER_MODES.
		constexpr u32 coeff_cdf_q_ctxs        = 4;           ///< \p COEFF_CDF_Q_CTXS.
		constexpr u32 primary_ref_none        = 7;           ///< \p PRIMARY_REF_NONE.

		// 302
		constexpr i32 sinpi_1_9 = 1321; ///< \p SINPI_1_9.
		constexpr i32 sinpi_2_9 = 2482; ///< \p SINPI_2_9.
		constexpr i32 sinpi_3_9 = 3344; ///< \p SINPI_3_9.
		constexpr i32 sinpi_4_9 = 3803; ///< \p SINPI_4_9.

		constexpr u32 max_num_operating_points = 32; ///< Maximum number of operating points.
	}
}
