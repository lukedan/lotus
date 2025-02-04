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
			_storage.resize(_size[0] * _size[1], lotus::zero);
		}
		void fill(const T &val) {
			std::fill(_storage.begin(), _storage.end(), val);
		}

		[[nodiscard]] T &at(std::uint32_t x, std::uint32_t y) {
			lotus::crash_if(x >= get_size()[0] || y >= get_size()[1]);
			return _storage[_size[0] * y + x];
		}
		[[nodiscard]] const T &at(std::uint32_t x, std::uint32_t y) const {
			lotus::crash_if(x >= get_size()[0] || y >= get_size()[1]);
			return _storage[_size[0] * y + x];
		}
		[[nodiscard]] T &operator()(std::uint32_t x, std::uint32_t y) {
			return at(x, y);
		}
		[[nodiscard]] const T &operator()(std::uint32_t x, std::uint32_t y) const {
			return at(x, y);
		}

		[[nodiscard]] lotus::mat22<T> gather_zero(lotus::cvec2i pos) const {
			auto value = [&](int x, int y) -> T {
				if (x < 0 || x >= get_size()[0]) {
					return 0;
				}
				if (y < 0 || y >= get_size()[1]) {
					return 0;
				}
				return at(x, y);
			};
			lotus::mat22<T> result = lotus::uninitialized;
			result(0, 0) = value(pos[0] + 0, pos[1] + 0);
			result(0, 1) = value(pos[0] + 1, pos[1] + 0);
			result(1, 0) = value(pos[0] + 0, pos[1] + 1);
			result(1, 1) = value(pos[0] + 1, pos[1] + 1);
			return result;
		}
		[[nodiscard]] std::pair<T, lotus::mat22<T>> sample_zero(vec2 pos) const {
			const auto x = static_cast<int>(std::floor(pos[0]));
			const auto y = static_cast<int>(std::floor(pos[1]));
			const auto gather_res = gather_zero({ x, y });
			const float lx = pos[0] - x;
			const float ly = pos[1] - y;
			return {
				std::lerp(
					std::lerp(gather_res(0, 0), gather_res(0, 1), lx),
					std::lerp(gather_res(1, 0), gather_res(1, 1), lx),
					ly
				),
				gather_res
			};
		}

		[[nodiscard]] lotus::cvec2u32 get_size() const {
			return _size;
		}
	private:
		std::vector<T> _storage;
		lotus::cvec2u32 _size = lotus::zero;
	};

	class method_base {
	public:
		explicit method_base(const shallow_water_test &test) : _test(&test) {
		}
		virtual ~method_base() = default;

		virtual void soft_reset() = 0;

		virtual void impulse(vec2 pos, float strength, float dt) = 0;

		virtual void timestep(float) = 0;
		[[nodiscard]] virtual float height(std::uint32_t x, std::uint32_t y) const = 0;

		virtual void on_terrain_changed() {
		}
	protected:
		[[nodiscard]] const shallow_water_test &_get_test() const {
			return *_test;
		}
	private:
		const shallow_water_test *_test = nullptr;
	};

	class implicit_separated_method : public method_base {
	public:
		explicit implicit_separated_method(const shallow_water_test &test) : method_base(test) {
		}

		void soft_reset() override {
			const lotus::cvec2u32 sz = _get_test()._terrain.get_size();
			_h.resize(sz);
			_ux.resize(sz - lotus::cvec2u32(1, 0));
			_uy.resize(sz - lotus::cvec2u32(0, 1));

			_h.fill(_get_test()._water_height);
			_ux.fill(0.0f);
			_uy.fill(0.0f);

			_h_prev = _h;
		}

		void impulse(vec2 pos, float strength, float dt) override {
			const auto x = static_cast<int>(std::round(pos[0] * _h.get_size()[0]));
			const auto y = static_cast<int>(std::round(pos[1] * _h.get_size()[1]));
			_h(x, y) += strength * dt;
		}

		void timestep(float dt) override {
			const auto [vol, cells] = _volume();
			_solve_horizontal(dt);
			_solve_vertical(dt);
			const auto [new_vol, new_cells] = _volume();
			_distribute_volume((vol - new_vol) / new_cells);
		}
		[[nodiscard]] float height(std::uint32_t x, std::uint32_t y) const override {
			return _h(x, y);
		}
	private:
		grid2<float> _h;
		grid2<float> _h_prev;
		grid2<float> _ux;
		grid2<float> _uy;

		[[nodiscard]] float _d(std::uint32_t x, std::uint32_t y) const {
			return std::max(0.0f, _h(x, y) - _get_test()._terrain(x, y)); // this clamp is crucial for stability
		}
		void _solve_horizontal(float dt) {
			const std::uint32_t w = _h.get_size()[0];
			const std::uint32_t h = _h.get_size()[1];
			const float cell_size = _get_test()._grid_size[0] / (w - 1);
			const float constant = _get_test()._gravity * (dt * dt) / (cell_size * cell_size);

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
					rhs[x] = h1 + (1.0f - _get_test()._damping) * (h1 - h2);
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
			const float cell_size = _get_test()._grid_size[1] / (h - 1);
			const float constant = _get_test()._gravity * (dt * dt) / (cell_size * cell_size);

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
					rhs[y] = h1 + (1.0f - _get_test()._damping) * (h1 - h2);
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
					const float bv = _get_test()._terrain(x, y);
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
					if (hv > _get_test()._terrain(x, y)) {
						hv += per_cell;
					}
				}
			}
		}
	};

	class explicit_method : public method_base {
	public:
		explicit explicit_method(const shallow_water_test &test) : method_base(test) {
		}

		void soft_reset() override {
			const lotus::cvec2u32 size = _get_test()._terrain.get_size();
			_h.resize(size);
			_ux.resize(size - lotus::cvec2u32(1, 0));
			_uy.resize(size - lotus::cvec2u32(0, 1));

			_ux.fill(0.0f);
			_uy.fill(0.0f);
			const float water_height = _get_test()._water_height;
			for (std::uint32_t y = 0; y < _h.get_size()[1]; ++y) {
				for (std::uint32_t x = 0; x < _h.get_size()[0]; ++x) {
					_h(x, y) = std::max(0.0f, std::lerp(water_height - _get_test()._terrain(x, y), water_height, 1.0f));
				}
			}
		}

		void impulse(vec2 pos, float strength, float dt) override {
			// TODO
		}

		void timestep(float dt) override {
			_advect_velocity(dt);
			_integrate_height(dt);
			_integrate_velocity(dt);
		}
		[[nodiscard]] float height(std::uint32_t x, std::uint32_t y) const override {
			return _get_test()._terrain(x, y) + _h(x, y);
		}
	private:
		grid2<float> _h;
		grid2<float> _ux;
		grid2<float> _uy;

		[[nodiscard]] static grid2<float> _advect_velocity_x(
			const grid2<float> &ux,
			const grid2<float> &uy,
			vec2 offy,
			float dt,
			grid2<vec2> *min_max = nullptr
		) {
			grid2<float> rx(ux.get_size());
			for (std::uint32_t y = 0; y < ux.get_size()[1]; ++y) {
				for (std::uint32_t x = 0; x < ux.get_size()[0]; ++x) {
					const vec2 p(x, y);
					const float vx = ux(x, y);
					const float vy = uy.sample_zero(p + offy).first;
					const auto [smp, gather] = ux.sample_zero(p - vec2(vx, vy) * dt);
					if (min_max) {
						const auto [min, max] = std::minmax({ gather(0, 0), gather(0, 1), gather(1, 0), gather(1, 1) });
						(*min_max)(x, y) = vec2(min, max);
					}
					rx(x, y) = smp;
				}
			}
			return rx;
		}
		void _advect_velocity(float dt) {
			// forward pass
			grid2<vec2> mmx(_ux.get_size());
			grid2<vec2> mmy(_uy.get_size());
			const grid2<float> ux1 = _advect_velocity_x(_ux, _uy, vec2(0.5f, -0.5f), dt, &mmx);
			const grid2<float> uy1 = _advect_velocity_x(_uy, _ux, vec2(0.5f, -0.5f), dt, &mmy);

			// backward pass
			const grid2<float> ux0 = _advect_velocity_x(ux1, uy1, vec2(0.5f, -0.5f), -dt);
			const grid2<float> uy0 = _advect_velocity_x(uy1, ux1, vec2(0.5f, -0.5f), -dt);

			// resolve
			for (std::uint32_t y = 0; y < ux1.get_size()[1]; ++y) {
				for (std::uint32_t x = 0; x < ux1.get_size()[0]; ++x) {
					float &uxo = _ux(x, y);
					const float uxr = ux1(x, y);
					const float ux = uxr - 0.5f * (ux0(x, y) - uxo);
					const vec2 minmax = mmx(x, y);
					uxo = ux > minmax[0] && ux < minmax[1] ? ux : uxr;
				}
			}
			for (std::uint32_t y = 0; y < uy1.get_size()[1]; ++y) {
				for (std::uint32_t x = 0; x < uy1.get_size()[0]; ++x) {
					float &uyo = _uy(x, y);
					const float uyr = uy1(x, y);
					const float uy = uyr - 0.5f * (uy0(x, y) - uyo);
					const vec2 minmax = mmy(x, y);
					uyo = uy > minmax[0] && uy < minmax[1] ? uy : uyr;
				}
			}
		}

		void _integrate_height(float dt) {
			const float dxx = _get_test()._grid_size[0] / (_h.get_size()[0] - 1);
			const float dxy = _get_test()._grid_size[1] / (_h.get_size()[1] - 1);
			const float fx = dt / dxx;
			const float fy = dt / dxy;
			grid2<float> nh(_h.get_size());
			for (std::uint32_t y = 0; y < _h.get_size()[1]; ++y) {
				for (std::uint32_t x = 0; x < _h.get_size()[0]; ++x) {
					const float h = _h(x, y);

					const float uxn = x > 0 ? _ux(x - 1, y) : 0.0f;
					const float uxp = x + 1 < _h.get_size()[0] ? _ux(x, y) : 0.0f;
					const float uyn = y > 0 ? _uy(x, y - 1) : 0.0f;
					const float uyp = y + 1 < _h.get_size()[1] ? _uy(x, y) : 0.0f;

					const float hbxn = uxn > 0.0f ? _h(x - 1, y) : h;
					const float hbxp = uxp < 0.0f ? _h(x + 1, y) : h;
					const float hbyn = uyn > 0.0f ? _h(x, y - 1) : h;
					const float hbyp = uyp < 0.0f ? _h(x, y + 1) : h;

					nh(x, y) = std::max(0.0f, h - (fx * (hbxp * uxp - hbxn * uxn) + fy * (hbyp * uyp - hbyn * uyn)));
				}
			}
			_h = std::move(nh);
		}

		[[nodiscard]] float _eta(std::uint32_t x, std::uint32_t y) const {
			return _get_test()._terrain(x, y) + _h(x, y);
		}
		void _integrate_velocity(float dt) {
			const float dxx = _get_test()._grid_size[0] / (_h.get_size()[0] - 1);
			const float dxy = _get_test()._grid_size[1] / (_h.get_size()[1] - 1);
			// TODO no external acceleration
			const float fx = _get_test()._gravity * dt / dxx;
			const float fy = _get_test()._gravity * dt / dxy;
			for (std::uint32_t y = 0; y < _ux.get_size()[1]; ++y) {
				for (std::uint32_t x = 0; x < _ux.get_size()[0]; ++x) {
					_ux(x, y) -= fx * (_eta(x + 1, y) - _eta(x, y));
				}
			}
			for (std::uint32_t y = 0; y < _uy.get_size()[1]; ++y) {
				for (std::uint32_t x = 0; x < _uy.get_size()[0]; ++x) {
					_uy(x, y) -= fy * (_eta(x, y + 1) - _eta(x, y));
				}
			}
		}
	};

	explicit shallow_water_test(const test_context &tctx) : test(tctx) {
		soft_reset();
	}

	void soft_reset() override {
		_render = debug_render();
		_render.ctx = &_get_test_context();

		_terrain.resize({ _size[0], _size[1] });
		_generate_terrain();

		// called last because it might depend on the terrain
		if (_method) {
			_method->soft_reset();
		}
	}

	void timestep(double dt, std::size_t iterations) override {
		if (!_method) {
			return;
		}

		if (_impulse) {
			_impulse = false;
			_method->impulse(vec2(_impulse_pos[0], _impulse_pos[1]), _impulse_vel, dt);
		}
		for (std::size_t i = 0; i < iterations; ++i) {
			_method->timestep(dt / iterations);
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
		const vec2 cell_size = lotus::vec::memberwise_divide(vec2(_grid_size[0], _grid_size[1]), (_terrain.get_size() - lotus::cvec2u32(1, 1)).into<scalar>());
		if (_method) {
			_draw_heightfield(
				[this](std::uint32_t x, std::uint32_t y) {
					return _method->height(x, y);
				},
				_terrain.get_size(), _render, cell_size, lotus::linear_rgba_f(0.3f, 0.3f, 1.0f, 1.0f), transform, _get_test_context().wireframe_surfaces
			);
		}
		_draw_heightfield(_terrain, _render, cell_size, lotus::linear_rgba_f(0.8f, 0.5f, 0.0f, 1.0f), transform, _get_test_context().wireframe_surfaces);
		_render.flush(ctx, q, uploader, color, depth, size);
	}

	void gui() override {
		static const char *const _method_names[] = {
			"None",
			"Implicit Separated",
			"Explicit",
		};
		static std::unique_ptr<method_base> (*_method_creators[])(shallow_water_test*) = {
			[](shallow_water_test*) -> std::unique_ptr<method_base> { return nullptr; },
			[](shallow_water_test *t) -> std::unique_ptr<method_base> { return std::make_unique<implicit_separated_method>(*t); },
			[](shallow_water_test *t) -> std::unique_ptr<method_base> { return std::make_unique<explicit_method>(*t); },
		};
		static_assert(std::size(_method_names) == std::size(_method_creators), "Array sizes mismatch");
		if (ImGui::Combo("Method", &_method_index, _method_names, std::size(_method_names))) {
			_method = _method_creators[_method_index](this);
			if (_method) {
				_method->soft_reset();
			}
		}
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
	std::unique_ptr<method_base> _method;

	int _size[2] = { 128, 128 };
	float _grid_size[2] = { 10.0f, 10.0f };
	float _water_height = 2.0f;
	float _gravity = 9.8f;
	float _damping = 0.0f;

	grid2<float> _terrain;

	bool _impulse = false;
	float _impulse_pos[2] = { 0.5f, 0.5f };
	float _impulse_vel = 100.0f;

	float _terrain_offset = 1.5f;
	float _terrain_amp = 5.0f;
	float _terrain_freq = 0.5f;
	float _terrain_phase[2] = { 0.0f, 0.0f };

	int _method_index = 0;


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

		const auto w = _terrain.get_size()[0];
		const auto h = _terrain.get_size()[1];
		for (std::uint32_t y = 0; y < h; ++y) {
			const float yp = _terrain_freq * (_grid_size[1] * y / (h - 1) + _terrain_phase[1]);
			for (std::uint32_t x = 0; x < w; ++x) {
				const float xp = _terrain_freq * (_grid_size[0] * x / (w - 1) + _terrain_phase[0]);
				_terrain(x, y) = _terrain_offset + _terrain_amp * noise(vec2(xp, yp));
			}
		}

		if (_method) {
			_method->on_terrain_changed();
		}
	}

	template <typename Callback> static void _draw_heightfield(
		Callback &&height,
		lotus::cvec2u32 sz,
		debug_render &render,
		vec2 cell_size,
		lotus::linear_rgba_f color,
		mat44s transform,
		bool wireframe
	) {
		std::vector<vec3> pos;
		std::vector<vec3> norm;
		std::vector<std::uint32_t> indices;
		for (std::uint32_t y = 0; y < sz[1]; ++y) {
			for (std::uint32_t x = 0; x < sz[0]; ++x) {
				const std::uint32_t x1y1 = pos.size();
				const std::uint32_t x0y1 = x1y1 - 1;
				const std::uint32_t x1y0 = x1y1 - sz[0];
				const std::uint32_t x0y0 = x0y1 - sz[0];
				pos.emplace_back(x * cell_size[0], height(x, y), y * cell_size[1]);
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
		for (std::uint32_t y = 0; y < sz[1]; ++y) {
			for (std::uint32_t x = 0; x < sz[0]; ++x) {
				const auto get_pos = [&](std::uint32_t xp, std::uint32_t yp) {
					return pos[yp * sz[0] + xp];
				};
				const vec3 xn = get_pos(x > 0 ? x - 1 : x, y);
				const vec3 xp = get_pos(x + 1 < sz[0] ? x + 1 : x, y);
				const vec3 yn = get_pos(x, y > 0 ? y - 1 : y);
				const vec3 yp = get_pos(x, y + 1 < sz[1] ? y + 1 : y);
				norm.emplace_back(lotus::vec::unsafe_normalize(lotus::vec::cross(xp - xn, yp - yn)));
			}
		}
		render.draw_body(pos, norm, indices, transform, color, wireframe);
	}
	static void _draw_heightfield(
		const grid2<float> &hf,
		debug_render &render,
		vec2 cell_size,
		lotus::linear_rgba_f color,
		mat44s transform,
		bool wireframe
	) {
		_draw_heightfield(hf, hf.get_size(), render, cell_size, color, transform, wireframe);
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
};
