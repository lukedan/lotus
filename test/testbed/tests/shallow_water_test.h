#pragma once

#include <vector>

#include "test.h"
#include "../utils.h"

class shallow_water_test : public test {
public:
	template <typename T> struct grid2 {
	public:
		grid2() = default;
		explicit grid2(lotus::cvec2u32 sz) {
			resize(sz);
		}

		void resize(lotus::cvec2u32 sz) {
			_size = sz;
			_storage.resize(_size[0] * _size[1]);
		}
		void fill(const T &val) {
			std::fill(_storage.begin(), _storage.end(), val);
		}

		[[nodiscard]] T &at(std::uint32_t x, std::uint32_t y) {
			return _storage[_size[0] * y + x];
		}
		[[nodiscard]] const T &at(std::uint32_t x, std::uint32_t y) const {
			return _storage[_size[0] * y + x];
		}
		[[nodiscard]] T &operator()(std::uint32_t x, std::uint32_t y) {
			return at(x, y);
		}
		[[nodiscard]] const T &operator()(std::uint32_t x, std::uint32_t y) const {
			return at(x, y);
		}

		[[nodiscard]] lotus::cvec2u32 get_size() const {
			return _size;
		}
	private:
		std::vector<T> _storage;
		lotus::cvec2u32 _size = lotus::zero;
	};

	explicit shallow_water_test(const test_context &tctx) : test(tctx) {
		soft_reset();
	}

	void soft_reset() override {
		_render = debug_render();
		_render.ctx = &_get_test_context();

		const auto w = static_cast<std::uint32_t>(_width);
		const auto h = static_cast<std::uint32_t>(_height);

		_b.resize({ w, h });
		_h.resize({ w, h });
		_ux.resize({ w - 1, h });
		_uy.resize({ w, h - 1 });

		_h.fill(_water_height);
		_b.fill(0.0f);
		_ux.fill(0.0f);
		_uy.fill(0.0f);

		// boundary
		const float border_value = 2.0f * _water_height;
		for (std::uint32_t x = 0; x < w; ++x) {
			_h(x, 0) = _h(x, h - 1) = _b(x, 0) = _b(x, h - 1) = border_value;
		}
		for (std::uint32_t y = 1; y + 1 < h; ++y) {
			_h(0, y) = _h(w - 1, y) = _b(0, y) = _b(w - 1, y) = border_value;
		}

		_h_prev = _h;
	}

	void timestep(double dt_total, std::size_t iterations) override {
		if (_impulse) {
			_impulse = false;
			_h(_h.get_size()[0] / 2, _h.get_size()[1] / 2) *= 2.0f;
		}

		const float dt = dt_total / iterations;
		for (std::size_t i = 0; i < iterations; ++i) {
			_solve_horizontal(dt);
			_solve_vertical(dt);
		}
	}

	void render(
		lotus::renderer::context &ctx,
		lotus::renderer::context::queue &q,
		lotus::renderer::constant_uploader &uploader,
		lotus::renderer::image2d_color color,
		lotus::renderer::image2d_depth_stencil depth,
		lotus::cvec2u32 size
	) override {
		std::vector<vec3> pos;
		std::vector<vec3> norm;
		std::vector<std::uint32_t> indices;
		const auto w = static_cast<std::uint32_t>(_h.get_size()[0]);
		const auto h = static_cast<std::uint32_t>(_h.get_size()[1]);
		for (std::uint32_t y = 0; y < h; ++y) {
			for (std::uint32_t x = 0; x < w; ++x) {
				const std::uint32_t x1y1 = pos.size();
				const std::uint32_t x0y1 = x1y1 - 1;
				const std::uint32_t x1y0 = x1y1 - w;
				const std::uint32_t x0y0 = x0y1 - w;
				pos.emplace_back(x * _cell_size, _h(x, y), y * _cell_size);
				if (x > 0 && y > 0) {
					indices.emplace_back(x0y0);
					indices.emplace_back(x1y0);
					indices.emplace_back(x1y1);
					indices.emplace_back(x0y0);
					indices.emplace_back(x1y1);
					indices.emplace_back(x0y1);
				}
			}
		}
		for (std::uint32_t y = 0; y < h; ++y) {
			for (std::uint32_t x = 0; x < w; ++x) {
				const auto get_pos = [&](std::uint32_t xp, std::uint32_t yp) {
					return pos[yp * w + xp];
				};
				const vec3 xn = get_pos(x > 0 ? x - 1 : x, y);
				const vec3 xp = get_pos(x + 1 < w ? x + 1 : x, y);
				const vec3 yn = get_pos(x, y > 0 ? y - 1 : y);
				const vec3 yp = get_pos(x, y + 1 < h ? y + 1 : y);
				norm.emplace_back(lotus::vec::unsafe_normalize(lotus::vec::cross(xp - xn, yp - yn)));
			}
		}
		_render.draw_body(pos, norm, indices, mat44s::identity(), lotus::linear_rgba_f(0.2f, 0.2f, 1.0f, 1.0f), false);
		_render.flush(ctx, q, uploader, color, depth, size);
	}

	void gui() override {
		if (ImGui::Button("Impulse")) {
			_impulse = true;
		}
		// TODO
	}

	inline static std::string_view get_name() {
		return "Shallow Water";
	}
protected:
	debug_render _render;

	int _width = 128;
	int _height = 128;
	float _cell_size = 0.2f;
	float _water_height = 0.2f;
	float _gravity = 9.8f;
	float _damping = 0.0f;

	grid2<float> _b;
	grid2<float> _h;
	grid2<float> _h_prev;
	grid2<float> _ux;
	grid2<float> _uy;

	bool _impulse = false;


	// https://en.wikipedia.org/wiki/Tridiagonal_matrix_algorithm
	static void _thomas_symmetric(
		std::span<const float> diag,
		std::span<const float> diag1,
		std::span<float> rhs, // used as scratch (d_i)
		std::span<float> out_x // used as scratch (c_i)
	) {
		// downward sweep
		out_x[0] = diag1[0] / diag[0];
		rhs[0] = rhs[0] / diag[0];
		for (std::size_t i = 1; i < diag.size(); ++i) {
			if (i + 1 < diag.size()) {
				out_x[i] = diag1[i] / (diag[i] - diag1[i - 1] * out_x[i - 1]);
			}
			rhs[i] = (rhs[i] - diag1[i - 1] * rhs[i - 1]) / (diag[i] - diag1[i - 1] * out_x[i - 1]);
		}

		// back substitution
		out_x[diag.size() - 1] = rhs[diag.size() - 1];
		for (std::size_t i = diag.size() - 1; i > 0; ) {
			--i;
			out_x[i] = rhs[i] - out_x[i] * out_x[i + 1];
		}
	}

	[[nodiscard]] float _d(std::uint32_t x, std::uint32_t y) const {
		return std::max(0.0f, _h(x, y) - _b(x, y));
	}
	void _solve_horizontal(float dt) {
		const std::uint32_t w = _h.get_size()[0];
		const std::uint32_t h = _h.get_size()[1];
		const float constant = _gravity * (dt * dt) / (_cell_size * _cell_size);

		std::vector<float> e(w);
		std::vector<float> f(w - 1);
		std::vector<float> rhs(w);
		std::vector<float> hi(w);

		for (std::uint32_t y = 0; y < h; ++y) {
			// compute system
			for (std::uint32_t x = 0; x < w; ++x) {
				const float h1 = _h(x, y);
				const float h2 = _h_prev(x, y);
				const float dprev = x > 0 ? _d(x - 1, y) + _d(x, y) : 0.0f;
				const float dnext = x + 1 < w ? _d(x, y) + _d(x + 1, y) : 0.0f;
				if (x + 1 < w) {
					f[x] = -constant * 0.5f * dnext;
				}
				e[x] = 1.0f + constant * 0.5f * (dprev + dnext);
				rhs[x] = h1 + (1.0f - _damping) * (h1 - h2);
			}

			_thomas_symmetric(e, f, rhs, hi);

			for (std::uint32_t x = 0; x < w; ++x) {
				_h_prev(x, y) = hi[x];
			}
		}

		std::swap(_h, _h_prev);
	}
	void _solve_vertical(float dt) {
		const std::uint32_t w = _h.get_size()[0];
		const std::uint32_t h = _h.get_size()[1];
		const float constant = _gravity * (dt * dt) / (_cell_size * _cell_size);

		std::vector<float> e(h);
		std::vector<float> f(h - 1);
		std::vector<float> rhs(h);
		std::vector<float> hi(h);

		for (std::uint32_t x = 0; x < w; ++x) {
			// compute system
			for (std::uint32_t y = 0; y < h; ++y) {
				const float h1 = _h(x, y);
				const float h2 = _h_prev(x, y);
				const float dprev = y > 0 ? _d(x, y - 1) + _d(x, y) : 0.0f;
				const float dnext = y + 1 < h ? _d(x, y) + _d(x, y + 1) : 0.0f;
				if (y + 1 < h) {
					f[y] = -constant * 0.5f * dnext;
				}
				e[y] = 1.0f + constant * 0.5f * (dprev + dnext);
				rhs[y] = h1 + (1.0f - _damping) * (h1 - h2);
			}

			_thomas_symmetric(e, f, rhs, hi);

			for (std::uint32_t y = 0; y < h; ++y) {
				_h_prev(x, y) = hi[y];
			}
		}

		std::swap(_h, _h_prev);
	}
};
