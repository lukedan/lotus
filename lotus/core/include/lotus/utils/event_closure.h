#pragma once

/// \file
/// Closures for event handlers.

#include "lotus/common.h"
#include "static_function.h"

namespace lotus {
	template <typename ...EventArgs> class event {
	public:
		struct head_node;

		/// A node in a doubly-linked looping chain of event handlers. Each node may hold additional data and is
		/// explicitly and manually managed by the event receiver rather than the emitter. Nodes must not move in memory.
		struct node {
			friend head_node;
		public:
			/// No copy construction.
			node(const node&) = delete;
			/// No copy assignment.
			node &operator=(const node&) = delete;
			/// Automatically unlinks the node.
			~node() {
				unlink();
			}

			/// Inserts this node after the given node. This node must not currently belong to any list.
			void link_after(node &n) {
				assert(_prev == this && _next == this);
				_prev = &n;
				_next = std::exchange(n._next, this);
				_next->_prev = this;
			}
			/// Unlinks this node from the list it's in. This is safe to call even if this node is already isolated.
			void unlink() {
				_prev->_next = _next;
				_next->_prev = _prev;
				_prev = _next = this;
			}
		protected:
			node *_prev; ///< The previous node in the chain.
			node *_next; ///< The next node in the chain.

			/// Initializes an isolated node.
			node(std::nullptr_t) : _prev(this), _next(this) {
			}
			/// Creates this node and immediately links to the given node.
			explicit node(node &link_to) : node(nullptr) {
				link_after(link_to);
			}
		};

		/// A node that contains a function pointer that can be invoked.
		struct closure_node : public node {
		public:
			using event_type = event; ///< Event type.

			/// Creates an isolated node with the given callback.
			template <typename ...Args> [[nodiscard]] inline static closure_node create_isolated(Args &&...args) {
				return closure_node(std::forward<Args>(args)...);
			}
			/// Creates a node using the given arguments that is linked immediately after the specified node.
			template <typename ...Args> [[nodiscard]] inline static closure_node create_linked(
				node &n, Args &&...args
			) {
				return closure_node(n, std::forward<Args>(args)...);
			}

			static_function<void(EventArgs...)> function; ///< The callback function.
		protected:
			/// Creates an isolated node and initializes \ref function.
			template <typename ...Args> explicit closure_node(Args &&...args) :
				node(nullptr), function(std::forward<Args>(args)...) {
			}
			/// Creates a node that immediately links after the given node.
			template <typename ...Args> explicit closure_node(node &n, Args &&...args) :
				node(n), function(std::forward<Args>(args)...) {
			}
		};

		/// A head node.
		struct head_node : public node {
		public:
			using event_type = event; ///< Event type.

			/// Initializes an isolated head node.
			head_node(std::nullptr_t) : node(nullptr) {
			}

			/// Invokes all \ref closure_node objects attached to this head node with the given arguments.
			template <typename ...Args> void invoke_all(Args &&...args) {
				for (node *cur = this->_next; cur != this; cur = cur->_next) {
					static_cast<closure_node*>(cur)->function(std::forward<Args>(args)...);
				}
			}
			/// Shorthand for \ref closure_node::create_linked().
			template <typename ...Args> closure_node create_linked_node(Args &&...args) {
				return closure_node::create_linked(*this, std::forward<Args>(args)...);
			}
		};
	};
}
