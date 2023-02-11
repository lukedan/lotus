#pragma once

/// \file
/// Stack allocator.

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

#include "common.h"
#include "lotus/common.h"
#include "lotus/containers/static_optional.h"

namespace lotus::memory {
	/// An allocator that allocates out of a stack. The user can make bookmarks in the stack that the allocator can
	/// unwind to.
	class stack_allocator {
	protected:
		struct _page_header;
		struct _bookmark;
	public:
		/// Whether or not to poision memory that has been freed.
		constexpr static bool should_poison_freed_memory = is_debugging;
		/// Allocator type for a stack allocator.
		class allocator {
		public:
			/// Creates an empty allocator.
			allocator(std::nullptr_t) : _alloc(nullptr) {
			}
			/// Creates an allocator for the given \ref stack_allocator.
			[[nodiscard]] inline static allocator create_for(stack_allocator &alloc) {
				allocator result = nullptr;
				result._alloc = &alloc;
				return result;
			}
			/// Copy constructor.
			allocator(const allocator&) = default;
			/// Copy assignment.
			allocator &operator=(const allocator&) = default;

			/// Calls \ref stack_allocator::_allocate().
			[[nodiscard]] std::byte *allocate(memory::size_alignment s) const {
				return _alloc->_allocate(s);
			}
			/// Memory allocated from a stack allocator cannot be freed in isolation.
			void free(std::byte*) const {
				// nothing to do
			}
		private:
			stack_allocator *_alloc; ///< The underlying allocator.
		};
		/// A STL container compatible allocator for \ref stack_allocator.
		template <typename T> class std_allocator {
			template <typename U> friend class std_allocator;
		public:
			using value_type = T; ///< Value type.

			/// Creates an empty (and invalid) allocator.
			std_allocator(std::nullptr_t) {
			}
			/// Creates an allocator for the given \ref stack_allocator.
			[[nodiscard]] inline static std_allocator create_for(stack_allocator &alloc) {
				std_allocator result = nullptr;
				result._alloc = &alloc;
				return result;
			}
			/// Conversion from an allocator of another type.
			template <typename U> explicit std_allocator(const std_allocator<U> &src) :
				_alloc(src._alloc) {
			}

			/// Allocates an array.
			[[nodiscard]] T *allocate(std::size_t n) const {
				return static_cast<T*>(static_cast<void*>(
					_alloc->_allocate(memory::size_alignment::of_array<T>(n))
				));
			}
			/// Does nothing. De-allocation only happens when popping bookmarks.
			void deallocate(T*, std::size_t) const {
			}

			/// Tests that two allocators refer to the same \ref stack_allocator.
			friend bool operator==(const std_allocator&, const std_allocator&) = default;
		protected:
			stack_allocator *_alloc; ///< The underlying allocator.
		};
		/// Vector type that uses a stack allocator.
		template <typename T> using vector_type = std::vector<T, std_allocator<T>>;
		/// String type that uses a stack allocator.
		template <typename Ch, typename Traits = std::char_traits<Ch>> using string_type =
			std::basic_string<Ch, Traits, std_allocator<Ch>>;
		/// An RAII bookmark.
		struct scoped_bookmark {
			friend stack_allocator;
		public:
			/// Creates an empty bookmark.
			scoped_bookmark(std::nullptr_t) : _alloc(nullptr) {
			}
			/// Move construction.
			scoped_bookmark(scoped_bookmark &&src) : _alloc(std::exchange(src._alloc, nullptr)) {
			}
			/// Move assignment.
			scoped_bookmark &operator=(scoped_bookmark &&src) {
				if (this != &src) {
					reset();
					_alloc = std::exchange(src._alloc, nullptr);
				}
				return *this;
			}
			/// Calls \ref pop_bookmark().
			~scoped_bookmark() {
				reset();
			}

			/// Allocates a piece of memory from the current segment.
			[[nodiscard]] std::byte *allocate(memory::size_alignment s) {
				if constexpr (_this_bookmark.is_enabled) {
					assert(_alloc && _alloc->_top_bookmark == *_this_bookmark);
				}
				return _alloc->_allocate(s);
			}
			/// Allocates memory for an object or an array of objects.
			template <typename T> [[nodiscard]] T *allocate(std::size_t count = 1) {
				return static_cast<T*>(allocate(memory::size_alignment::of_array<T>(count)));
			}

			/// Creates an \ref allocator for the allocator associated with this bookmark.
			[[nodiscard]] allocator create_allocator() const {
				return allocator::create_for(*_alloc);
			}
			/// Creates a \ref allocator for the given type.
			template <typename T> [[nodiscard]] std_allocator<T> create_std_allocator() const {
				return std_allocator<T>::create_for(*_alloc);
			}
			/// Convenience function for creating a \p std::vector using the given parameters and this allocator.
			template <
				typename T, typename ...Args
			> [[nodiscard]] vector_type<T> create_vector_array(Args &&...args) {
				return vector_type<T>(std::forward<Args>(args)..., create_std_allocator<T>());
			}
			/// Convenience function for creating a \p std::vector with the specified reserved space using the given
			/// parameters and this allocator.
			template <typename T> [[nodiscard]] vector_type<T> create_reserved_vector_array(std::size_t capacity) {
				vector_type<T> result(create_std_allocator<T>());
				result.reserve(capacity);
				return result;
			}
			/// Convenience function for creating a \p std::basic_string using the given parameters and this
			/// allocator.
			template <
				typename Ch = char, typename Traits = std::char_traits<Ch>, typename ...Args
			> [[nodiscard]] string_type<Ch, Traits> create_string(Args &&...args) {
				return string_type<Ch, Traits>(std::forward<Args>(args)..., create_std_allocator<Ch>());
			}
			/// Convenience function for creating a \p std::u8string using the given parameters and this allocator.
			template <typename ...Args> [[nodiscard]] string_type<char8_t> create_u8string(Args &&...args) {
				return string_type<char8_t>(std::forward<Args>(args)..., create_std_allocator<char8_t>());
			}

			/// Resets this object, popping the bookmark if necessary.
			void reset() {
				if (_alloc) {
					_alloc->_pop_bookmark();
					_alloc = nullptr;
				}
			}
		protected:
			stack_allocator *_alloc; ///< The allocator;
			/// Position of this bookmark used for debugging.
			[[no_unique_address]] debug_value<_bookmark*> _this_bookmark;

			/// Creates an object for the given allocator and sets a bookmark.
			explicit scoped_bookmark(stack_allocator &alloc) : _alloc(&alloc) {
				_alloc->_set_bookmark();
				_this_bookmark.if_enabled([&](_bookmark *&b) {
					b = _alloc->_top_bookmark;
				});
			}
		};

		/// Default constructor.
		stack_allocator() = default;
		/// No copy construction.
		stack_allocator(const stack_allocator&) = delete;
		/// No copy assignment.
		stack_allocator &operator=(const stack_allocator&) = delete;
		/// Frees all pages.
		~stack_allocator();

		/// Creates a bookmark and returns it.
		[[nodiscard]] scoped_bookmark bookmark() {
			return scoped_bookmark(*this);
		}

		/// Frees all pages in \ref _free_pages.
		void free_unused_pages();

		/// Returns the \ref stack_allocator for this thread.
		static stack_allocator &for_this_thread();

		std::size_t page_size = 8 * 1024 * 1024; /// Size of a page.
		std::byte *(*allocate_page)(memory::size_alignment) = memory::raw::allocate; ///< Used to allocate the pages.
		void (*free_page)(std::byte*) = memory::raw::free; ///< Used to free a page.
	protected:
		/// Reference to a page.
		struct _page_ref {
			/// No initialization.
			_page_ref(uninitialized_t) {
			}
			/// Initializes this reference to empty.
			_page_ref(std::nullptr_t) : memory(nullptr), header(nullptr), current(nullptr), end(nullptr) {
			}
			/// Creates a new reference to the given newly allocated page. \ref header is not initialized.
			[[nodiscard]] static _page_ref to_new_page(std::byte*, std::size_t sz);

			/// Allocates a block of memory from this page. If there's not enough space within this page, this
			/// function returns \p nullptr. The returned memory block is not initialized.
			[[nodiscard]] std::byte *allocate(memory::size_alignment);
			/// \overload
			template <typename T> [[nodiscard]] void *allocate() {
				return allocate(memory::size_alignment::of<T>());
			}

			/// Calls the destructor of \ref header, then empties this page and re-allocate the header.
			void reset(_page_header new_header) {
				std::byte *old_current = current;
				header->~_page_header();
				current = memory;
				header = new (allocate<_page_header>()) _page_header(new_header);
				maybe_poison_range(current, old_current);
			}

			/// Lowers the `current' pointer and poisons the freed range if necessary.
			void lower_current(std::byte *new_current) {
				assert(new_current >= memory && new_current <= current);
				std::byte *old_current = std::exchange(current, new_current);
				maybe_poison_range(current, old_current);
			}
			/// Poisons all bytes in the page between the two pointers, or after the given pointer if the end is not
			/// specified. This is only done if \ref should_poison_freed_memory is \p true.
			void maybe_poison_range(std::byte *ptr, std::byte *custom_end = nullptr) {
				crash_if(ptr < memory || ptr > end);
				if (custom_end) {
					crash_if(custom_end < ptr || custom_end > end);
				} else {
					custom_end = end;
				}
				if constexpr (should_poison_freed_memory) {
					memory::poison(ptr, custom_end - ptr);
				}
			}

			/// Tests if this reference is empty.
			[[nodiscard]] explicit operator bool() const {
				return memory != nullptr;
			}

			std::byte *memory; ///< Pointer to the memory block.
			_page_header *header; ///< The header of this page.
			std::byte *current; ///< Next byte that could be allocated.
			std::byte *end; ///< Pointer past the page.
		};
		/// Header of a page.
		struct _page_header {
			/// No initialization.
			_page_header(uninitialized_t) {
			}
			/// Creates a header object with the given reference to the previous page.
			[[nodiscard]] static _page_header create(_page_ref prev, void (*free)(std::byte*));

			_page_ref previous = uninitialized; ///< The previous page.
			void (*free_page)(std::byte*); ///< The function that should be used to free this page.
		};
		/// Bookmark data.
		struct _bookmark {
			/// No initialization.
			_bookmark(uninitialized_t) {
			}
			/// Creates a new bookmark object.
			[[nodiscard]] static _bookmark create(std::byte *page, std::byte *cur, _bookmark *prev);

			std::byte *page; ///< Address of the page that this bookmark is in.
			std::byte *current; ///< Position of the bookmark within the page.
			_bookmark *previous; ///< The previous bookmark.
		};

		/// Creates a new page and allocates a \ref _page_ref at the front to the current top page.
		[[nodiscard]] _page_ref _allocate_new_page(_page_ref prev, std::size_t size) const {
			auto result = _page_ref::to_new_page(allocate_page(memory::size_alignment(size, alignof(_page_header))), size);
			result.header = new (result.allocate<_page_header>()) _page_header(_page_header::create(prev, free_page));
			return result;
		}
		/// \overload
		[[nodiscard]] _page_ref _allocate_new_page(_page_ref prev) const {
			return _allocate_new_page(prev, page_size);
		}

		/// Sets a new bookmark.
		void _set_bookmark() {
			auto mark = _bookmark::create(_top_page.memory, _top_page.current, _top_bookmark);
			_top_bookmark = new (_allocate(memory::size_alignment::of<_bookmark>())) _bookmark(mark);
		}
		/// Resets the allocator to the state before the last bookmark was allocated. All allocated memory since then
		/// must be properly freed by this point.
		void _pop_bookmark();

		/// Allocates a new block of memory.
		std::byte *_allocate(memory::size_alignment);

		/// Replaces \ref _top_page with a new page. If \ref _free_pages is empty, a page is taken from the list;
		/// otherwise a new page is allocated.
		void _take_page();
		/// Assumes that \ref _top_page is empty and returns it to \ref _free_pages.
		void _return_page();

		_page_ref _top_page = nullptr; ///< The page currently in use.
		/// A list of free pages. All pages in this list have correct \ref _page_ref::current fields (i.e., only
		/// accounting for the header).
		_page_ref _free_pages = nullptr;
		_bookmark *_top_bookmark = nullptr; ///< The most recent bookmark.
	};
}

namespace lotus {
	/// Shorthand for creating a memory bookmark for scratch memory on the current thread.
	[[nodiscard]] inline static memory::stack_allocator::scoped_bookmark get_scratch_bookmark() {
		return memory::stack_allocator::for_this_thread().bookmark();
	}
}
