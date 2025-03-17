#include "lotus/memory/stack_allocator.h"

/// \file
/// Implementation of the stack allocator.

namespace lotus::memory {
	stack_allocator::_page_ref stack_allocator::_page_ref::to_new_page(std::byte *ptr, usize sz) {
		_page_ref result = uninitialized;
		result.memory = ptr;
		result.current = ptr;
		result.end = ptr + sz;
		if constexpr (should_poison_freed_memory) {
			memory::poison(result.memory, sz);
		}
		return result;
	}

	std::byte *stack_allocator::_page_ref::allocate(memory::size_alignment s) {
		auto sz = static_cast<usize>(end - current);
		void *v_current = current;
		if (void *result = std::align(s.alignment, s.size, v_current, sz)) {
			current = static_cast<std::byte*>(v_current) + s.size;
			return static_cast<std::byte*>(result);
		}
		return nullptr;
	}


	stack_allocator::_page_header stack_allocator::_page_header::create(_page_ref prev, void (*free)(std::byte*)) {
		_page_header result = uninitialized;
		result.previous = prev;
		result.free_page = free;
		return result;
	}


	stack_allocator::_bookmark stack_allocator::_bookmark::create(std::byte *page, std::byte *cur, _bookmark *prev) {
		_bookmark result = uninitialized;
		result.page = page;
		result.current = cur;
		result.previous = prev;
		return result;
	}


	stack_allocator::~stack_allocator() {
		assert(_top_bookmark == nullptr);
		free_unused_pages();
		while (_top_page) {
			_page_ref next = _top_page.header->previous;
			auto free_func = _top_page.header->free_page;
			free_func(_top_page.memory);
			_top_page = next;
		}
	}

	std::byte *stack_allocator::_allocate(memory::size_alignment s) {
		if (std::byte *result = _top_page.allocate(s)) {
			return result;
		}
		_take_page();
		if (std::byte *result = _top_page.allocate(s)) {
			return result;
		}
		_return_page();
		_top_page = _allocate_new_page(_top_page, page_size + s.size);
		return _top_page.allocate(s);
	}

	void stack_allocator::_pop_bookmark() {
		crash_if(_top_bookmark == nullptr);
		_bookmark mark = *_top_bookmark;
		_top_bookmark->~_bookmark();
		_top_bookmark = mark.previous;
		while (_top_page.memory != mark.page) {
			_return_page();
		}
		_top_page.lower_current(mark.current);
	}

	void stack_allocator::free_unused_pages() {
		while (_free_pages) {
			_page_ref next = _free_pages.header->previous;
			auto free_func = _free_pages.header->free_page;
			free_func(_free_pages.memory);
			_free_pages = next;
		}
	}

	void stack_allocator::_take_page() {
		if (_free_pages) {
			_page_ref page = _free_pages;
			_free_pages = _free_pages.header->previous;
			page.header->previous = _top_page;
			_top_page = page;
		} else {
			_top_page = _allocate_new_page(_top_page);
		}
	}

	void stack_allocator::_return_page() {
		_page_ref new_top = _top_page.header->previous;
		_top_page.reset(_page_header::create(_free_pages, _top_page.header->free_page));
		_free_pages = _top_page;
		_top_page = new_top;
	}

	stack_allocator &stack_allocator::for_this_thread() {
		static thread_local stack_allocator _allocator;
		return _allocator;
	}
}
