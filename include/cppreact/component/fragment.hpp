#pragma once

#include "function_component.hpp"
#include "../vnode/children.hpp"
#include "../vnode/create.hpp"

namespace cppreact {

struct FragmentProps {
  Children children{};

  bool operator==(const FragmentProps&) const = default;
};

inline const FunctionComponent Fragment = [](const FragmentProps& props) -> VNode {
  return props.children;
};

}
