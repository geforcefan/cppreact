#pragma once

#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementText.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Types.h>

#include <set>

#include "cppreact/native.hpp"
#include "cppreact/render.hpp"
#include "cppreact/renderer.hpp"

namespace cppreact::renderers {

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
  case Rml::Input::KI_NUMPADENTER: return "Enter";
  case Rml::Input::KI_ESCAPE: return "Escape";
  case Rml::Input::KI_TAB: return "Tab";
  case Rml::Input::KI_SPACE: return " ";
  case Rml::Input::KI_DELETE: return "Delete";
  case Rml::Input::KI_BACK: return "Backspace";
  case Rml::Input::KI_LEFT: return "ArrowLeft";
  case Rml::Input::KI_RIGHT: return "ArrowRight";
  case Rml::Input::KI_UP: return "ArrowUp";
  case Rml::Input::KI_DOWN: return "ArrowDown";
  case Rml::Input::KI_LSHIFT:
  case Rml::Input::KI_RSHIFT: return "Shift";
  default: break;
  }
  if (identifier >= Rml::Input::KI_A && identifier <= Rml::Input::KI_Z)
    return std::string(1, static_cast<char>('a' + (identifier - Rml::Input::KI_A)));
  if (identifier >= Rml::Input::KI_0 && identifier <= Rml::Input::KI_9)
    return std::string(1, static_cast<char>('0' + (identifier - Rml::Input::KI_0)));
  return "";
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

class RmlRenderer : public Renderer {
public:
  NodeHandle document() const override { return document_handle; }

  Container create_container(Rml::Element* mount) {
    NodeHandle handle = register_element(mount);
    if (document_handle == kNullNode) {
      Rml::ElementDocument* owner = mount->GetOwnerDocument();
      document_handle = owner ? register_element(owner) : handle;
    }
    return Container{this, handle, {}};
  }

  NodeHandle create_element(std::string_view tag, const Props&) override {
    Rml::String name(tag);
    Rml::ElementPtr instance =
        Rml::Factory::InstanceElement(nullptr, name, name, Rml::XMLAttributes());
    Rml::Element* raw = instance.get();
    NodeHandle handle = register_element(raw);
    owned[handle] = std::move(instance);
    if (native_tags().count(std::string(tag))) natives[handle] = static_cast<NativeRmlElement*>(raw);
    return handle;
  }

  NodeHandle create_text(std::string_view text) override {
    Rml::ElementPtr instance =
        Rml::Factory::InstanceElement(nullptr, "#text", "#text", Rml::XMLAttributes());
    if (Rml::ElementText* element = rmlui_dynamic_cast<Rml::ElementText*>(instance.get())) {
      element->SetText(Rml::String(text));
    }
    Rml::Element* raw = instance.get();
    NodeHandle handle = register_element(raw);
    owned[handle] = std::move(instance);
    return handle;
  }

  void set_text(NodeHandle node, std::string_view text) override {
    if (Rml::ElementText* element = rmlui_dynamic_cast<Rml::ElementText*>(get_element(node))) {
      element->SetText(Rml::String(text));
    }
  }

  void insert_before(NodeHandle parent, NodeHandle node, NodeHandle anchor) override {
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

    Rml::Element* anchor_element = anchor != kNullNode ? get_element(anchor) : nullptr;
    if (anchor_element) {
      parent_element->InsertBefore(std::move(child), anchor_element);
    } else {
      parent_element->AppendChild(std::move(child));
    }
  }

  void remove_child(NodeHandle parent, NodeHandle node) override {
    Rml::Element* parent_element = get_element(parent);
    Rml::Element* node_element = get_element(node);
    if (parent_element && node_element) {
      unregister_element(node_element);
      parent_element->RemoveChild(node_element);
    } else {
      elements.erase(node);
      owned.erase(node);
      listeners.erase(node);
      natives.erase(node);
    }
  }

  NodeHandle next_sibling(NodeHandle node) override {
    Rml::Element* element = get_element(node);
    if (!element) return kNullNode;
    Rml::Element* sibling = element->GetNextSibling();
    return sibling ? get_handle(sibling) : kNullNode;
  }

  NodeHandle parent_node(NodeHandle node) override {
    Rml::Element* element = get_element(node);
    if (!element) return kNullNode;
    Rml::Element* parent = element->GetParentNode();
    return parent ? get_handle(parent) : kNullNode;
  }

  void set_property(NodeHandle node, std::string_view name, const Value& value,
                    const Value& old_value) override {
    Rml::Element* element = get_element(node);
    if (!element) return;
    const Style* previous_style = std::get_if<Style>(&old_value);
    const Style* next_style = std::get_if<Style>(&value);
    if (previous_style || next_style) {
      if (previous_style)
        for (const auto& [key, declaration] : *previous_style) {
          bool retained = false;
          if (next_style)
            for (const auto& [next_key, next_declaration] : *next_style)
              if (next_key == key && !next_declaration.empty()) retained = true;
          if (!retained) element->RemoveProperty(Rml::String(key));
        }
      if (next_style)
        for (const auto& [key, declaration] : *next_style)
          if (!declaration.empty()) element->SetProperty(Rml::String(key), Rml::String(declaration));
      return;
    }
    Rml::String attribute(name);
    if (std::holds_alternative<std::monostate>(value)) {
      element->RemoveAttribute(attribute);
    } else {
      element->SetAttribute(attribute, stringify(value));
    }
  }

  ListenerToken add_event_listener(NodeHandle node, std::string_view type, EventHandler handler,
                                   bool capture) override {
    std::vector<Rml::String> rml_types = rml_event_aliases(type);
    if (rml_types.empty()) return kNullListener;
    std::string event_type(type);
    ListenerToken token = next_listener++;
    auto listener = std::make_unique<HandlerListener>(this, node, event_type, capture);
    listener->handler = std::move(handler);
    if (Rml::Element* element = get_element(node)) {
      for (const Rml::String& rml_type : rml_types)
        element->AddEventListener(rml_type, listener.get(), capture);
    }
    listeners[node][event_type].emplace(token, std::move(listener));
    return token;
  }

  void set_event_handler(NodeHandle node, std::string_view type, EventHandler handler,
                         bool capture) override {
    std::vector<Rml::String> rml_types = rml_event_aliases(type);
    if (rml_types.empty()) return;
    std::string event_type(type);
    ListenerToken prop_token = capture ? kCaptureListener : kNullListener;
    auto& slot = listeners[node][event_type];
    auto existing = slot.find(prop_token);
    if (!handler) {
      if (existing != slot.end()) {
        detach_listener(node, existing->second.get());
        slot.erase(existing);
      }
      return;
    }
    if (existing != slot.end()) {
      existing->second->handler = std::move(handler);
      return;
    }
    auto listener = std::make_unique<HandlerListener>(this, node, event_type, capture);
    listener->handler = std::move(handler);
    if (Rml::Element* element = get_element(node)) {
      for (const Rml::String& rml_type : rml_types)
        element->AddEventListener(rml_type, listener.get(), capture);
    }
    slot.emplace(prop_token, std::move(listener));
  }

  void remove_event_listener(NodeHandle node, std::string_view type, ListenerToken token) override {
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

  void focus(NodeHandle node, bool select_all) override {
    Rml::Element* element = get_element(node);
    if (!element) return;
    element->Focus();
    if (select_all)
      if (auto* control = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(element)) control->Select();
  }

  Box measure(NodeHandle node) override {
    Rml::Element* element = get_element(node);
    if (!element) return {};
    const Rml::Vector2f offset = element->GetAbsoluteOffset(Rml::BoxArea::Border);
    const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
    return {offset.x, offset.y, size.x, size.y};
  }

  void set_native_prop(NodeHandle node, std::string_view name, const Payload& value) override {
    auto entry = natives.find(node);
    if (entry != natives.end()) entry->second->set_native_prop(name, value);
  }

  Payload native_reference(NodeHandle node) override {
    auto entry = natives.find(node);
    return entry != natives.end() ? entry->second->native_reference() : Payload{};
  }

  void* native_element(NodeHandle node) override { return get_element(node); }

private:
  class HandlerListener : public Rml::EventListener {
  public:
    HandlerListener(RmlRenderer* owner, NodeHandle current, std::string event_type, bool captures)
        : type(std::move(event_type)), capture(captures), renderer(owner), node(current) {}

    void ProcessEvent(Rml::Event& native) override {
      EventHandler current = handler;
      if (!current) return;
      if (type == "change" && native.GetParameter<bool>("linebreak", false)) return;
      Event event;
      NodeHandle target = renderer->get_handle(native.GetTargetElement());
      event.target = target != kNullNode ? target : node;
      event.current_target = node;
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
      event.stop_propagation = [&native] { native.StopPropagation(); };
      event.prevent_default = [] {};
      current(event);
    }

    EventHandler handler{};
    std::string type;
    bool capture = false;

  private:
    RmlRenderer* renderer;
    NodeHandle node;
  };

  void detach_listener(NodeHandle node, HandlerListener* listener) {
    if (Rml::Element* element = get_element(node))
      for (const Rml::String& rml_type : rml_event_aliases(listener->type))
        element->RemoveEventListener(rml_type, listener, listener->capture);
  }

  void unregister_element(Rml::Element* element) {
    if (!element) return;
    for (int index = element->GetNumChildren(true) - 1; index >= 0; --index)
      unregister_element(element->GetChild(index));
    auto entry = handles.find(element);
    if (entry == handles.end()) return;
    NodeHandle handle = entry->second;
    auto listener_entry = listeners.find(handle);
    if (listener_entry != listeners.end())
      for (auto& [event_type, slot] : listener_entry->second)
        for (auto& registered : slot)
          detach_listener(handle, registered.second.get());
    handles.erase(entry);
    elements.erase(handle);
    owned.erase(handle);
    listeners.erase(handle);
    natives.erase(handle);
  }

  NodeHandle register_element(Rml::Element* element) {
    NodeHandle handle = next_handle++;
    elements[handle] = element;
    handles[element] = handle;
    return handle;
  }

  Rml::Element* get_element(NodeHandle node) const {
    auto entry = elements.find(node);
    return entry == elements.end() ? nullptr : entry->second;
  }

  NodeHandle get_handle(Rml::Element* element) const {
    auto entry = handles.find(element);
    return entry == handles.end() ? kNullNode : entry->second;
  }

  static Rml::String stringify(const Value& value) {
    if (const bool* flag = std::get_if<bool>(&value)) return *flag ? "true" : "false";
    if (const double* number = std::get_if<double>(&value)) {
      char buffer[32];
      std::snprintf(buffer, sizeof(buffer), "%g", *number);
      return Rml::String(buffer);
    }
    if (const std::string* text = std::get_if<std::string>(&value)) return Rml::String(*text);
    return Rml::String();
  }

  std::unordered_map<NodeHandle, Rml::Element*> elements{};
  std::unordered_map<Rml::Element*, NodeHandle> handles{};
  std::unordered_map<NodeHandle, Rml::ElementPtr> owned{};
  std::unordered_map<NodeHandle, NativeElement*> natives{};
  std::unordered_map<NodeHandle,
                     std::unordered_map<std::string,
                                        std::map<ListenerToken, std::unique_ptr<HandlerListener>>>>
      listeners{};
  NodeHandle next_handle = 1;
  ListenerToken next_listener = 2;
  NodeHandle document_handle = kNullNode;
};

}
