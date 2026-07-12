#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementText.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Types.h>

#include "cppreact/host/native.hpp"
#include "cppreact/render.hpp"
#include "cppreact/host/host.hpp"
#include "cppreact/event/synthetic_event.hpp"

namespace cppreact::hosts {

class NativeRmlElement : public Rml::Element, public NativeElement {
public:
  using Rml::Element::Element;
};

inline std::set<std::string>& native_tags() {
  static std::set<std::string> tags;
  return tags;
}

inline void register_native_tag(std::string tag) {
  native_tags().insert(std::move(tag));
}

inline std::string rml_key_name(int identifier) {
  switch (identifier) {
  case Rml::Input::KI_RETURN:
  case Rml::Input::KI_NUMPADENTER: return "enter";
  case Rml::Input::KI_ESCAPE: return "escape";
  case Rml::Input::KI_TAB: return "tab";
  case Rml::Input::KI_SPACE: return " ";
  case Rml::Input::KI_DELETE: return "delete";
  case Rml::Input::KI_BACK: return "backspace";
  case Rml::Input::KI_LEFT: return "arrow_left";
  case Rml::Input::KI_RIGHT: return "arrow_right";
  case Rml::Input::KI_UP: return "arrow_up";
  case Rml::Input::KI_DOWN: return "arrow_down";
  case Rml::Input::KI_LSHIFT:
  case Rml::Input::KI_RSHIFT: return "shift";
  default: break;
  }
  if (identifier >= Rml::Input::KI_A && identifier <= Rml::Input::KI_Z)
    return std::string(1, static_cast<char>('a' + (identifier - Rml::Input::KI_A)));
  if (identifier >= Rml::Input::KI_0 && identifier <= Rml::Input::KI_9)
    return std::string(1, static_cast<char>('0' + (identifier - Rml::Input::KI_0)));
  return "";
}

inline Rml::String rml_element_tag(std::string_view tag) {
  static const std::unordered_map<std::string, Rml::String> mapping = {
      {"view", "view"}, {"text", "text"},         {"input", "input"},
      {"image", "img"}, {"textarea", "textarea"},
  };
  auto entry = mapping.find(std::string(tag));
  return entry == mapping.end() ? Rml::String(tag) : entry->second;
}

inline Rml::String rml_event_name(std::string_view name) {
  static const std::unordered_map<std::string, Rml::String> mapping = {
      {"click", "click"},          {"mouse_down", "mousedown"}, {"mouse_up", "mouseup"},
      {"mouse_move", "mousemove"}, {"mouse_over", "mouseover"}, {"mouse_out", "mouseout"},
      {"key_down", "keydown"},     {"key_up", "keyup"},         {"wheel", "mousescroll"},
      {"focus", "focus"},          {"blur", "blur"},            {"change", "change"},
  };
  auto entry = mapping.find(std::string(name));
  return entry == mapping.end() ? Rml::String() : entry->second;
}

inline std::vector<Rml::String> rml_event_aliases(std::string_view name) {
  Rml::String primary = rml_event_name(name);
  if (primary.empty()) return {};
  std::vector<Rml::String> aliases{primary};
  if (name == "mouse_move") {
    aliases.emplace_back("dragstart");
    aliases.emplace_back("drag");
  } else if (name == "mouse_up") {
    aliases.emplace_back("dragend");
  }
  return aliases;
}

class RmlHost : public Host {
public:
  DomNode document() override { return document_handle; }

  Container create_container(Rml::Element* mount) {
    DomNode handle = register_element(mount);
    if (document_handle == null_dom_node) {
      Rml::ElementDocument* owner = mount->GetOwnerDocument();
      document_handle = owner ? register_element(owner) : handle;
    }
    return Container{this, handle, {}};
  }

  DomNode create_element(std::string_view tag) override {
    Rml::String name = rml_element_tag(tag);
    Rml::ElementPtr instance =
        Rml::Factory::InstanceElement(nullptr, name, name, Rml::XMLAttributes());
    Rml::Element* raw = instance.get();
    DomNode handle = register_element(raw);
    owned[handle] = std::move(instance);
    if (native_tags().count(std::string(tag))) {
      natives[handle] = static_cast<NativeRmlElement*>(raw);
    }
    return handle;
  }

  DomNode create_text(std::string_view text) override {
    Rml::ElementPtr instance =
        Rml::Factory::InstanceElement(nullptr, "#text", "#text", Rml::XMLAttributes());
    if (Rml::ElementText* element = rmlui_dynamic_cast<Rml::ElementText*>(instance.get())) {
      element->SetText(Rml::String(text));
    }
    Rml::Element* raw = instance.get();
    DomNode handle = register_element(raw);
    owned[handle] = std::move(instance);
    return handle;
  }

  void set_text(DomNode node, std::string_view text) override {
    if (Rml::ElementText* element = rmlui_dynamic_cast<Rml::ElementText*>(get_element(node))) {
      element->SetText(Rml::String(text));
    }
  }

  void insert_before(DomNode parent, DomNode node, DomNode anchor) override {
    Rml::Element* parent_element = get_element(parent);
    Rml::Element* node_element = get_element(node);
    if (!parent_element || !node_element) return;

    Rml::ElementPtr child;
    auto owned_entry = owned.find(node);
    if (owned_entry != owned.end()) {
      child = std::move(owned_entry->second);
      owned.erase(owned_entry);
    } else if (Rml::Element* current_parent = node_element->GetParentNode()) {
      child = current_parent->RemoveChild(node_element);
    }
    if (!child) return;

    Rml::Element* anchor_element = anchor != null_dom_node ? get_element(anchor) : nullptr;
    if (anchor_element) {
      parent_element->InsertBefore(std::move(child), anchor_element);
    } else {
      parent_element->AppendChild(std::move(child));
    }
  }

  void remove(DomNode node) override {
    Rml::Element* node_element = get_element(node);
    Rml::Element* parent_element = node_element ? node_element->GetParentNode() : nullptr;
    if (parent_element && node_element) {
      unregister_element(node_element);
      parent_element->RemoveChild(node_element);
    } else {
      elements.erase(node);
      owned.erase(node);
      listeners.erase(node);
      node_event_handlers.erase(node);
      natives.erase(node);
    }
  }

  DomNode next_sibling(DomNode node) override {
    Rml::Element* element = get_element(node);
    if (!element) return null_dom_node;
    Rml::Element* sibling = element->GetNextSibling();
    return sibling ? get_handle(sibling) : null_dom_node;
  }

  DomNode parent_node(DomNode node) override {
    Rml::Element* element = get_element(node);
    if (!element) return null_dom_node;
    Rml::Element* parent = element->GetParentNode();
    return parent ? get_handle(parent) : null_dom_node;
  }

  void set_property(DomNode node, std::string_view name, const Value& value) override {
    Rml::Element* element = get_element(node);
    if (!element) return;
    Rml::String attribute(name);
    if (std::holds_alternative<std::monostate>(value)) {
      element->RemoveAttribute(attribute);
    } else {
      element->SetAttribute(attribute, stringify(value));
    }
  }

  Value property_value(DomNode node, std::string_view name) override {
    Rml::Element* element = get_element(node);
    if (!element) return {};
    if (name == "value") {
      if (auto* control = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(element)) {
        return std::string(control->GetValue());
      }
    }
    Rml::Variant* attribute = element->GetAttribute(Rml::String(name));
    if (!attribute) return {};
    return std::string(attribute->Get<Rml::String>());
  }

  void set_style_property(DomNode node, std::string_view name,
                          std::string_view value) override {
    Rml::Element* element = get_element(node);
    if (!element) return;
    if (value.empty()) {
      element->RemoveProperty(Rml::String(name));
    } else {
      element->SetProperty(Rml::String(name), Rml::String(value));
    }
  }

  void set_event_handler(DomNode node, std::string_view name,
                         std::function<void(const SyntheticEvent&)> handler) override {
    if (!handler) {
      node_event_handlers[node].erase(std::string(name));
      return;
    }
    node_event_handlers[node][std::string(name)] = std::move(handler);
  }

  std::function<void(const SyntheticEvent&)> event_handler(DomNode node,
                                                           std::string_view name) override {
    const auto node_entry = node_event_handlers.find(node);
    if (node_entry == node_event_handlers.end()) return {};
    const auto handler_entry = node_entry->second.find(std::string(name));
    return handler_entry != node_entry->second.end()
               ? handler_entry->second
               : std::function<void(const SyntheticEvent&)>{};
  }

  EventListenerToken add_event_listener(DomNode node, std::string_view type,
                                        cppreact::EventListener listener, bool capture) override {
    std::vector<Rml::String> rml_types = rml_event_aliases(type);
    if (rml_types.empty()) return null_event_listener;
    std::string event_type(type);
    EventListenerToken token = next_listener++;
    auto proxy = std::make_unique<ProxyListener>(this, node, event_type, capture);
    proxy->listener = std::move(listener);
    if (Rml::Element* element = get_element(node)) {
      for (const Rml::String& rml_type : rml_types) {
        element->AddEventListener(rml_type, proxy.get(), capture);
      }
    }
    listeners[node][event_type].emplace(token, std::move(proxy));
    return token;
  }

  void remove_event_listener(DomNode node, std::string_view type,
                             EventListenerToken token) override {
    auto per_node = listeners.find(node);
    if (per_node == listeners.end()) return;
    std::string event_type(type);
    auto entry = per_node->second.find(event_type);
    if (entry == per_node->second.end()) return;
    auto found = entry->second.find(token);
    if (found == entry->second.end()) return;
    detach_listener(node, found->second.get());
    entry->second.erase(found);
  }

  void focus(DomNode node, bool select_all) override {
    Rml::Element* element = get_element(node);
    if (!element) return;
    element->Focus();
    if (select_all) {
      if (auto* control = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(element)) {
        control->Select();
      }
    }
  }

  void update_layout() override {
    Rml::Element* element = get_element(document_handle);
    if (!element) return;
    if (Rml::ElementDocument* owner = element->GetOwnerDocument()) owner->UpdateDocument();
  }

  void set_native_property(DomNode node, std::string_view name, const RawPayload& value) override {
    auto entry = natives.find(node);
    if (entry != natives.end()) entry->second->set_native_property(name, value);
  }

  RawPayload native_reference(DomNode node) override {
    auto entry = natives.find(node);
    return entry != natives.end() ? entry->second->native_reference() : RawPayload{};
  }

  void* native_element(DomNode node) override { return get_element(node); }

private:
  static Rml::String stringify(const Value& value) {
    if (const bool* flag = std::get_if<bool>(&value)) return *flag ? "true" : "false";
    if (const double* number = std::get_if<double>(&value)) {
      return Rml::String(detail::number_to_text(*number));
    }
    if (const std::string* text = std::get_if<std::string>(&value)) return Rml::String(*text);
    return Rml::String();
  }

  class ProxyListener : public Rml::EventListener {
  public:
    ProxyListener(RmlHost* owner, DomNode current, std::string event_type, bool captures)
        : type(std::move(event_type)), capture(captures), host(owner), node(current) {}

    void ProcessEvent(Rml::Event& native) override {
      cppreact::EventListener current = listener;
      if (!current) return;
      if (type == "change" && native.GetParameter<bool>("linebreak", false)) return;
      SyntheticEvent event;
      DomNode target = host->get_handle(native.GetTargetElement());
      event.target = target != null_dom_node ? target : node;
      event.current_target = node;
      event.event_phase = event.target == node    ? event_phase::at_target
                          : capture               ? event_phase::capturing
                                                  : event_phase::bubbling;
      event.type = type;
      event.client_x = native.GetParameter<float>("mouse_x", 0.0f);
      event.client_y = native.GetParameter<float>("mouse_y", 0.0f);
      event.button = native.GetParameter<int>("button", 0);
      event.shift_key = native.GetParameter<int>("shift_key", 0) != 0;
      event.ctrl_key = native.GetParameter<int>("ctrl_key", 0) != 0;
      event.alt_key = native.GetParameter<int>("alt_key", 0) != 0;
      event.meta_key = native.GetParameter<int>("meta_key", 0) != 0;
      event.key = rml_key_name(native.GetParameter<int>("key_identifier", 0));
      event.value = native.GetParameter<Rml::String>("value", Rml::String());
      event.delta_x = native.GetParameter<float>("wheel_delta_x", 0.0f);
      event.delta_y = native.GetParameter<float>("wheel_delta_y", 0.0f);
      event.native_stop_propagation = [&native] { native.StopPropagation(); };
      current(event);
    }

    cppreact::EventListener listener{};
    std::string type;
    bool capture = false;

  private:
    RmlHost* host;
    DomNode node;
  };

  void detach_listener(DomNode node, ProxyListener* listener) {
    if (Rml::Element* element = get_element(node)) {
      for (const Rml::String& rml_type : rml_event_aliases(listener->type)) {
        element->RemoveEventListener(rml_type, listener, listener->capture);
      }
    }
  }

  void unregister_element(Rml::Element* element) {
    if (!element) return;
    for (int index = element->GetNumChildren(true) - 1; index >= 0; --index) {
      unregister_element(element->GetChild(index));
    }
    auto entry = handles.find(element);
    if (entry == handles.end()) return;
    DomNode handle = entry->second;
    auto listener_entry = listeners.find(handle);
    if (listener_entry != listeners.end()) {
      for (auto& [event_type, event_listeners] : listener_entry->second) {
        for (auto& registered : event_listeners) {
          detach_listener(handle, registered.second.get());
        }
      }
    }
    handles.erase(entry);
    elements.erase(handle);
    owned.erase(handle);
    listeners.erase(handle);
    node_event_handlers.erase(handle);
    natives.erase(handle);
  }

  DomNode register_element(Rml::Element* element) {
    DomNode handle = next_handle++;
    elements[handle] = element;
    handles[element] = handle;
    return handle;
  }

  Rml::Element* get_element(DomNode node) const {
    auto entry = elements.find(node);
    return entry == elements.end() ? nullptr : entry->second;
  }

  DomNode get_handle(Rml::Element* element) const {
    auto entry = handles.find(element);
    return entry == handles.end() ? null_dom_node : entry->second;
  }

  std::unordered_map<DomNode, Rml::Element*> elements{};
  std::unordered_map<Rml::Element*, DomNode> handles{};
  std::unordered_map<DomNode, Rml::ElementPtr> owned{};
  std::unordered_map<DomNode, NativeElement*> natives{};
  std::unordered_map<DomNode, std::map<std::string, std::function<void(const SyntheticEvent&)>>>
      node_event_handlers{};
  std::unordered_map<
      DomNode,
      std::unordered_map<std::string, std::map<EventListenerToken, std::unique_ptr<ProxyListener>>>>
      listeners{};
  DomNode next_handle = 1;
  EventListenerToken next_listener = 1;
  DomNode document_handle = null_dom_node;
};

}
