#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "renderer.hpp"
#include "context.hpp"
#include "value.hpp"

namespace cppreact {

struct ComponentInstance;

struct VNode;

struct TextTag {
  std::string text{};
};

struct ElementTag {
  std::string tag{};
};

struct FragmentTag {};

using ComponentFunction = VNode (*)(const Props&);

using NodeType = std::variant<TextTag, ElementTag, ComponentFunction, FragmentTag>;

namespace vnode_flag {
constexpr std::uint8_t none = 0;
constexpr std::uint8_t insert = 1 << 0;
constexpr std::uint8_t matched = 1 << 1;
}

struct VNode {
  NodeType type{FragmentTag{}};
  Props props{};
  std::string key{};
  Ref ref{};
  std::vector<VNode> children{};

  std::shared_ptr<ContextBinding> context_binding{};
  std::shared_ptr<ContextCell> context_cell{};

  std::optional<ContextChain> context_scope{};

  NodeHandle dom = kNullNode;
  std::shared_ptr<ComponentInstance> component{};
  std::int32_t depth = 0;
  std::int32_t index = -1;
  std::uint8_t flags = vnode_flag::none;

  VNode() = default;
  VNode(const char* value);
  VNode(std::string value);
  VNode(int value);
  VNode(double value);

  VNode(VNode&&) noexcept;
  VNode& operator=(VNode&&) noexcept;
  VNode(const VNode&) = delete;
  VNode& operator=(const VNode&) = delete;
  ~VNode();
};

bool is_text(const VNode& node);
bool is_element(const VNode& node);
bool is_component(const VNode& node);
bool is_fragment(const VNode& node);

VNode h(std::string tag, Props props = {});
VNode h(std::string tag, Props props, std::vector<VNode> children);
VNode h(ComponentFunction component, Props props = {});
VNode h(ComponentFunction component, Props props, std::vector<VNode> children);

template <class... Children>
VNode h(std::string tag, Props props, Children&&... children) {
  std::vector<VNode> collected;
  collected.reserve(sizeof...(children));
  (collected.emplace_back(std::forward<Children>(children)), ...);
  return h(std::move(tag), std::move(props), std::move(collected));
}

template <class... Children>
VNode h(ComponentFunction component, Props props, Children&&... children) {
  std::vector<VNode> collected;
  collected.reserve(sizeof...(children));
  (collected.emplace_back(std::forward<Children>(children)), ...);
  return h(std::move(component), std::move(props), std::move(collected));
}

VNode fragment(std::vector<VNode> children = {});

template <class... Children>
VNode fragment(Children&&... children) {
  std::vector<VNode> collected;
  collected.reserve(sizeof...(children));
  (collected.emplace_back(std::forward<Children>(children)), ...);
  return fragment(std::move(collected));
}

VNode text(std::string value);

VNode clone(const VNode& node);

template <class T>
VNode provide(const Context<T>& context, T value, std::vector<VNode> children) {
  VNode node = fragment(std::move(children));
  node.context_binding = std::make_shared<ContextBinding>(ContextBinding{
      context.id,
      Payload{std::static_pointer_cast<void>(std::make_shared<T>(std::move(value)))},
      &payload_values_equal<T>});
  return node;
}

template <class T, class... Children>
VNode provide(const Context<T>& context, T value, Children&&... children) {
  std::vector<VNode> collected;
  collected.reserve(sizeof...(children));
  (collected.emplace_back(std::forward<Children>(children)), ...);
  return provide(context, std::move(value), std::move(collected));
}

template <class Range, class Transform>
std::vector<VNode> map(const Range& items, Transform&& transform) {
  std::vector<VNode> nodes;
  std::size_t index = 0;
  for (const auto& item : items) {
    if constexpr (std::is_invocable_v<Transform&, decltype(item), std::size_t>) {
      nodes.push_back(transform(item, index));
    } else {
      nodes.push_back(transform(item));
    }
    ++index;
  }
  return nodes;
}

template <class Producer>
VNode when(bool condition, Producer&& producer) {
  if (condition) {
    if constexpr (std::is_invocable_v<Producer>) {
      return producer();
    } else {
      return std::forward<Producer>(producer);
    }
  }
  return fragment();
}

class Component {
public:
  template <class Render, class = decltype(+std::declval<Render&>())>
  Component(Render render) : function_pointer(+render) {}

  template <class... Children>
  VNode operator()(Props props = {}, Children&&... children) const {
    return h(function_pointer, std::move(props), std::forward<Children>(children)...);
  }

  ComponentFunction function() const;

private:
  ComponentFunction function_pointer;
};

Component memo(Component component);
bool is_memoized(ComponentFunction function);

}

#include "component.hpp"
