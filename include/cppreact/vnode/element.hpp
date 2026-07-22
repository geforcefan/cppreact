#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "create.hpp"

namespace cppreact {

namespace detail {

template <class Properties>
void apply_payload_to_dom(Host& host, DomNode dom, const Payload& next, const Payload* old) {
  std::shared_ptr<const Properties> typed_next = payload_as<Properties>(next);
  if (!typed_next) return;
  std::shared_ptr<const Properties> typed_old = old ? payload_as<Properties>(*old) : nullptr;
  apply_props_to_dom(host, dom, *typed_next, typed_old ? typed_old.get() : nullptr);
}

}

template <class Properties>
struct Element {
  std::string tag;

  VNode operator()(Properties properties = {}) const {
    std::string key{};
    Reference ref{};
    if constexpr (requires { properties.key; }) key = properties.key;
    if constexpr (requires { properties.ref; }) ref = properties.ref;
    std::vector<VNode> children{};
    if constexpr (requires { properties.children; }) {
      children = std::move(properties.children.nodes);
    }
    return create_vnode(
        ElementTag{tag, make_payload(std::move(properties)),
                   &detail::apply_payload_to_dom<Properties>},
        std::move(key), std::move(ref), std::move(children));
  }
};

}
