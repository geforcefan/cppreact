#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

#include "../vnode/options.hpp"
#include "../host/host.hpp"
#include "../value/object.hpp"
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

inline bool is_event_property(std::string_view name) {
  return name.size() > 3 && name[0] == 'o' && name[1] == 'n' && name[2] == '_';
}

inline bool ends_with_capture(std::string_view name) {
  constexpr std::string_view suffix = "_capture";
  return name.size() > suffix.size() + 3 && name.substr(name.size() - suffix.size()) == suffix;
}

inline std::string get_event_type(std::string_view name) {
  std::string_view event_type = name.substr(3);
  if (ends_with_capture(name)) {
    event_type = event_type.substr(0, event_type.size() - 8);
  }
  return std::string(event_type);
}

inline void set_style(Host& host, DomNode dom, const std::string& name,
                      std::string_view style_value) {
  host.set_style_property(dom, name, style_value);
}

inline void set_property(Host& host, DomNode dom, std::string_view name,
                         const Value& value, const Value& old_value) {
  if (name == "style") {
    const std::shared_ptr<Object>* old_entries = std::get_if<std::shared_ptr<Object>>(&old_value);
    const std::shared_ptr<Object>* new_entries = std::get_if<std::shared_ptr<Object>>(&value);
    const Object* old_style = old_entries ? old_entries->get() : nullptr;
    const Object* new_style = new_entries ? new_entries->get() : nullptr;

    if (old_style) {
      for (const auto& [old_name, old_style_value] : *old_style) {
        if (!new_style || !new_style->get(old_name)) {
          set_style(host, dom, old_name, "");
        }
      }
    }

    if (new_style) {
      for (const auto& [new_name, new_style_value] : *new_style) {
        const Value* previous = old_style ? old_style->get(new_name) : nullptr;
        if (!previous || !values_equal(*previous, new_style_value)) {
          const std::string* text = std::get_if<std::string>(&new_style_value);
          set_style(host, dom, new_name, text ? *text : "");
        }
      }
    }
  }

  else if (is_event_property(name)) {
    const bool use_capture = ends_with_capture(name);
    const std::string event_type = get_event_type(name);
    const std::string listener_name = event_type + (use_capture ? "capture" : "");

    const EventCallback* next_handler = value_as<EventCallback>(value);
    const EventCallback* old_handler = value_as<EventCallback>(old_value);
    const bool has_next = next_handler && *next_handler;
    const bool had_old = old_handler && *old_handler;

    host.set_event_handler(dom, listener_name, has_next ? *next_handler : EventCallback{});

    std::map<std::string, detail::EventListenerEntry>& listeners =
        detail::event_listeners()[dom];

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

  else if (std::holds_alternative<RawPayload>(value) ||
           std::holds_alternative<RawPayload>(old_value)) {
    const RawPayload* payload = std::get_if<RawPayload>(&value);
    host.set_native_property(dom, name, payload ? *payload : RawPayload{});
  }

  else if (std::holds_alternative<Callback>(value) ||
           std::holds_alternative<ReferenceObject>(value) ||
           std::holds_alternative<std::shared_ptr<Object>>(value)) {
    return;
  }

  else {
    host.set_property(dom, name, value);
  }
}

}
