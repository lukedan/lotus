#pragma once

/// \file
/// AV1 enumerations.

#include "lotus/types.h"

namespace lotus::av1 {
	// 8
	/// Features. Constants starting with \p SEG_LVL.
	enum class features : u8 {
		alt_q      = 0, ///< \p SEG_LVL_ALT_Q.
		alt_lf_y_v = 1, ///< \p SEG_LVL_ALT_LF_Y_V.
		ref_frame  = 5, ///< \p SEG_LVL_REF_FRAME.
		skip       = 6, ///< \p SEG_LVL_SKIP.
		globalmv   = 7, ///< \p SEG_LVL_GLOBALMV.

		max = 8 ///< \p SEG_LVL_MAX.
	};

	/// Inverse transform modes.
	enum class inverse_transform : u8 {
		dct_dct           = 0,  ///< \p DCT_DCT.
		adst_dct          = 1,  ///< \p ADST_DCT.
		dct_adst          = 2,  ///< \p DCT_ADST.
		adst_adst         = 3,  ///< \p ADST_ADST.
		flipadst_dct      = 4,  ///< \p FLIPADST_DCT.
		dct_flipadst      = 5,  ///< \p DCT_FLIPADST.
		flipadst_flipadst = 6,  ///< \p FLIPADST_FLIPADST.
		adst_flipadst     = 7,  ///< \p ADST_FLIPADST.
		flipadst_adst     = 8,  ///< \p FLIPADST_ADST.
		idtx              = 9,  ///< \p IDTX.
		v_dct             = 10, ///< \p V_DCT.
		h_dct             = 11, ///< \p H_DCT.
		v_adst            = 12, ///< \p V_ADST.
		h_adst            = 13, ///< \p H_ADST.
		v_flipadst        = 14, ///< \p V_FLIPADST.
		h_flipadst        = 15, ///< \p H_FLIPADST.
	};

	// 11
	/// Warp model.
	enum class warp_model : u8 {
		identity    = 0, ///< \p IDENTITY.
		translation = 1, ///< \p TRANSLATION.
		rotzoom     = 2, ///< \p ROTZOOM.
		affine      = 3, ///< \p AFFINE.
	};

	// 16
	/// Transform class.
	enum class tx_class : u8 {
		both_2d    = 0, ///< \p TX_CLASS_2D.
		horizontal = 1, ///< \p TX_CLASS_HORIZ.
		vertical   = 2, ///< \p TX_CLASS_VERT.
	};

	// 117
	/// 6.4.2. Color config semantics
	/// Values used for \p color_primaries.
	enum class color_primaries : u8 {
		bt_709       = 1,  ///< \p CP_BT_709.
		unspecified  = 2,  ///< \p CP_UNSPECIFIED.
		bt_470_m     = 4,  ///< \p CP_BT_470_M.
		bt_470_b_g   = 5,  ///< \p CP_BT_470_B_G.
		bt_601       = 6,  ///< \p CP_BT_601.
		smpte_240    = 7,  ///< \p CP_SMPTE_240.
		generic_film = 8,  ///< \p CP_GENERIC_FILM.
		bt_2020      = 9,  ///< \p CP_BT_2020.
		xyz          = 10, ///< \p CP_XYZ.
		smpte_431    = 11, ///< \p CP_SMPTE_431.
		smpte_432    = 12, ///< \p CP_SMPTE_432.
		ebu_3213     = 22, ///< \p CP_EBU_3213.
	};

	/// 6.4.2. Color config semantics
	/// Values used for \p transfer_characteristics.
	enum class transfer_characteristics : u8 {
		bt_709      = 1,  ///< \p TC_BT_709.
		unspecified = 2,  ///< \p TC_UNSPECIFIED.
		// TODO
		srgb        = 13, ///< \p TC_SRGB.
		// TODO
	};

	// 118
	/// 6.4.2. Color config semantics
	/// Values used for \p matrix_coefficients.
	enum class matrix_coefficients : u8 {
		identity    = 0, ///< \p MC_IDENTITY.
		bt_709      = 1, ///< \p MC_BT_709.
		unspecified = 2, ///< \p MC_UNSPECIFIED.
		// TODO
	};

	// 119
	/// 6.4.2. Color config semantics
	/// Values used for \p chroma_sample_position.
	enum class chroma_sample_position : u8 {
		unknown   = 0, ///< \p CSP_UNKNOWN.
		vertical  = 1, ///< \p CSP_VERTICAL.
		colocated = 2, ///< \p CSP_COLOCATED.
	};

	// 150
	/// 6.8.2. Uncompressed header semantics
	/// Values used for \p frame_type.
	enum class frame_type : u8 {
		key_frame        = 0, ///< \p KEY_FRAME.
		inter_frame      = 1, ///< \p INTER_FRAME.
		intra_only_frame = 2, ///< \p INTRA_ONLY_FRAME.
		switch_frame     = 3, ///< \p SWITCH_FRAME.
	};

	// 162
	/// 6.8.9. Interpolation filter semantics
	/// Values used for \p interpolation_filter.
	enum class interpolation : u8 {
		eighttap        = 0, ///< \p EIGHTTAP.
		eighttap_smooth = 1, ///< \p EIGHTTAP_SMOOTH.
		eighttap_sharp  = 2, ///< \p EIGHTTAP_SHARP.
		bilinear        = 3, ///< \p BILINEAR.
		switchable      = 4, ///< \p SWITCHABLE.
	};

	// 169
	/// 6.8.21. TX mode semantics
	/// Values used for \p TxMode.
	enum class tx_mode : u8 {
		only_4x4 = 0, ///< \p ONLY_4X4.
		largest  = 1, ///< \p TX_MODE_LARGEST.
		select   = 2, ///< \p TX_MODE_SELECT.
	};

	// 172
	/// 6.10.4. Decode partition semantics
	/// Values used for \p partition.
	enum class partition : u8 {
		none   = 0, ///< \p PARTITION_NONE.
		horz   = 1, ///< \p PARTITION_HORZ.
		vert   = 2, ///< \p PARTITION_VERT.
		split  = 3, ///< \p PARTITION_SPLIT.
		horz_a = 4, ///< \p PARTITION_HORZ_A.
		horz_b = 5, ///< \p PARTITION_HORZ_B.
		vert_a = 6, ///< \p PARTITION_VERT_A.
		vert_b = 7, ///< \p PARTITION_VERT_B.
		horz_4 = 8, ///< \p PARTITION_HORZ_4.
		vert_4 = 9, ///< \p PARTITION_VERT_4.
	};

	/// 6.10.4. Decode partition semantics
	/// Values used for \p subSize.
	enum class block_size : u8 {
		b4x4     = 0,  ///< \p BLOCK_4X4.
		b4x8     = 1,  ///< \p BLOCK_4X8.
		b8x4     = 2,  ///< \p BLOCK_8X4.
		b8x8     = 3,  ///< \p BLOCK_8X8.
		b8x16    = 4,  ///< \p BLOCK_8X16.
		b16x8    = 5,  ///< \p BLOCK_16X8.
		b16x16   = 6,  ///< \p BLOCK_16X16.
		b16x32   = 7,  ///< \p BLOCK_16X32.
		b32x16   = 8,  ///< \p BLOCK_32X16.
		b32x32   = 9,  ///< \p BLOCK_32X32.
		b32x64   = 10, ///< \p BLOCK_32X64.
		b64x32   = 11, ///< \p BLOCK_64X32.
		b64x64   = 12, ///< \p BLOCK_64X64.
		b64x128  = 13, ///< \p BLOCK_64X128.
		b128x64  = 14, ///< \p BLOCK_128X64.
		b128x128 = 15, ///< \p BLOCK_128X128.
		b4x16    = 16, ///< \p BLOCK_4X16.
		b16x4    = 17, ///< \p BLOCK_16X4.
		b8x32    = 18, ///< \p BLOCK_8X32.
		b32x8    = 19, ///< \p BLOCK_32X8.
		b16x64   = 20, ///< \p BLOCK_16X64.
		b64x16   = 21, ///< \p BLOCK_64X16.
		invalid  = 22, ///< \p BLOCK_INVALID.
	};

	// 174
	/// 6.10.6 Intra frame mode info semantics
	/// 6.10.22. Inter block mode info semantics
	/// Values for \p intra_frame_y_mode and \p uv_mode.
	enum class prediction_mode : u8 {
		dc                = 0,  ///< \p DC_PRED.
		v                 = 1,  ///< \p V_PRED.
		h                 = 2,  ///< \p H_PRED.
		d45               = 3,  ///< \p D45_PRED.
		d135              = 4,  ///< \p D135_PRED.
		d113              = 5,  ///< \p D113_PRED.
		d157              = 6,  ///< \p D157_PRED.
		d203              = 7,  ///< \p D203_PRED.
		d67               = 8,  ///< \p D67_PRED.
		smooth            = 9,  ///< \p SMOOTH_PRED.
		smooth_v          = 10, ///< \p SMOOTH_V_PRED.
		smooth_h          = 11, ///< \p SMOOTH_H_PRED.
		paeth             = 12, ///< \p PAETH_PRED.
		uv_cfl            = 13, ///< \p UV_CFL_PRED.
		nearestmv         = 14, ///< \p NEARESTMV.
		nearmv            = 15, ///< \p NEARMV.
		globalmv          = 16, ///< \p GLOBALMV.
		newmv             = 17, ///< \p NEWMV.
		nearest_nearestmv = 18, ///< \p NEAREST_NEARESTMV.
		near_nearmv       = 19, ///< \p NEAR_NEARMV.
		nearest_newmv     = 20, ///< \p NEAREST_NEWMV.
		new_nearestmv     = 21, ///< \p NEW_NEARESTMV.
		near_newmv        = 22, ///< \p NEAR_NEWMV.
		new_nearmv        = 23, ///< \p NEW_NEARMV.
		global_globalmv   = 24, ///< \p GLOBAL_GLOBALMV.
		new_newmv         = 25, ///< \p NEW_NEWMV.
	};

	// 177
	/// 6.10.15. Loop restoration params semantics
	/// Values used for \p FrameRestorationType.
	enum class frame_restoration : u8 {
		none       = 0, ///< \p RESTORE_NONE.
		wiener     = 1, ///< \p RESTORE_WIENER.
		sgrproj    = 2, ///< \p RESTORE_SGRPROJ.
		switchable = 3, ///< \p RESTORE_SWITCHABLE.
	};

	/// 6.10.16. TX size semantics
	/// Values used for \p TxSize.
	enum class tx_size : u8 {
		size_4x4   = 0,  ///< \p TX_4X4.
		size_8x8   = 1,  ///< \p TX_8X8.
		size_16x16 = 2,  ///< \p TX_16X16.
		size_32x32 = 3,  ///< \p TX_32X32.
		size_64x64 = 4,  ///< \p TX_64X64.
		size_4x8   = 5,  ///< \p TX_4X8.
		size_8x4   = 6,  ///< \p TX_8X4.
		size_8x16  = 7,  ///< \p TX_8X16.
		size_16x8  = 8,  ///< \p TX_16X8.
		size_16x32 = 9,  ///< \p TX_16X32.
		size_32x16 = 10, ///< \p TX_32X16.
		size_32x64 = 11, ///< \p TX_32X64.
		size_64x32 = 12, ///< \p TX_64X32.
		size_4x16  = 13, ///< \p TX_4X16.
		size_16x4  = 14, ///< \p TX_16X4.
		size_8x32  = 15, ///< \p TX_8X32.
		size_32x8  = 16, ///< \p TX_32X8.
		size_16x64 = 17, ///< \p TX_16X64.
		size_64x16 = 18, ///< \p TX_64X16.
	};

	/// 6.10.19. Transform type semantics
	/// Transform sets. Sets can have duplicate values.
	enum class tx_set : u8 {
		dct_only = 0, ///< \p TX_SET_DCTONLY.
		intra_1  = 1, ///< \p TX_SET_INTRA_1.
		intra_2  = 2, ///< \p TX_SET_INTRA_2.
		inter_1  = 1, ///< \p TX_SET_INTER_1.
		inter_2  = 2, ///< \p TX_SET_INTER_2.
		inter_3  = 3, ///< \p TX_SET_INTER_3.
	};

	// 181
	/// 6.10.23. Filter intra mode info semantics
	/// Values used for \p filter_intra_mode.
	enum class intra_filtering : u8 {
		dc_pred    = 0, ///< \p FILTER_DC_PRED.
		v_pred     = 1, ///< \p FILTER_V_PRED.
		h_pred     = 2, ///< \p FILTER_H_PRED.
		d157_pred  = 3, ///< \p FILTER_D157_PRED.
		paeth_pred = 4, ///< \p FILTER_PAETH_PRED.
	};

	// 182
	/// 6.10.24. Ref frames semantics
	/// Values used for \p RefFrame.
	enum class reference_frame : i8 {
		none    = -1, ///< \p NONE.
		intra   = 0,  ///< \p INTRA_FRAME.
		last    = 1,  ///< \p LAST_FRAME.
		last2   = 2,  ///< \p LAST2_FRAME.
		last3   = 3,  ///< \p LAST3_FRAME.
		golden  = 4,  ///< \p GOLDEN_FRAME.
		bwdref  = 5,  ///< \p BWDREF_FRAME.
		altref2 = 6,  ///< \p ALTREF2_FRAME.
		altref  = 7,  ///< \p ALTREF_FRAME.
	};

	// 185
	/// 6.10.26. Read motion mode semantics
	/// Values used for \p motion_mode.
	enum class motion_compensation : u8 {
		simple    = 0, ///< \p SIMPLE.
		obmc      = 1, ///< \p OBMC.
		localwarp = 2, ///< \p LOCALWARP.
	};

	/// 6.10.27. Read inter intra semantics
	/// Values used for \p interintra_mode.
	enum class intra_prediction : u8 {
		dc     = 0, ///< \p II_DC_PRED.
		v      = 1, ///< \p II_V_PRED.
		h      = 2, ///< \p II_H_PRED.
		smooth = 3, ///< \p II_SMOOTH_PRED.
	};

	// 186
	/// 6.10.28. Read compound type semantics
	/// Values used for \p compound_type.
	enum class compound_method : u8 {
		wedge    = 0, ///< \p COMPOUND_WEDGE.
		diffwtd  = 1, ///< \p COMPOUND_DIFFWTD.
		average  = 2, ///< \p COMPOUND_AVERAGE.
		intra    = 3, ///< \p COMPOUND_INTRA.
		distance = 4, ///< \p COMPOUND_DISTANCE.
	};

	// 187
	/// 6.10.29. MV semantics
	/// Values used for \p mv_joint.
	enum class mv_joint : u8 {
		zero   = 0, ///< \p MV_JOINT_ZERO.
		hnzvz  = 1, ///< \p MV_JOINT_HNZVZ.
		hzvnz  = 2, ///< \p MV_JOINT_HZVNZ.
		hnzvnz = 3, ///< \p MV_JOINT_HNZVNZ
	};

	// 191
	/// 6.10.36. Read CFL alphas semantics
	/// Values used for \p signU and \p signV.
	enum class cfl_sign : u8 {
		zero     = 0, ///< CFL_SIGN_ZERO.
		negative = 1, ///< CFL_SIGN_NEG.
		positive = 2, ///< CFL_SIGN_POS.
	};
}
