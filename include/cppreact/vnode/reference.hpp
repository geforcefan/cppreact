#pragma once

#include <functional>
#include <memory>
#include <variant>

#include "../value/callback.hpp"
#include "../host/dom.hpp"

namespace cppreact {

struct ReferenceObject {
  std::shared_ptr<DomNode> dom = std::make_shared<DomNode>(null_dom_node);

  DomNode current() const { return *dom; }

  bool operator==(const ReferenceObject& other) const {
    return dom == other.dom;
  }
};

using ReferenceCallback = std::function<void(DomNode)>;

using Reference = std::variant<std::monostate, ReferenceObject, Callback>;

inline bool has_reference(const Reference& reference) {
  if (const Callback* callback = std::get_if<Callback>(&reference)) {
    return static_cast<bool>(*callback);
  }
  return std::holds_alternative<ReferenceObject>(reference);
}

}
