#pragma once

/// \file
/// Stack allocator.

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <thread>

#include "pbd/common.h"

namespace pbd {
	/// An allocator that allocates out of a stack. The user can make bookmarks in the stack that the allocator can
	/// unwind to.
	class stack_allocator {
	public:
		/// An RAII bookmark.
		struct scoped_bookmark {
		public:
			/// Creates an empty object.
			scoped_bookmark() = default;
			/// Creates a new bookmark object.
			[[nodiscard]] inline static scoped_bookmark create(stack_allocator &alloc = for_this_thread()) {
				scoped_bookmark result;
				result._alloc = &alloc;
				alloc.set_bookmark();
				return result;
			}
			/// Move construction.
			scoped_bookmark(scoped_bookmark &&src) : _alloc(std::exchange(src._alloc, nullptr)) {
			}
			/// Move construction.
			scoped_bookmark &operator=(scoped_bookmark &&src) {
				reset();
				_alloc = std::exchange(src._alloc, nullptr);
				return *this;
			}
			/// Calls \ref pop_bookmark().
			~scoped_bookmark() {
				reset();
			}

			/// Resets this object, popping the bookmark if necessary.
			void reset() {
				if (_alloc) {
					_alloc->pop_bookmark();
					_alloc = nullptr;
				}
			}
		protected:
			stack_allocator *_alloc = nullptr; ///< The allocator;
		};
		/// A STL container compatible allocator for \ref stack_allocator.
		template <typename T> class allocator {
			template <typename U> friend class allocator;
		public:
			using value_type = T; ///< Value type.

			/// Initializes \ref _thread_id.
			allocator() : _thread_id(std::this_thread::get_id()) {
			}
			/// Conversion from an allocator of another type.
			template <typename U> explicit allocator(const allocator<U> &src) :
				_thread_id(src._thread_id) {
			}

			/// Allocates an array.
			[[nodiscard]] T *allocate(std::size_t n) const {
				assert(std::this_thread::get_id() == _thread_id);
				return static_cast<T*>(stack_allocator::for_this_thread().allocate(sizeof(T) * n, alignof(T)));
			}
			/// Does nothing. De-allocation only happens when popping bookmarks.
			void deallocate(T*, std::size_t) const {
			}

			/// If two objects are on the same thread, they use the same underlying allocator.
			friend bool operator==(const allocator&, const allocator&) = default;
		protected:
			std::thread::id _thread_id; ///< The thread ID of this allocator.
		};

		/// Default constructor.
		stack_allocator() = default;
		/// No copy construction.
		stack_allocator(const stack_allocator&) = delete;
		/// No copy assignment.
		stack_allocator &operator=(const stack_allocator&) = delete;
		/// Frees all pages.
		~stack_allocator() {
			assert(_top_bookmark == nullptr);
			free_unused_pages();
			while (_top_page) {
				_page_ref next = _top_page.header->previous;
				auto free_func = _top_page.header->free_page;
				free_func(_top_page.memory);
				_top_page = next;
			}
		}

		/// Allocates a new block of memory.
		void *allocate(std::size_t size, std::size_t align) {
			if (void *result = _top_page.allocate(size, align)) {
				return result;
			}
			_take_page();
			if (void *result = _top_page.allocate(size, align)) {
				return result;
			}
			_return_page();
			_top_page = _allocate_new_page(_top_page, page_size + size);
			return _top_page.allocate(size, align);
		}
		/// Allocates a new block of memory. This function does not initialize the memory.
		template <typename T> T *allocate() {
			return static_cast<T*>(allocate(sizeof(T), alignof(T)));
		}

		/// Sets a new bookmark.
		void set_bookmark() {
			auto mark = _bookmark::create(_top_page.memory, _top_page.current, _top_bookmark);
			_top_bookmark = new (allocate<_bookmark>()) _bookmark(mark);
		}
		/// Resets the allocator to the state before the last bookmark was allocated. All allocated memory since then
		/// must be properly freed by this point.
		void pop_bookmark() {
			assert(_top_bookmark);
			_bookmark mark = *_top_bookmark;
			_top_bookmark->~_bookmark();
			_top_bookmark = mark.previous;
			while (_top_page.memory != mark.page) {
				_return_page();
			}
			_top_page.current = mark.current;
		}

		/// Frees all pages in \ref _free_pages.
		void free_unused_pages() {
			while (_free_pages) {
				_page_ref next = _free_pages.header->previous;
				auto free_func = _free_pages.header->free_page;
				free_func(_free_pages.memory);
				_free_pages = next;
			}
		}

		/// Returns the \ref stack_allocator for this thread.
		static stack_allocator &for_this_thread();

		std::size_t page_size = 8 * 1024 * 1024; /// Size of a page.
		void *(*allocate_page)(std::size_t) = std::malloc; ///< Used to allocate the pages.
		void (*free_page)(void*) = std::free; ///< Used to free a page.
	protected:
		struct _page_header;
		/// Reference to a page.
		struct _page_ref {
			/// No initialization.
			_page_ref(uninitialized_t) {
			}
			/// Initializes this reference to empty.
			_page_ref(std::nullptr_t) : memory(nullptr), header(nullptr), current(nullptr), end(nullptr) {
			}
			/// Creates a new reference to the given newly allocated page. \ref header is not initialized.
			[[nodiscard]] inline static _page_ref to_new_page(void *ptr, std::size_t sz) {
				_page_ref result = uninitialized;
				result.memory = ptr;
				result.current = ptr;
				result.end = static_cast<std::byte*>(ptr) + sz;
				return result;
			}

			/// Allocates a block of memory from this page. If there's not enough space within this page, this
			/// function returns \p nullptr. The returned memory block is not initialized.
			[[nodiscard]] void *allocate(std::size_t size, std::size_t align) {
				std::size_t sz = static_cast<std::byte*>(end) - static_cast<std::byte*>(current);
				if (void *result = std::align(align, size, current, sz)) {
					current = static_cast<std::byte*>(current) + size;
					return result;
				}
				return nullptr;
			}
			/// \overload
			template <typename T> [[nodiscard]] T *allocate() {
				return static_cast<T*>(allocate(sizeof(T), alignof(T)));
			}

			/// Calls the destructor of \ref header, then empties this page and re-allocate the header.
			void reset(_page_header new_header) {
				header->~_page_header();
				current = memory;
				header = new (allocate<_page_header>()) _page_header(new_header);
			}

			/// Tests if this reference is empty.
			[[nodiscard]] explicit operator bool() const {
				return memory != nullptr;
			}

			void *memory; ///< Pointer to the memory block.
			_page_header *header; ///< The header of this page.
			void *current; ///< Next byte that could be allocated.
			void *end; ///< Pointer past the page.
		};
		/// Header of a page.
		struct _page_header {
			/// No initialization.
			_page_header(uninitialized_t) {
			}
			/// Creates a header object with the given reference to the previous page.
			[[nodiscard]] inline static _page_header create(_page_ref prev, void (*free)(void*)) {
				_page_header result = uninitialized;
				result.previous = prev;
				result.free_page = free;
				return result;
			}

			_page_ref previous = uninitialized; ///< The previous page.
			void (*free_page)(void*); ///< The function that should be used to free this page.
		};
		/// Bookmark data.
		struct _bookmark {
			/// No initialization.
			_bookmark(uninitialized_t) {
			}
			/// Creates a new bookmark object.
			[[nodiscard]] inline static _bookmark create(void *page, void *cur, _bookmark *prev) {
				_bookmark result = uninitialized;
				result.page = page;
				result.current = cur;
				result.previous = prev;
				return result;
			}

			void *page; ///< Address of the page that this bookmark is in.
			void *current; ///< Position of the bookmark within the page.
			_bookmark *previous; ///< The previous bookmark.
		};

		/// Creates a new page and allocates a \ref _page_ref at the front to the current top page.
		[[nodiscard]] _page_ref _allocate_new_page(_page_ref prev, std::size_t size) const {
			auto result = _page_ref::to_new_page(allocate_page(size), size);
			result.header = new (result.allocate<_page_header>()) _page_header(_page_header::create(prev, free_page));
			return result;
		}
		/// \overload
		[[nodiscard]] _page_ref _allocate_new_page(_page_ref prev) const {
			return _allocate_new_page(prev, page_size);
		}

		/// Replaces \ref _top_page with a new page. If \ref _free_pages is empty, a page is taken from the list;
		/// otherwise a new page is allocated.
		void _take_page() {
			if (_free_pages) {
				_page_ref page = _free_pages;
				_free_pages = _free_pages.header->previous;
				page.header->previous = _top_page;
				_top_page = page;
			} else {
				_top_page = _allocate_new_page(_top_page);
			}
		}
		/// Assumes that \ref _top_page is empty and returns it to \ref _free_pages.
		void _return_page() {
			_page_ref new_top = _top_page.header->previous;
			_top_page.reset(_page_header::create(_free_pages, _top_page.header->free_page));
			_free_pages = _top_page;
			_top_page = new_top;
		}

		_page_ref _top_page = nullptr; ///< The page currently in use.
		/// A list of free pages. All pages in this list have correct \ref _page_ref::current fields (i.e., only
		/// accounting for the header).
		_page_ref _free_pages = nullptr;
		_bookmark *_top_bookmark = nullptr; ///< The most recent bookmark.
	};
}
