#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "reference.hpp"
#include "../component/function_component.hpp"
#include "../value/object.hpp"

namespace cppreact {

struct ComponentInstance;

namespace detail {

inline std::string number_to_text(double value) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value > 0 ? "Infinity" : "-Infinity";
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%g", value);
  return std::string(buffer);
}

}

struct TextTag {
  std::string text{};
};

struct ElementTag {
  std::string tag{};
};

struct FragmentTag {};

struct NullTag {};

using VNodeType = std::variant<TextTag, ElementTag, FunctionComponent, FragmentTag, NullTag>;

struct VNode {
  VNodeType type{FragmentTag{}};
  Object props{};
  std::string key{};
  Reference ref{};
  std::vector<VNode> children{};

  std::int32_t depth = 0;
  DomNode dom = null_dom_node;
  std::shared_ptr<ComponentInstance> component{};
  std::int32_t index = -1;
  std::uint8_t flags = 0;

  VNode() = default;
  VNode(std::nullptr_t) : type(NullTag{}) {}
  VNode(const char* value) : type(TextTag{std::string(value)}) {}
  VNode(std::string value) : type(TextTag{std::move(value)}) {}
  template <class Number,
            std::enable_if_t<std::is_arithmetic_v<Number> && !std::is_same_v<Number, bool>, int> = 0>
  VNode(Number value) : type(TextTag{detail::number_to_text(static_cast<double>(value))}) {}

  VNode(VNode&&) noexcept = default;
  VNode& operator=(VNode&&) noexcept = default;
  VNode(const VNode&) = delete;
  VNode& operator=(const VNode&) = delete;
};

inline bool is_text(const VNode& node) { return std::holds_alternative<TextTag>(node.type); }
inline bool is_element(const VNode& node) { return std::holds_alternative<ElementTag>(node.type); }
inline bool is_component(const VNode& node) {
  return std::holds_alternative<FunctionComponent>(node.type);
}
inline bool is_fragment(const VNode& node) {
  return std::holds_alternative<FragmentTag>(node.type);
}
inline bool is_null(const VNode& node) { return std::holds_alternative<NullTag>(node.type); }

}
