add_library(lotus_av1 STATIC)
configure_lotus_module(lotus_av1)

target_include_directories(lotus_av1 PUBLIC "include/")
target_sources(lotus_av1
	PUBLIC
		"include/lotus/av1/block_decoding.h"
		"include/lotus/av1/cdf.h"
		"include/lotus/av1/cdf_context.h"
		"include/lotus/av1/common.h"
		"include/lotus/av1/decoder.h"
		"include/lotus/av1/enums.h"
		"include/lotus/av1/functions.h"
		"include/lotus/av1/obu.h"
		"include/lotus/av1/quantizer_matrix.h"
		"include/lotus/av1/reader.h"
		"include/lotus/av1/state.h"
		"include/lotus/av1/symbol_decoder.h"
		"include/lotus/av1/tables.h"
	PRIVATE
		"src/av1/block_decoding.cpp"
		"src/av1/cdf.cpp"
		"src/av1/decoder.cpp"
		"src/av1/functions.cpp"
		"src/av1/reader.cpp"
		"src/av1/symbol_decoder.cpp")

target_link_libraries(lotus_av1
	PUBLIC
		lotus_core)
