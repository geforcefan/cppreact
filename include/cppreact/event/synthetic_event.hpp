#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "../host/dom.hpp"
#include "key.hpp"

namespace cppreact {

struct SyntheticEvent {
  std::string type{};
  DomNode target = null_dom_node;

  double client_x = 0;
  double client_y = 0;
  bool ctrl_key = false;
  bool shift_key = false;
  bool alt_key = false;
  bool meta_key = false;
  int button = 0;
  double movement_x = 0;
  double movement_y = 0;

  Key key = Key::Unknown;

  double delta_x = 0;
  double delta_y = 0;
  double delta_z = 0;
  int delta_mode = 0;

  std::string data{};
  std::string value{};

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
};

using Event = SyntheticEvent;

using EventCallback = std::function<void(const Event&)>;

}
