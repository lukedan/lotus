#include <iostream>

#include <pbd/math/matrix.h>
#include <pbd/math/vector.h>
#include <pbd/math/quaternion.h>

constexpr auto mat1 = pbd::mat34d::identity();
constexpr auto mat2 = pbd::mat34d::zero();
constexpr pbd::mat34d mat3{
	{1.0, 2.0, 3.0, 4.0},
	{2.0, 3.0, 4.0, 5.0},
	{0.0, 1.0, 2.0, 3.0}
};
constexpr auto vec1 = pbd::cvec3d::create({ 1.0, 2.0, 3.0 });

constexpr auto trans = mat1.transposed();
constexpr auto row = trans.row(2);
constexpr auto col = trans.column(2);
constexpr auto block = trans.block<4, 3>(0, 0);
constexpr auto block2 = trans.block<3, 2>(1, 1);
constexpr auto comp = row[2];

constexpr auto mul = mat3 * trans;
constexpr auto add = mat3 + mat1;
constexpr auto sub = mat3 - mat1;
constexpr auto scale1 = mat3 * 3.0;
constexpr auto scale2 = 3.0 * mat3;
constexpr auto scale3 = mat3 / 3.0;
constexpr auto neg = -mat3;

constexpr auto sqr_norm = vec1.squared_norm();
constexpr auto dot_prod = pbd::vec::dot(vec1, trans.row(1).transposed());

constexpr auto quat = pbd::quatd::zero();
constexpr auto quat2 = pbd::quatd::from_wxyz({ 1.0, 2.0, 3.0, 4.0 });
constexpr auto inv_quat2 = quat2.inverse();
constexpr auto quat3 = quat2 + inv_quat2;

constexpr auto quat_w = quat.w();
constexpr auto quat_mag = quat.squared_magnitude();

int main() {
	auto x = mat2(2, 1);

	auto norm = pbd::vec::unsafe_normalize(vec1);

	std::cout << "(" << norm[0] << ", " << norm[1] << ", " << norm[2] << ")\n";

	return 0;
}
