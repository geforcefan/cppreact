#pragma once

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cppreact/native.hpp"
#include "cppreact/render.hpp"
#include "cppreact/renderer.hpp"

namespace cppreact::renderers {

class TestRenderer : public Renderer {
public:
  struct Node {
    NodeHandle id = kNullNode;
    bool is_text = false;
    std::string tag{};
    std::string text{};
    NodeHandle parent = kNullNode;
    std::vector<NodeHandle> children{};
    std::vector<std::pair<std::string, std::string>> attributes{};
    std::unordered_map<std::string, std::map<ListenerToken, EventHandler>> listeners{};
  };

  TestRenderer() {
    root_container = next_id++;
    nodes[root_container] = Node{root_container, false, "#container", "", kNullNode, {}, {}, {}};
  }

  NodeHandle document() const override { return root_container; }

  Container create_container() { return Container{this, root_container, {}}; }

  NodeHandle create_element(std::string_view tag, const Props&) override {
    NodeHandle id = next_id++;
    nodes[id] = Node{id, false, std::string(tag), "", kNullNode, {}, {}, {}};
    auto native = native_by_tag.find(std::string(tag));
    if (native != native_by_tag.end()) natives[id] = native->second;
    log.push_back("create element " + std::string(tag));
    return id;
  }

  NodeHandle create_text(std::string_view value) override {
    NodeHandle id = next_id++;
    nodes[id] = Node{id, true, "", std::string(value), kNullNode, {}, {}, {}};
    log.push_back("create text " + std::string(value));
    return id;
  }

  void set_text(NodeHandle node, std::string_view value) override {
    nodes[node].text = std::string(value);
    log.push_back("set text " + std::string(value));
  }

  void insert_before(NodeHandle parent, NodeHandle node, NodeHandle anchor) override {
    detach(node);
    nodes[node].parent = parent;
    std::vector<NodeHandle>& siblings = nodes[parent].children;
    if (anchor == kNullNode) {
      siblings.push_back(node);
    } else {
      auto position = std::find(siblings.begin(), siblings.end(), anchor);
      siblings.insert(position, node);
    }
    log.push_back("insert " + std::to_string(node));
  }

  void remove_child(NodeHandle parent, NodeHandle node) override {
    (void)parent;
    detach(node);
    natives.erase(node);
    boxes.erase(node);
    log.push_back("remove " + std::to_string(node));
  }

  NodeHandle next_sibling(NodeHandle node) override {
    NodeHandle parent = nodes[node].parent;
    if (parent == kNullNode) return kNullNode;
    std::vector<NodeHandle>& siblings = nodes[parent].children;
    auto position = std::find(siblings.begin(), siblings.end(), node);
    if (position == siblings.end() || position + 1 == siblings.end()) return kNullNode;
    return *(position + 1);
  }

  NodeHandle parent_node(NodeHandle node) override {
    return nodes[node].parent;
  }

  void set_property(NodeHandle node, std::string_view name, const Value& value,
                    const Value&) override {
    Node& target = nodes[node];
    std::string key(name);
    auto position = std::find_if(target.attributes.begin(), target.attributes.end(),
                                 [&key](const auto& entry) { return entry.first == key; });
    if (const Style* declarations = std::get_if<Style>(&value)) {
      std::string text;
      for (const auto& [property, declaration] : *declarations) {
        if (declaration.empty()) continue;
        if (!text.empty()) text += "; ";
        text += property + ": " + declaration;
      }
      if (position != target.attributes.end()) position->second = text;
      else target.attributes.emplace_back(key, text);
      return;
    }
    if (std::holds_alternative<std::monostate>(value)) {
      if (position != target.attributes.end()) target.attributes.erase(position);
      return;
    }
    std::string text = stringify(value);
    if (position != target.attributes.end()) {
      position->second = text;
    } else {
      target.attributes.emplace_back(key, text);
    }
  }

  ListenerToken add_event_listener(NodeHandle node, std::string_view type, EventHandler handler,
                                   bool) override {
    ListenerToken token = next_listener++;
    nodes[node].listeners[std::string(type)].emplace(token, std::move(handler));
    return token;
  }

  void set_event_handler(NodeHandle node, std::string_view type, EventHandler handler,
                         bool capture) override {
    ListenerToken prop_token = capture ? kCaptureListener : kNullListener;
    auto& slot = nodes[node].listeners[std::string(type)];
    if (!handler) {
      slot.erase(prop_token);
      return;
    }
    slot[prop_token] = std::move(handler);
  }

  void remove_event_listener(NodeHandle node, std::string_view type, ListenerToken token) override {
    auto entry = nodes[node].listeners.find(std::string(type));
    if (entry != nodes[node].listeners.end()) entry->second.erase(token);
  }

  Box measure(NodeHandle node) override {
    auto entry = boxes.find(node);
    return entry == boxes.end() ? Box{} : entry->second;
  }

  void set_box(NodeHandle node, Box box) { boxes[node] = box; }

  void register_native(std::string tag, NativeElement* element) {
    native_by_tag[std::move(tag)] = element;
  }

  void set_native_prop(NodeHandle node, std::string_view name, const Payload& value) override {
    auto entry = natives.find(node);
    if (entry != natives.end()) entry->second->set_native_prop(name, value);
  }

  Payload native_reference(NodeHandle node) override {
    auto entry = natives.find(node);
    return entry != natives.end() ? entry->second->native_reference() : Payload{};
  }

  std::string serialize() {
    return serialize(root_container);
  }

  std::string serialize(NodeHandle node) {
    std::string out;
    for (NodeHandle child : nodes[node].children) serialize_node(child, out);
    return out;
  }

  void fire(NodeHandle node, const std::string& type, Event event = {}) {
    auto entry = nodes[node].listeners.find(type);
    if (entry == nodes[node].listeners.end()) return;
    event.target = node;
    event.current_target = node;
    event.type = type;
    event.stop_propagation = [this] { propagation_stopped = true; };
    event.prevent_default = [this] { default_prevented = true; };
    std::map<ListenerToken, EventHandler> current = entry->second;
    for (const auto& [token, handler] : current)
      if (handler) handler(event);
  }

  bool propagation_stopped = false;
  bool default_prevented = false;

  NodeHandle find_by_text(const std::string& text) {
    for (const auto& [id, node] : nodes) {
      if (node.is_text && node.text == text) return id;
    }
    return kNullNode;
  }

  NodeHandle find_first(const std::string& tag) {
    NodeHandle best = kNullNode;
    for (const auto& [id, node] : nodes) {
      if (!node.is_text && node.tag == tag && (best == kNullNode || id < best)) best = id;
    }
    return best;
  }

  std::vector<std::string> log{};

private:
  void detach(NodeHandle node) {
    NodeHandle parent = nodes[node].parent;
    if (parent == kNullNode) return;
    std::vector<NodeHandle>& siblings = nodes[parent].children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), node), siblings.end());
    nodes[node].parent = kNullNode;
  }

  static std::string stringify(const Value& value) {
    if (const bool* flag = std::get_if<bool>(&value)) return *flag ? "true" : "false";
    if (const double* number = std::get_if<double>(&value)) {
      char buffer[32];
      std::snprintf(buffer, sizeof(buffer), "%g", *number);
      return std::string(buffer);
    }
    if (const std::string* text = std::get_if<std::string>(&value)) return *text;
    return "";
  }

  void serialize_node(NodeHandle node, std::string& out) {
    Node& current = nodes[node];
    if (current.is_text) {
      out += current.text;
      return;
    }
    out += "<" + current.tag;
    std::vector<std::pair<std::string, std::string>> sorted = current.attributes;
    std::sort(sorted.begin(), sorted.end());
    for (const auto& [name, text] : sorted) out += " " + name + "=\"" + text + "\"";
    out += ">";
    for (NodeHandle child : current.children) serialize_node(child, out);
    out += "</" + current.tag + ">";
  }

  std::unordered_map<NodeHandle, Node> nodes{};
  std::unordered_map<NodeHandle, Box> boxes{};
  std::unordered_map<std::string, NativeElement*> native_by_tag{};
  std::unordered_map<NodeHandle, NativeElement*> natives{};
  NodeHandle next_id = 1;
  ListenerToken next_listener = 2;
  NodeHandle root_container = kNullNode;
};

}
