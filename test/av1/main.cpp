#include "lotus/av1/decoder.h"
#include "lotus/helpers/mpeg.h"

#include <iostream>
#include <fstream>

#include "lotus/utils/strings.h"

using namespace lotus::types;
namespace mpeg = lotus::mpeg;
namespace av1 = lotus::av1;

[[nodiscard]] std::string parse_four_character_code(u32 v) {
	char name[5] = {};
	for (u32 i = 0; i < 4; ++i) {
		name[i] = static_cast<char>(v >> ((3 - i) * 8));
	}
	return name;
}

using pos_t = std::ios::pos_type;
using off_t = std::ios::off_type;

av1::decoder av1decoder;

void parse_box(std::ifstream &reader, pos_t region_end, u32 depth = 0) {
	auto read_byte = [&reader]() {
		return static_cast<std::byte>(reader.get());
	};

	static u32 handler_type = 0;
	bool reset_handler_type = false;

	const std::string indent(depth * 4, ' ');
	while (reader.tellg() < region_end) {
		if (reader.fail()) {
			std::cout << indent << "Fail\n";
			return;
		}

		const pos_t pos = reader.tellg();
		const auto box = mpeg::box::read(read_byte);

		std::cout << indent << "- Box " << parse_four_character_code(box.type) << "\n";
		std::cout << indent << "  Size " << box.size << "\n";

		const pos_t box_end = box.size == 0 ? region_end : pos + static_cast<off_t>(box.size);

		switch (box.type) {
		case lotus::make_four_character_code_reverse(u8"moov"): [[fallthrough]];
		case lotus::make_four_character_code_reverse(u8"trak"): [[fallthrough]];
		case lotus::make_four_character_code_reverse(u8"mdia"): [[fallthrough]];
		case lotus::make_four_character_code_reverse(u8"minf"): [[fallthrough]];
		case lotus::make_four_character_code_reverse(u8"stbl"): [[fallthrough]];
		case lotus::make_four_character_code_reverse(u8"edts"): [[fallthrough]];
		case lotus::make_four_character_code_reverse(u8"dinf"): [[fallthrough]];
		case lotus::make_four_character_code_reverse(u8"mvex"): [[fallthrough]];
		case lotus::make_four_character_code_reverse(u8"moof"): [[fallthrough]];
		case lotus::make_four_character_code_reverse(u8"traf"): [[fallthrough]];
		case lotus::make_four_character_code_reverse(u8"mfra"): [[fallthrough]];
		case lotus::make_four_character_code_reverse(u8"udta"): [[fallthrough]];
		case lotus::make_four_character_code_reverse(u8"meco"):
			parse_box(reader, box_end, depth + 1);
			break;
		case mpeg::box_types::file_type::type:
			{
				const auto ftyp = mpeg::box_types::file_type::read(read_byte, box_end - reader.tellg());
				std::cout << indent << "  Major brand " << parse_four_character_code(ftyp.major_brand) << "\n";
				std::cout << indent << "  Minor version " << ftyp.minor_version << "\n";
				std::cout << indent << "  Compatible brands\n";
				for (const u32 b : ftyp.compatible_brands) {
					std::cout << indent << "    " << parse_four_character_code(b) << "\n";
				}
			}
			break;
		case mpeg::box_types::movie_header::type:
			{
				const auto mvhd = mpeg::box_types::movie_header::read(read_byte);
				std::cout << indent << "  Version " << static_cast<i32>(mvhd.version) << "\n";
				std::cout << indent << "  Creation time " << mvhd.creation_time << "\n";
				std::cout << indent << "  Modification time " << mvhd.modification_time << "\n";
				std::cout << indent << "  Timescale " << mvhd.timescale << "\n";
				std::cout << indent << "  Duration " << mvhd.duration << "\n";
				std::cout << indent << "  Rate " << mvhd.rate << "\n";
				std::cout << indent << "  Volume " << mvhd.volume << "\n";
				std::cout << indent << "  Matrix\n";
				for (u32 r = 0; r < 3; ++r) {
					std::cout << indent << "    ";
					for (u32 c = 0; c < 3; ++c) {
						std::cout << mvhd.matrix(r, c) << "\t";
					}
					std::cout << "\n";
				}
				std::cout << indent << "  Next track ID " << mvhd.next_track_id << "\n";
			}
			break;
		case mpeg::box_types::handler::type:
			{
				const auto hdlr = mpeg::box_types::handler::read(read_byte);
				std::cout << indent << "  Type " << parse_four_character_code(hdlr.handler_type) << "\n";
				std::cout << indent << "  Name \"" << lotus::string::to_generic(hdlr.name) << "\"\n";
				handler_type = hdlr.handler_type;
				reset_handler_type = true;
			}
			break;
		case mpeg::box_types::sample_description::type:
			{
				switch (handler_type) {
				case mpeg::box_types::visual_sample_entry::type:
					{
						auto vide = mpeg::box_types::sample_description::read(read_byte);
						for (const mpeg::box_types::sample_description::entry &entry : vide.entries) {
							const auto compressor_name = lotus::string::to_generic(entry.contents.compressorname);
							std::cout << indent << "    - Entry\n";
							std::cout << indent << "      Type " <<
								parse_four_character_code(entry.header.type) << "\n";
							std::cout << indent << "      Size " << entry.header.size << "\n";
							std::cout << indent << "      Width " << entry.contents.width << "\n";
							std::cout << indent << "      Height " << entry.contents.height << "\n";
							std::cout << indent << "      Compressor name \"" << compressor_name << "\"\n";
							using tbox_t = mpeg::box_types::visual_sample_entry::trailing_box;
							for (const tbox_t &tbox : entry.contents.trailing_boxes) {
								std::cout << indent << "        - Trailing\n";
								std::cout << indent << "          Type " <<
									parse_four_character_code(tbox.header.type) << "\n";
								std::cout << indent << "          Size " << tbox.header.size << "\n";
								// TODO hack
								/*if (tbox.header.type == mpeg::box_types::av1_codec_configuration::type) {
									std::cout << reader.tellg() << "\n";
									av1::reader av1reader(read_byte);
									av1decoder.process_open_bitstream_unit(av1reader, 0);
									av1decoder.process_open_bitstream_unit(av1reader, 0);
								}*/
							}
						}
					}
					break;
				default:
					std::cout <<
						indent << "  Unrecognized handler type " << parse_four_character_code(handler_type) <<
						" (" << handler_type << ")\n";
					break;
				}
			}
			break;
		case mpeg::box_types::time_to_sample::type:
			{
				const auto stts = mpeg::box_types::time_to_sample::read(read_byte);
				for (const mpeg::box_types::time_to_sample::entry &entry : stts.entries) {
					std::cout <<
						indent << "    - Sample count " << entry.sample_count <<
						"\t Delta " << entry.sample_delta << "\n";
				}
			}
			break;
		case mpeg::box_types::sample_size::type:
			{
				const auto stsz = mpeg::box_types::sample_size::read(read_byte);
				for (const u32 size : stsz.entry_size) {
					std::cout << indent << "    - Entry size " << size << "\n";
				}
			}
			break;
		case mpeg::box_types::sample_to_chunk::type:
			{
				const auto stsc = mpeg::box_types::sample_to_chunk::read(read_byte);
				for (const mpeg::box_types::sample_to_chunk::entry &entry : stsc.entries) {
					std::cout << indent << "    - Entry\n";
					std::cout << indent << "      First chunk " << entry.first_chunk << "\n";
					std::cout << indent << "      Samples per chunk " << entry.samples_per_chunk << "\n";
					std::cout << indent << "      Sample description index " << entry.sample_description_index << "\n";
				}
			}
			break;
		case mpeg::box_types::chunk_offset::type:
			{
				const auto stco = mpeg::box_types::chunk_offset::read(read_byte);
				for (const u32 off : stco.chunk_offsets) {
					std::cout << indent << "    - Chunk offset " << off << "\n";
				}

				// decode chunks
				for (const u32 off : stco.chunk_offsets) {
					const pos_t bitstream_offset = reader.tellg();
					reader.seekg(off, std::ios::beg);

					av1::reader av1reader(read_byte);
					for (u32 i = 0; i < 10; ++i) {
						av1decoder.process_open_bitstream_unit(av1reader, 0);
					}

					reader.seekg(bitstream_offset, std::ios::beg);
				}
			}
			break;
		}

		reader.seekg(box_end, std::ios::beg);
	}

	if (reset_handler_type) {
		handler_type = 0;
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cout << "Missing arg\n";
		return 1;
	}

	std::ifstream reader(argv[1], std::ios::binary);

	reader.seekg(0, std::ios::end);
	const pos_t file_size = reader.tellg();
	reader.seekg(0, std::ios::beg);

	parse_box(reader, file_size);

	return 0;
}
