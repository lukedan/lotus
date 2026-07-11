#pragma once

/// \file
/// Basic implementation of an application class.

#include <stack>
#include <fstream>

#include <lotus/utils/strings.h>
#include <lotus/profiler.h>
#include <lotus/system/application.h>
#include <lotus/system/window.h>
#include <lotus/gpu/context.h>
#include <lotus/gpu/device.h>
#include <lotus/renderer/context/context.h>
#include <lotus/renderer/context/asset_manager.h>
#include <lotus/renderer/context/constant_uploader.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <lotus/system/dear_imgui.h>
#include <lotus/renderer/dear_imgui.h>
#include <lotus/renderer/debug_drawing.h>

namespace lotus::helpers {
	/// Base class for a test application that comes with a window, a GPU context, a rendering context, ImGUI, and
	/// other utilities.
	class application {
	public:
		/// Initializes the application.
		application(int argc, char **argv, std::u8string_view app_name) :
			_argc(argc),
			_argv(argv),
			_app(app_name),
			_gpu_adapter_properties(uninitialized),
			_shader_utils(gpu::shader_utility::create()) {

			_window = _wrap(new auto(_app.create_window()));
			_window->set_title(app_name);
		}
		/// Default virtual destructor.
		virtual ~application() = default;

		/// Initializes GPU resources. This should be called immediately after the constructor.
		void initialize() {
			gpu::context_options gpu_context_options = gpu::context_options::none;

			// parse command line args
			for (int i = 1; i < _argc; ++i) {
				if (std::string_view(_argv[i]) == "-v") {
					gpu_context_options |= gpu::context_options::enable_validation;
				} else if (std::string_view(_argv[i]) == "-d") {
					gpu_context_options |= gpu::context_options::enable_debug_info;
				}
			}

			_gpu_context = _wrap(new auto(gpu::context::create(
				gpu_context_options,
				[this](
					gpu::debug_message_severity severity,
					gpu::context::debug_message_id id,
					std::u8string_view msg
				) {
					_on_gpu_debug_message(severity, id, msg);
				}
			)));

			gpu::adapter best_adapter = nullptr;
			{ // choose adapter
				int best_adapter_score = std::numeric_limits<int>::min();
				std::vector<gpu::adapter> adapters = _gpu_context->get_all_adapters();
				for (gpu::adapter &adap : adapters) {
					const int score = _score_device(adap);
					if (score > best_adapter_score) {
						best_adapter_score = score;
						best_adapter = adap;
					}
				}
			}

			{ // create device, context, and swap chain
				_gpu_adapter_properties = best_adapter.get_properties();
				log().debug("Choosing adapter: {}", string::to_generic(_gpu_adapter_properties.name));
				std::vector<gpu::command_queue> gpu_cmd_queues;
				std::tie(_gpu_device, gpu_cmd_queues) = best_adapter.create_device(_get_desired_queues());

				_context = _wrap(new auto(renderer::context::create(
					*_gpu_context, _gpu_adapter_properties, _gpu_device, gpu_cmd_queues
				)));
				_context->on_batch_statistics_available =
					[this](renderer::batch_index, renderer::batch_statistics_late stats) {
						batch_stats_late = std::move(stats);
					};
			}

			// get queues
			_asset_loading_queue = _context->get_queue(_get_asset_loading_queue_index());
			_constant_upload_queue = _context->get_queue(_get_constant_upload_queue_index());
			_debug_drawing_queue = _context->get_queue(_get_debug_drawing_queue_index());
			_present_queue = _context->get_queue(_get_present_queue_index());

			// create pools
			_imgui_pool = _context->request_pool(u8"ImGUI Resources", gpu::memory_type_index::invalid, 1024 * 1024);
			_constant_pool = _context->request_pool(
				u8"Constants", gpu::memory_type_index::invalid, 8 * 1024 * 1024
			);
			_constant_upload_pool = _context->request_pool(
				u8"Uploaded constants", _context->get_upload_memory_type_index(), 8 * 1024 * 1024
			);

			// create swap chain
			_swap_chain = _context->request_swap_chain(
				u8"Swap Chain",
				*_window, _present_queue, 2, { gpu::format::r8g8b8a8_srgb, gpu::format::b8g8r8a8_srgb }
			);

			// create asset manager
			_assets = _wrap(new auto(renderer::assets::manager::create(
				*_context, _asset_loading_queue, &_shader_utils
			)));
			if (const char *asset_library_path = std::getenv("LOTUS_ASSET_LIBRARY_PATH")) {
				_assets->asset_library_path = asset_library_path;
			} else {
				crash_if(true); // missing environment variable LOTUS_ASSET_LIBRARY_PATH
			}
			_assets->additional_shader_include_paths = _get_additional_shader_include_paths();

			// initialize ImGUI and debug drawing
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGui::StyleColorsDark();
			_imgui_sctx = _wrap(new auto(system::dear_imgui::context::create()));
			_imgui_rctx = _wrap(new auto(renderer::dear_imgui::context::create(
				*_assets, _debug_drawing_queue
			)));

			_window->on_resize = [this](system::window_events::resize &resize) {
				_on_resize_raw(resize);
			};
			_window->on_close_request = [this](system::window_events::close_request &req) {
				_on_close_request(req);
			};
			_window->on_mouse_move = [this](system::window_events::mouse::move &move) {
				_on_mouse_move_raw(move);
			};
			_window->on_mouse_leave = [this]() {
				_on_mouse_leave_raw();
			};
			_window->on_mouse_button_down = [this](system::window_events::mouse::button_down &down) {
				_on_mouse_down_raw(down);
			};
			_window->on_mouse_button_up = [this](system::window_events::mouse::button_up &up) {
				_on_mouse_up_raw(up);
			};
			_window->on_mouse_scroll = [this](system::window_events::mouse::scroll &sc) {
				_on_mouse_scroll_raw(sc);
			};
			_window->on_capture_broken = [this]() {
				_on_capture_broken_raw();
			};
			_window->on_key_down = [this](system::window_events::key_down &down) {
				_on_key_down_raw(down);
			};
			_window->on_key_up = [this](system::window_events::key_up &up) {
				_on_key_up_raw(up);
			};
			_window->on_text_input = [this](system::window_events::text_input &text) {
				_on_text_input_raw(text);
			};

			// finish
			_on_initialized();
		}

		/// Runs the application until exit.
		int run() {
			crash_if(!_context); // not initialized!
			_window->show_and_activate();
			bool quit = false;
			while (!quit) {
				system::message_type msg_type;
				do {
					msg_type = _app.process_message_nonblocking();
					quit = quit || msg_type == system::message_type::quit;
				} while (msg_type != system::message_type::none);

				if (_get_back_buffer_size() == zero) {
					continue;
				}

				_process_frame_full();
			}
			return 0;
		}

		/// "Early" statistics of the previous batch.
		std::vector<renderer::batch_statistics_early> batch_stats_early;
		/// "Late" statistics of the previous batch.
		renderer::batch_statistics_late batch_stats_late = zero;
		f32 cpu_frame_time_ms = 0.0f; ///< CPU time of previous frame in milliseconds.
	protected:
		const int _argc;
		const char *const *const _argv;

		system::application _app; ///< The system application.
		std::unique_ptr<system::window> _window; ///< The main window.

		std::unique_ptr<gpu::context> _gpu_context; ///< The GPU context.
		gpu::device _gpu_device = nullptr; ///< The GPU device.
		gpu::adapter_properties _gpu_adapter_properties; ///< The GPU adapter properties.
		gpu::shader_utility _shader_utils; ///< Shader utilities.

		std::unique_ptr<renderer::context> _context; ///< The renderer context.
		std::unique_ptr<renderer::assets::manager> _assets; ///< The asset manager.
		renderer::swap_chain _swap_chain = nullptr; ///< The swap chain.


		/// Returns the current window size.
		[[nodiscard]] cvec2f64 _get_window_size() const {
			return _window_size;
		}
		/// Returns the size of the back buffer in pixels.
		[[nodiscard]] cvec2u32 _get_back_buffer_size() const {
			return _back_buffer_size;
		}


		/// Scores an adapter. The GPU device will be created using the one with the highest score. Derived classes can
		/// override this to control device selection strategy. By default, discrete devices are scored as 1 and
		/// otherwise 0.
		[[nodiscard]] virtual int _score_device(gpu::adapter &adapter) const {
			auto props = adapter.get_properties();
			return props.is_discrete ? 1 : 0;
		}
		/// Returns the queue types for GPU queues that should be created.
		[[nodiscard]] virtual std::span<const gpu::queue_family> _get_desired_queues() const = 0;
		/// Derived classes should override this to return the desired GPU queue index used for loading assets.
		[[nodiscard]] virtual u32 _get_asset_loading_queue_index() const = 0;
		/// Derived classes should override this to return the desired GPU queue index used for uploading constant
		/// buffers.
		[[nodiscard]] virtual u32 _get_constant_upload_queue_index() const = 0;
		/// Derived classes should override this to return the desired GPU queue index used for ImGUI and debug
		/// drawing.
		[[nodiscard]] virtual u32 _get_debug_drawing_queue_index() const = 0;
		/// Derived classes should override this to return the desired GPU queue index used for presenting to swap
		/// chains.
		[[nodiscard]] virtual u32 _get_present_queue_index() const = 0;
		/// Derived classes should override this and return additional shader include paths.
		[[nodiscard]] virtual std::vector<std::filesystem::path> _get_additional_shader_include_paths() const = 0;

		/// Called when the application has been fully initialized.
		virtual void _on_initialized() {
		}

		/// Callback for when the window is resized.
		virtual void _on_resize(system::window_events::resize&) {
		}
		/// Callback for when the user requests to close the window. By default, this function quits unconditionally.
		virtual void _on_close_request(system::window_events::close_request &req) {
			req.should_close = true;
			_app.quit();
		}
		/// Callback for non-ImGUI mouse move events.
		virtual void _on_mouse_move(system::window_events::mouse::move&) {
		}
		/// Callback for mouse leave events.
		virtual void _on_mouse_leave() {
		}
		/// Callback for non-ImGUI mouse down events.
		virtual void _on_mouse_down(system::window_events::mouse::button_down&) {
		}
		/// Callback for non-ImGUI mouse up events.
		virtual void _on_mouse_up(system::window_events::mouse::button_up&) {
		}
		/// Callback for non-ImGUI mouse scroll events.
		virtual void _on_mouse_scroll(system::window_events::mouse::scroll&) {
		}
		/// Callback for when mouse capture is broken.
		virtual void _on_capture_broken() {
		}
		/// Callback for non-ImGUI key down events.
		virtual void _on_key_down(system::window_events::key_down&) {
		}
		/// Callback for non-ImGUI key up events.
		virtual void _on_key_up(system::window_events::key_up&) {
		}
		/// Callback for non-ImGUI text input events.
		virtual void _on_text_input(system::window_events::text_input&) {
		}

		/// Filters out certain backend-specific messages.
		template <gpu::backend_type Backend> static bool _filter_message(gpu::context::debug_message_id id) {
			if constexpr (Backend == gpu::backend_type::vulkan) {
				return
					id != static_cast<gpu::context::debug_message_id>(0xFC68BE96) &&
					id != static_cast<gpu::context::debug_message_id>(0xA5625282);
			}
			return true;
		}
		/// Callback for debug messages from the graphics API.
		virtual void _on_gpu_debug_message(
			gpu::debug_message_severity severity,
			gpu::context::debug_message_id id,
			std::u8string_view msg
		) {
			if (!_filter_message<gpu::current_backend>(id)) {
				return;
			}
			switch (severity) {
			case gpu::debug_message_severity::debug:
				log().debug("{}", string::to_generic(msg));
				break;
			case gpu::debug_message_severity::information:
				log().info("{}", string::to_generic(msg));
				break;
			case gpu::debug_message_severity::warning:
				log().warn("{}", string::to_generic(msg));
				break;
			case gpu::debug_message_severity::error:
				log().error("{}", string::to_generic(msg));
				break;
			}
		}

		/// Callback for frame processing. Presenting, debug drawing, and ImGUI do not need to be handled. The
		/// dependency objects need to be acquired on all queues that use uploaded constants or assets.
		virtual void _process_frame(
			renderer::constant_uploader&, renderer::dependency constants_dep, renderer::dependency assets_dep
		) = 0;
		/// Derived classes should override this to use ImGUI. No need to call \p ImGui::NewFrame() or
		/// \p ImGui::Render().
		virtual void _process_imgui() {
		}

		/// Shows an ImGUI window with all the statistics.
		void _show_statistics_window() {
			if (ImGui::Begin("Statistics")) {
				ImGui::Text("CPU: %f ms", cpu_frame_time_ms);

				ImGui::Separator();
				ImGui::Text("GPU Timers");
				if (ImGui::BeginTable("TimersTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableSetupColumn("Name");
					ImGui::TableSetupColumn("Duration (ms)");
					ImGui::TableHeadersRow();

					for (const auto &queue_results : batch_stats_late.timer_results) {
						for (const auto &t : queue_results) {
							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							std::string n(string::to_generic(t.name));
							ImGui::Text("%s", n.c_str());

							ImGui::TableNextColumn();
							ImGui::Text("%f", t.duration_ms);
						}
					}

					ImGui::EndTable();
				}

				/*
				ImGui::Separator();
				ImGui::Text("Constant Buffer Uploads");
				if (ImGui::BeginTable("CBTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableSetupColumn("Size");
					ImGui::TableSetupColumn("Size Without Padding");
					ImGui::TableSetupColumn("Num Buffers");
					ImGui::TableHeadersRow();

					for (const auto &queue_stats : batch_stats_early) {
						for (const auto &b : queue_stats.immediate_constant_buffers) {
							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::Text("%" PRIu32, b.immediate_constant_buffer_size);

							ImGui::TableNextColumn();
							ImGui::Text("%" PRIu32, b.immediate_constant_buffer_size_no_padding);

							ImGui::TableNextColumn();
							ImGui::Text("%" PRIu32, b.num_immediate_constant_buffers);
						}
					}

					ImGui::EndTable();
				}

				ImGui::Separator();
				ImGui::Text("Constant Buffers");
				if (ImGui::BeginTable("CBSigTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableSetupColumn("Count");
					ImGui::TableSetupColumn("Hash");
					ImGui::TableSetupColumn("Size");
					ImGui::TableHeadersRow();

					usize keep_count = 5;
					std::vector<_constant_buffer_info> heap;
					std::greater<_constant_buffer_info> pred;
					for (const auto &queue_stats : batch_stats_early) {
						for (auto &&[sig, count] : queue_stats.constant_buffer_counts) {
							heap.emplace_back(sig, count);
							std::push_heap(heap.begin(), heap.end(), pred);
							if (heap.size() > keep_count) {
								std::pop_heap(heap.begin(), heap.end(), pred);
								heap.pop_back();
							}
						}
					}

					std::sort(heap.begin(), heap.end(), pred);

					for (const auto &p : heap) {
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::Text("%" PRIu32, p.count);

						ImGui::TableNextColumn();
						ImGui::Text("0x%08" PRIX32, p.signature.hash);

						ImGui::TableNextColumn();
						ImGui::Text("%" PRIu32, p.signature.size);
					}

					ImGui::EndTable();
				}
				*/

				ImGui::Separator();
				ImGui::Text("Transitions");
				if (ImGui::BeginTable("TransitionTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableSetupColumn("Image2D");
					ImGui::TableSetupColumn("Image3D");
					ImGui::TableSetupColumn("Buffer");
					ImGui::TableSetupColumn("Raw Buffer");
					ImGui::TableHeadersRow();

					for (const auto &queue_stats : batch_stats_early) {
						for (const auto &t : queue_stats.transitions) {
							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::Text("%" PRIu32 " (%" PRIu32 ")", t.submitted_image2d_transitions, t.requested_image2d_transitions);

							ImGui::TableNextColumn();
							ImGui::Text("%" PRIu32 " (%" PRIu32 ")", t.submitted_image3d_transitions, t.requested_image3d_transitions);

							ImGui::TableNextColumn();
							ImGui::Text("%" PRIu32 " (%" PRIu32 ")", t.submitted_buffer_transitions, t.requested_buffer_transitions);

							ImGui::TableNextColumn();
							ImGui::Text("%" PRIu32 " (%" PRIu32 ")", t.submitted_raw_buffer_transitions, t.requested_raw_buffer_transitions);
						}
					}

					ImGui::EndTable();
				}
			}
			ImGui::End();
		}
		/// Indicates that the profiler window should be shown.
		void _show_cpu_profiler_window() {
			_profiler_open = true;
		}
	private:
		/*
		/// Information about constant buffers and how many times they're used.
		struct _constant_buffer_info {
			/// Initializes all fields of this struct.
			_constant_buffer_info(renderer::statistics::constant_buffer_signature sig, u32 c) :
				signature(sig), count(c) {
			}

			/// Comparison of usage counts.
			[[nodiscard]] friend std::strong_ordering operator<=>(
				_constant_buffer_info lhs, _constant_buffer_info rhs
			) {
				return lhs.count <=> rhs.count;
			}

			/// The signature of the constant buffer.
			renderer::statistics::constant_buffer_signature signature = zero;
			u32 count = 0; ///< The number of times it's used.
		};
		*/

		bool _profiler_open = true; ///< Whether the profiler is open.
		bool _profiler_frozen = false; ///< Whether the profiler is frozen.
		f32 _profiler_scale = 1.0f; ///< Scale for the profiler view.
		std::vector<profiler::thread_samples> _profiler_frame; ///< Currently displayed profiler frame.

		std::unique_ptr<system::dear_imgui::context> _imgui_sctx; ///< System context for ImGUI.
		std::unique_ptr<renderer::dear_imgui::context> _imgui_rctx; ///< Graphics context for ImGUI.

		renderer::context::queue _asset_loading_queue = nullptr; ///< Queue used for asset loading.
		renderer::context::queue _constant_upload_queue = nullptr; ///< Queue used for constant buffer uploading.
		renderer::context::queue _debug_drawing_queue = nullptr; ///< Queue used for debug drawing.
		renderer::context::queue _present_queue = nullptr; ///< Queue used for presenting.

		renderer::pool _imgui_pool = nullptr; ///< Pool used for ImGUI resources.
		renderer::pool _constant_pool = nullptr; ///< Pool used for constant buffers.
		renderer::pool _constant_upload_pool = nullptr; ///< Pool used for uploading constant buffers.

		cvec2f64 _window_size = zero; ///< Current window size.
		cvec2u32 _back_buffer_size = zero; ///< Current back buffer size in pixels.


		/// Wraps an object created by \p new with a \p std::unique_ptr.
		template <typename T> static std::unique_ptr<T> _wrap(T *new_ptr) {
			return std::unique_ptr<T>(new_ptr);
		}

		/// Updates ImGUI window size, then calls \ref _on_resize().
		void _on_resize_raw(system::window_events::resize &resize) {
			_window_size = resize.new_size;
			_back_buffer_size = _window_size.into<u32>();

			_imgui_sctx->on_resize(resize);
			_swap_chain.resize(_back_buffer_size);
			_on_resize(resize);
		}
		/// Filters out ImGUI mouse move events.
		void _on_mouse_move_raw(system::window_events::mouse::move &move) {
			_imgui_sctx->on_mouse_move(move);
			if (!ImGui::GetIO().WantCaptureMouse) {
				_on_mouse_move(move);
			}
		}
		/// Passes the message to ImGUI.
		void _on_mouse_leave_raw() {
			_imgui_sctx->on_mouse_leave();
			_on_mouse_leave();
		}
		/// Filters out ImGUI mouse down events.
		void _on_mouse_down_raw(system::window_events::mouse::button_down &down) {
			_imgui_sctx->on_mouse_down(*_window, down);
			if (!ImGui::GetIO().WantCaptureMouse) {
				_on_mouse_down(down);
			}
		}
		/// Filters out ImGUI mouse up events.
		void _on_mouse_up_raw(system::window_events::mouse::button_up &up) {
			_imgui_sctx->on_mouse_up(*_window, up);
			if (!ImGui::GetIO().WantCaptureMouse) {
				_on_mouse_up(up);
			}
		}
		/// Filters out ImGUI mouse scroll events.
		void _on_mouse_scroll_raw(system::window_events::mouse::scroll &sc) {
			_imgui_sctx->on_mouse_scroll(sc);
			if (!ImGui::GetIO().WantCaptureMouse) {
				_on_mouse_scroll(sc);
			}
		}
		/// Notifies ImGUI of the event, then calls \ref _on_capture_broken().
		void _on_capture_broken_raw() {
			_imgui_sctx->on_capture_broken();
			_on_capture_broken();
		}
		// TODO these handlers may need more work
		/// Filters out ImGUI key down events.
		void _on_key_down_raw(system::window_events::key_down &down) {
			if (ImGui::GetIO().WantCaptureKeyboard) {
				_imgui_sctx->on_key_down(down);
			} else {
				_on_key_down(down);
			}
		}
		/// Filters out ImGUI key up events.
		void _on_key_up_raw(system::window_events::key_up &up) {
			if (ImGui::GetIO().WantCaptureKeyboard) {
				_imgui_sctx->on_key_up(up);
			} else {
				_on_key_up(up);
			}
		}
		/// Filters out ImGUI text input events.
		void _on_text_input_raw(system::window_events::text_input &text) {
			if (ImGui::GetIO().WantCaptureKeyboard) {
				_imgui_sctx->on_text_input(text);
			} else {
				_on_text_input(text);
			}
		}

		/// Shows an ImGUI window with CPU profiler statistics.
		void _process_cpu_profiler_window() {
			profiler::scope p1;

			const u64 frequency = profiler::get_timer_frequency();
			const auto into_seconds = [frequency](profiler::time_t d) {
				return static_cast<f64>(d) / static_cast<f64>(frequency);
			};
			const auto format_seconds = [](f64 seconds) -> std::string {
				if (seconds > 0.1f) {
					return std::format("{:g}s", seconds);
				}
				if (seconds > 0.1f / 1000.0f) {
					return std::format("{:g}ms", seconds * 1000.0f);
				}
				if (seconds > 0.1f / 1000000.0f) {
					return std::format("{:g}us", seconds * 1000000.0f);
				}
				return std::format("{:g}ns", seconds * 1000000000.0f);
			};

			if (!_profiler_open) {
				return;
			}

			const char *const left_arrow = "<";
			const char *const right_arrow = ">";
			const f32 left_arrow_width = ImGui::CalcTextSize(left_arrow).x;
			const f32 right_arrow_width = ImGui::CalcTextSize(right_arrow).x;

			ImGui::SetNextWindowSize(ImVec2(1000.0f, 300.0f), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("CPU Profiler", &_profiler_open)) {
				ImGui::Checkbox("Frozen", &_profiler_frozen);
				ImGui::SameLine();
				ImGui::SliderFloat("Scale", &_profiler_scale, 0.0f, 1.0f);

				// find min/max timestamps
				profiler::time_t timestamp_min = std::numeric_limits<profiler::time_t>::max();
				profiler::time_t timestamp_max = std::numeric_limits<profiler::time_t>::min();
				for (const profiler::thread_samples &t : _profiler_frame) {
					for (const profiler::samples &s : t.batches) {
						if (!s.timestamps.empty()) {
							timestamp_min = std::min(timestamp_min, s.timestamps.front().time);
							break;
						}
					}
					for (auto it = t.batches.rbegin(); it != t.batches.rend(); ++it) {
						if (!it->timestamps.empty()) {
							timestamp_max = std::max(timestamp_max, it->timestamps.back().time);
							break;
						}
					}
				}
				const f64 duration = into_seconds(timestamp_max - timestamp_min);

				const f32 window_width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize;
				ImGui::SetNextWindowContentSize(ImVec2(window_width / _profiler_scale, 0.0f));
				constexpr ImGuiWindowFlags enable_all_scrollbars =
					ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar;
				if (ImGui::BeginChild("Profiler Contents", ImVec2(0.0f, 0.0f), 0, enable_all_scrollbars)) {
					const f32 scroll_x = ImGui::GetScrollX();
					const f32 bar_height = ImGui::GetTextLineHeight();
					const f64 time_scale = window_width / (_profiler_scale * duration);
					for (usize ti = 0; ti < _profiler_frame.size(); ++ti) {
						ImGui::PushID(static_cast<int>(ti));
						const profiler::thread_samples &thread = _profiler_frame[ti];

						ImGui::SetCursorPosX(scroll_x);
						ImGui::Text("%s", std::format("Thread {}", thread.thread_id).c_str());
						const ImVec2 cursor_screen = ImGui::GetCursorScreenPos();
						usize max_stack_depth = 0;
						for (usize bi = 0; bi < thread.batches.size(); ++bi) {
							ImGui::PushID(static_cast<int>(bi));
							const profiler::samples &samples = thread.batches[bi];

							std::stack<profiler::timestamp> sample_stack;
							for (usize si = 0; si < samples.timestamps.size(); ++si) {
								const profiler::timestamp &ts = samples.timestamps[si];

								if (ts.label) {
									sample_stack.emplace(ts);
									continue;
								}

								crash_if(sample_stack.empty());
								const profiler::timestamp tbeg = sample_stack.top();
								sample_stack.pop();
								const usize stack_depth = sample_stack.size();
								max_stack_depth = std::max(stack_depth, max_stack_depth);

								const f64 beg = into_seconds(tbeg.time - timestamp_min);
								const f64 end = into_seconds(ts.time - timestamp_min);
								const auto *label = reinterpret_cast<const char*>(tbeg.label);
								const f64 xbeg = time_scale * beg;
								const f64 xend = xbeg + time_scale * (end - beg);
								const f64 clamped_xbeg = std::max<f64>(scroll_x, xbeg);
								const f64 clamped_xend = std::min<f64>(scroll_x + window_width, xend);
								if (clamped_xbeg < clamped_xend) {
									ImGui::PushID(static_cast<int>(si));

									const ImVec2 rect_min = ImVec2(
										static_cast<f32>(clamped_xbeg),
										bar_height * static_cast<f32>(stack_depth)
									);
									const ImVec2 rect_size = ImVec2(
										std::max<f32>(FLT_MIN, static_cast<f32>(clamped_xend - clamped_xbeg)),
										bar_height
									);
									const ImVec2 rect_min_ss = cursor_screen + rect_min;
									const ImVec2 rect_max_ss = cursor_screen + rect_min + rect_size;

									const bool hover = ImGui::IsMouseHoveringRect(rect_min_ss, rect_max_ss);
									if (hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
										_profiler_scale = static_cast<f32>((end - beg) / duration);
										ImGui::SetScrollX(static_cast<f32>(
											beg * (window_width / (duration * _profiler_scale))
										));
									}

									ImGui::GetWindowDrawList()->AddRectFilled(
										rect_min_ss,
										rect_max_ss,
										ImGui::GetColorU32(hover ? ImGuiCol_ButtonHovered : ImGuiCol_Button)
									);
									ImGui::GetWindowDrawList()->AddRect(
										rect_min_ss,
										rect_max_ss,
										ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, std::min(rect_size.x, 1.0f)))
									);
									if (rect_size.x > 1.0f) { // don't draw text (costly) if the rect is too small
										ImVec2 text_pos = rect_min_ss;
										ImVec4 clip_rect =
											ImVec4(rect_min_ss.x, rect_min_ss.y, rect_max_ss.x, rect_max_ss.y);
										if (clamped_xbeg > xbeg) {
											ImGui::GetWindowDrawList()->AddText(
												ImGui::GetFont(),
												ImGui::GetFontSize(),
												text_pos,
												ImGui::GetColorU32(ImGuiCol_TextLink),
												left_arrow,
												nullptr,
												0.0f,
												&clip_rect
											);
											text_pos.x += left_arrow_width;
											clip_rect.x += left_arrow_width;
										}
										if (clamped_xend < xend) {
											ImGui::GetWindowDrawList()->AddText(
												ImGui::GetFont(),
												ImGui::GetFontSize(),
												ImVec2(rect_max_ss.x - right_arrow_width, text_pos.y),
												ImGui::GetColorU32(ImGuiCol_TextLink),
												right_arrow,
												nullptr,
												0.0f,
												&clip_rect
											);
											clip_rect.z -= right_arrow_width;
										}
										ImGui::GetWindowDrawList()->AddText(
											ImGui::GetFont(),
											ImGui::GetFontSize(),
											text_pos + ImVec2(3.0f, 0.0f),
											ImGui::GetColorU32(ImGuiCol_Text),
											label,
											nullptr,
											0.0f,
											&clip_rect
										);
									}

									if (hover) {
										if (ImGui::BeginTooltip()) {
											ImGui::Text("%s", label);
											ImGui::Text(
												"%s - %s",
												format_seconds(beg).c_str(),
												format_seconds(end).c_str()
											);
											ImGui::Text(
												"Duration: %s",
												format_seconds(into_seconds(ts.time - tbeg.time)).c_str()
											);
											ImGui::EndTooltip();
										}
									}

									ImGui::PopID();
								}
							}
							ImGui::PopID();
						}
						ImGui::PopID();
					}
				}
				ImGui::EndChild();
			}
			ImGui::End();
		}

		/// Processes a single frame.
		void _process_frame_full() {
			profiler::thread_manager::get_thread_data().flush();
			const std::vector<profiler::thread_samples> profiler_output =
				profiler::thread_manager::instance().flush();
			if (!_profiler_frozen) {
				_profiler_frame = profiler_output;
			}

			profiler::scope p1(u8"Frame");

			auto frame_cpu_begin = std::chrono::high_resolution_clock::now();

			renderer::dependency asset_dep = _context->request_dependency(u8"Asset upload dependency");
			renderer::dependency constant_dep = _context->request_dependency(u8"Constants upload dependency");

			{ // update assets
				auto timer = _asset_loading_queue.start_timer(u8"Update Assets");
				asset_dep = _assets->update();
			}

			renderer::constant_uploader uploader(
				*_context, _constant_upload_queue, _constant_upload_pool, _constant_pool
			);

			_process_frame(uploader, constant_dep, asset_dep);

			{ // ImGUI
				profiler::scope p2(u8"ImGUI");
				ImGui::NewFrame();
				_process_cpu_profiler_window();
				_process_imgui();
				{
					profiler::scope p3(u8"Render");
					ImGui::Render();
					_imgui_rctx->render(
						renderer::image2d_color(_swap_chain, gpu::color_render_target_access::create_preserve_and_write()),
						_get_back_buffer_size(), uploader, _imgui_pool
					);
				}
			}

			// upload constants
			uploader.end_frame(constant_dep);

			// finally, present and execute
			_present_queue.present(_swap_chain, u8"Present");

			{
				// set up debug output buffer
				std::ofstream debug_fout;
				constexpr bool _enable_debug_analysis = false;
				if constexpr (_enable_debug_analysis) {
					debug_fout = std::ofstream("test.txt");
					_context->on_execution_log =
						[&debug_fout](std::u8string_view text) {
							debug_fout.write(
								reinterpret_cast<const char*>(text.data()),
								static_cast<std::streamsize>(text.size())
							);
							debug_fout.flush();
						};
				}

				batch_stats_early = _context->execute_all();

				if constexpr (_enable_debug_analysis) {
					_context->on_execution_log = nullptr;
				}
			}

			auto frame_cpu_end = std::chrono::high_resolution_clock::now();
			cpu_frame_time_ms = std::chrono::duration<f32, std::milli>(frame_cpu_end - frame_cpu_begin).count();
		}
	};
}
