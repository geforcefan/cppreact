#pragma once

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "vnode.hpp"

namespace cppreact {

inline VNode text(std::string value) { return VNode(std::move(value)); }

template <class Producer>
VNode when(bool condition, Producer&& producer) {
  if (condition) {
    if constexpr (std::is_invocable_v<Producer>) {
      return producer();
    } else {
      return std::forward<Producer>(producer);
    }
  }
  return nullptr;
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

}
