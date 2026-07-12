#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "../host/dom.hpp"
#include "../value/raw_payload.hpp"

namespace cppreact {

namespace event_phase {
inline constexpr int capturing = 1;
inline constexpr int at_target = 2;
inline constexpr int bubbling = 3;
}

struct SyntheticEvent {
  std::string type{};
  DomNode target = null_dom_node;
  DomNode current_target = null_dom_node;

  int event_phase = 0;
  double time_stamp = 0;
  bool bubbles = false;
  bool cancelable = false;

  double detail = 0;

  double client_x = 0;
  double client_y = 0;
  bool ctrl_key = false;
  bool shift_key = false;
  bool alt_key = false;
  bool meta_key = false;
  int button = 0;
  int buttons = 0;
  DomNode related_target = null_dom_node;
  double movement_x = 0;
  double movement_y = 0;

  std::string key{};
  std::string code{};
  int location = 0;
  bool repeat = false;

  double delta_x = 0;
  double delta_y = 0;
  double delta_z = 0;
  int delta_mode = 0;

  std::string data{};
  std::string value{};

  RawPayload native{};
  std::function<void()> native_prevent_default{};
  std::function<void()> native_stop_propagation{};

  mutable bool default_prevented = false;
  mutable bool propagation_stopped = false;
  mutable std::uint64_t dispatched = 0;

  void prevent_default() const {
    default_prevented = true;
    if (native_prevent_default) native_prevent_default();
  }

  void stop_propagation() const {
    if (native_stop_propagation) native_stop_propagation();
    propagation_stopped = true;
  }

  bool is_default_prevented() const { return default_prevented; }
  bool is_propagation_stopped() const { return propagation_stopped; }

  bool get_modifier_state(std::string_view modifier_key) const {
    if (modifier_key == "alt") return alt_key;
    if (modifier_key == "control") return ctrl_key;
    if (modifier_key == "meta") return meta_key;
    if (modifier_key == "shift") return shift_key;
    return false;
  }
};

using Event = SyntheticEvent;

using EventCallback = std::function<void(const Event&)>;

}
