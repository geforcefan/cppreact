#pragma once

#include <string>
#include <utility>

#include "use_effect.hpp"
#include "use_ref.hpp"

namespace cppreact {

inline Host* current_component_host() {
  return detail::current_component ? detail::current_component->host : nullptr;
}

inline void use_document_event(const std::string& type, EventCallback handler) {
  Host* host = current_component_host();
  EventCallback& latest = use_ref<EventCallback>(EventCallback{});
  latest = std::move(handler);
  EventCallback* latest_pointer = &latest;
  use_effect(
      [host, type, latest_pointer]() -> CleanupFunction {
        if (!host) return {};
        DomNode document = host->document();
        EventListenerToken token = host->add_event_listener(
            document, type,
            [latest_pointer](SyntheticEvent& event) {
              EventCallback current = *latest_pointer;
              if (current) current(event);
            },
            false);
        return [host, document, type, token] {
          host->remove_event_listener(document, type, token);
        };
      },
      {});
}

}
