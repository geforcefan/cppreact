#pragma once

#include <vector>

#include "renderer.hpp"
#include "context.hpp"
#include "vnode.hpp"
#include "component.hpp"

namespace cppreact {

struct ComponentInstance;

NodeHandle diff(Renderer& renderer, NodeHandle parent_dom, VNode& next, VNode& old,
                const ContextChain& context, NodeHandle old_dom, int depth);

NodeHandle diff_children(Renderer& renderer, NodeHandle parent_dom,
                         std::vector<VNode>& new_children, std::vector<VNode>& old_children,
                         const ContextChain& context, NodeHandle old_dom, int depth);

NodeHandle render_instance(Renderer& renderer, ComponentInstance* instance,
                           const ContextChain& context, NodeHandle old_dom);

NodeHandle first_dom(const VNode& node);
NodeHandle first_dom(const std::vector<VNode>& nodes);

void flush_refs();

}

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

#include "component.hpp"
#include "options.hpp"

namespace cppreact {

namespace detail {
inline thread_local std::vector<std::pair<Ref, NodeHandle>> ref_queue{};
}

inline bool same_type(const VNode& left, const VNode& right) {
  if (left.type.index() != right.type.index()) return false;
  if (is_element(left)) {
    return std::get<ElementTag>(left.type).tag == std::get<ElementTag>(right.type).tag;
  }
  if (is_component(left)) {
    return std::get<ComponentFunction>(left.type) == std::get<ComponentFunction>(right.type);
  }
  return true;
}

inline bool is_event_prop(std::string_view name) {
  return name.size() > 3 && name[0] == 'o' && name[1] == 'n' && name[2] == '_';
}

inline bool ends_with_capture(std::string_view name) {
  static constexpr std::string_view suffix = "_capture";
  return name.size() > suffix.size() + 3 && name.substr(name.size() - suffix.size()) == suffix;
}

inline std::string get_event_type(std::string_view name) {
  std::string_view core = name.substr(3);
  if (ends_with_capture(name)) core = core.substr(0, core.size() - 8);
  return std::string(core);
}

inline NodeHandle place_node(Renderer& renderer, NodeHandle parent, NodeHandle node_dom,
                                   NodeHandle old_dom) {
  if (node_dom == kNullNode) return old_dom;
  if (node_dom == old_dom) return renderer.next_sibling(old_dom);
  renderer.insert_before(parent, node_dom, old_dom);
  return old_dom;
}

inline bool props_shallow_equal(const Props& next, const Props& old) {
  if (next.size() != old.size()) return false;
  for (const auto& [name, value] : next) {
    const Value* other = old.find(name);
    if (!other || !values_equal(value, *other)) return false;
  }
  return true;
}

inline NodeHandle place_nodes(Renderer& renderer, NodeHandle parent, const std::vector<VNode>& nodes,
                               NodeHandle old_dom) {
  for (const VNode& node : nodes) {
    if (is_text(node) || is_element(node)) {
      old_dom = place_node(renderer, parent, node.dom, old_dom);
    } else if (is_component(node)) {
      if (node.component) old_dom = place_nodes(renderer, parent, node.component->rendered, old_dom);
    } else {
      old_dom = place_nodes(renderer, parent, node.children, old_dom);
    }
  }
  return old_dom;
}

inline void unmount_dom(Renderer& renderer, NodeHandle parent, VNode& node, bool skip_remove = false) {
  if (node.ref) node.ref(kNullNode);

  const bool is_host = is_text(node) || is_element(node);
  const NodeHandle child_parent = is_host ? node.dom : parent;

  std::vector<VNode>* list = &node.children;
  if (is_component(node)) list = node.component ? &node.component->rendered : nullptr;
  if (list) {
    for (VNode& child : *list) unmount_dom(renderer, child_parent, child, skip_remove || is_host);
  }

  if (!skip_remove && is_host && node.dom != kNullNode) renderer.remove_child(parent, node.dom);
}

inline void diff_props(Renderer& renderer, NodeHandle dom, const Props& new_props,
                       const Props& old_props) {
  for (const auto& [name, value] : old_props) {
    if (new_props.has(name)) continue;
    if (is_event_prop(name)) {
      renderer.set_event_handler(dom, get_event_type(name), EventHandler{}, ends_with_capture(name));
    } else if (std::holds_alternative<Payload>(value)) {
      renderer.set_native_prop(dom, name, Payload{});
    } else {
      renderer.set_property(dom, name, std::monostate{}, value);
    }
  }
  for (const auto& [name, value] : new_props) {
    if (is_event_prop(name)) {
      const EventHandler* handler = std::get_if<EventHandler>(&value);
      renderer.set_event_handler(dom, get_event_type(name), handler ? *handler : EventHandler{},
                                ends_with_capture(name));
      continue;
    }
    const Value* old_value = old_props.find(name);
    const bool changed = !old_value || !values_equal(*old_value, value);
    if (!changed) continue;
    if (const Payload* payload = std::get_if<Payload>(&value)) {
      renderer.set_native_prop(dom, name, *payload);
    } else {
      renderer.set_property(dom, name, value, old_value ? *old_value : Value{std::monostate{}});
    }
  }
}

inline void flush_refs() {
  std::vector<std::pair<Ref, NodeHandle>> batch;
  batch.swap(detail::ref_queue);
  for (auto& [ref, node] : batch) {
    if (ref) ref(node);
  }
}

inline NodeHandle first_dom(const VNode& node) {
  if (is_text(node) || is_element(node)) return node.dom;
  if (is_component(node)) {
    return node.component ? first_dom(node.component->rendered) : kNullNode;
  }
  return first_dom(node.children);
}

inline NodeHandle first_dom(const std::vector<VNode>& nodes) {
  for (const VNode& node : nodes) {
    NodeHandle found = first_dom(node);
    if (found != kNullNode) return found;
  }
  return kNullNode;
}

inline NodeHandle render_instance(Renderer& renderer, ComponentInstance* instance,
                                  const ContextChain& context, NodeHandle old_dom) {
  instance->bits &= static_cast<std::uint8_t>(~component_bit::dirty);
  if (options().before_render) options().before_render(*instance);
  VNode result = instance->render_function(instance->props);

  int guard = 0;
  while ((instance->bits & component_bit::dirty) && ++guard < 25) {
    instance->bits &= static_cast<std::uint8_t>(~component_bit::dirty);
    if (options().before_render) options().before_render(*instance);
    result = instance->render_function(instance->props);
  }
  instance->bits &= static_cast<std::uint8_t>(~component_bit::dirty);

  std::vector<VNode> output;
  if (is_fragment(result) && !result.context_binding) {
    output = std::move(result.children);
  } else {
    output.push_back(std::move(result));
  }

  NodeHandle next_anchor = diff_children(renderer, instance->parent_dom, output, instance->rendered,
                                         context, old_dom, instance->depth + 1);
  instance->rendered = std::move(output);
  if (options().diffed) options().diffed(*instance);
  return next_anchor;
}

inline NodeHandle diff(Renderer& renderer, NodeHandle parent_dom, VNode& next, VNode& old,
                       const ContextChain& context, NodeHandle old_dom, int depth) {
  if (options().before_diff) options().before_diff(next);

  ContextChain child_context = context;
  if (next.context_scope) {
    child_context = *next.context_scope;
  } else if (next.context_binding) {
    const ContextBinding& binding = *next.context_binding;
    bool reuse_cell =
        old.context_cell && old.context_binding && old.context_binding->id == binding.id;
    next.context_cell = reuse_cell ? std::move(old.context_cell) : std::make_shared<ContextCell>();
    bool changed = reuse_cell && binding.values_match &&
                   !binding.values_match(next.context_cell->value, binding.value);
    next.context_cell->value = binding.value;
    if (changed) notify_context(*next.context_cell);
    child_context = extend_context(context, binding.id, next.context_cell);
  }

  if (is_text(next)) {
    const std::string& new_text = std::get<TextTag>(next.type).text;
    if (is_text(old) && old.dom != kNullNode) {
      next.dom = old.dom;
      if (new_text != std::get<TextTag>(old.type).text) renderer.set_text(next.dom, new_text);
    } else {
      next.dom = renderer.create_text(new_text);
    }
    return place_node(renderer, parent_dom, next.dom, old_dom);
  }

  if (is_element(next)) {
    const std::string& tag = std::get<ElementTag>(next.type).tag;
    bool reuse = is_element(old) && old.dom != kNullNode && std::get<ElementTag>(old.type).tag == tag;
    if (reuse) {
      next.dom = old.dom;
      diff_props(renderer, next.dom, next.props, old.props);
    } else {
      next.dom = renderer.create_element(tag, next.props);
      diff_props(renderer, next.dom, next.props, Props{});
    }

    const bool had_ref = reuse && static_cast<bool>(old.ref);
    const bool has_ref = static_cast<bool>(next.ref);
    if (had_ref != has_ref) {
      if (had_ref) old.ref(kNullNode);
      if (has_ref) detail::ref_queue.push_back({next.ref, next.dom});
    }

    std::vector<VNode> empty_children;
    std::vector<VNode>& old_children = reuse ? old.children : empty_children;
    NodeHandle inner_start = reuse ? first_dom(old.children) : kNullNode;
    diff_children(renderer, next.dom, next.children, old_children, child_context, inner_start, depth + 1);
    return place_node(renderer, parent_dom, next.dom, old_dom);
  }

  if (is_component(next)) {
    ComponentFunction function = std::get<ComponentFunction>(next.type);
    bool reuse = is_component(old) && old.component &&
                 std::get<ComponentFunction>(old.type) == function;
    if (reuse) {
      next.component = std::move(old.component);
      ComponentInstance* instance = next.component.get();
      const bool no_children = next.children.empty() && instance->children.empty();
      if (is_memoized(function) && !(instance->bits & component_bit::dirty) && no_children &&
          props_shallow_equal(next.props, instance->props)) {
        next.dom = first_dom(instance->rendered);
        return place_nodes(renderer, parent_dom, instance->rendered, old_dom);
      }
    } else {
      next.component = std::make_shared<ComponentInstance>();
    }
    ComponentInstance* instance = next.component.get();
    instance->render_function = function;
    instance->props = next.props;
    instance->children = std::move(next.children);
    instance->context = context;
    instance->renderer = &renderer;
    instance->parent_dom = parent_dom;
    instance->depth = depth;

    NodeHandle next_anchor = render_instance(renderer, instance, context, old_dom);
    next.dom = first_dom(instance->rendered);
    return next_anchor;
  }

  std::vector<VNode> empty_children;
  std::vector<VNode>& old_children = is_fragment(old) ? old.children : empty_children;
  NodeHandle next_anchor =
      diff_children(renderer, parent_dom, next.children, old_children, child_context, old_dom, depth + 1);
  next.dom = first_dom(next.children);
  return next_anchor;
}

inline NodeHandle diff_children(Renderer& renderer, NodeHandle parent_dom,
                                std::vector<VNode>& new_children, std::vector<VNode>& old_children,
                                const ContextChain& context, NodeHandle old_dom, int depth) {
  const std::size_t new_count = new_children.size();
  const std::size_t old_count = old_children.size();

  std::vector<int> match(new_count, -1);
  std::vector<char> used(old_count, 0);

  std::unordered_map<std::string, int> old_by_key;
  for (std::size_t j = 0; j < old_count; ++j) {
    if (!old_children[j].key.empty()) old_by_key.emplace(old_children[j].key, static_cast<int>(j));
  }

  for (std::size_t i = 0; i < new_count; ++i) {
    VNode& child = new_children[i];
    int found = -1;
    if (!child.key.empty()) {
      auto entry = old_by_key.find(child.key);
      if (entry != old_by_key.end() && !used[entry->second] &&
          same_type(child, old_children[entry->second])) {
        found = entry->second;
      }
    } else if (i < old_count && !used[i] && old_children[i].key.empty() &&
               same_type(child, old_children[i])) {
      found = static_cast<int>(i);
    } else {
      for (std::size_t j = 0; j < old_count; ++j) {
        if (!used[j] && old_children[j].key.empty() && same_type(child, old_children[j])) {
          found = static_cast<int>(j);
          break;
        }
      }
    }
    if (found >= 0) {
      used[found] = 1;
      match[i] = found;
    }
  }

  VNode empty_old;
  for (std::size_t i = 0; i < new_count; ++i) {
    VNode& child = new_children[i];
    VNode& old_node = match[i] >= 0 ? old_children[match[i]] : empty_old;
    old_dom = diff(renderer, parent_dom, child, old_node, context, old_dom, depth);
  }

  for (std::size_t j = 0; j < old_count; ++j) {
    if (!used[j]) unmount_dom(renderer, parent_dom, old_children[j]);
  }

  return old_dom;
}

}
