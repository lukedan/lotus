#include "lotus/system/platforms/macos/dialog.h"

/// \file
/// Dialog implementation for MacOS.

#include "lotus/compiler.h"

LOTUS_PRAGMA_PUSH_DIAGNOSTICS
LOTUS_PRAGMA_IGNORE_DIAGNOSTICS_ANONYMOUS_ENUM_CONVERSION
LOTUS_PRAGMA_IGNORE_DIAGNOSTICS_DEPRECATED_DECLARATIONS
LOTUS_PRAGMA_IGNORE_DIAGNOSTICS_MISSING_METHOD_RETURN_TYPE
#include <AppKit/NSAlert.h>
#include <AppKit/NSOpenPanel.h>
LOTUS_PRAGMA_POP_DIAGNOSTICS

#include "lotus/system/platforms/macos/details.h"
#include "lotus/system/window.h"

namespace lotus::system::platforms::macos::dialog {
	/// A struct that stores the response of a sheet.
	class _sheet_response {
	public:
		/// Marks the sheet as completed with the given response.
		void mark_done(NSModalResponse r) {
			_done = true;
			_result = r;
		}

		/// Returns whether the sheet has finished presenting.
		[[nodiscard]] bool is_done() const {
			return _done;
		}
		/// Returns the result of the sheet.
		[[nodiscard]] NSModalResponse get_result() const {
			return _result;
		}
	private:
		bool _done = false; ///< Whether the sheet has finished presenting.
		NSModalResponse _result = NSModalResponseCancel; ///< The result of the sheet.
	};

	namespace message_box {
		void show(std::u8string_view contents, std::u8string_view caption, style s, system::window *parent) {
			auto *const box = [[NSAlert alloc] init];

			{ // caption
				NSString *const c = _details::conversions::to_ns_string(caption);
				[box setMessageText: c];
				[c release];
			}

			{ // contents
				NSString *const c = _details::conversions::to_ns_string(contents);
				[box setInformativeText: c];
				[c release];
			}

			{ // style
				constexpr static enums::sequential_mapping<style, NSAlertStyle> _mapping{
					std::pair(style::plain,       NSAlertStyleInformational),
					std::pair(style::information, NSAlertStyleInformational),
					std::pair(style::warning,     NSAlertStyleWarning      ),
					std::pair(style::error,       NSAlertStyleCritical     ),
				};
				[box setAlertStyle: _mapping[s]];
			}

			// show
			__block _sheet_response result;
			if (parent) {
				auto *const wnd = static_cast<NSWindow*>(parent->get_native_handle());
				[box
					beginSheetModalForWindow: wnd
					completionHandler:        ^(NSModalResponse response) {
						result.mark_done(response);
						[NSApp stopModal];
					}
				];
				do {
					[NSApp runModalForWindow: wnd];
				} while (!result.is_done());
			} else {
				result.mark_done([box runModal]);
			}
			[box release];
		}
	}

	namespace file {
		std::filesystem::path open(system::window *parent) {
			auto *const d = [[NSOpenPanel alloc] init];
			[d setCanChooseFiles: true];
			[d setCanChooseDirectories: false];
			[d setAllowsMultipleSelection: false];

			// show
			__block _sheet_response result;
			if (parent) {
				auto *const wnd = static_cast<NSWindow*>(parent->get_native_handle());
				[d
					beginSheetModalForWindow: wnd
					completionHandler:        ^(NSModalResponse response) {
						result.mark_done(response);
						[NSApp stopModal];
					}
				];
				do {
					[NSApp runModalForWindow: wnd];
				} while (!result.is_done());
			} else {
				result.mark_done([d runModal]);
			}

			// retrieve results
			std::filesystem::path result_path;
			if (result.get_result() == NSModalResponseOK && d.URLs.count == 1) {
				if (const NSString *const path = d.URLs.firstObject.path) {
					result_path = [path UTF8String];
				}
			}
			[d release];
			return result_path;
		}
	}
}
