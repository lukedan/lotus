#pragma once

#include <fstream>

#include <lotus/utils/strings.h>
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

namespace lotus {
	/// Base class for a test application that comes with a window, a GPU context, a rendering context, ImGUI, and other
	/// utilities.
	class application {
	public:
		/// Initializes the application.
		application(int argc, char **argv, std::u8string_view app_name, gpu::context_options gpu_context_options) :
			_argc(argc),
			_argv(argv),
			_app(app_name),
			_gpu_context(gpu::context::create(
				gpu_context_options,
				[this](
					gpu::debug_message_severity severity,
					gpu::context::debug_message_id id,
					std::u8string_view msg
				) {
					_on_gpu_debug_message(severity, id, msg);
				}
			)),
			_gpu_adapter_properties(uninitialized),
			_shader_utils(gpu::shader_utility::create()) {

			_window = _wrap(new auto(_app.create_window()));
		}
		/// Default virtual destructor.
		virtual ~application() = default;

		/// Initializes GPU resources. This should be called immediately after the constructor.
		void initialize() {
			gpu::adapter best_adapter = nullptr;

			{ // choose adapter
				int best_adapter_score = std::numeric_limits<int>::min();
				_gpu_context.enumerate_adapters([&](gpu::adapter adap) {
					int score = _score_device(adap);
					if (score > best_adapter_score) {
						best_adapter_score = score;
						best_adapter = adap;
					}
				});
			}

			{ // create device, context, and swap chain
				_gpu_adapter_properties = best_adapter.get_properties();
				log().debug("Choosing adapter: {}", string::to_generic(_gpu_adapter_properties.name));
				std::vector<gpu::command_queue> gpu_cmd_queues;
				std::tie(_gpu_device, gpu_cmd_queues) = best_adapter.create_device(_get_desired_queues());

				_context = _wrap(new auto(renderer::context::create(
					_gpu_context, _gpu_adapter_properties, _gpu_device, gpu_cmd_queues
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
			_assets->asset_library_path = _get_asset_library_path();
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
			_window->show_and_activate();
			bool quit = false;
			while (!quit) {
				system::message_type msg_type;
				do {
					msg_type = _app.process_message_nonblocking();
					quit = quit || msg_type == system::message_type::quit;
				} while (msg_type != system::message_type::none);

				if (_get_window_size() == zero) {
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
		float cpu_frame_time_ms = 0.0f; ///< CPU time of previous frame in milliseconds.
	protected:
		const int _argc;
		const char *const *const _argv;

		system::application _app; ///< The system application.
		std::unique_ptr<system::window> _window; ///< The main window.

		gpu::context _gpu_context; ///< The GPU context.
		gpu::device _gpu_device = nullptr; ///< The GPU device.
		gpu::adapter_properties _gpu_adapter_properties; ///< The GPU adapter properties.
		gpu::shader_utility _shader_utils; ///< Shader utilities.

		std::unique_ptr<renderer::context> _context; ///< The renderer context.
		std::unique_ptr<renderer::assets::manager> _assets; ///< The asset manager.
		renderer::swap_chain _swap_chain = nullptr; ///< The swap chain.


		/// Returns the current window size.
		[[nodiscard]] cvec2u32 _get_window_size() const {
			return _window_size;
		}


		/// Scores an adapter. The GPU device will be created using the one with the highest score. Derived classes can
		/// override this to control device selection strategy. By default, discrete devices are scored as 1 and
		/// otherwise 0.
		[[nodiscard]] virtual int _score_device(gpu::adapter adapter) const {
			auto props = adapter.get_properties();
			return props.is_discrete ? 1 : 0;
		}
		/// Returns the queue types for GPU queues that should be created.
		[[nodiscard]] virtual std::span<const gpu::queue_family> _get_desired_queues() const = 0;
		/// Derived classes should override this to return the desired GPU queue index used for loading assets.
		[[nodiscard]] virtual std::uint32_t _get_asset_loading_queue_index() const = 0;
		/// Derived classes should override this to return the desired GPU queue index used for uploading constant
		/// buffers.
		[[nodiscard]] virtual std::uint32_t _get_constant_upload_queue_index() const = 0;
		/// Derived classes should override this to return the desired GPU queue index used for ImGUI and debug
		/// drawing.
		[[nodiscard]] virtual std::uint32_t _get_debug_drawing_queue_index() const = 0;
		/// Derived classes should override this to return the desired GPU queue index used for presenting to swap
		/// chains.
		[[nodiscard]] virtual std::uint32_t _get_present_queue_index() const = 0;
		/// Derived classes should override this and return the asset library path.
		[[nodiscard]] virtual std::filesystem::path _get_asset_library_path() const = 0;
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
				return id != 0xFC68BE96 && id != 0xA5625282;
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

					std::size_t keep_count = 5;
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
	private:
		/*
		/// Information about constant buffers and how many times they're used.
		struct _constant_buffer_info {
			/// Initializes all fields of this struct.
			_constant_buffer_info(renderer::statistics::constant_buffer_signature sig, std::uint32_t c) :
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
			std::uint32_t count = 0; ///< The number of times it's used.
		};
		*/

		std::unique_ptr<system::dear_imgui::context> _imgui_sctx; ///< System context for ImGUI.
		std::unique_ptr<renderer::dear_imgui::context> _imgui_rctx; ///< Graphics context for ImGUI.

		renderer::context::queue _asset_loading_queue = nullptr; ///< Queue used for asset loading.
		renderer::context::queue _constant_upload_queue = nullptr; ///< Queue used for constant buffer uploading.
		renderer::context::queue _debug_drawing_queue = nullptr; ///< Queue used for debug drawing.
		renderer::context::queue _present_queue = nullptr; ///< Queue used for presenting.

		renderer::pool _imgui_pool = nullptr; ///< Pool used for ImGUI resources.
		renderer::pool _constant_pool = nullptr; ///< Pool used for constant buffers.
		renderer::pool _constant_upload_pool = nullptr; ///< Pool used for uploading constant buffers.

		cvec2u32 _window_size = zero; ///< Current window size.


		/// Wraps an object created by \p new with a \p std::unique_ptr.
		template <typename T> static std::unique_ptr<T> _wrap(T *new_ptr) {
			return std::unique_ptr<T>(new_ptr);
		}

		/// Updates ImGUI window size, then calls \ref _on_resize().
		void _on_resize_raw(system::window_events::resize &resize) {
			_imgui_sctx->on_resize(resize);
			_swap_chain.resize(resize.new_size);
			_window_size = resize.new_size;
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

		/// Processes a single frame.
		void _process_frame_full() {
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

			// ImGUI
			ImGui::NewFrame();
			_process_imgui();
			ImGui::Render();
			_imgui_rctx->render(
				renderer::image2d_color(_swap_chain, gpu::color_render_target_access::create_preserve_and_write()),
				_get_window_size(), uploader, _imgui_pool
			);

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
			cpu_frame_time_ms = std::chrono::duration<float, std::milli>(frame_cpu_end - frame_cpu_begin).count();
		}
	};
}
