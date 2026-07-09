#pragma once

#include <vector>

#include "renderer.hpp"
#include "vnode.hpp"
#include "component.hpp"
#include "diff.hpp"
#include "hooks.hpp"

namespace cppreact {

void flush();

struct Container {
  Renderer* renderer = nullptr;
  NodeHandle mount = kNullNode;
  std::vector<VNode> tree{};
};

void render(VNode next, Container& container);

}

#include <algorithm>
#include <memory>
#include <vector>

#include "component.hpp"
#include "context.hpp"
#include "diff.hpp"
#include "hooks.hpp"

namespace cppreact {

namespace detail {

inline thread_local std::vector<std::weak_ptr<ComponentInstance>> rerender_queue{};
inline thread_local bool flushing = false;

inline void install_once() {
  static bool installed = false;
  if (!installed) {
    installed = true;
    install_hooks();
  }
}

}

inline void enqueue_render(ComponentInstance* instance) {
  if (instance->bits & component_bit::dirty) return;
  instance->bits |= component_bit::dirty;
  detail::rerender_queue.push_back(instance->weak_from_this());
}

inline void flush() {
  if (detail::flushing) return;
  detail::flushing = true;

  do {
    while (!detail::rerender_queue.empty()) {
      std::vector<std::weak_ptr<ComponentInstance>> batch;
      batch.swap(detail::rerender_queue);

      std::vector<std::shared_ptr<ComponentInstance>> live;
      live.reserve(batch.size());
      for (std::weak_ptr<ComponentInstance>& weak : batch) {
        std::shared_ptr<ComponentInstance> instance = weak.lock();
        if (instance && (instance->bits & component_bit::dirty)) live.push_back(std::move(instance));
      }

      std::sort(live.begin(), live.end(),
                [](const std::shared_ptr<ComponentInstance>& left,
                   const std::shared_ptr<ComponentInstance>& right) {
                  return left->depth < right->depth;
                });

      for (const std::shared_ptr<ComponentInstance>& instance : live) {
        if (!(instance->bits & component_bit::dirty)) continue;
        instance->bits &= static_cast<std::uint8_t>(~component_bit::dirty);
        NodeHandle old_dom = first_dom(instance->rendered);
        render_instance(*instance->renderer, instance.get(), instance->context, old_dom);
      }
    }
    flush_refs();
    commit_effects();
  } while (!detail::rerender_queue.empty());

  detail::flushing = false;
}

inline void render(VNode next, Container& container) {
  detail::install_once();
  std::vector<VNode> next_tree;
  next_tree.push_back(std::move(next));
  NodeHandle old_dom = first_dom(container.tree);
  diff_children(*container.renderer, container.mount, next_tree, container.tree, ContextChain{},
                old_dom, 0);
  container.tree = std::move(next_tree);
  flush_refs();
  commit_effects();
}

}
