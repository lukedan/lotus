#include <iostream>

#include <lotus/graphics/context.h>
#include <lotus/graphics/commands.h>
#include <lotus/graphics/descriptors.h>
#include <lotus/system/window.h>
#include <lotus/system/application.h>

int main() {
	lotus::system::application app(u8"test");
	lotus::graphics::context ctx;

	lotus::graphics::device dev = nullptr;
	ctx.enumerate_adapters([&](lotus::graphics::adapter adap) {
		dev = adap.create_device();
		return false;
	});
	auto cmd_queue = dev.create_command_queue();
	auto cmd_alloc = dev.create_command_allocator();

	auto wnd = app.create_window();
	auto swapchain = ctx.create_swap_chain_for_window(
		wnd, dev, cmd_queue, 2, lotus::graphics::pixel_format::r8g8b8a8_snorm
	);
	auto cmdpool = dev.create_command_allocator();

	wnd.show_and_activate();
	while (app.process_message_blocking()) {
	}

	return 0;
}
