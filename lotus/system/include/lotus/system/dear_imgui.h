#pragma once

/// \file
/// Header-only support for Dear ImGui. This is not included by other parts of the library. Include all necessary
/// Dear ImGui headers before this file.

#include "common.h"

namespace lotus::system::dear_imgui {
	/// System support for Dear ImGui.
	class context {
	public:
		/// Creates a new Dear ImGui system context.
		[[nodiscard]] inline static context create() {
			auto &io = ImGui::GetIO();
			io.BackendPlatformName = "imgui_impl_lotus_system";
			io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
			return context();
		}

		/// Called when the window is resized.
		void on_resize(window_events::resize &e) {
			auto &io = ImGui::GetIO();
			io.DisplaySize = ImVec2(e.new_size[0], e.new_size[1]);
		}
		/// Called when a mouse move event is detected.
		void on_mouse_move(window_events::mouse::move &e) {
			auto &io = ImGui::GetIO();
			io.AddMousePosEvent(e.new_position[0], e.new_position[1]);
		}
		/// Called when a mouse down event is detected.
		void on_mouse_down(window_events::mouse::button_down &e) {
			auto &io = ImGui::GetIO();
			io.AddMousePosEvent(e.position[0], e.position[1]);
			io.AddMouseButtonEvent(_to_imgui_mouse_button(e.button), true);
		}
		/// Called when a mouse up event is detected.
		void on_mouse_up(window_events::mouse::button_up &e) {
			auto &io = ImGui::GetIO();
			io.AddMousePosEvent(e.position[0], e.position[1]);
			io.AddMouseButtonEvent(_to_imgui_mouse_button(e.button), false);
		}
		/// Called when a mouse scroll event is detected.
		void on_mouse_scroll(window_events::mouse::scroll &e) {
			auto &io = ImGui::GetIO();
			io.AddMousePosEvent(e.position[0], e.position[1]);
			io.AddMouseWheelEvent(e.offset[0], e.offset[1]);
		}
	private:
		/// Does nothing.
		context() {
		}

		/// Converts a \ref mouse_button to a ImGui mouse button.
		[[nodiscard]] inline static int _to_imgui_mouse_button(mouse_button btn) {
			switch (btn) {
			case mouse_button::primary:
				return ImGuiMouseButton_Left;
			case mouse_button::secondary:
				return ImGuiMouseButton_Right;
			case mouse_button::middle:
				return ImGuiMouseButton_Middle;
			default:
				crash_if(true);
				return ImGuiMouseButton_Left;
			}
		}
	};
}
