#pragma once

#include <cstdio>
#include <set>
#include <string>

#include "component.hpp"

namespace cppreact {

namespace detail {

inline std::string number_to_text(double value) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%g", value);
  return std::string(buffer);
}

inline void extract_key(VNode& node, Props& props) {
  Value key = props.take("key");
  if (const std::string* text = std::get_if<std::string>(&key)) {
    node.key = *text;
  } else if (const double* number = std::get_if<double>(&key)) {
    node.key = number_to_text(*number);
  }
}

inline void extract_ref(VNode& node, Props& props) {
  Value value = props.take("ref");
  if (const Payload* payload = std::get_if<Payload>(&value)) {
    if (payload->data) node.ref = *static_cast<Ref*>(payload->data.get());
  }
}

inline std::set<ComponentFunction>& memoized_components() {
  static std::set<ComponentFunction> registry;
  return registry;
}

}

inline VNode::VNode(const char* value) : type(TextTag{std::string(value)}) {}
inline VNode::VNode(std::string value) : type(TextTag{std::move(value)}) {}
inline VNode::VNode(int value) : type(TextTag{detail::number_to_text(static_cast<double>(value))}) {}
inline VNode::VNode(double value) : type(TextTag{detail::number_to_text(value)}) {}

inline VNode::VNode(VNode&&) noexcept = default;
inline VNode& VNode::operator=(VNode&&) noexcept = default;
inline VNode::~VNode() = default;

inline bool is_text(const VNode& node) { return std::holds_alternative<TextTag>(node.type); }
inline bool is_element(const VNode& node) { return std::holds_alternative<ElementTag>(node.type); }
inline bool is_component(const VNode& node) {
  return std::holds_alternative<ComponentFunction>(node.type);
}
inline bool is_fragment(const VNode& node) { return std::holds_alternative<FragmentTag>(node.type); }

inline VNode h(std::string tag, Props props) {
  return h(std::move(tag), std::move(props), std::vector<VNode>{});
}

inline VNode h(std::string tag, Props props, std::vector<VNode> children) {
  VNode node;
  detail::extract_key(node, props);
  detail::extract_ref(node, props);
  node.type = ElementTag{std::move(tag)};
  node.props = std::move(props);
  node.children = std::move(children);
  return node;
}

inline VNode h(ComponentFunction component, Props props) {
  return h(std::move(component), std::move(props), std::vector<VNode>{});
}

inline VNode h(ComponentFunction component, Props props, std::vector<VNode> children) {
  VNode node;
  detail::extract_key(node, props);
  node.type = std::move(component);
  node.props = std::move(props);
  node.children = std::move(children);
  return node;
}

inline VNode fragment(std::vector<VNode> children) {
  VNode node;
  node.type = FragmentTag{};
  node.children = std::move(children);
  return node;
}

inline VNode text(std::string value) { return VNode(std::move(value)); }

inline VNode clone(const VNode& node) {
  VNode copy;
  copy.type = node.type;
  copy.props = node.props;
  copy.key = node.key;
  copy.ref = node.ref;
  copy.context_binding = node.context_binding;
  copy.context_cell = node.context_cell;
  copy.context_scope = node.context_scope;
  copy.children.reserve(node.children.size());
  for (const VNode& child : node.children) copy.children.push_back(clone(child));
  return copy;
}

inline ComponentFunction Component::function() const { return function_pointer; }

inline Component memo(Component component) {
  detail::memoized_components().insert(component.function());
  return component;
}

inline bool is_memoized(ComponentFunction function) {
  return detail::memoized_components().count(function) != 0;
}

}
