#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "../component/component.hpp"
#include "../component/flags.hpp"
#include "../component/fragment.hpp"
#include "../vnode/options.hpp"
#include "../host/host.hpp"
#include "children.hpp"
#include "../host/appliers.hpp"

namespace cppreact {

DomNode diff_element_nodes(Host& host, DomNode dom, VNode& new_vnode,
                              VNode& old_vnode, const GlobalContext& global_context,
                              CommitQueue& commit_queue, ReferenceQueue& reference_queue);

void commit_root(CommitQueue& commit_queue, VNode& root, ReferenceQueue& reference_queue);

namespace detail {

inline thread_local ComponentInstance* current_rendering = nullptr;

inline std::map<const void*, std::function<void()>>& reference_cleanups() {
  static thread_local std::map<const void*, std::function<void()>> cleanups{};
  return cleanups;
}

}

inline DomNode diff(Host& host, DomNode parent_dom, VNode& new_vnode,
                       VNode& old_vnode, const GlobalContext& global_context,
                       CommitQueue& commit_queue, DomNode old_dom,
                       ReferenceQueue& reference_queue) {
  if (options().diff) options().diff(new_vnode);

  if (is_component(new_vnode)) {
    ComponentTag& new_tag = std::get<ComponentTag>(new_vnode.type);

    std::shared_ptr<ComponentInstance> component;
    if (old_vnode.component) {
      component = new_vnode.component = old_vnode.component;
    } else {
      component = new_vnode.component = std::make_shared<ComponentInstance>();
      component->render_function = new_tag.render;
      component->global_context = global_context;
      component->flags |= component_flag::dirty;
    }

    if (old_vnode.component) {
      if (!(component->flags & component_flag::force) && component->should_component_update &&
          !component->should_component_update(new_tag.props)) {
        component->props = new_tag.props;
        component->flags &= static_cast<std::uint8_t>(~component_flag::dirty);

        if (!component->render_callbacks.empty()) {
          commit_queue.push_back(component);
        }

        DomNode anchor = old_dom;
        for (VNode& child : component->rendered) {
          if (!is_null(child)) {
            anchor = insert(host, child, anchor, parent_dom, false);
          }
        }
        new_vnode.dom = first_dom(component->rendered);

        if (options().diffed) options().diffed(new_vnode);
        return anchor;
      }

      if (component->component_will_update) {
        component->component_will_update(new_tag.props);
      }
    }

    component->global_context = global_context;
    component->props = new_tag.props;
    component->host = &host;
    component->parent_dom = parent_dom;
    component->depth = new_vnode.depth;
    component->flags &= static_cast<std::uint8_t>(~component_flag::force);

    std::int32_t render_attempts = 0;
    VNode render_result_vnode;
    do {
      component->flags &= static_cast<std::uint8_t>(~component_flag::dirty);
      if (options().render) options().render(new_vnode);

      ComponentInstance* previous_rendering = detail::current_rendering;
      detail::current_rendering = component.get();
      render_result_vnode = (*component->render_function)(component->props);
      detail::current_rendering = previous_rendering;
    } while ((component->flags & component_flag::dirty) && ++render_attempts < 25);

    GlobalContext child_global_context = global_context;
    if (component->get_child_context) {
      GlobalContext extension = component->get_child_context();
      for (auto& [id, provider] : extension) child_global_context[id] = provider;
    }

    std::vector<VNode> render_result;
    if (is_component(render_result_vnode) &&
        std::get<ComponentTag>(render_result_vnode.type).render == Fragment.render_function() &&
        render_result_vnode.key.empty()) {
      render_result = std::move(render_result_vnode.children);
    } else if (is_fragment(render_result_vnode)) {
      render_result = std::move(render_result_vnode.children);
    } else {
      render_result.push_back(std::move(render_result_vnode));
    }

    new_vnode.children = std::move(render_result);
    old_dom = diff_children(host, parent_dom, new_vnode, component->rendered,
                            child_global_context, commit_queue, old_dom, reference_queue);
    component->rendered = std::move(new_vnode.children);
    new_vnode.children.clear();

    if (!component->render_callbacks.empty()) {
      commit_queue.push_back(component);
    }
  } else {
    old_dom = new_vnode.dom = diff_element_nodes(host, old_vnode.dom, new_vnode, old_vnode,
                                                 global_context, commit_queue, reference_queue);
  }

  if (options().diffed) options().diffed(new_vnode);

  return old_dom;
}

inline DomNode diff_element_nodes(Host& host, DomNode dom, VNode& new_vnode,
                                     VNode& old_vnode, const GlobalContext& global_context,
                                     CommitQueue& commit_queue,
                                     ReferenceQueue& reference_queue) {
  if (is_text(new_vnode)) {
    const std::string& new_text = std::get<TextTag>(new_vnode.type).text;
    if (dom == null_dom_node) {
      dom = host.create_text(new_text);
    } else if (!is_text(old_vnode) ||
               new_text != std::get<TextTag>(old_vnode.type).text) {
      host.set_text(dom, new_text);
    }

    return dom;
  }

  if (is_element(new_vnode)) {
    ElementTag& new_tag = std::get<ElementTag>(new_vnode.type);

    if (dom == null_dom_node) {
      dom = host.create_element(new_tag.tag);
    }

    const ElementTag* old_tag =
        is_element(old_vnode) ? &std::get<ElementTag>(old_vnode.type) : nullptr;
    const Payload* old_props =
        old_tag && old_tag->tag == new_tag.tag ? &old_tag->props : nullptr;

    if (new_tag.apply) {
      new_tag.apply(host, dom, new_tag.props, old_props);
    }
  }

  diff_children(host, dom, new_vnode, old_vnode.children, global_context, commit_queue,
                first_dom(old_vnode.children), reference_queue);

  return dom;
}

inline void apply_reference(const Reference& reference, DomNode value) {
  if (const Callback* callback = std::get_if<Callback>(&reference)) {
    auto& cleanups = detail::reference_cleanups();
    const auto cleanup_entry = cleanups.find(callback->identity());
    const bool had_cleanup = cleanup_entry != cleanups.end();
    if (had_cleanup) {
      cleanup_entry->second();
      cleanups.erase(cleanup_entry);
    }

    if (!had_cleanup || value != null_dom_node) {
      if (const auto* with_cleanup = callback->as<std::function<void()>(DomNode)>()) {
        std::function<void()> cleanup = (*with_cleanup)(value);
        if (cleanup) cleanups[callback->identity()] = std::move(cleanup);
      } else if (const ReferenceCallback* function =
                     callback->as<ReferenceCallback>()) {
        (*function)(value);
      }
    }
  } else if (const ReferenceObject* object = std::get_if<ReferenceObject>(&reference)) {
    *object->dom = value;
  }
}

inline void unmount(Host& host, VNode& vnode, bool skip_remove) {
  if (options().unmount) options().unmount(vnode);

  if (has_reference(vnode.ref)) {
    const ReferenceObject* object = std::get_if<ReferenceObject>(&vnode.ref);
    if (!object || object->current() == null_dom_node || object->current() == vnode.dom) {
      apply_reference(vnode.ref, null_dom_node);
    }
  }

  if (vnode.component) {
    if (vnode.component->component_will_unmount) {
      vnode.component->component_will_unmount();
    }

    vnode.component->host = nullptr;
    vnode.component->parent_dom = null_dom_node;
    vnode.component->global_context.clear();
  }

  std::vector<VNode>* children_list =
      is_component(vnode) ? (vnode.component ? &vnode.component->rendered : nullptr)
                          : &vnode.children;
  if (children_list) {
    for (VNode& child : *children_list) {
      if (!is_null(child)) {
        unmount(host, child, skip_remove || !is_component(vnode));
      }
    }
  }

  if (!skip_remove && vnode.dom != null_dom_node) {
    host.remove(vnode.dom);
  }

  if (vnode.dom != null_dom_node) {
    detail::clear_event_listeners(vnode.dom);
  }

  vnode.dom = null_dom_node;
  vnode.component = nullptr;
}

inline void commit_root(CommitQueue& commit_queue, VNode& root,
                        ReferenceQueue& reference_queue) {
  for (auto& [reference, value] : reference_queue) {
    apply_reference(reference, value);
  }

  if (options().commit) options().commit(root, commit_queue);

  for (const std::weak_ptr<ComponentInstance>& entry : commit_queue) {
    std::shared_ptr<ComponentInstance> component = entry.lock();
    if (!component) continue;
    std::vector<RenderCallback> callbacks = std::move(component->render_callbacks);
    component->render_callbacks.clear();
    for (const RenderCallback& entry_callback : callbacks) {
      if (entry_callback.callback) entry_callback.callback();
    }
  }
}

}

#include "../component/component.inl"
