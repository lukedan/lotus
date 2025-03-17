#include <cmath>
#include <cfenv>
#include <random>

#include <lotus/utils/custom_float.h>
#include <lotus/logging.h>

using namespace lotus::types;

[[nodiscard]] bool test_f32_to_f64(float f32) {
	auto myf32 = lotus::float32::reinterpret(f32);

	auto myf64 = myf32.into<lotus::float64>();
	auto myi64 = myf64.reinterpret_as<u64>();
	auto sysi64 = std::bit_cast<u64>(static_cast<double>(f32));
	if (std::isnan(f32) != myf64.is_nan()) {
		return false;
	}
	if (!myf64.is_nan()) {
		return myi64 == sysi64;
	}
	return true;
}

[[nodiscard]] bool test_f64_to_f32(double f64) {
	auto myf64 = lotus::float64::reinterpret(f64);

	auto myf32 = myf64.into<lotus::float32>();
	auto myi32 = myf32.reinterpret_as<u32>();
	auto sysi32 = std::bit_cast<u32>(static_cast<float>(f64));
	if (std::isnan(f64) != myf32.is_nan()) {
		return false;
	}
	if (!myf32.is_nan()) {
		return myi32 == sysi32;
	}
	return true;
}

int main() {
	std::fesetround(FE_TOWARDZERO);

	for (u32 i = 0; ; ++i) {
		if (i % 10000000 == 0) {
			lotus::log().debug("f32 -> f64  {:.1f}%", 100.0 * i / static_cast<double>(std::numeric_limits<u32>::max()));
		}

		lotus::crash_if(!test_f32_to_f64(std::bit_cast<float>(i)));

		if (i == std::numeric_limits<u32>::max()) {
			break;
		}
	}

	constexpr u64 f64_interval = 2100000000;
	for (u64 i = 0; ; ) {
		if (i % (f64_interval * 10000000) == 0) {
			lotus::log().debug("f64 -> f32  {:.1f}%", 100.0 * i / static_cast<double>(std::numeric_limits<u64>::max()));
		}

		lotus::crash_if(!test_f64_to_f32(std::bit_cast<double>(i)));

		auto new_i = i + f64_interval;
		if (new_i < i) {
			break;
		}
		i = new_i;
	}

	std::default_random_engine rng;
	std::uniform_int_distribution<u64> range(0);

	lotus::log().debug("Now testing random f64 -> f32");
	for (u64 i = 0; ; ++i) {
		if (i % 10000000 == 0) {
			lotus::log().debug("Tested {} numbers", i);
		}

		u64 x = range(rng);
		bool ok = test_f64_to_f32(std::bit_cast<double>(x));
		if (!ok) {
			lotus::log().error("Bad value: {}", x);
			std::abort();
		}
	}

	return 0;
}
