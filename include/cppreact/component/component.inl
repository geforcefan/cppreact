#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "component.hpp"
#include "flags.hpp"
#include "../diff/children.hpp"
#include "../diff/diff.hpp"
#include "../vnode/clone.hpp"

namespace cppreact {

namespace detail {

inline thread_local std::vector<std::shared_ptr<ComponentInstance>> rerender_queue{};

}

inline void ComponentInstance::set_state() {
  if (host) enqueue_render(shared_from_this());
}

inline void ComponentInstance::force_update() {
  if (!host) return;
  flags |= component_flag::force;
  enqueue_render(shared_from_this());
}

inline void enqueue_render(const std::shared_ptr<ComponentInstance>& component) {
  if (!(component->flags & component_flag::dirty)) {
    component->flags |= component_flag::dirty;
    detail::rerender_queue.push_back(component);
  }
}

inline void render_component(const std::shared_ptr<ComponentInstance>& component) {
  if (!component->host) return;
  Host& host = *component->host;

  CommitQueue commit_queue;
  ReferenceQueue reference_queue;

  DomNode old_dom = first_dom(component->rendered);

  VNode new_vnode;
  new_vnode.type = ComponentTag{component->render_function, component->props};
  new_vnode.depth = component->depth;
  if (options().vnode) options().vnode(new_vnode);

  VNode old_vnode;
  old_vnode.type = ComponentTag{component->render_function, {}};
  old_vnode.component = component;

  component->rendering_self = true;
  diff(host, component->parent_dom, new_vnode, old_vnode, component->global_context,
       commit_queue, old_dom, reference_queue);
  component->rendering_self = false;

  commit_root(commit_queue, new_vnode, reference_queue);
}

inline void process() {
  std::size_t queue_length_after_sort = 1;

  while (!detail::rerender_queue.empty()) {
    if (detail::rerender_queue.size() > queue_length_after_sort) {
      std::stable_sort(detail::rerender_queue.begin(), detail::rerender_queue.end(),
                       [](const std::shared_ptr<ComponentInstance>& left,
                          const std::shared_ptr<ComponentInstance>& right) {
                         return left->depth < right->depth;
                       });
    }

    std::shared_ptr<ComponentInstance> component = detail::rerender_queue.front();
    detail::rerender_queue.erase(detail::rerender_queue.begin());
    queue_length_after_sort = detail::rerender_queue.size();

    if (component->flags & component_flag::dirty) {
      render_component(component);
    }
  }
}

}
