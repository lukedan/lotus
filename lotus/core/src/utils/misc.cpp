#include "lotus/utils/misc.h"

#include <cstdio>

namespace lotus {
	bool load_binary_file(
		const std::filesystem::path &path, std::span<std::byte>(*allocate)(std::size_t, void*), void *user_data
	) {
		if (FILE *fin = std::fopen(path.string().c_str(), "rb")) {
			if (std::fseek(fin, 0, SEEK_END) == 0) {
				const long pos = std::ftell(fin);
				if (pos >= 0) {
					std::rewind(fin);
					const auto size = static_cast<std::size_t>(pos);
					const std::span<std::byte> storage = allocate(size, user_data);
					if (storage.size() == size) {
						const std::size_t bytes_read = std::fread(storage.data(), 1, size, fin);
						if (bytes_read == size) {
							std::fclose(fin);
							return true;
						}
					}
				}
			}
			std::fclose(fin);
		}
		return false;
	}
}
