#include "lotus/types.h"
#include "lotus/utils/job_system.h"
#include "lotus/logging.h"

using namespace lotus;

std::tuple<i32> multiply_by_2(u32, i32 x) {
	return { x * 2 };
}

std::tuple<i32> add_1(u32, i32 x) {
	return { x + 1 };
}

int main() {
	const u32 threads = std::thread::hardware_concurrency();
	log().debug("Threads: {}", threads);
	auto jman = job_system::manager::spawn_workers(threads);
	const u32 count = 100000000;
	std::vector<i32> inputs;
	for (u32 i = 0; i < count; ++i) {
		inputs.emplace_back(i);
	}

	{
		log().debug("Start parallel");
		job_system::resource_handle h1 = jman.create_resource_with_value<std::vector<i32>>(inputs);
		job_system::resource_handle h2 = jman.create_resource<std::vector<i32>>();
		job_system::resource_handle h3 = jman.create_resource<std::vector<i32>>();
		job_system::resource_handle h4 = jman.create_resource<i64>();
		jman.schedule_multi_job<10000>(multiply_by_2, { h1 }, { h2 }, count);
		jman.schedule_mono_job(
			static_cast<std::tuple<>(*)(const std::vector<i32>&)>([](const std::vector<i32>&) -> std::tuple<> {
				log().debug("Multiply done");
				return {};
			}),
			{ h2 }, {}
		);
		jman.schedule_multi_job<10000>(add_1, { h2 }, { h3 }, count);
		jman.schedule_mono_job(
			static_cast<std::tuple<>(*)(const std::vector<i32>&)>([](const std::vector<i32>&) -> std::tuple<> {
				log().debug("Add done");
				return {};
			}),
			{ h3 }, {}
		);
		jman.schedule_mono_job(
			static_cast<std::tuple<i64>(*)(std::vector<i32>&)>([](std::vector<i32> &in) -> std::tuple<i64> {
				i64 res = 0;
				for (const i32 x : in) {
					res += x;
				}
				return { res };
			}),
			{ h3 }, { h4 }
		);
		const auto res = jman.get_resource_value_blocking<i64>(h4);
		log().debug("Parallel: {}", res);
	}

	{
		log().debug("Start serial");
		std::vector<i32> h2(count);
		for (u32 i = 0; i < count; ++i) {
			h2[i] = std::get<0>(multiply_by_2(i, inputs[i]));
		}
		log().debug("Multiply done");
		std::vector<i32> h3(count);
		for (u32 i = 0; i < count; ++i) {
			h3[i] = std::get<0>(add_1(i, h2[i]));
		}
		log().debug("Add done");
		i64 result = 0;
		for (const i32 x : h3) {
			result += x;
		}
		log().debug("Serial: {}", result);
	}

	return 0;
}
