#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "value.hpp"

namespace cppreact {

using NodeHandle = std::uint32_t;
constexpr NodeHandle kNullNode = 0;

using ListenerToken = std::uint32_t;
constexpr ListenerToken kNullListener = 0;
constexpr ListenerToken kCaptureListener = 1;

struct Event {
  NodeHandle target = kNullNode;
  NodeHandle current_target = kNullNode;
  std::string type{};

  double client_x = 0;
  double client_y = 0;
  int button = 0;
  bool shift_key = false;
  bool ctrl_key = false;
  bool alt_key = false;
  bool meta_key = false;
  std::string key{};
  std::string value{};
  double delta_x = 0;
  double delta_y = 0;

  std::function<void()> stop_propagation{};
  std::function<void()> prevent_default{};

  Payload native{};
};

using Ref = std::function<void(NodeHandle)>;

struct Box {
  double x = 0;
  double y = 0;
  double width = 0;
  double height = 0;

  bool operator==(const Box& other) const {
    return x == other.x && y == other.y && width == other.width && height == other.height;
  }
};

class Renderer {
public:
  virtual ~Renderer() = default;

  virtual NodeHandle document() const = 0;

  virtual NodeHandle create_element(std::string_view tag, const Props& props) = 0;
  virtual NodeHandle create_text(std::string_view text) = 0;
  virtual void set_text(NodeHandle node, std::string_view text) = 0;

  virtual void insert_before(NodeHandle parent, NodeHandle node, NodeHandle anchor) = 0;
  virtual void remove_child(NodeHandle parent, NodeHandle node) = 0;
  virtual NodeHandle next_sibling(NodeHandle node) = 0;
  virtual NodeHandle parent_node(NodeHandle node) = 0;

  virtual void set_property(NodeHandle node, std::string_view name, const Value& value,
                            const Value& old_value) = 0;
  virtual void set_event_handler(NodeHandle node, std::string_view type, EventHandler handler,
                                 bool capture) = 0;
  virtual ListenerToken add_event_listener(NodeHandle node, std::string_view type,
                                           EventHandler handler, bool capture) = 0;
  virtual void remove_event_listener(NodeHandle node, std::string_view type,
                                     ListenerToken token) = 0;

  virtual void focus(NodeHandle, bool select_all) { (void)select_all; }

  virtual void set_inner_html(NodeHandle, std::string_view) {}

  virtual Box measure(NodeHandle) { return {}; }

  virtual void set_native_prop(NodeHandle, std::string_view, const Payload&) {}
  virtual Payload native_reference(NodeHandle) { return {}; }

  virtual void* native_element(NodeHandle) { return nullptr; }
};

}
