#include "lotus/logging.h"
#include "lotus/math/quaternion.h"
#include "lotus/math/vector.h"
#include "lotus/math/auto_diff/context.h"
#include "lotus/math/auto_diff/operations.h"
#include "lotus/math/auto_diff/utils.h"

int main() {
	using namespace lotus::types;
	using namespace lotus::matrix_types;
	using namespace lotus::vector_types;
	using namespace lotus::auto_diff;

	{
		lotus::log().debug("--------------------");
		context ctx;

		variable<f32> x = ctx.create_variable<f32>("x", 3.0f);
		variable<f32> y = ctx.create_variable<f32>("y", 5.0f);

		auto f = [](auto x, auto y) {
			return x * 2.0f - x * x + x * y;
		};

		expression v = f(x, y);
		f32 vr = f(x.get_value(), y.get_value());
		expression dvdx = v.diff(x);
		expression dv2dxdy = dvdx.diff(y);
		expression dv2d2x = dvdx.diff(x);

		lotus::log().debug("v = {} = {}", v.to_string(), v.eval<f32>());
		lotus::log().debug("reference = {}", vr);
		lotus::log().debug("dv/dx = {} = {}", dvdx.to_string(), dvdx.eval<f32>());
		lotus::log().debug("dv2/dxdy = {} = {}", dv2dxdy.to_string(), dv2dxdy.eval<f32>());
		lotus::log().debug("dv2/d2x = {} = {}", dv2d2x.to_string(), dv2d2x.eval<f32>());
	}

	{
		lotus::log().debug("--------------------");
		context ctx;

		cvec3<variable<f32>> x = ctx.create_matrix_variable("x", cvec3f32(1.0f, 2.0f, 3.0f));
		cvec3<variable<f32>> y = ctx.create_matrix_variable("y", cvec3f32(5.0f, 7.0f, 6.0f));
		cvec3<expression> xe = mat::into_expression(x);
		cvec3<expression> ye = mat::into_expression(y);

		auto f = [](auto x, auto y) {
			return lotus::vecu::normalize(lotus::vec::cross(x, y));
		};

		cvec3<expression> v = f(xe, ye);
		cvec3f32 vr = f(mat::eval<f32>(xe), mat::eval<f32>(ye));
		cvec3<expression> dvdx0 = mat::diff(v, x[0]);
		cvec3<expression> dvdx0_simp = mat::simplify(dvdx0);

		lotus::log().debug("vx = {} = {}", v[0].to_string(), v[0].eval<f32>());
		lotus::log().debug("reference = {}", vr[0]);
		lotus::log().debug("dvx/dxx = {} = {}", dvdx0[0].to_string(), dvdx0[0].eval<f32>());
		lotus::log().debug("simplified: {} = {}", dvdx0_simp[0].to_string(), dvdx0_simp[0].eval<f32>());
	}

	{
		lotus::log().debug("--------------------");
		context ctx;

		mat33f32 tangent_frame = lotus::tangent_frame<f32>::from_normal(lotus::vecu::normalize(cvec3f32(3.0f, 2.0f, -5.0f))).get_world_to_tangent_matrix();
		cvec3f32 ra_local = cvec3f32(0.2f, 0.0f, 0.0f);
		cvec3f32 rb_local = cvec3f32(0.1f, 0.2f, -0.1f);
		f32 m = 1.0f;
		f32 dt = 0.01f;
		f32 k = 0.1f;
		f32 lambda = 0.1f;
		cvec3f32 pred_pa = lotus::zero;

		cvec3<variable<f32>> pa = ctx.create_matrix_variable("pa", cvec3f32(3.0f, 7.0f, 6.0f));
		cvec3<variable<f32>> pb = ctx.create_matrix_variable("pb", cvec3f32(-2.0f, -4.0f, 1.0f));
		cvec4<variable<f32>> qa = ctx.create_matrix_variable("qa", cvec4f32(1.0f, 0.0f, 0.0f, 0.0f));
		cvec4<variable<f32>> qb = ctx.create_matrix_variable("qb", cvec4f32(1.0f, 0.0f, 0.0f, 0.0f));

		expression pred_energy = (pa - pred_pa).squared_norm() * m / (2.0f * dt * dt);

		cvec3<expression> pae = mat::into_expression(pa);
		cvec3<expression> pbe = mat::into_expression(pb);
		lotus::quaternion<expression> qae = lotus::quat::from_vec4_xyzw(mat::into_expression(qa));
		lotus::quaternion<expression> qbe = lotus::quat::from_vec4_xyzw(mat::into_expression(qb));

		cvec3<expression> ra = qae.rotate(ra_local) + pae;
		cvec3<expression> rb = qbe.rotate(rb_local) + pbe;

		cvec3<expression> c = tangent_frame * (ra - rb);
		cvec3<expression> e = 0.5f * k * lotus::matm::multiply(c, c) + lambda * c;

		mat33<expression> rot_mat = qae.into_rotation_matrix();

		expression obj = pred_energy + e[0] + e[1] + e[2];
		expression obj_simp = obj.simplified();

		expression dobjdqx = obj.diff(qa[0]);
		expression dobjdqx_simp = dobjdqx.simplified();

		lotus::log().debug("obj = {} = {}", obj.to_string(), obj.eval<f32>());
		lotus::log().debug("simplified: {} = {}", obj_simp.to_string(), obj_simp.eval<f32>());
		lotus::log().debug("dobj/dqx = {} = {}", dobjdqx.to_string(), dobjdqx.eval<f32>());
		lotus::log().debug("simplified: {} = {}", dobjdqx_simp.to_string(), dobjdqx_simp.eval<f32>());
	}

	return 0;
}
