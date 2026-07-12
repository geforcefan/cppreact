#pragma once

#include <functional>
#include <utility>
#include <vector>

#include "diff/diff.hpp"

namespace cppreact {

struct Container {
  Host* host = nullptr;
  DomNode mount = null_dom_node;
  VNode root{};
};

namespace detail {

inline thread_local std::function<void()> passive_effects_flush{};

}

inline void render(VNode vnode, Container& container) {
  Host& host = *container.host;
  if (options().root) options().root(vnode, container);

  std::vector<VNode> root_children;
  root_children.push_back(std::move(vnode));
  VNode new_root = h(Fragment, Object{}, std::move(root_children));

  CommitQueue commit_queue;
  ReferenceQueue reference_queue;

  VNode old_root = std::move(container.root);
  DomNode old_dom = old_root.component ? first_dom(old_root.component->rendered) : null_dom_node;

  diff(host, container.mount, new_root, old_root, GlobalContext{}, commit_queue, old_dom,
       reference_queue);

  commit_root(commit_queue, new_root, reference_queue);

  container.root = std::move(new_root);

  if (detail::passive_effects_flush) detail::passive_effects_flush();
}

inline void flush() {
  process();
  if (detail::passive_effects_flush) detail::passive_effects_flush();
}

}
