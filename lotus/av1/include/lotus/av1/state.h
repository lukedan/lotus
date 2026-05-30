#pragma once

/// \file
/// Persistent state variables used across the AV1 decoding process.

#include <vector>

#include "lotus/common.h"
#include "lotus/types.h"
#include "lotus/math/vector.h"

#include "lotus/av1/common.h"
#include "lotus/av1/cdf.h"

namespace lotus::av1::state {
	/// Resizable 2D grid.
	template <typename T> struct grid2 {
	public:
		/// Creates a grid without allocating memory.
		grid2(zero_t) {
		}

		/// Allocates a new grid with the given dimensions, and initializes all elements to the given value.
		[[nodiscard]] static grid2 allocate(u32 w, u32 h, T value) {
			grid2 result = zero;
			result._width  = w;
			result._height = h;
			result._storage = std::make_unique<T[]>(w * h);
			std::fill(result._storage.get(), result._storage.get() + w * h, value);
			return result;
		}

		/// Indexing.
		[[nodiscard]] T &operator()(u32 y, u32 x) {
			// TODO Morton swizzling?
			crash_if(x >= _width || y >= _height);
			return _storage[y * _width + x];
		}
		/// \overload
		[[nodiscard]] const T &operator()(u32 y, u32 x) const {
			crash_if(x >= _width || y >= _height);
			return _storage[y * _width + x];
		}

		/// \return The width of the grid.
		[[nodiscard]] u32 get_width() const {
			return _width;
		}
		/// \return The height of the grid.
		[[nodiscard]] u32 get_height() const {
			return _height;
		}
	private:
		std::unique_ptr<T[]> _storage; ///< Storage.
		u32 _width = 0; ///< Width.
		u32 _height = 0; ///< Height.
	};

	/// Block related state.
	struct block {
		using palette_t = std::array<channel_t, constants::palette_colors>; ///< Palette type.

		/// Zero initialization.
		block(zero_t) {
		}

		grid2<u16> curr_frame[3] = { zero, zero, zero }; ///< \p CurrFrame.

		grid2<prediction_mode> y_modes = zero; ///< \p YModes.
		grid2<prediction_mode> uv_modes = zero; ///< \p UVModes.
		grid2<bool> is_inters = zero; ///< \p IsInters.
		grid2<bool> skip_modes = zero; ///< \p SkipModes.
		grid2<bool> skips = zero; ///< \p Skips.
		grid2<block_size> sizes = zero; ///< \p MiSizes.
		grid2<segment_id_t> segment_ids = zero; ///< \p SegmentIds.
		segment_id_t **prev_segment_ids = nullptr; ///< \p PrevSegmentIds.
		grid2<u8> palette_sizes[2] = { zero, zero }; ///< \p PaletteSizes.
		grid2<reference_frame> ref_frames[2] = { zero, zero }; ///< \p RefFrames.
		interpolation ***interp_filters = nullptr; ///< \p InterpFilters.
		grid2<i8> cdef_idx = zero; ///< \p cdef_idx.
		grid2<palette_t> palette_colors[2] = { zero, zero }; ///< \p PaletteColors.
		grid2<inverse_transform> tx_types = zero; ///< \p TxTypes.
		grid2<tx_size> inter_tx_sizes = zero; ///< \p InterTxSizes.

		/// \p BlockDecoded with each element offset by 1 on each direction.
		grid2<bool> block_decoded_val[3] = { zero, zero, zero };

		// left context
		std::vector<u8> left_level_context[3]; ///< \p LeftLevelContext.
		std::vector<u8> left_dc_context[3]; ///< \p LeftDcContext.
		std::vector<bool> left_seg_pred_context; ///< \p LeftSegPredContext.

		// above context
		std::vector<u8> above_level_context[3]; ///< \p AboveLevelContext.
		std::vector<u8> above_dc_context[3]; ///< \p AboveDcContext.
		std::vector<bool> above_seg_pred_context; ///< \p AboveSegPredContext.

		u32 col_starts[constants::max_tile_cols] = {}; ///< \p MiColStarts.
		u32 row_starts[constants::max_tile_rows] = {}; ///< \p MiRowStarts.
		i8 delta_lf[constants::frame_lf_count] = {}; ///< \p DeltaLF.

		u8 current_q_index = 0; ///< \p CurrentQIndex.
		tx_size tx_size = zero; ///< \p TxSize.
		inverse_transform plane_tx_type = zero; ///< \p PlaneTxType.
		u32 max_luma_h = 0; ///< \p MaxLumaH.
		u32 max_luma_w = 0; ///< \p MaxLumaW.

		bool read_deltas       : 1 = false; ///< \p ReadDeltas.
		bool seen_frame_header : 1 = false; ///< \p SeenFrameHeader.

		/// Allocates a state object for the given image size.
		[[nodiscard]] static block allocate(cvec2u32 size) {
			constexpr u32 mi_per_sb = constants::max_sb_size / constants::mi_size;
			const u32 mi_rows = mi_per_sb * ((size[1] + constants::max_sb_size - 1) / constants::max_sb_size);
			const u32 mi_cols = mi_per_sb * ((size[0] + constants::max_sb_size - 1) / constants::max_sb_size);
			const u32 frame_width  = mi_cols * constants::mi_size;
			const u32 frame_height = mi_rows * constants::mi_size;

			block result = zero;

			result.curr_frame[0] = grid2<u16>::allocate(frame_width, frame_height, 0);
			result.curr_frame[1] = grid2<u16>::allocate(frame_width, frame_height, 0);
			result.curr_frame[2] = grid2<u16>::allocate(frame_width, frame_height, 0);

			result.y_modes           = grid2<prediction_mode>::allocate(mi_cols, mi_rows, zero);
			result.uv_modes          = grid2<prediction_mode>::allocate(mi_cols, mi_rows, zero);
			result.is_inters         = grid2<bool>::allocate(mi_cols, mi_rows, false);
			result.skip_modes        = grid2<bool>::allocate(mi_cols, mi_rows, false);
			result.skips             = grid2<bool>::allocate(mi_cols, mi_rows, false);
			result.sizes             = grid2<block_size>::allocate(mi_cols, mi_rows, zero);
			result.segment_ids       = grid2<segment_id_t>::allocate(mi_cols, mi_rows, zero);
			result.palette_sizes[0]  = grid2<u8>::allocate(mi_cols, mi_rows, 0);
			result.palette_sizes[1]  = grid2<u8>::allocate(mi_cols, mi_rows, 0);
			result.ref_frames[0]     = grid2<reference_frame>::allocate(mi_cols, mi_rows, zero);
			result.ref_frames[1]     = grid2<reference_frame>::allocate(mi_cols, mi_rows, zero);
			result.cdef_idx          = grid2<i8>::allocate(mi_cols, mi_rows, 0);
			result.palette_colors[0] = grid2<palette_t>::allocate(mi_cols, mi_rows, { {} });
			result.palette_colors[1] = grid2<palette_t>::allocate(mi_cols, mi_rows, { {} });
			result.tx_types          = grid2<inverse_transform>::allocate(mi_cols, mi_rows, zero);
			result.inter_tx_sizes    = grid2<enum tx_size>::allocate(mi_cols, mi_rows, zero);

			for (u32 i = 0; i < 3; ++i) {
				result.block_decoded_val[i] = grid2<bool>::allocate(mi_cols + 2, mi_rows + 2, zero);
			}

			for (u32 i = 0; i < 3; ++i) {
				result.left_level_context[i].resize(mi_rows);
				result.left_dc_context[i].resize(mi_rows);
			}
			result.left_seg_pred_context.resize(mi_rows);

			for (u32 i = 0; i < 3; ++i) {
				result.above_level_context[i].resize(mi_cols);
				result.above_dc_context[i].resize(mi_cols);
			}
			result.above_seg_pred_context.resize(mi_cols);

			return result;
		}

		/// \p BlockDecoded.
		[[nodiscard]] bool &block_decoded(u32 plane, i32 y, i32 x) {
			return block_decoded_val[plane](static_cast<u32>(y + 1), static_cast<u32>(x + 1));
		}
		/// \overload
		[[nodiscard]] bool block_decoded(u32 plane, i32 y, i32 x) const {
			return block_decoded_val[plane](static_cast<u32>(y + 1), static_cast<u32>(x + 1));
		}
	};

	/// Block range information. The variables start with \p Mi.
	struct block_range {
		/// Zero initialization.
		block_range(zero_t) {
		}

		u32 col_start = 0; ///< \p MiColStart.
		u32 col_end   = 0; ///< \p MiColEnd.
		u32 row_start = 0; ///< \p MiRowStart.
		u32 row_end   = 0; ///< \p MiRowEnd.

		/// 5.11.51. Is inside function
		[[nodiscard]] bool is_inside(u32 r, u32 c) const {
			return c >= col_start && c < col_end && r >= row_start && r < row_end;
		}
	};

	/// 5.11.5. Decode block syntax
	/// State computed in \p decode_block().
	struct block_decoding {
		/// Zero initialization.
		block_decoding(zero_t) {
		}

		u32 row = 0; ///< \p MiRow.
		u32 col = 0; ///< \p MiCol.
		block_size size = zero; ///< \p MiSize.

		bool avail_u        : 1 = false; ///< \p AvailU.
		bool avail_l        : 1 = false; ///< \p AvailL.
		bool avail_u_chroma : 1 = false; ///< \p AvailUChroma.
		bool avail_l_chroma : 1 = false; ///< \p AvailLChroma.
		bool has_chroma     : 1 = false; ///< \p HasChroma.
	};

	/// Loop restoration related context.
	struct ref_lr {
		/// Zero initialization.
		ref_lr(zero_t) {
		}

		i32 ref_sgr_xqd[3][2] = {}; ///< \p RefSgrXqd.
		i32 ref_lr_wiener[3][2][constants::wiener_coeffs] = {}; ///< \p RefLrWiener.
	};

	/// Color map state.
	struct color_map {
		/// Type for \p ColorMapY and \p ColorMapUV. One channel of the color map.
		using channel = std::array<std::array<u32, constants::max_sb_size>, constants::max_sb_size>;

		/// No initialization.
		color_map(uninitialized_t) {
		}

		channel y;  ///< \p ColorMapY.
		channel uv; ///< \p ColorMapUV.
	};

	/// Motion vector related context.
	struct motion_vector {
		u32 new_mv_context = 0; ///< \p NewMvContext.
		u32 zero_mv_context = 0; ///< \p ZeroMvContext.
		u32 ref_mv_context = 0; ///< \p RefMvContext.

		u32 num_mv_found = 0; ///< \p NumMvFound.

		cvec2i32 ref_stack_mv[constants::max_ref_mv_stack_size][2] = {}; ///< \p RefStackMv.
		cvec2i32 mv[2] = {}; ///< \p Mv.
		cvec2i32 pred_mv[2] = {}; ///< \p PredMv.
		cvec2i32 global_mvs[2] = {}; ///< \p GlobalMvs.

		// TODO size?
		u8 *drl_ctx_stack; ///< \p DrlCtxStack.

		/// Zero initialization.
		motion_vector(zero_t) {
		}
	};

	/// 5.11.39. Coefficients syntax
	struct coeffs {
		/// Zero initialization.
		coeffs(zero_t) {
		}

		i32 quant[1024] = {}; ///< \p Quant.
		i32 dequant[64][64] = {}; ///< \p Dequant.
		i32 residual[64][64] = {}; ///< \p Residual.
	};

	/// CDF context.
	struct cdf {
		/// Zero initialization.
		cdf(zero_t) {
			std::memset(&non_coeff, 0, sizeof(non_coeff));
			std::memset(&coeff, 0, sizeof(coeff));
		}

		av1::cdf::non_coeff non_coeff; ///< CDF initialized by \p init_non_coeff_cdfs().
		av1::cdf::coeff coeff; ///< CDF initialized by \p init_coeff_cdfs().

		/// 6.8.2. Uncompressed header semantics
		/// \p init_non_coeff_cdfs().
		void init_non_coeff_cdfs() {
			non_coeff = av1::cdf::non_coeff::defaults;
		}
		/// 6.8.2. Uncompressed header semantics
		/// \p init_coeff_cdfs().
		void init_coeff_cdfs(u32 base_q_idx) {
			u32 idx;
			if (base_q_idx <= 20) {
				idx = 0;
			} else if (base_q_idx <= 60) {
				idx = 1;
			} else if (base_q_idx <= 120) {
				idx = 2;
			} else {
				idx = 3;
			}
			coeff = av1::cdf::coeff::get_defaults(idx);
		}
	};
}
