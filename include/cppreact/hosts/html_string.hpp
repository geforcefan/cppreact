#pragma once

#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cppreact/host/native.hpp"
#include "cppreact/render.hpp"
#include "cppreact/host/host.hpp"
#include "cppreact/event/synthetic_event.hpp"

namespace cppreact::hosts {

class HtmlStringHost : public Host {
public:
  struct Listener {
    EventListener listener{};
    bool capture = false;
  };

  struct StoredNode {
    DomNode id = null_dom_node;
    bool is_text = false;
    std::string tag{};
    std::string text{};
    DomNode parent = null_dom_node;
    std::vector<DomNode> children{};
    std::vector<std::pair<std::string, std::string>> attributes{};
    std::map<std::string, Value> properties{};
    std::vector<std::pair<std::string, std::string>> style{};
    std::unordered_map<std::string, std::map<EventListenerToken, Listener>> listeners{};
  };

  HtmlStringHost() {
    root_container = next_id++;
    nodes[root_container] = StoredNode{root_container, false, "#container", "", null_dom_node,
                                 {},             {},    {},          {},        {}};
  }

  DomNode document() override { return root_container; }

  Container create_container() { return Container{this, root_container, {}}; }

  DomNode create_element(std::string_view tag) override {
    DomNode id = next_id++;
    nodes[id] = StoredNode{id, false, std::string(tag), "", null_dom_node, {}, {}, {}, {}, {}};
    auto native = native_by_tag.find(std::string(tag));
    if (native != native_by_tag.end()) natives[id] = native->second;
    log.push_back("create element " + std::string(tag));
    return id;
  }

  DomNode create_text(std::string_view value) override {
    DomNode id = next_id++;
    nodes[id] = StoredNode{id, true, "", std::string(value), null_dom_node, {}, {}, {}, {}, {}};
    log.push_back("create text " + std::string(value));
    return id;
  }

  void set_text(DomNode node, std::string_view value) override {
    nodes[node].text = std::string(value);
    log.push_back("set text " + std::string(value));
  }

  void insert_before(DomNode parent, DomNode node, DomNode anchor) override {
    detach(node);
    nodes[node].parent = parent;
    std::vector<DomNode>& siblings = nodes[parent].children;
    if (anchor == null_dom_node) {
      siblings.push_back(node);
    } else {
      auto position = std::find(siblings.begin(), siblings.end(), anchor);
      siblings.insert(position, node);
    }
    log.push_back("insert " + std::to_string(node));
  }

  void remove(DomNode node) override {
    if (nodes[node].parent == null_dom_node) return;
    detach(node);
    natives.erase(node);
    log.push_back("remove " + std::to_string(node));
  }

  DomNode next_sibling(DomNode node) override {
    DomNode parent = nodes[node].parent;
    if (parent == null_dom_node) return null_dom_node;
    std::vector<DomNode>& siblings = nodes[parent].children;
    auto position = std::find(siblings.begin(), siblings.end(), node);
    if (position == siblings.end() || position + 1 == siblings.end()) return null_dom_node;
    return *(position + 1);
  }

  DomNode parent_node(DomNode node) override { return nodes[node].parent; }

  void set_property(DomNode node, std::string_view name, const Value& value) override {
    StoredNode& target = nodes[node];
    std::string key(name);
    auto position = std::find_if(target.attributes.begin(), target.attributes.end(),
                                 [&key](const auto& entry) { return entry.first == key; });
    if (std::holds_alternative<std::monostate>(value)) {
      if (position != target.attributes.end()) target.attributes.erase(position);
      target.properties.erase(key);
      return;
    }
    target.properties[key] = value;
    std::string text = stringify(value);
    if (position != target.attributes.end()) {
      position->second = text;
    } else {
      target.attributes.emplace_back(key, text);
    }
  }

  Value property_value(DomNode node, std::string_view name) override {
    StoredNode& target = nodes[node];
    const auto entry = target.properties.find(std::string(name));
    return entry != target.properties.end() ? entry->second : Value{};
  }

  void set_style_property(DomNode node, std::string_view name,
                          std::string_view value) override {
    std::vector<std::pair<std::string, std::string>>& style = nodes[node].style;
    std::string key(name);
    auto position = std::find_if(style.begin(), style.end(),
                                 [&key](const auto& entry) { return entry.first == key; });
    if (value.empty()) {
      if (position != style.end()) style.erase(position);
      return;
    }
    if (position != style.end()) {
      position->second = std::string(value);
    } else {
      style.emplace_back(key, std::string(value));
    }
  }

  void set_event_handler(DomNode node, std::string_view name,
                         std::function<void(const SyntheticEvent&)> handler) override {
    log.push_back("set handler " + std::string(name));
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
                                        EventListener listener, bool capture) override {
    EventListenerToken token = next_listener++;
    nodes[node].listeners[std::string(type)].emplace(token,
                                                     Listener{std::move(listener), capture});
    return token;
  }

  void remove_event_listener(DomNode node, std::string_view type,
                             EventListenerToken token) override {
    auto entry = nodes[node].listeners.find(std::string(type));
    if (entry != nodes[node].listeners.end()) entry->second.erase(token);
  }

  void register_native(std::string tag, NativeElement* element) {
    native_by_tag[std::move(tag)] = element;
  }

  void set_native_property(DomNode node, std::string_view name, const Payload& value) override {
    auto entry = natives.find(node);
    if (entry != natives.end()) entry->second->set_native_property(name, value);
  }

  Payload native_reference(DomNode node) override {
    auto entry = natives.find(node);
    return entry != natives.end() ? entry->second->native_reference() : Payload{};
  }

  std::string inner_html() { return inner_html(root_container); }

  std::string inner_html(DomNode node) {
    std::string out;
    for (DomNode child : nodes[node].children) serialize_node(child, out);
    return out;
  }

  void dispatch_event(DomNode node, const std::string& type, SyntheticEvent event = {}) {
    bool stopped = false;
    event.target = node;
    event.type = type;
    event.native_stop_propagation = [this, &stopped] {
      propagation_stopped = true;
      stopped = true;
    };
    event.native_prevent_default = [this] { default_prevented = true; };

    std::vector<DomNode> ancestors;
    for (DomNode current = nodes[node].parent; current != null_dom_node;
         current = nodes[current].parent) {
      ancestors.push_back(current);
    }

    for (auto ancestor = ancestors.rbegin(); ancestor != ancestors.rend(); ++ancestor) {
      dispatch(*ancestor, type, event, DispatchPhase::capture);
      if (stopped) return;
    }

    dispatch(node, type, event, DispatchPhase::target);
    if (stopped) return;

    for (DomNode ancestor : ancestors) {
      dispatch(ancestor, type, event, DispatchPhase::bubble);
      if (stopped) return;
    }
  }

  bool propagation_stopped = false;
  bool default_prevented = false;

  std::string tag_name(DomNode node) { return nodes[node].tag; }

  DomNode find_by_text(const std::string& text) {
    for (const auto& [id, node] : nodes) {
      if (node.is_text && node.text == text) return id;
    }
    return null_dom_node;
  }

  DomNode find_first(const std::string& tag) {
    DomNode best = null_dom_node;
    for (const auto& [id, node] : nodes) {
      if (!node.is_text && node.tag == tag && (best == null_dom_node || id < best)) best = id;
    }
    return best;
  }

  std::vector<std::string> log{};

private:
  enum class DispatchPhase { capture, target, bubble };

  static std::string stringify(const Value& value) {
    if (const bool* flag = std::get_if<bool>(&value)) return *flag ? "true" : "false";
    if (const double* number = std::get_if<double>(&value)) return detail::number_to_text(*number);
    if (const std::string* text = std::get_if<std::string>(&value)) return *text;
    return "";
  }

  void dispatch(DomNode node, const std::string& type, SyntheticEvent& event,
                DispatchPhase phase) {
    auto entry = nodes[node].listeners.find(type);
    if (entry == nodes[node].listeners.end()) return;
    event.current_target = node;
    event.event_phase = phase == DispatchPhase::capture  ? event_phase::capturing
                        : phase == DispatchPhase::target ? event_phase::at_target
                                                         : event_phase::bubbling;
    std::map<EventListenerToken, Listener> current = entry->second;
    for (const auto& [token, listener] : current) {
      if (!listener.listener) continue;
      if (phase == DispatchPhase::capture && !listener.capture) continue;
      if (phase == DispatchPhase::bubble && listener.capture) continue;
      listener.listener(event);
    }
  }

  void detach(DomNode node) {
    DomNode parent = nodes[node].parent;
    if (parent == null_dom_node) return;
    std::vector<DomNode>& siblings = nodes[parent].children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), node), siblings.end());
    nodes[node].parent = null_dom_node;
  }

  void serialize_node(DomNode node, std::string& out) {
    StoredNode& current = nodes[node];
    if (current.is_text) {
      out += current.text;
      return;
    }
    out += "<" + current.tag;
    std::vector<std::pair<std::string, std::string>> sorted = current.attributes;
    if (!current.style.empty()) {
      std::string style_text;
      for (const auto& [property, declaration] : current.style) {
        if (declaration.empty()) continue;
        if (!style_text.empty()) style_text += "; ";
        style_text += property + ": " + declaration;
      }
      sorted.emplace_back("style", style_text);
    }
    std::sort(sorted.begin(), sorted.end());
    for (const auto& [name, text] : sorted) out += " " + name + "=\"" + text + "\"";
    out += ">";
    for (DomNode child : current.children) serialize_node(child, out);
    out += "</" + current.tag + ">";
  }

  std::unordered_map<DomNode, StoredNode> nodes{};
  std::unordered_map<DomNode, std::map<std::string, std::function<void(const SyntheticEvent&)>>>
      node_event_handlers{};
  std::unordered_map<std::string, NativeElement*> native_by_tag{};
  std::unordered_map<DomNode, NativeElement*> natives{};
  DomNode next_id = 1;
  EventListenerToken next_listener = 1;
  DomNode root_container = null_dom_node;
};

}
