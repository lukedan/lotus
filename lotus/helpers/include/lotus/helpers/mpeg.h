#pragma once

/// \file
/// Utilities for handling ISO/IEC 14496-12 (MPEG) files.

#include <bit>
#include <concepts>
#include <variant>

#include "lotus/common.h"
#include "lotus/types.h"
#include "lotus/math/matrix.h"
#include "lotus/utils/misc.h"

namespace lotus::mpeg {
	/// Byte reader concept.
	template <typename T> concept byte_reader = requires(T a) {
		{ a() } -> std::same_as<std::byte>;
	};

	namespace _details {
		/// Converts the given big-endian value to native endianness.
		template <std::integral T> [[nodiscard]] constexpr T maybe_byteswap(T n) {
			if constexpr (std::endian::native == std::endian::little) {
				return std::byteswap(n);
			}
			return n;
		}

		/// Reads a number of bytes.
		template <
			u32 NumBytes, typename Ret = minimum_unsigned_type_bits_t<NumBytes * 8>
		> Ret read(byte_reader auto r) {
			Ret result = 0;
			for (u32 i = 0; i < NumBytes; ++i) {
				result |= static_cast<Ret>(r()) << (i * 8);
			}
			return result;
		}
		/// Reads 8 bits.
		u8 read8(byte_reader auto r) {
			return static_cast<u8>(r());
		}
		/// Reads 16 bits and converts them from big endian to native endian.
		u16 read16(byte_reader auto r) {
			return maybe_byteswap(read<2>(r));
		}
		/// Reads 32 bits and converts them from big endian to native endian.
		u32 read32(byte_reader auto r) {
			return maybe_byteswap(read<4>(r));
		}
		/// Reads 64 bits and converts them from big endian to native endian.
		u64 read64(byte_reader auto r) {
			return maybe_byteswap(read<8>(r));
		}

		/// Reads a null-terminated UTF-8 string.
		std::u8string read_string(byte_reader auto r) {
			std::u8string result;
			while (true) {
				const auto c = static_cast<char8_t>(r());
				if (c == u8'\0') {
					break;
				}
				result += c;
			}
			return result;
		}
		/// Reads a fixed-length UTF-8 string.
		template <u32 MaxLen> void read_fixed_string(byte_reader auto r, char8_t (&out)[MaxLen + 1]) {
			const u32 len = std::min(MaxLen, static_cast<u32>(r()));
			for (u32 i = 1; i < MaxLen; ++i) {
				out[i - 1] = static_cast<char8_t>(r());
			}
			out[len] = u8'\0';
		}
	}

	/// 4.2 Object Structure
	/// <tt>class Box</tt>.
	struct box {
		/// Zero initialization.
		box(zero_t) {
		}

		u64 size = 0; ///< \p size or \p largesize.
		u32 type = 0; ///< \p type.
		u8 user_type[16] = {}; ///< \p usertype.

		/// Reads a box.
		[[nodiscard]] static box read(byte_reader auto r) {
			box result = zero;
			result.size = _details::read32(r);
			result.type = _details::read32(r);
			if (result.size == 1) {
				result.size = _details::read64(r);
			}
			if (result.type == make_four_character_code_reverse(u8"uuid")) {
				for (u32 i = 0; i < 16; ++i) {
					result.user_type[i] = static_cast<u8>(r());
				}
			}
			return result;
		}
	};

	/// 4.2 Object Structure
	/// Extra fields in <tt>class FullBox</tt>.
	struct full_box {
		/// Zero initialization.
		full_box(zero_t) {
		}

		u8 version = 0; ///< \p version.
		u32 flags = 0; ///< \p flags.

		/// Reads a full box.
		[[nodiscard]] static full_box read(byte_reader auto r) {
			full_box result = zero;
			result.version = _details::read8(r);
			result.flags = _details::read<3>(r);
			return result;
		}
	};

	/// Various types of boxes.
	namespace box_types {
		/// 4.3 File Type Box
		struct file_type {
			constexpr static u32 type = make_four_character_code_reverse(u8"ftyp"); ///< Box type.

			/// Zero initialization.
			file_type(zero_t) {
			}

			u32 major_brand = 0; ///< \p major_brand.
			u32 minor_version = 0; ///< \p minor_version.
			std::vector<u32> compatible_brands; ///< \p compatible_brands.

			/// Reads a file type box.
			[[nodiscard]] static file_type read(byte_reader auto r, u64 remaining_size) {
				file_type result = zero;
				result.major_brand   = _details::read32(r);
				result.minor_version = _details::read32(r);
				const usize num_compatible_brands = (remaining_size - 8) / 4;
				result.compatible_brands.reserve(num_compatible_brands);
				for (usize i = 0; i < num_compatible_brands; ++i) {
					result.compatible_brands.emplace_back(_details::read32(r));
				}
				return result;
			}
		};

		/// 8.2.2 Movie Header box
		struct movie_header : full_box {
			constexpr static u32 type = make_four_character_code_reverse(u8"mvhd"); ///< Box type.

			/// Zero initializes.
			movie_header(zero_t) : full_box(zero) {
			}
			/// Initializes the base box.
			explicit movie_header(full_box b) : full_box(b) {
			}

			u64 creation_time = 0; ///< \p creation_time.
			u64 modification_time = 0; ///< \p modification_time.
			u64 duration = 0; ///< \p duration.
			u32 timescale = 0; ///< \p timescale.
			u32 rate = 0; ///< \p rate.
			mat33u32 matrix = zero; ///< \p matrix.
			u32 next_track_id = 0; ///< \p next_track_ID.
			u16 volume = 0; ///< \p volume.

			/// Reads a movie header box, including its full box fields.
			[[nodiscard]] static movie_header read(byte_reader auto r) {
				movie_header result(full_box::read(r));
				if (result.version == 1) {
					result.creation_time     = _details::read64(r);
					result.modification_time = _details::read64(r);
					result.timescale         = _details::read32(r);
					result.duration          = _details::read64(r);
				} else {
					result.creation_time     = _details::read32(r);
					result.modification_time = _details::read32(r);
					result.timescale         = _details::read32(r);
					result.duration          = _details::read32(r);
				}
				result.rate   = _details::read32(r);
				result.volume = _details::read16(r);
				_details::read16(r); // reserved
				_details::read32(r); // reserved[0]
				_details::read32(r); // reserved[1]
				for (u32 row = 0; row < 3; ++row) {
					for (u32 col = 0; col < 3; ++col) {
						result.matrix(row, col) = _details::read32(r);
					}
				}
				for (u32 i = 0; i < 6; ++i) {
					_details::read32(r); // pre_defined
				}
				result.next_track_id = _details::read32(r);
				return result;
			}
		};

		/// 8.4.3 Handler Reference Box
		struct handler : full_box {
			constexpr static u32 type = make_four_character_code_reverse(u8"hdlr"); ///< Box type.

			/// Zero initialization.
			handler(zero_t) : full_box(zero) {
			}
			/// Initializes the base box.
			explicit handler(full_box b) : full_box(b) {
			}

			u32 handler_type = 0; ///< \p handler_type.
			std::u8string name; ///< \p name.

			/// Reads a handler reference box, including its full box fields.
			[[nodiscard]] static handler read(byte_reader auto r) {
				handler result(full_box::read(r));
				_details::read32(r); // pre_defined
				result.handler_type = _details::read32(r);
				for (u32 i = 0; i < 3; ++i) {
					_details::read32(r); // reserved
				}
				result.name = _details::read_string(r);
				return result;
			}
		};

		/// 8.5.2 Sample Description Box
		/// <tt>class SampleEntry</tt>.
		struct sample_entry {
			/// Zero initialization.
			sample_entry(zero_t) {
			}

			u16 data_reference_index = 0; ///< \p data_reference_index.

			/// Reads a sample entry box.
			[[nodiscard]] static sample_entry read(byte_reader auto r) {
				sample_entry result = zero;
				for (u32 i = 0; i < 6; ++i) {
					_details::read8(r); // reserved
				}
				result.data_reference_index = _details::read16(r);
				return result;
			}
		};

		/// https://aomediacodec.github.io/av1-isobmff/
		/// 2.3. AV1 Codec Configuration Box
		struct av1_codec_configuration {
			constexpr static u32 type = make_four_character_code_reverse(u8"av1C"); ///< Box type.
			/// Sample entry type.
			constexpr static u32 sample_entry_type = make_four_character_code_reverse(u8"av01");

			/// Zero initialization.
			av1_codec_configuration(zero_t) {
			}

			u8 version = 0; ///< \p version.
			u8 seq_profile = 0; ///< \p seq_profile.
			u8 seq_level_idx_0 = 0; ///< \p seq_level_idx_0.
			u8 chroma_sample_position = 0; ///< \p chroma_sample_position.
			u8 initial_presentation_delay = 0; ///< \p initial_presentation_delay_minus_one.

			bool seq_tier_0                         : 1 = false; ///< \p seq_tier_0.
			bool high_bitdepth                      : 1 = false; ///< \p high_bitdepth.
			bool twelve_bit                         : 1 = false; ///< \p twelve_bit.
			bool monochrome                         : 1 = false; ///< \p monochrome.
			bool chroma_subsampling_x               : 1 = false; ///< \p chroma_subsampling_x.
			bool chroma_subsampling_y               : 1 = false; ///< \p chroma_subsampling_y.
			bool initial_presentation_delay_present : 1 = false; ///< \p initial_presentation_delay_present.

			/// Reads an AV1 codec configuration.
			[[nodiscard]] static av1_codec_configuration read(byte_reader auto r) {
				av1_codec_configuration result = zero;
				{
					const u8 b1 = _details::read8(r);
					result.version = b1 & 0x7Fu;
				}
				{
					const u8 b2 = _details::read8(r);
					result.seq_profile     = b2 >> 5;
					result.seq_level_idx_0 = b2 & 0x1Fu;
				}
				{
					const u8 b3 = _details::read8(r);
					result.seq_tier_0             = b3 & 0x80u;
					result.high_bitdepth          = b3 & 0x40u;
					result.twelve_bit             = b3 & 0x20u;
					result.monochrome             = b3 & 0x10u;
					result.chroma_subsampling_x   = b3 & 0x8u;
					result.chroma_subsampling_y   = b3 & 0x4u;
					result.chroma_sample_position = b3 & 0x3u;
				}
				{
					const u8 b4 = _details::read8(r);
					result.initial_presentation_delay_present = b4 & 0x10u;
					if (result.initial_presentation_delay_present) {
						result.initial_presentation_delay = (b4 & 0xFu) + 1;
					}
				}
				return result;
			}
		};

		/// 8.5.2 Sample Description Box
		/// <tt>class VisualSampleEntry</tt>.
		struct visual_sample_entry : sample_entry {
			constexpr static u32 type = make_four_character_code_reverse(u8"vide"); ///< Handler type.

			/// A trailing box.
			struct trailing_box {
				using value_type = std::variant<av1_codec_configuration>; ///< Value type.

				/// Zero initialization.
				trailing_box(zero_t) {
				}

				box header = zero; ///< The header.
				value_type value = av1_codec_configuration(zero); ///< Value.
			};

			/// Zero initialization.
			visual_sample_entry(zero_t) : sample_entry(zero) {
			}
			/// Initializes the base sample entry.
			explicit visual_sample_entry(sample_entry s) : sample_entry(s) {
			}

			u16 width  = 0; ///< \p width.
			u16 height = 0; ///< \p height.
			u32 horizresolution = 0; ///< \p horizresolution.
			u32 vertresolution  = 0; ///< \p vertresolution.
			u16 frame_count = 0; ///< \p frame_count.
			char8_t compressorname[33] = {}; ///< \p compressorname.
			u16 depth = 0; ///< \p depth.

			std::vector<trailing_box> trailing_boxes; ///< Trailing boxes.

			/// Reads a visual sample entry box, including its sample entry fields.
			[[nodiscard]] static visual_sample_entry read(byte_reader auto r) {
				visual_sample_entry result(sample_entry::read(r));
				_details::read16(r); // pre_defined
				_details::read16(r); // reserved
				for (u32 i = 0; i < 3; ++i) {
					_details::read32(r); // pre_defined
				}
				result.width  = _details::read16(r);
				result.height = _details::read16(r);
				result.horizresolution = _details::read32(r);
				result.vertresolution  = _details::read32(r);
				_details::read32(r); // reserved
				result.frame_count = _details::read16(r);
				_details::read_fixed_string<32>(r, result.compressorname);
				result.depth = _details::read16(r);
				_details::read16(r); // pre_defined
				return result;
			}
		};

		/// 8.5.2 Sample Description Box
		/// <tt>class SampleDescriptionBox</tt>.
		struct sample_description : full_box {
			constexpr static u32 type = make_four_character_code_reverse(u8"stsd"); ///< Box type.

			/// An entry.
			struct entry {
				/// Zero initialization.
				entry(zero_t) {
				}

				box header = zero; ///< The header.
				visual_sample_entry contents = zero; ///< The contents of this entry.
			};

			/// Zero initialization.
			sample_description(zero_t) : full_box(zero) {
			}
			/// Initializes the base box.
			explicit sample_description(full_box b) : full_box(b) {
			}

			std::vector<entry> entries; ///< Entries.

			/// Reads a sample description box, including its full box fields.
			[[nodiscard]] static sample_description read(byte_reader auto r) {
				sample_description result(full_box::read(r));
				const u32 entry_count = _details::read32(r);
				result.entries.reserve(entry_count);
				for (u32 i = 0; i < entry_count; ++i) {
					entry &e = result.entries.emplace_back(zero);
					e.header = box::read(r);
					e.contents = visual_sample_entry::read(r);
					if (e.header.type == av1_codec_configuration::sample_entry_type) {
						while (true) {
							const auto b = box::read(r);
							if (b.type == av1_codec_configuration::type) {
								visual_sample_entry::trailing_box &tbox =
									e.contents.trailing_boxes.emplace_back(zero);
								tbox.header = b;
								tbox.value.emplace<av1_codec_configuration>(av1_codec_configuration::read(r));
								break;
							}
						}
					}
				}
				return result;
			}
		};

		/// 8.6.1.2 Decoding Time to Sample Box
		struct time_to_sample : full_box {
			constexpr static u32 type = make_four_character_code_reverse(u8"stts"); ///< Box type.

			/// An entry.
			struct entry {
				/// Zero initialization.
				entry(zero_t) {
				}
				/// Initializes all fields of this struct.
				entry(u32 count, u32 delta) : sample_count(count), sample_delta(delta) {
				}

				u32 sample_count = 0;
				u32 sample_delta = 0;
			};

			/// Zero initialization.
			time_to_sample(zero_t) : full_box(zero) {
			}
			/// Initializes the base box.
			explicit time_to_sample(full_box b) : full_box(b) {
			}

			std::vector<entry> entries; ///< Entries.

			/// Reads a time to sample box, including its full box fields.
			[[nodiscard]] static time_to_sample read(byte_reader auto r) {
				time_to_sample result(full_box::read(r));
				const u32 entry_count = _details::read32(r);
				result.entries.reserve(entry_count);
				for (u32 i = 0; i < entry_count; ++i) {
					const u32 count = _details::read32(r);
					const u32 delta = _details::read32(r);
					result.entries.emplace_back(count, delta);
				}
				return result;
			}
		};

		/// 8.7.3.2 Sample Size Box
		struct sample_size : full_box {
			constexpr static u32 type = make_four_character_code_reverse(u8"stsz"); ///< Box type.

			/// Zero initialization.
			sample_size(zero_t) : full_box(zero) {
			}
			/// Initializes the base box.
			explicit sample_size(full_box b) : full_box(b) {
			}

			std::vector<u32> entry_size; ///< \p entry_size.

			/// Reads a sample size box, including its full box fields.
			[[nodiscard]] static sample_size read(byte_reader auto r) {
				sample_size result(full_box::read(r));
				const u32 sample_size = _details::read32(r);
				const u32 sample_count = _details::read32(r);
				result.entry_size.reserve(sample_count);
				for (u32 i = 0; i < sample_count; ++i) {
					result.entry_size.emplace_back(sample_size == 0 ? _details::read32(r) : sample_size);
				}
				return result;
			}
		};

		/// 8.7.4 Sample To Chunk Box
		struct sample_to_chunk : full_box {
			constexpr static u32 type = make_four_character_code_reverse(u8"stsc"); ///< Box type.

			/// An entry.
			struct entry {
				/// Zero initialization.
				entry(zero_t) {
				}
				/// Initializes all fields of this struct.
				entry(u32 fc, u32 spc, u32 sdi) :
					first_chunk(fc), samples_per_chunk(spc), sample_description_index(sdi) {
				}

				u32 first_chunk = 0; ///< \p first_chunk.
				u32 samples_per_chunk = 0; ///< \p samples_per_chunk.
				u32 sample_description_index = 0; ///< \p sample_description_index.
			};

			/// Zero initialization.
			sample_to_chunk(zero_t) : full_box(zero) {
			}
			/// Initializes the base box.
			explicit sample_to_chunk(full_box b) : full_box(b) {
			}

			std::vector<entry> entries; ///< Entries.

			/// Reads a sample to chunk box, including its full box fields.
			[[nodiscard]] static sample_to_chunk read(byte_reader auto r) {
				sample_to_chunk result(full_box::read(r));
				const u32 entry_count = _details::read32(r);
				result.entries.reserve(entry_count);
				for (u32 i = 0; i < entry_count; ++i) {
					const u32 first_chunk               = _details::read32(r);
					const u32 samples_per_chunk         = _details::read32(r);
					const u32 samples_description_index = _details::read32(r);
					result.entries.emplace_back(first_chunk, samples_per_chunk, samples_description_index);
				}
				return result;
			}
		};

		/// 8.7.5 Chunk Offset Box
		struct chunk_offset : full_box {
			constexpr static u32 type = make_four_character_code_reverse(u8"stco"); ///< Box type.

			/// Zero initialization.
			chunk_offset(zero_t) : full_box(zero) {
			}
			/// Initializes the base box.
			explicit chunk_offset(full_box b) : full_box(b) {
			}

			std::vector<u32> chunk_offsets; ///< \p chunk_offset.

			/// Reads a chunk offset box, including its full box fields.
			[[nodiscard]] static chunk_offset read(byte_reader auto r) {
				chunk_offset result(full_box::read(r));
				const u32 entry_count = _details::read32(r);
				for (u32 i = 0; i < entry_count; ++i) {
					result.chunk_offsets.emplace_back(_details::read32(r));
				}
				return result;
			}
		};
	}
}
