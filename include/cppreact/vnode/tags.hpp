#pragma once

#include <optional>
#include <string>

#include "element.hpp"
#include "children.hpp"
#include "../host/appliers.hpp"
#include "../value/style.hpp"
#include "../event/synthetic_event.hpp"

namespace cppreact::tags {

#define CPPREACT_POINTER_EVENT_FIELDS                                                     \
  EventCallback on_click{};                                                               \
  EventCallback on_click_capture{};                                                      \
  EventCallback on_double_click{};                                                       \
  EventCallback on_double_click_capture{};                                               \
  EventCallback on_mouse_down{};                                                         \
  EventCallback on_mouse_down_capture{};                                                 \
  EventCallback on_mouse_up{};                                                           \
  EventCallback on_mouse_up_capture{};                                                   \
  EventCallback on_mouse_move{};                                                         \
  EventCallback on_mouse_move_capture{};                                                 \
  EventCallback on_mouse_over{};                                                         \
  EventCallback on_mouse_over_capture{};                                                 \
  EventCallback on_mouse_out{};                                                          \
  EventCallback on_mouse_out_capture{};                                                  \
  EventCallback on_wheel{};                                                              \
  EventCallback on_wheel_capture{};

#define CPPREACT_FOCUS_EVENT_FIELDS                                                       \
  EventCallback on_key_down{};                                                           \
  EventCallback on_key_down_capture{};                                                   \
  EventCallback on_key_up{};                                                             \
  EventCallback on_key_up_capture{};                                                     \
  EventCallback on_focus{};                                                              \
  EventCallback on_focus_capture{};                                                      \
  EventCallback on_blur{};                                                               \
  EventCallback on_blur_capture{};

struct ViewProps {
  std::string class_name{};
  Style style{};
  CPPREACT_POINTER_EVENT_FIELDS
  CPPREACT_FOCUS_EVENT_FIELDS
  std::string key{};
  Reference ref{};
  Children children{};
};

struct TextProps {
  std::string class_name{};
  Style style{};
  CPPREACT_POINTER_EVENT_FIELDS
  std::string key{};
  Reference ref{};
  Children children{};
};

struct InputProps {
  std::string class_name{};
  Style style{};
  std::string type{};
  std::optional<std::string> value{};
  std::optional<bool> checked{};
  bool disabled = false;
  CPPREACT_POINTER_EVENT_FIELDS
  CPPREACT_FOCUS_EVENT_FIELDS
  EventCallback on_change{};
  EventCallback on_change_capture{};
  std::string key{};
  Reference ref{};
};

struct TextareaProps {
  std::string class_name{};
  Style style{};
  std::optional<std::string> value{};
  bool disabled = false;
  CPPREACT_POINTER_EVENT_FIELDS
  CPPREACT_FOCUS_EVENT_FIELDS
  EventCallback on_change{};
  EventCallback on_change_capture{};
  std::string key{};
  Reference ref{};
};

struct ImageProps {
  std::string class_name{};
  Style style{};
  std::string source{};
  CPPREACT_POINTER_EVENT_FIELDS
  std::string key{};
  Reference ref{};
};

#undef CPPREACT_POINTER_EVENT_FIELDS
#undef CPPREACT_FOCUS_EVENT_FIELDS

inline void apply_props_to_dom(Host& host, DomNode dom, const ViewProps& next,
                               const ViewProps* old) {
  apply_common_props_to_dom(host, dom, next, old);
  apply_event_handlers_to_dom(host, dom, next, old);
}

inline void apply_props_to_dom(Host& host, DomNode dom, const TextProps& next,
                               const TextProps* old) {
  apply_common_props_to_dom(host, dom, next, old);
  apply_event_handlers_to_dom(host, dom, next, old);
}

inline void apply_props_to_dom(Host& host, DomNode dom, const InputProps& next,
                               const InputProps* old) {
  apply_common_props_to_dom(host, dom, next, old);
  apply_event_handlers_to_dom(host, dom, next, old);
  apply_text_attribute_to_dom(host, dom, "type", next.type, old ? &old->type : nullptr);
  apply_flag_attribute_to_dom(host, dom, "disabled", next.disabled, old ? &old->disabled : nullptr);
  apply_controlled_property_to_dom(host, dom, "value", next.value, old ? &old->value : nullptr);
  apply_controlled_property_to_dom(host, dom, "checked", next.checked,
                                   old ? &old->checked : nullptr);
}

inline void apply_props_to_dom(Host& host, DomNode dom, const TextareaProps& next,
                               const TextareaProps* old) {
  apply_common_props_to_dom(host, dom, next, old);
  apply_event_handlers_to_dom(host, dom, next, old);
  apply_flag_attribute_to_dom(host, dom, "disabled", next.disabled, old ? &old->disabled : nullptr);
  apply_controlled_property_to_dom(host, dom, "value", next.value, old ? &old->value : nullptr);
}

inline void apply_props_to_dom(Host& host, DomNode dom, const ImageProps& next,
                               const ImageProps* old) {
  apply_common_props_to_dom(host, dom, next, old);
  apply_event_handlers_to_dom(host, dom, next, old);
  apply_text_attribute_to_dom(host, dom, "src", next.source, old ? &old->source : nullptr);
}

inline const Element<ViewProps> View{"view"};
inline const Element<TextProps> Text{"text"};
inline const Element<InputProps> Input{"input"};
inline const Element<TextareaProps> Textarea{"textarea"};
inline const Element<ImageProps> Image{"image"};

}
