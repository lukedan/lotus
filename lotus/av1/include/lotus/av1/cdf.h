#pragma once

/// \file
/// CDF definitions.

#include <utility>

#include "lotus/types.h"

#include "lotus/av1/common.h"
#include "lotus/av1/enums.h"

namespace lotus::av1::cdf {
	using value_t = u16; ///< Value type.

	/// Type for \p TileIntraFrameYModeCdf.
	struct intra_frame_y_mode_t {
		/// Value.
		value_t value[constants::intra_mode_contexts][constants::intra_mode_contexts][constants::intra_modes + 1];
	};
	/// Type for \p YModeCdf.
	struct y_mode_t {
		value_t value[constants::block_size_groups][constants::intra_modes + 1]; ///< Value.
	};
	/// Type for \p UVModeCflNotAllowedCdf.
	struct uv_mode_cfl_not_allowed_t {
		value_t value[constants::intra_modes][constants::uv_intra_modes_cfl_not_allowed + 1]; ///< Value.
	};
	/// Type for \p UVModeCflAllowedCdf.
	struct uv_mode_cfl_allowed_t {
		value_t value[constants::intra_modes][constants::uv_intra_modes_cfl_allowed + 1]; ///< Value.
	};
	/// Type for \p AngleDeltaCdf.
	struct angle_delta_t {
		value_t values[constants::directional_modes][2 * constants::max_angle_delta + 2]; ///< Value.
	};
	/// Type for \p IntrabcCdf.
	struct intrabc_t {
		value_t value[3]; ///< Value.
	};
	/// Type for \p PartitionW8Cdf.
	struct partition_w8_t {
		value_t value[constants::partition_contexts][5]; ///< Value.
	};
	/// Type for \p PartitionW16Cdf.
	struct partition_w16_t {
		value_t value[constants::partition_contexts][11]; ///< Value.
	};
	/// Type for \p PartitionW32Cdf.
	struct partition_w32_t {
		value_t value[constants::partition_contexts][11]; ///< Value.
	};
	/// Type for \p PartitionW64Cdf.
	struct partition_w64_t {
		value_t value[constants::partition_contexts][11]; ///< Value.
	};
	/// Type for \p PartitionW128Cdf.
	struct partition_w128_t {
		value_t value[constants::partition_contexts][9]; ///< Value.
	};
	/// Type for \p Tx8x8Cdf.
	struct tx_8x8_t {
		value_t value[constants::tx_size_contexts][constants::max_tx_depth + 1]; ///< Value.
	};
	/// Type for \p Tx16x16Cdf, \p Tx32x32Cdf, and \p Tx64x64Cdf.
	struct tx_depth_t {
		value_t value[constants::tx_size_contexts][constants::max_tx_depth + 2]; ///< Value.
	};
	/// Type for \p TxfmSplitCdf.
	struct txfm_split_t {
		value_t value[constants::txfm_partition_contexts][3]; ///< Value.
	};
	/// Type for \p FilterIntraModeCdf.
	struct filter_intra_mode_t {
		value_t value[6]; ///< Value.
	};
	/// Type for \p FilterIntraCdf.
	struct filter_intra_t {
		value_t value[constants::block_sizes][3]; ///< Value.
	};
	/// Type for \p SegmentIdCdf.
	struct segment_id_t {
		value_t value[constants::segment_id_contexts][constants::max_segments + 1]; ///< Value.
	};
	/// Type for \p SegmentIdPredictedCdf.
	struct segment_id_predicted_t {
		value_t value[constants::segment_id_predicted_contexts][3]; ///< Value.
	};
	/// Type for \p MvClass0HpCdf.
	struct mv_class0_hp_t {
		value_t value[3]; ///< Value.
	};
	/// Type for \p MvHpCdf.
	struct mv_hp_t {
		value_t value[3]; ///< Value.
	};
	/// Type for \p MvSignCdf.
	struct mv_sign_t {
		value_t value[3]; ///< Value.
	};
	/// Type for \p MvBitCdf.
	struct mv_bit_t {
		value_t value[constants::mv_offset_bits][3]; ///< Value.
	};
	/// Type for \p MvClass0BitCdf.
	struct mv_class0_bit_t {
		value_t value[3]; ///< Value.
	};
	/// Type for \p NewMvCdf.
	struct new_mv_t {
		value_t value[constants::new_mv_contexts][3]; ///< Value.
	};
	/// Type for \p ZeroMvCdf.
	struct zero_mv_t {
		value_t value[constants::zero_mv_contexts][3]; ///< Value.
	};
	/// Type for \p RefMvCdf.
	struct ref_mv_t {
		value_t value[constants::ref_mv_contexts][3]; ///< Value.
	};
	/// Type for \p DrlModeCdf.
	struct drl_mode_t {
		value_t value[constants::drl_mode_contexts][3]; ///< Value.
	};
	/// Type for \p IsInterCdf.
	struct is_inter_t {
		value_t value[constants::is_inter_contexts][3]; ///< Value.
	};
	/// Type for \p SkipModeCdf.
	struct skip_mode_t {
		value_t value[constants::skip_mode_contexts][3]; ///< Value.
	};
	/// Type for \p SkipCdf.
	struct skip_t {
		value_t value[constants::skip_contexts][3]; ///< Value.
	};
	/// Type for \p CompoundModeCdf.
	struct compound_mode_t {
		value_t value[constants::compound_mode_contexts][constants::compound_modes + 1]; ///< Value.
	};
	/// Type for \p InterpFilterCdf.
	struct interp_filter_t {
		value_t value[constants::interp_filter_contexts][constants::interp_filters + 1]; ///< Value.
	};
	/// Type for \p MvJointCdf.
	struct mv_joint_t {
		value_t value[constants::mv_joints + 1]; ///< Value.
	};
	/// Type for \p MvClassCdf.
	struct mv_class_t {
		value_t value[2][constants::mv_classes + 1]; ///< Value.
	};
	/// Type for \p MvClass0FrCdf.
	struct mv_class0_fr_t {
		value_t value[2][constants::class0_size][constants::mv_joints + 1]; ///< Value.
	};
	/// Type for \p MvFrCdf.
	struct mv_fr_t {
		value_t value[2][constants::mv_joints + 1]; ///< Value.
	};
	/// Type for \p PaletteYSizeCdf.
	struct palette_y_size_t {
		value_t value[constants::palette_block_size_contexts][constants::palette_sizes + 1]; ///< Value.
	};
	/// Type for \p PaletteUVSizeCdf.
	struct palette_uv_size_t {
		value_t value[constants::palette_block_size_contexts][constants::palette_sizes + 1]; ///< Value.
	};
	/// Type for \p PaletteSize2YColorCdf.
	struct palette_size_2_y_color_t {
		value_t value[constants::palette_color_contexts][3]; ///< Value.
	};
	/// Type for \p PaletteSize3YColorCdf.
	struct palette_size_3_y_color_t {
		value_t value[constants::palette_color_contexts][4]; ///< Value.
	};
	/// Type for \p PaletteSize4YColorCdf.
	struct palette_size_4_y_color_t {
		value_t value[constants::palette_color_contexts][5]; ///< Value.
	};
	/// Type for \p PaletteSize5YColorCdf.
	struct palette_size_5_y_color_t {
		value_t value[constants::palette_color_contexts][6]; ///< Value.
	};
	/// Type for \p PaletteSize6YColorCdf.
	struct palette_size_6_y_color_t {
		value_t value[constants::palette_color_contexts][7]; ///< Value.
	};
	/// Type for \p PaletteSize7YColorCdf.
	struct palette_size_7_y_color_t {
		value_t value[constants::palette_color_contexts][8]; ///< Value.
	};
	/// Type for \p PaletteSize8YColorCdf.
	struct palette_size_8_y_color_t {
		value_t value[constants::palette_color_contexts][9]; ///< Value.
	};
	/// Type for \p PaletteSize2UVColorCdf.
	struct palette_size_2_uv_color_t {
		value_t value[constants::palette_color_contexts][3]; ///< Value.
	};
	/// Type for \p PaletteSize3UVColorCdf.
	struct palette_size_3_uv_color_t {
		value_t value[constants::palette_color_contexts][4]; ///< Value.
	};
	/// Type for \p PaletteSize4UVColorCdf.
	struct palette_size_4_uv_color_t {
		value_t value[constants::palette_color_contexts][5]; ///< Value.
	};
	/// Type for \p PaletteSize5UVColorCdf.
	struct palette_size_5_uv_color_t {
		value_t value[constants::palette_color_contexts][6]; ///< Value.
	};
	/// Type for \p PaletteSize6UVColorCdf.
	struct palette_size_6_uv_color_t {
		value_t value[constants::palette_color_contexts][7]; ///< Value.
	};
	/// Type for \p PaletteSize7UVColorCdf.
	struct palette_size_7_uv_color_t {
		value_t value[constants::palette_color_contexts][8]; ///< Value.
	};
	/// Type for \p PaletteSize8UVColorCdf.
	struct palette_size_8_uv_color_t {
		value_t value[constants::palette_color_contexts][9]; ///< Value.
	};
	/// Type for \p PaletteYModeCdf.
	struct palette_y_mode_t {
		value_t value[constants::palette_block_size_contexts][constants::palette_y_mode_contexts][3]; ///< Value.
	};
	/// Type for \p PaletteUVModeCdf.
	struct palette_uv_mode_t {
		value_t value[constants::palette_uv_mode_contexts][3]; ///< Value.
	};
	/// Type for \p DeltaQCdf.
	struct delta_q_t {
		value_t value[constants::delta_q_small + 2]; ///< Value.
	};
	/// Type for \p DeltaLfCdf.
	struct delta_lf_t {
		value_t value[constants::delta_lf_small + 2]; ///< Value.
	};
	/// Type for \p IntraTxTypeSet1Cdf.
	struct intra_tx_type_set1_t {
		value_t value[2][constants::intra_modes][8]; ///< Value.
	};
	/// Type for \p IntraTxTypeSet2Cdf.
	struct intra_tx_type_set2_t {
		value_t value[3][constants::intra_modes][6]; ///< Value.
	};
	/// Type for \p InterTxTypeSet1Cdf.
	struct inter_tx_type_set1_t {
		value_t value[2][17]; ///< Value.
	};
	/// Type for \p InterTxTypeSet2Cdf.
	struct inter_tx_type_set2_t {
		value_t value[13]; ///< Value.
	};
	/// Type for \p InterTxTypeSet3Cdf.
	struct inter_tx_type_set3_t {
		value_t value[4][3]; ///< Value.
	};
	/// Type for \p InterIntraCdf.
	struct inter_intra_t {
		value_t value[constants::block_size_groups - 1][3]; ///< Value.
	};
	/// Type for \p InterIntraModeCdf.
	struct inter_intra_mode_t {
		value_t value[constants::block_size_groups - 1][constants::interintra_modes + 1]; ///< Value.
	};
	/// Type for \p WedgeIndexCdf.
	struct wedge_index_t {
		value_t value[constants::block_sizes][16 + 1]; ///< Value.
	};
	/// Type for \p WedgeInterIntraCdf.
	struct wedge_inter_intra_t {
		value_t value[constants::block_sizes][3]; ///< Value.
	};
	/// Type for \p CflSignCdf.
	struct cfl_sign_t {
		value_t value[constants::cfl_joint_signs + 1]; ///< Value.
	};
	/// Type for \p CflAlphaCdf.
	struct cfl_alpha_t {
		value_t value[constants::cfl_alpha_contexts][constants::cfl_alphabet_size + 1]; ///< Value.
	};
	/// Type for \p UseWienerCdf.
	struct use_wiener_t {
		value_t value[2 + 1]; ///< Value.
	};
	/// Type for \p UseSgrprojCdf.
	struct use_sgrproj_t {
		value_t value[2 + 1]; ///< Value.
	};
	/// Type for \p RestorationTypeCdf.
	struct restoration_type_t {
		value_t value[std::to_underlying(frame_restoration::switchable) + 1]; ///< Value.
	};

	/// Type for \p TxbSkipCdf.
	struct txb_skip_t {
		value_t value[constants::tx_sizes][constants::txb_skip_contexts][3]; ///< Value.
	};
	/// Type for \p EobPt16Cdf.
	struct eob_pt_16_t {
		value_t value[constants::plane_types][2][6]; ///< Value.
	};
	/// Type for \p EobPt32Cdf.
	struct eob_pt_32_t {
		value_t value[constants::plane_types][2][7]; ///< Value.
	};
	/// Type for \p EobPt64Cdf.
	struct eob_pt_64_t {
		value_t value[constants::plane_types][2][8]; ///< Value.
	};
	/// Type for \p EobPt128Cdf.
	struct eob_pt_128_t {
		value_t value[constants::plane_types][2][9]; ///< Value.
	};
	/// Type for \p EobPt256Cdf.
	struct eob_pt_256_t {
		value_t value[constants::plane_types][2][10]; ///< Value.
	};
	/// Type for \p EobPt512Cdf.
	struct eob_pt_512_t {
		value_t value[constants::plane_types][11]; ///< Value.
	};
	/// Type for \p EobPt1024Cdf.
	struct eob_pt_1024_t {
		value_t value[constants::plane_types][12]; ///< Value.
	};
	/// Type for \p EobExtraCdf.
	struct eob_extra_t {
		value_t value[constants::tx_sizes][constants::plane_types][constants::eob_coef_contexts][3]; ///< Value.
	};
	/// Type for \p DcSignCdf.
	struct dc_sign_t {
		value_t value[constants::plane_types][constants::dc_sign_contexts][3]; ///< Value.
	};
	/// Type for \p CoeffBaseEob.
	struct coeff_base_eob_t {
		value_t value[constants::tx_sizes][constants::plane_types][constants::sig_coef_contexts_eob][4]; ///< Value.
	};
	/// Type for \p CoeffBaseCdf.
	struct coeff_base_t {
		value_t value[constants::tx_sizes][constants::plane_types][constants::sig_coef_contexts][5]; ///< Value.
	};
	/// Type for \p CoeffBrCdf.
	struct coeff_br_t {
		/// Value.
		value_t value[constants::tx_sizes][constants::plane_types][constants::level_contexts][constants::br_cdf_size + 1];
	};

	/// Initial CDF values.
	namespace defaults {
		// 416
		extern const intra_frame_y_mode_t intra_frame_y_mode; ///< \p Default_Intra_Frame_Y_Mode_Cdf.
		// 417
		extern const y_mode_t y_mode; ///< \p Default_Y_Mode_Cdf.
		// 418
		extern const uv_mode_cfl_not_allowed_t uv_mode_cfl_not_allowed; ///< \p Default_Uv_Mode_Cfl_Not_Allowed_Cdf.
		// 419
		extern const uv_mode_cfl_allowed_t uv_mode_cfl_allowed; ///< \p Default_Uv_Mode_Cfl_Allowed_Cdf.
		extern const angle_delta_t angle_delta; ///< \p Default_Angle_Delta_Cdf.
		extern const intrabc_t intrabc; ///< \p Default_Intrabc_Cdf.
		// 420
		extern const partition_w8_t partition_w8; ///< \p Default_Partition_W8_Cdf.
		extern const partition_w16_t partition_w16; ///< \p Default_Partition_W16_Cdf.
		extern const partition_w32_t partition_w32; ///< \p Default_Partition_W32_Cdf.
		extern const partition_w64_t partition_w64; ///< \p Default_Partition_W64_Cdf.
		extern const partition_w128_t partition_w128; ///< \p Default_Partition_W128_Cdf.
		extern const tx_8x8_t tx_8x8; ///< \p Default_Tx_8x8_Cdf.
		// 421
		extern const tx_depth_t tx_16x16; ///< \p Default_Tx_16x16_Cdf.
		extern const tx_depth_t tx_32x32; ///< \p Default_Tx_32x32_Cdf.
		extern const tx_depth_t tx_64x64; ///< \p Default_Tx_64x64_Cdf.
		extern const txfm_split_t txfm_split; ///< \p Default_Txfm_Split_Cdf.
		extern const filter_intra_mode_t filter_intra_mode; ///< \p Default_Filter_Intra_Mode_Cdf.
		extern const filter_intra_t filter_intra; ///< \p Default_Filter_Intra_Cdf.
		// 422
		extern const segment_id_t segment_id; ///< \p Default_Segment_Id_Cdf.
		extern const segment_id_predicted_t segment_id_predicted; ///< \p Default_Segment_Id_Predicted_Cdf.
		extern const mv_class0_hp_t mv_class0_hp; ///< \p Default_Mv_Class0_Hp_Cdf.
		extern const mv_hp_t mv_hp; ///< \p Default_Mv_Hp_Cdf.
		extern const mv_sign_t mv_sign; ///< \p Default_Mv_Sign_Cdf.
		extern const mv_bit_t mv_bit; ///< \p Default_Mv_Bit_Cdf.
		// 423
		extern const mv_class0_bit_t mv_class0_bit; ///< \p Default_Mv_Class0_Bit_Cdf.
		extern const new_mv_t new_mv; ///< \p Default_New_Mv_Cdf.
		extern const zero_mv_t zero_mv; ///< \p Default_Zero_Mv_Cdf.
		extern const ref_mv_t ref_mv; ///< \p Default_Ref_Mv_Cdf.
		extern const drl_mode_t drl_mode; ///< \p Default_Drl_Mode_Cdf.
		extern const is_inter_t is_inter; ///< \p Default_Is_Inter_Cdf.
		// 424
		extern const skip_mode_t skip_mode; ///< \p Default_Skip_Mode_Cdf.
		extern const skip_t skip; ///< \p Default_Skip_Cdf.
		// 425
		extern const compound_mode_t compound_mode; ///< \p Default_Compound_Mode_Cdf.
		extern const interp_filter_t interp_filter; ///< \p Default_Interp_Filter_Cdf.
		// 426
		extern const mv_joint_t mv_joint; ///< \p Default_Mv_Joint_Cdf.
		extern const mv_class_t mv_class; ///< \p Default_Mv_Class_Cdf.
		// 427
		extern const mv_class0_fr_t mv_class0_fr; ///< \p Default_Mv_Class0_Fr_Cdf.
		extern const mv_fr_t mv_fr; ///< \p Default_Mv_Fr_Cdf.
		extern const palette_y_size_t palette_y_size; ///< \p Default_Palette_Y_Size_Cdf.
		extern const palette_uv_size_t palette_uv_size; ///< \p Default_Palette_Uv_Size_Cdf.
		// 428
		extern const palette_size_2_y_color_t palette_size_2_y_color; ///< \p Default_Palette_Size_2_Y_Color_Cdf.
		extern const palette_size_3_y_color_t palette_size_3_y_color; ///< \p Default_Palette_Size_3_Y_Color_Cdf.
		extern const palette_size_4_y_color_t palette_size_4_y_color; ///< \p Default_Palette_Size_4_Y_Color_Cdf.
		extern const palette_size_5_y_color_t palette_size_5_y_color; ///< \p Default_Palette_Size_5_Y_Color_Cdf.
		extern const palette_size_6_y_color_t palette_size_6_y_color; ///< \p Default_Palette_Size_6_Y_Color_Cdf.
		extern const palette_size_7_y_color_t palette_size_7_y_color; ///< \p Default_Palette_Size_7_Y_Color_Cdf.
		extern const palette_size_8_y_color_t palette_size_8_y_color; ///< \p Default_Palette_Size_8_Y_Color_Cdf.
		// 430
		extern const palette_size_2_uv_color_t palette_size_2_uv_color; ///< \p Default_Palette_Size_2_Uv_Color_Cdf.
		extern const palette_size_3_uv_color_t palette_size_3_uv_color; ///< \p Default_Palette_Size_3_Uv_Color_Cdf.
		extern const palette_size_4_uv_color_t palette_size_4_uv_color; ///< \p Default_Palette_Size_4_Uv_Color_Cdf.
		extern const palette_size_5_uv_color_t palette_size_5_uv_color; ///< \p Default_Palette_Size_5_Uv_Color_Cdf.
		extern const palette_size_6_uv_color_t palette_size_6_uv_color; ///< \p Default_Palette_Size_6_Uv_Color_Cdf.
		extern const palette_size_7_uv_color_t palette_size_7_uv_color; ///< \p Default_Palette_Size_7_Uv_Color_Cdf.
		extern const palette_size_8_uv_color_t palette_size_8_uv_color; ///< \p Default_Palette_Size_8_Uv_Color_Cdf.
		// 431
		extern const palette_y_mode_t palette_y_mode; ///< \p Default_Palette_Y_Mode_Cdf.
		extern const palette_uv_mode_t palette_uv_mode; ///< \p Default_Palette_Uv_Mode_Cdf.
		extern const delta_q_t delta_q; ///< \p Default_Delta_Q_Cdf.
		extern const delta_lf_t delta_lf; ///< \p Default_Delta_Lf_Cdf.
		// 432
		extern const intra_tx_type_set1_t intra_tx_type_set1; ///< \p Default_Intra_Tx_Type_Set1_Cdf.
		// 433
		extern const intra_tx_type_set2_t intra_tx_type_set2; ///< \p Default_Intra_Tx_Type_Set2_Cdf.
		// 434
		extern const inter_tx_type_set1_t inter_tx_type_set1; ///< \p Default_Inter_Tx_Type_Set1_Cdf.
		extern const inter_tx_type_set2_t inter_tx_type_set2; ///< \p Default_Inter_Tx_Type_Set2_Cdf.
		extern const inter_tx_type_set3_t inter_tx_type_set3; ///< \p Default_Inter_Tx_Type_Set3_Cdf.
		// 435
		extern const inter_intra_t inter_intra; ///< \p Default_Inter_Intra_Cdf.
		extern const inter_intra_mode_t inter_intra_mode; ///< \p Default_Inter_Intra_Mode_Cdf.
		// 436
		extern const wedge_index_t wedge_index; ///< \p Default_Wedge_Index_Cdf.
		// 437
		extern const wedge_inter_intra_t wedge_inter_intra; ///< \p Default_Wedge_Inter_Intra_Cdf.
		// 439
		extern const cfl_sign_t cfl_sign; ///< \p Default_Cfl_Sign_Cdf.
		extern const cfl_alpha_t cfl_alpha; ///< \p Default_Cfl_Alpha_Cdf.
		extern const use_wiener_t use_wiener; ///< \p Default_Use_Wiener_Cdf.
		extern const use_sgrproj_t use_sgrproj; ///< \p Default_Use_Sgrproj_Cdf.
		extern const restoration_type_t restoration_type; ///< \p Default_Restoration_Type_Cdf.

		// 440
		extern const txb_skip_t txb_skip[constants::coeff_cdf_q_ctxs]; ///< \p Default_Txb_Skip_Cdf.
		// 445
		extern const eob_pt_16_t eob_pt_16[constants::coeff_cdf_q_ctxs]; ///< \p Default_Eob_Pt_16_Cdf.
		// 446
		extern const eob_pt_32_t eob_pt_32[constants::coeff_cdf_q_ctxs]; ///< \p Default_Eob_Pt_32_Cdf.
		extern const eob_pt_64_t eob_pt_64[constants::coeff_cdf_q_ctxs]; ///< \p Default_Eob_Pt_64_Cdf.
		// 447
		extern const eob_pt_128_t eob_pt_128[constants::coeff_cdf_q_ctxs]; ///< \p Default_Eob_Pt_128_Cdf.
		extern const eob_pt_256_t eob_pt_256[constants::coeff_cdf_q_ctxs]; ///< \p Default_Eob_Pt_256_Cdf.
		// 448
		extern const eob_pt_512_t eob_pt_512[constants::coeff_cdf_q_ctxs]; ///< \p Default_Eob_Pt_512_Cdf.
		extern const eob_pt_1024_t eob_pt_1024[constants::coeff_cdf_q_ctxs]; ///< \p Default_Eob_Pt_1024_Cdf.
		// 449
		extern const eob_extra_t eob_extra[constants::coeff_cdf_q_ctxs]; ///< \p Default_Eob_Extra_Cdf.
		// 453
		extern const dc_sign_t dc_sign[constants::coeff_cdf_q_ctxs]; ///< \p Default_Dc_Sign_Cdf.
		// 454
		extern const coeff_base_eob_t coeff_base_eob[constants::coeff_cdf_q_ctxs]; ///< \p Default_Coeff_Base_Eob_Cdf.
		// 458
		extern const coeff_base_t coeff_base[constants::coeff_cdf_q_ctxs]; ///< \p Default_Coeff_Base_Cdf.
		// 492
		extern const coeff_br_t coeff_br[constants::coeff_cdf_q_ctxs]; ///< \p Default_Coeff_Br_Cdf.
	}

	/// CDF tables not used in the \p coeff() syntax structure.
	struct non_coeff {
		// 155
		y_mode_t y_mode; ///< \p YModeCdf.
		uv_mode_cfl_not_allowed_t uv_mode_cfl_not_allowed; ///< \p UVModeCflNotAllowedCdf.
		uv_mode_cfl_allowed_t uv_mode_cfl_allowed; ///< \p UVModeCflAllowedCdf.
		angle_delta_t angle_delta; ///< \p AngleDeltaCdf.
		intrabc_t intrabc; ///< \p IntrabcCdf.
		partition_w8_t partition_w8; ///< \p PartitionW8Cdf.
		partition_w16_t partition_w16; ///< \p PartitionW16Cdf.
		partition_w32_t partition_w32; ///< \p PartitionW32Cdf.
		partition_w64_t partition_w64; ///< \p PartitionW64Cdf.
		partition_w128_t partition_w128; ///< \p PartitionW128Cdf.
		segment_id_t segment_id; ///< \p SegmentIdCdf.
		// 156
		segment_id_predicted_t segment_id_predicted; ///< \p SegmentIdPredictedCdf.
		tx_8x8_t tx_8x8; ///< \p Tx8x8Cdf.
		tx_depth_t tx_16x16; ///< \p Tx16x16Cdf.
		tx_depth_t tx_32x32; ///< \p Tx32x32Cdf.
		tx_depth_t tx_64x64; ///< \p Tx64x64Cdf.
		txfm_split_t txfm_split; ///< \p TxfmSplitCdf.
		filter_intra_mode_t filter_intra_mode; ///< \p FilterIntraModeCdf.
		filter_intra_t filter_intra; ///< \p FilterIntraCdf.
		interp_filter_t interp_filter; ///< \p InterpFilterCdf.
		new_mv_t new_mv; ///< \p NewMvCdf.
		zero_mv_t zero_mv; ///< \p ZeroMvCdf.
		ref_mv_t ref_mv; ///< \p RefMvCdf.
		compound_mode_t compound_mode; ///< \p CompoundModeCdf.
		drl_mode_t drl_mode; ///< \p DrlModeCdf.
		is_inter_t is_inter; ///< \p IsInterCdf.
		skip_mode_t skip_mode; ///< \p SkipModeCdf.
		skip_t skip; ///< \p SkipCdf.
		mv_joint_t mv_joint[constants::mv_contexts]; ///< \p MvJointCdf.
		mv_class_t mv_class[constants::mv_contexts]; ///< \p MvClassCdf.
		mv_class0_bit_t mv_class0_bit[constants::mv_contexts][2]; ///< \p MvClass0BitCdf.
		// 157
		mv_fr_t mv_fr[constants::mv_contexts]; ///< \p MvFrCdf.
		mv_class0_fr_t mv_class0_fr[constants::mv_contexts]; ///< \p MvClass0FrCdf.
		mv_class0_hp_t mv_class0_hp[constants::mv_contexts][2]; ///< \p MvClass0HpCdf.
		mv_sign_t mv_sign[constants::mv_contexts][2]; ///< \p MvSignCdf.
		mv_bit_t mv_bit[constants::mv_contexts][2]; ///< \p MvBitCdf.
		mv_hp_t mv_hp[constants::mv_contexts][2]; ///< \p MvHpCdf.
		palette_y_mode_t palette_y_mode; ///< \p PaletteYModeCdf.
		palette_uv_mode_t palette_uv_mode; ///< \p PaletteUVModeCdf.
		palette_y_size_t palette_y_size; ///< \p PaletteYSizeCdf.
		palette_uv_size_t palette_uv_size; ///< \p PaletteUVSizeCdf.
		palette_size_2_y_color_t palette_size_2_y_color; ///< \p PaletteSize2YColorCdf.
		palette_size_2_uv_color_t palette_size_2_uv_color; ///< \p PaletteSize2UVColorCdf.
		palette_size_3_y_color_t palette_size_3_y_color; ///< \p PaletteSize3YColorCdf.
		palette_size_3_uv_color_t palette_size_3_uv_color; ///< \p PaletteSize3UVColorCdf.
		palette_size_4_y_color_t palette_size_4_y_color; ///< \p PaletteSize4YColorCdf.
		palette_size_4_uv_color_t palette_size_4_uv_color; ///< \p PaletteSize4UVColorCdf.
		palette_size_5_y_color_t palette_size_5_y_color; ///< \p PaletteSize5YColorCdf.
		palette_size_5_uv_color_t palette_size_5_uv_color; ///< \p PaletteSize5UVColorCdf.
		palette_size_6_y_color_t palette_size_6_y_color; ///< \p PaletteSize6YColorCdf.
		palette_size_6_uv_color_t palette_size_6_uv_color; ///< \p PaletteSize6UVColorCdf.
		palette_size_7_y_color_t palette_size_7_y_color; ///< \p PaletteSize7YColorCdf.
		palette_size_7_uv_color_t palette_size_7_uv_color; ///< \p PaletteSize7UVColorCdf.
		palette_size_8_y_color_t palette_size_8_y_color; ///< \p PaletteSize8YColorCdf.
		palette_size_8_uv_color_t palette_size_8_uv_color; ///< \p PaletteSize8UVColorCdf.
		delta_q_t delta_q; ///< \p DeltaQCdf.
		// 158
		delta_lf_t delta_lf; ///< \p DeltaLFCdf.
		delta_lf_t delta_lf_multi[constants::frame_lf_count]; ///< \p DeltaLFMultiCdf.
		intra_tx_type_set1_t intra_tx_type_set1; ///< \p IntraTxTypeSet1Cdf.
		intra_tx_type_set2_t intra_tx_type_set2; ///< \p IntraTxTypeSet2Cdf.
		inter_tx_type_set1_t inter_tx_type_set1; ///< \p InterTxTypeSet1Cdf.
		inter_tx_type_set2_t inter_tx_type_set2; ///< \p InterTxTypeSet2Cdf.
		inter_tx_type_set3_t inter_tx_type_set3; ///< \p InterTxTypeSet3Cdf.
		inter_intra_t inter_intra; ///< \p InterIntraCdf.
		cfl_sign_t cfl_sign; ///< \p CflSignCdf.
		wedge_inter_intra_t wedge_inter_intra; ///< \p WedgeInterIntraCdf.
		inter_intra_mode_t inter_intra_mode; ///< \p InterIntraModeCdf.
		wedge_index_t wedge_index; ///< \p WedgeIndexCdf.
		cfl_alpha_t cfl_alpha; ///< \p CflAlphaCdf.
		use_wiener_t use_wiener; ///< \p UseWienerCdf.
		use_sgrproj_t use_sgrproj; ///< \p UseSgrprojCdf.
		restoration_type_t restoration_type; ///< RestorationTypeCdf.

		static const non_coeff defaults; ///< Default coefficients.
	};

	/// CDF tables used in the \p coeff() syntax structure.
	struct coeff {
		txb_skip_t txb_skip; ///< \p TxbSkipCdf.
		eob_pt_16_t eob_pt_16; ///< \p EobPt16Cdf.
		eob_pt_32_t eob_pt_32; ///< \p EobPt32Cdf.
		eob_pt_64_t eob_pt_64; ///< \p EobPt64Cdf.
		eob_pt_128_t eob_pt_128; ///< \p EobPt128Cdf.
		eob_pt_256_t eob_pt_256; ///< \p EobPt256Cdf.
		eob_pt_512_t eob_pt_512; ///< \p EobPt512Cdf.
		eob_pt_1024_t eob_pt_1024; ///< \p EobPt1024Cdf.
		eob_extra_t eob_extra; ///< \p EobExtraCdf.
		dc_sign_t dc_sign; ///< \p DcSignCdf.
		coeff_base_eob_t coeff_base_eob; ///< \p CoeffBaseEobCdf.
		coeff_base_t coeff_base; ///< \p CoeffBaseCdf.
		coeff_br_t coeff_br; ///< \p CoeffBrCdf.

		/// Returns default CDFs corresponding to the given \p idx value.
		[[nodiscard]] static coeff get_defaults(u32 idx);
	};
}
