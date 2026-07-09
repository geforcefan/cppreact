#pragma once

#include <utility>
#include <vector>

#include "vnode.hpp"

namespace cppreact::tags {

#define CPPREACT_TAG(name)                                                        \
  inline VNode name(Props props, std::vector<VNode> children) {                   \
    return h(#name, std::move(props), std::move(children));                        \
  }                                                                               \
  template <class... Children>                                                    \
  inline VNode name(Props props = {}, Children&&... children) {                   \
    return h(#name, std::move(props), std::forward<Children>(children)...);       \
  }

CPPREACT_TAG(div)
CPPREACT_TAG(span)
CPPREACT_TAG(p)
CPPREACT_TAG(button)
CPPREACT_TAG(a)
CPPREACT_TAG(ul)
CPPREACT_TAG(ol)
CPPREACT_TAG(li)
CPPREACT_TAG(input)
CPPREACT_TAG(img)
CPPREACT_TAG(label)
CPPREACT_TAG(section)
CPPREACT_TAG(header)
CPPREACT_TAG(footer)
CPPREACT_TAG(nav)
CPPREACT_TAG(h1)
CPPREACT_TAG(h2)
CPPREACT_TAG(h3)
CPPREACT_TAG(strong)
CPPREACT_TAG(em)
CPPREACT_TAG(pre)
CPPREACT_TAG(code)

#undef CPPREACT_TAG

}
