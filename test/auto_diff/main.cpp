#include "lotus/logging.h"
#include "lotus/math/vector.h"
#include "lotus/math/auto_diff/context.h"
#include "lotus/math/auto_diff/operations.h"
#include "lotus/math/auto_diff/utils.h"

int main() {
	using namespace lotus::types;
	using namespace lotus::matrix_types;
	using namespace lotus::vector_types;
	using namespace lotus::auto_diff;

	context ctx;

	{
		auto x = ctx.create_variable<f32>("x", 3.0f);
		auto y = ctx.create_variable<f32>("y", 5.0f);
		auto v = x * 2.0f - x * x + x * y;
		auto dvdx = v.diff(x);
		auto dv2dxdy = dvdx.diff(y);
		auto dv2d2x = dvdx.diff(x);

		lotus::log().debug("{} = {}", v.to_string(), v.eval<f32>());
		lotus::log().debug("{} = {}", dvdx.to_string(), dvdx.eval<f32>());
		lotus::log().debug("{} = {}", dv2dxdy.to_string(), dv2dxdy.eval<f32>());
		lotus::log().debug("{} = {}", dv2d2x.to_string(), dv2d2x.eval<f32>());
	}

	{
		cvec3<variable<f32>> x = ctx.create_matrix_variable("x", cvec3f32(1.0f, 2.0f, 3.0f));
		cvec3<expression> xe = mat::into_expression(x);

		auto vx = lotus::vecu::normalize(lotus::vec::cross(xe, xe));
		lotus::log().debug("{}", vx[0].to_string());
	}

	return 0;
}
