/*#define _CRTDBG_MAP_ALLOC*/ // does not compile
#include <cstdlib>
#if _WIN32
#	include <crtdbg.h>
#endif

#include <csignal>
#include <random>
#include <functional>
#include <atomic>
#include <compare>

#include "lotus/containers/short_vector.h"
#include "lotus/logging.h"

using lotus::log;
using namespace lotus::types;

std::atomic_bool should_exit = false;
void ctrl_c_handler(int) {
	should_exit = true;
}

usize alloc_count = 0;

struct v {
	constexpr static u32 array_size = 4096;

	v() : data(new unsigned[array_size]) {
		++alloc_count;
	}
	v(unsigned x) : data(new unsigned[array_size]) {
		data[0] = x;
		++alloc_count;
	}
	v(const v &src) : data(new unsigned[array_size]) {
		data[0] = src.data[0];
		++alloc_count;
	}
	v &operator=(const v &src) {
		data[0] = src.data[0];
		return *this;
	}
	~v() {
		lotus::crash_if(data == nullptr);
		delete[] data;
		data = nullptr;
		--alloc_count;
	}

	friend std::strong_ordering operator<=>(const v &lhs, const v &rhs) {
		return lhs.data[0] <=> rhs.data[0];
	}
	friend bool operator==(const v &lhs, const v &rhs) {
		return lhs.data[0] == rhs.data[0];
	}

	unsigned *data = nullptr;
};

using tv   = lotus::short_vector<v,   255>;
using tvv  = lotus::short_vector<tv,  1>;
using tvvv = lotus::short_vector<tvv, 4>;

using rv   = std::vector<v>;
using rvv  = std::vector<rv>;
using rvvv = std::vector<rvv>;

std::default_random_engine rng(123456);
std::uniform_int_distribution<usize> count_dist1(0, 500);
std::uniform_int_distribution<usize> count_dist2(0, 10);

tvvv test_vec;
rvvv ref_vec;

bool compare() {
	bool result = true;
	auto bad = [&]() {
		result = false;
	};

	if (test_vec.size() != ref_vec.size()) {
		bad();
	} else {
		for (usize i = 0; i < test_vec.size(); ++i) {
			if (test_vec[i].size() != ref_vec[i].size()) {
				bad();
			} else {
				for (usize j = 0; j < test_vec[i].size(); ++j) {
					if (test_vec[i][j].size() != ref_vec[i][j].size()) {
						bad();
					} else {
						for (usize k = 0; k < test_vec[i][j].size(); ++k) {
							if (test_vec[i][j][k] != ref_vec[i][j][k]) {
								bad();
							}
						}
					}
				}
			}
		}
	}
	return result;
}

void push_back_new() {
	auto sz1 = count_dist2(rng);
	rvv re;
	tvv te;
	for (usize i = 0; i < sz1; ++i) {
		tv elem;
		auto sz2 = count_dist1(rng);
		for (usize j = 0; j < sz2; ++j) {
			elem.emplace_back(rng());
		}
		te.emplace_back(elem);
		re.emplace_back(elem.begin(), elem.end());
	}
	test_vec.emplace_back(std::move(te));
	ref_vec.emplace_back(std::move(re));
}
void duplicate_random() {
	if (test_vec.empty()) {
		return;
	}
	std::uniform_int_distribution<usize> idx_dist(0, test_vec.size() - 1);
	auto idx = idx_dist(rng);
	test_vec.emplace_back(test_vec[idx]);
	ref_vec.emplace_back(ref_vec[idx]);
}
void shuffle() {
	for (usize i = 0; i < test_vec.size(); ++i) {
		usize idx = std::uniform_int_distribution<usize>(i, test_vec.size() - 1)(rng);
		std::swap(test_vec[idx], test_vec[i]);
		std::swap(ref_vec[idx], ref_vec[i]);
	}
}
void assign_random() {
	if (test_vec.empty()) {
		return;
	}
	std::uniform_int_distribution<usize> idx_dist(0, test_vec.size() - 1);
	auto idx1 = idx_dist(rng);
	auto idx2 = idx_dist(rng);
	test_vec[idx1] = test_vec[idx2];
	ref_vec[idx1] = ref_vec[idx2];
}
void erase_seq() {
	std::uniform_int_distribution<usize> idx_dist(0, test_vec.size());
	auto [beg, end] = std::minmax({ idx_dist(rng), idx_dist(rng) });
	test_vec.erase(test_vec.begin() + beg, test_vec.begin() + end);
	ref_vec.erase(
		ref_vec.begin() + static_cast<std::ptrdiff_t>(beg),
		ref_vec.begin() + static_cast<std::ptrdiff_t>(end)
	);
}
void shrink_to_fit() {
	test_vec.shrink_to_fit();
	ref_vec.shrink_to_fit();
}
void insert_random() {
	auto sz1 = count_dist2(rng);
	tvvv test_val;
	rvvv ref_val;
	for (usize i = 0; i < sz1; ++i) {
		auto sz2 = count_dist2(rng);
		tvv test_val2;
		rvv ref_val2;
		for (usize j = 0; j < sz2; ++j) {
			auto sz3 = count_dist1(rng);
			tv elem;
			for (usize k = 0; k < sz3; ++k) {
				elem.emplace_back(rng());
			}
			test_val2.emplace_back(elem);
			ref_val2.emplace_back(elem.begin(), elem.end());
		}
		test_val.emplace_back(std::move(test_val2));
		ref_val.emplace_back(std::move(ref_val2));
	}
	std::uniform_int_distribution<usize> idx_dist(0, test_vec.size());
	const auto idx = static_cast<std::ptrdiff_t>(idx_dist(rng));
	test_vec.insert(test_vec.begin() + idx, test_val.begin(), test_val.end());
	ref_vec.insert(ref_vec.begin() + idx, ref_val.begin(), ref_val.end());
}

int main() {
#if _WIN32
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	void (*funcs[])() = {
		push_back_new,
		duplicate_random,
		shuffle,
		assign_random,
		erase_seq,
		shrink_to_fit,
		insert_random,
	};
	std::u8string_view func_names[] = {
		u8"Push back New",
		u8"Duplicate Random",
		u8"Shuffle",
		u8"Assign Random",
		u8"Erase Sequence",
		u8"Shrink To Fit",
		u8"Insert Random",
	};
	static_assert(std::size(funcs) == std::size(func_names), "Function array size mismatch");
	std::uniform_int_distribution<usize> op_dist(0, std::size(func_names) - 1);

	std::signal(SIGINT, ctrl_c_handler);

	for (usize i = 0; !should_exit; ++i) {
		auto op = op_dist(rng);
		log().debug("{}: {}", i, lotus::string::to_generic(func_names[op]));
		funcs[op]();
		lotus::crash_if(!compare());
	}

	test_vec.clear();
	ref_vec.clear();
	lotus::crash_if(alloc_count > 0);

	log().debug("Exiting");
	return 0;
}
