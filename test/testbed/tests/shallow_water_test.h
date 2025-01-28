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

		const auto w = static_cast<std::uint32_t>(_size[0]);
		const auto h = static_cast<std::uint32_t>(_size[1]);

		_b.resize({ w, h });
		_h.resize({ w, h });
		_ux.resize({ w - 1, h });
		_uy.resize({ w, h - 1 });

		_h.fill(_water_height);
		_ux.fill(0.0f);
		_uy.fill(0.0f);

		_generate_terrain();

		_h_prev = _h;
	}

	void timestep(double dt, std::size_t iterations) override {
		if (_impulse) {
			_impulse = false;
			_h(_h.get_size()[0] / 2, _h.get_size()[1] / 2) += _impulse_vel * dt;
		}

		for (std::size_t i = 0; i < iterations; ++i) {
			const auto [vol, cells] = _volume();
			_solve_horizontal(dt);
			_solve_vertical(dt);
			const auto [new_vol, new_cells] = _volume();
			_distribute_volume((vol - new_vol) / new_cells);
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
		const mat44s transform({
			{ 1.0f, 0.0f, 0.0f, -0.5f * _grid_size[0] },
			{ 0.0f, 1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f, -0.5f * _grid_size[1] },
			{ 0.0f, 0.0f, 0.0f, 0.0f },
		});
		const vec2 cell_size = lotus::vec::memberwise_divide(vec2(_grid_size[0], _grid_size[1]), (_h.get_size() - lotus::cvec2u32(1, 1)).into<scalar>());
		_draw_heightfield(_h, _render, cell_size, lotus::linear_rgba_f(0.3f, 0.3f, 1.0f, 1.0f), transform, _get_test_context().wireframe_surfaces);
		_draw_heightfield(_b, _render, cell_size, lotus::linear_rgba_f(0.8f, 0.5f, 0.0f, 1.0f), transform, _get_test_context().wireframe_surfaces);
		_render.flush(ctx, q, uploader, color, depth, size);
	}

	void gui() override {
		ImGui::SliderInt2("Divisions", _size, 4, 2048);
		ImGui::SliderFloat2("Grid Size", _grid_size, 1.0f, 1000.0f);
		ImGui::SliderFloat("Water Height", &_water_height, 0.0f, 10.0f);
		ImGui::SliderFloat("Gravity", &_gravity, -20.0f, 20.0f);
		ImGui::SliderFloat("Damping", &_damping, 0.0f, 1.0f);
		ImGui::Separator();

		if (ImGui::Button("Impulse")) {
			_impulse = true;
		}
		ImGui::SliderFloat2("Impulse Pos", _impulse_pos, 0.0f, 1.0f);
		ImGui::SliderFloat("Impulse Velocity", &_impulse_vel, 0.0f, 300.0f);
		ImGui::Separator();

		bool terrain_changed = false;
		terrain_changed = ImGui::SliderFloat("Terrain Offset", &_terrain_offset, -5.0f, 5.0f) || terrain_changed;
		terrain_changed = ImGui::SliderFloat("Terrain Amplitude", &_terrain_amp, 0.0f, 20.0f) || terrain_changed;
		terrain_changed = ImGui::SliderFloat("Terrain Frequency", &_terrain_freq, 0.0f, 1.0f) || terrain_changed;
		terrain_changed = ImGui::SliderFloat2("Terrain Phase", _terrain_phase, -5.0f, 5.0f) || terrain_changed;
		if (terrain_changed) {
			_generate_terrain();
		}

		test::gui();
	}

	inline static std::string_view get_name() {
		return "Shallow Water";
	}
protected:
	debug_render _render;

	int _size[2] = { 128, 128 };
	float _grid_size[2] = { 10.0f, 10.0f };
	float _water_height = 2.0f;
	float _gravity = 9.8f;
	float _damping = 0.0f;

	grid2<float> _b;
	grid2<float> _h;
	grid2<float> _h_prev;
	grid2<float> _ux;
	grid2<float> _uy;

	bool _impulse = false;
	float _impulse_pos[2] = { 0.0f, 0.0f };
	float _impulse_vel = 100.0f;

	float _terrain_offset = 1.5f;
	float _terrain_amp = 5.0f;
	float _terrain_freq = 0.5f;
	float _terrain_phase[2] = { 0.0f, 0.0f };


	void _generate_terrain() {
		const auto mmul = [](auto x, auto y) {
			return lotus::vec::memberwise_multiply(x, y);
		};
		const auto fract = [](float x) {
			return x - static_cast<int>(x);
		};
		const auto fractv = [](auto x) {
			return x - x.template into<int>().template into<float>();
		};
		const auto hash = [&mmul, &fract, &fractv](vec2 x) {
			const vec2 k(0.3183099, 0.3678794);
			x = mmul(x, k) + vec2(k[1], k[0]);
			return -vec2(1.0f, 1.0f) + 2.0f * fractv(16.0f * k * fract(x[0] * x[1] * (x[0] + x[1])));
		};
		// https://www.shadertoy.com/view/XdXBRH
		const auto noise = [&mmul, &hash](vec2 p) {
			const lotus::cvec2i i(std::floor(p[0]), std::floor(p[1]));
			const vec2 f = p - i.into<float>();
			const vec2 u = mmul(mmul(mmul(f, f), f), mmul(f, (f * 6.0f - vec2(15.0f, 15.0f))) + vec2(10.0f, 10.0f));
			const vec2 ga = hash(i.into<float>() + vec2(0.0f, 0.0f));
			const vec2 gb = hash(i.into<float>() + vec2(1.0f, 0.0f));
			const vec2 gc = hash(i.into<float>() + vec2(0.0f, 1.0f));
			const vec2 gd = hash(i.into<float>() + vec2(1.0f, 1.0f));
			const float va = lotus::vec::dot(ga, f - vec2(0.0f, 0.0f));
			const float vb = lotus::vec::dot(gb, f - vec2(1.0f, 0.0f));
			const float vc = lotus::vec::dot(gc, f - vec2(0.0f, 1.0f));
			const float vd = lotus::vec::dot(gd, f - vec2(1.0f, 1.0f));
			return va + u[0] * (vb - va) + u[1] * (vc - va) + u[0] * u[1] * (va - vb - vc + vd);
		};

		for (std::uint32_t y = 0; y < _size[1]; ++y) {
			const float yp = _terrain_freq * (_grid_size[1] * y / (_size[1] - 1) + _terrain_phase[1]);
			for (std::uint32_t x = 0; x < _size[0]; ++x) {
				const float xp = _terrain_freq * (_grid_size[0] * x / (_size[0] - 1) + _terrain_phase[0]);
				_b(x, y) = _terrain_offset + _terrain_amp * noise(vec2(xp, yp));
			}
		}
	}

	static void _draw_heightfield(
		const grid2<float> &hf,
		debug_render &render,
		vec2 cell_size,
		lotus::linear_rgba_f color,
		mat44s transform,
		bool wireframe
	) {
		std::vector<vec3> pos;
		std::vector<vec3> norm;
		std::vector<std::uint32_t> indices;
		const std::uint32_t w = hf.get_size()[0];
		const std::uint32_t h = hf.get_size()[1];
		for (std::uint32_t y = 0; y < h; ++y) {
			for (std::uint32_t x = 0; x < w; ++x) {
				const std::uint32_t x1y1 = pos.size();
				const std::uint32_t x0y1 = x1y1 - 1;
				const std::uint32_t x1y0 = x1y1 - w;
				const std::uint32_t x0y0 = x0y1 - w;
				pos.emplace_back(x * cell_size[0], hf(x, y), y * cell_size[1]);
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
		render.draw_body(pos, norm, indices, transform, color, wireframe);
	}


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
		return std::max(0.0f, _h(x, y) - _b(x, y)); // this clamp is crucial for stability
	}
	void _solve_horizontal(float dt) {
		const std::uint32_t w = _h.get_size()[0];
		const std::uint32_t h = _h.get_size()[1];
		const float cell_size = _grid_size[0] / (w - 1);
		const float constant = _gravity * (dt * dt) / (cell_size * cell_size);

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
		const float cell_size = _grid_size[1] / (h - 1);
		const float constant = _gravity * (dt * dt) / (cell_size * cell_size);

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

	[[nodiscard]] std::pair<float, std::uint32_t> _volume() const {
		float volume = 0.0f;
		std::uint32_t liquid_cells = 0;
		for (std::uint32_t y = 0; y < _h.get_size()[1]; ++y) {
			for (std::uint32_t x = 0; x < _h.get_size()[0]; ++x) {
				const float hv = _h(x, y);
				const float bv = _b(x, y);
				if (hv > bv) {
					volume += hv - bv;
					++liquid_cells;
				}
			}
		}
		return { volume, liquid_cells };
	}

	void _distribute_volume(float per_cell) {
		for (std::uint32_t y = 0; y < _h.get_size()[1]; ++y) {
			for (std::uint32_t x = 0; x < _h.get_size()[0]; ++x) {
				float &hv = _h(x, y);
				if (hv > _b(x, y)) {
					hv += per_cell;
				}
			}
		}
	}
};
