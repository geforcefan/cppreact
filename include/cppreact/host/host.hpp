#pragma once

#include <cstddef>
#include <functional>
#include <string_view>

#include "dom.hpp"
#include "../value/payload.hpp"
#include "../value/value.hpp"

namespace cppreact {

struct SyntheticEvent;

using EventListenerToken = std::size_t;
constexpr EventListenerToken null_event_listener = 0;

using EventListener = std::function<void(SyntheticEvent&)>;

class Host {
public:
  virtual ~Host() = default;

  virtual DomNode create_element(std::string_view tag) = 0;
  virtual DomNode create_text(std::string_view text) = 0;
  virtual void set_text(DomNode node, std::string_view text) = 0;
  virtual void insert_before(DomNode parent, DomNode node, DomNode before) = 0;
  virtual void remove(DomNode node) = 0;
  virtual DomNode next_sibling(DomNode node) = 0;
  virtual DomNode parent_node(DomNode node) = 0;

  virtual void set_property(DomNode node, std::string_view name, const Value& value) = 0;
  virtual Value property_value(DomNode node, std::string_view name) { return {}; }
  virtual void set_style_property(DomNode node, std::string_view name,
                                  std::string_view value) = 0;

  virtual void set_event_handler(DomNode node, std::string_view name,
                                 std::function<void(const SyntheticEvent&)> handler) = 0;
  virtual std::function<void(const SyntheticEvent&)> event_handler(DomNode node,
                                                                   std::string_view name) = 0;
  virtual EventListenerToken add_event_listener(DomNode node, std::string_view type,
                                                EventListener listener, bool capture) = 0;
  virtual void remove_event_listener(DomNode node, std::string_view type,
                                     EventListenerToken token) = 0;

  virtual DomNode document() = 0;
  virtual void focus(DomNode, bool) {}

  virtual void set_native_property(DomNode, std::string_view, const Payload&) {}
  virtual Payload native_reference(DomNode) { return {}; }
  virtual void* native_element(DomNode) { return nullptr; }
};

}
