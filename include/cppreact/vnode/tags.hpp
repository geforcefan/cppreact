#pragma once

#include <utility>
#include <vector>

#include "create.hpp"

namespace cppreact::tags {

#define CPPREACT_TAG(name, tag)                                                   \
  inline VNode name(Object props, std::vector<VNode> children) {                   \
    return h(tag, std::move(props), std::move(children));                         \
  }                                                                               \
  template <class... Children>                                                    \
  inline VNode name(Object props = {}, Children&&... children) {                   \
    return h(tag, std::move(props), std::forward<Children>(children)...);         \
  }

CPPREACT_TAG(View, "view")
CPPREACT_TAG(Text, "text")
CPPREACT_TAG(Input, "input")
CPPREACT_TAG(Textarea, "textarea")
CPPREACT_TAG(Image, "image")

#undef CPPREACT_TAG

}
