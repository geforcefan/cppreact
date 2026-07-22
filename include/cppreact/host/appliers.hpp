#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "../vnode/options.hpp"
#include "../host/host.hpp"
#include "../value/style.hpp"
#include "../value/value.hpp"
#include "../event/synthetic_event.hpp"

namespace cppreact {

namespace detail {

struct EventListenerEntry {
  std::uint64_t attached = 0;
  EventListenerToken token = null_event_listener;
};

inline std::map<DomNode, std::map<std::string, EventListenerEntry>>& event_listeners() {
  static thread_local std::map<DomNode, std::map<std::string, EventListenerEntry>> listeners{};
  return listeners;
}

inline std::uint64_t& event_clock() {
  static thread_local std::uint64_t clock = 1;
  return clock;
}

inline void clear_event_listeners(DomNode node) { event_listeners().erase(node); }

}

inline void apply_event_handler_to_dom(Host& host, DomNode dom, const std::string& event_type, bool use_capture,
                        const EventCallback& next, const EventCallback* old) {
  const bool has_next = static_cast<bool>(next);
  const bool had_old = old && static_cast<bool>(*old);
  if (!has_next && !had_old) return;

  const std::string listener_name = event_type + (use_capture ? "capture" : "");

  host.set_event_handler(dom, listener_name, has_next ? next : EventCallback{});

  std::map<std::string, detail::EventListenerEntry>& listeners = detail::event_listeners()[dom];

  if (has_next) {
    detail::EventListenerEntry& entry = listeners[listener_name];
    if (!had_old) {
      entry.attached = detail::event_clock();
      Host* host_pointer = &host;
      entry.token = host.add_event_listener(
          dom, event_type,
          [host_pointer, dom, listener_name](SyntheticEvent& event) {
            auto& nodes = detail::event_listeners();
            const auto node_entry = nodes.find(dom);
            if (node_entry == nodes.end()) return;
            const auto listener_entry = node_entry->second.find(listener_name);
            if (listener_entry == node_entry->second.end()) return;
            const detail::EventListenerEntry& listener = listener_entry->second;

            std::function<void(const SyntheticEvent&)> handler =
                host_pointer->event_handler(dom, listener_name);
            if (!handler) return;

            if (event.dispatched == 0) {
              event.dispatched = detail::event_clock()++;
            } else if (event.dispatched < listener.attached) {
              return;
            }

            if (options().event) options().event(event);
            handler(event);
          },
          use_capture);
    }
  } else {
    const auto entry = listeners.find(listener_name);
    if (entry != listeners.end()) {
      host.remove_event_listener(dom, event_type, entry->second.token);
      listeners.erase(entry);
    }
    if (listeners.empty()) detail::event_listeners().erase(dom);
  }
}

inline void apply_text_attribute_to_dom(Host& host, DomNode dom, std::string_view name,
                                 const std::string& next, const std::string* old) {
  if (old && *old == next) return;
  if (next.empty()) {
    if (old && !old->empty()) host.set_property(dom, name, Value{});
    return;
  }
  host.set_property(dom, name, next);
}

inline void apply_flag_attribute_to_dom(Host& host, DomNode dom, std::string_view name, bool next,
                                 const bool* old) {
  if (old && *old == next) return;
  if (!next) {
    if (old && *old) host.set_property(dom, name, Value{});
    return;
  }
  host.set_property(dom, name, true);
}

template <class Stored>
void apply_controlled_property_to_dom(Host& host, DomNode dom, std::string_view name,
                                      const std::optional<Stored>& next,
                                      const std::optional<Stored>* old) {
  if (next) {
    if (!values_equal(Value(*next), host.property_value(dom, name))) {
      host.set_property(dom, name, *next);
    }
    return;
  }
  if (old && *old) host.set_property(dom, name, Value{});
}

inline void apply_style_to_dom(Host& host, DomNode dom, const Style& next, const Style* old) {
  if (old) {
    for (const StyleProperty& previous : *old) {
      if (!next.get(previous.name)) host.set_style_property(dom, previous.name, "");
    }
  }
  for (const StyleProperty& property : next) {
    const std::string* previous = old ? old->get(property.name) : nullptr;
    if (!previous || *previous != property.value) {
      host.set_style_property(dom, property.name, property.value);
    }
  }
}

inline void apply_native_property_to_dom(Host& host, DomNode dom, std::string_view name, const Payload& next,
                         const Payload* old) {
  if (old && *old == next) return;
  host.set_native_property(dom, name, next);
}

template <class Properties>
void apply_common_props_to_dom(Host& host, DomNode dom, const Properties& next, const Properties* old) {
  if constexpr (requires { next.class_name; }) {
    apply_text_attribute_to_dom(host, dom, "class", next.class_name, old ? &old->class_name : nullptr);
  }
  if constexpr (requires { next.style; }) {
    apply_style_to_dom(host, dom, next.style, old ? &old->style : nullptr);
  }
}

#define CPPREACT_APPLY_EVENT(field, event_type, use_capture)                              \
  if constexpr (requires { next.field; }) {                                              \
    apply_event_handler_to_dom(host, dom, event_type, use_capture, next.field,                          \
                old ? &old->field : nullptr);                                            \
  }

template <class Properties>
void apply_event_handlers_to_dom(Host& host, DomNode dom, const Properties& next, const Properties* old) {
  CPPREACT_APPLY_EVENT(on_click, "click", false)
  CPPREACT_APPLY_EVENT(on_click_capture, "click", true)
  CPPREACT_APPLY_EVENT(on_double_click, "double_click", false)
  CPPREACT_APPLY_EVENT(on_double_click_capture, "double_click", true)
  CPPREACT_APPLY_EVENT(on_mouse_down, "mouse_down", false)
  CPPREACT_APPLY_EVENT(on_mouse_down_capture, "mouse_down", true)
  CPPREACT_APPLY_EVENT(on_mouse_up, "mouse_up", false)
  CPPREACT_APPLY_EVENT(on_mouse_up_capture, "mouse_up", true)
  CPPREACT_APPLY_EVENT(on_mouse_move, "mouse_move", false)
  CPPREACT_APPLY_EVENT(on_mouse_move_capture, "mouse_move", true)
  CPPREACT_APPLY_EVENT(on_mouse_over, "mouse_over", false)
  CPPREACT_APPLY_EVENT(on_mouse_over_capture, "mouse_over", true)
  CPPREACT_APPLY_EVENT(on_mouse_out, "mouse_out", false)
  CPPREACT_APPLY_EVENT(on_mouse_out_capture, "mouse_out", true)
  CPPREACT_APPLY_EVENT(on_key_down, "key_down", false)
  CPPREACT_APPLY_EVENT(on_key_down_capture, "key_down", true)
  CPPREACT_APPLY_EVENT(on_key_up, "key_up", false)
  CPPREACT_APPLY_EVENT(on_key_up_capture, "key_up", true)
  CPPREACT_APPLY_EVENT(on_wheel, "wheel", false)
  CPPREACT_APPLY_EVENT(on_wheel_capture, "wheel", true)
  CPPREACT_APPLY_EVENT(on_focus, "focus", false)
  CPPREACT_APPLY_EVENT(on_focus_capture, "focus", true)
  CPPREACT_APPLY_EVENT(on_blur, "blur", false)
  CPPREACT_APPLY_EVENT(on_blur_capture, "blur", true)
  CPPREACT_APPLY_EVENT(on_change, "change", false)
  CPPREACT_APPLY_EVENT(on_change_capture, "change", true)
}

#undef CPPREACT_APPLY_EVENT

}
