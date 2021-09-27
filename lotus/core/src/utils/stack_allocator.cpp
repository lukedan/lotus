#include "lotus/utils/stack_allocator.h"

/// \file
/// Implementation of the stack allocator.

namespace lotus {
	stack_allocator::scoped_bookmark stack_allocator::scoped_bookmark::create(stack_allocator &alloc) {
		scoped_bookmark result;
		result._alloc = &alloc;
		alloc.set_bookmark();
		return result;
	}


	stack_allocator::_page_ref stack_allocator::_page_ref::to_new_page(void *ptr, std::size_t sz) {
		_page_ref result = uninitialized;
		result.memory = ptr;
		result.current = ptr;
		result.end = static_cast<std::byte*>(ptr) + sz;
		return result;
	}

	void *stack_allocator::_page_ref::allocate(std::size_t size, std::size_t align) {
		std::size_t sz = static_cast<std::byte*>(end) - static_cast<std::byte*>(current);
		if (void *result = std::align(align, size, current, sz)) {
			current = static_cast<std::byte*>(current) + size;
			return result;
		}
		return nullptr;
	}


	stack_allocator::_page_header stack_allocator::_page_header::create(_page_ref prev, void (*free)(void*)) {
		_page_header result = uninitialized;
		result.previous = prev;
		result.free_page = free;
		return result;
	}


	stack_allocator::_bookmark stack_allocator::_bookmark::create(void *page, void *cur, _bookmark *prev) {
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

	void *stack_allocator::allocate(std::size_t size, std::size_t align) {
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

	void stack_allocator::pop_bookmark() {
		assert(_top_bookmark);
		_bookmark mark = *_top_bookmark;
		_top_bookmark->~_bookmark();
		_top_bookmark = mark.previous;
		while (_top_page.memory != mark.page) {
			_return_page();
		}
		_top_page.current = mark.current;
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
