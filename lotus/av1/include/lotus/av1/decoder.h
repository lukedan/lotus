#pragma once

/// \file
/// AV1 decoder.

#include <__filesystem/filesystem_error.h>

#include "lotus/av1/common.h"
#include "lotus/av1/reader.h"

namespace lotus::av1 {
	/// AV1 decoder.
	class decoder {
	public:
		/// B.2. Length delimited bitstream syntax
		/// \p process_bitstream().
		void process_bitstream(reader&, u64 sz);
		/// 5.3.1. General OBU syntax
		void process_open_bitstream_unit(reader&, u64 sz);

		/// 5.9.1. General frame header OBU syntax
		void process_frame_header_obu(const obu::header&, reader&);

		/// 5.10. Frame OBU syntax
		void process_frame_obu(const obu::header&, reader&, u64 sz);

		/// B.2. Length delimited bitstream syntax
		/// \p temporal_unit().
		void process_temporal_unit(reader&, u64 sz);
		/// B.2. Length delimited bitstream syntax
		/// \p frame_unit().
		void process_frame_unit(reader&, u64 sz);

		/// Samples the output image at the given location.
		[[nodiscard]] cvec3u32 get_sample(cvec2u32) const;
	private:
		obu::sequence_header _seq_header = zero; ///< Sequence header.
		obu::uncompressed_header _frame_header = zero; ///< Frame header.

		// state
		state::block _sb = zero; ///< Block state.
		state::cdf _cdf = zero; ///< CDF.

		/// Debug export image.
		void _debug_export_image(const std::filesystem::path&) const;
	};
}
