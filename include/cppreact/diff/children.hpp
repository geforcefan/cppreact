#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "../component/component.hpp"
#include "../vnode/flags.hpp"
#include "../host/host.hpp"

namespace cppreact {

using CommitQueue = std::vector<std::weak_ptr<ComponentInstance>>;
using ReferenceQueue = std::vector<std::pair<Reference, DomNode>>;

DomNode diff(Host& host, DomNode parent_dom, VNode& new_vnode, VNode& old_vnode,
                const GlobalContext& global_context, CommitQueue& commit_queue,
                DomNode old_dom, ReferenceQueue& reference_queue);

void unmount(Host& host, VNode& vnode, bool skip_remove = false);

void apply_reference(const Reference& reference, DomNode value);

inline DomNode first_dom(const VNode& node) {
  if (is_text(node) || is_element(node)) return node.dom;
  if (is_component(node)) {
    if (!node.component) return null_dom_node;
    for (const VNode& child : node.component->rendered) {
      DomNode found = first_dom(child);
      if (found != null_dom_node) return found;
    }
    return null_dom_node;
  }
  for (const VNode& child : node.children) {
    DomNode found = first_dom(child);
    if (found != null_dom_node) return found;
  }
  return null_dom_node;
}

inline DomNode first_dom(const std::vector<VNode>& nodes) {
  for (const VNode& node : nodes) {
    DomNode found = first_dom(node);
    if (found != null_dom_node) return found;
  }
  return null_dom_node;
}

inline int find_matching_index(const VNode& child_vnode, const std::vector<VNode>& old_children,
                               int skewed_index, int remaining_old_children) {
  const std::string& key = child_vnode.key;
  const VNodeType& type = child_vnode.type;
  const int old_children_length = static_cast<int>(old_children.size());
  const VNode* old_vnode = skewed_index >= 0 && skewed_index < old_children_length
                               ? &old_children[skewed_index]
                               : nullptr;
  const bool matched =
      old_vnode && !is_null(*old_vnode) && (old_vnode->flags & vnode_flag::matched) == 0;

  bool should_search = remaining_old_children > (matched ? 1 : 0);

  auto same_type = [](const VNodeType& left, const VNodeType& right) {
    if (left.index() != right.index()) return false;
    if (const ElementTag* element = std::get_if<ElementTag>(&left)) {
      return element->tag == std::get<ElementTag>(right).tag;
    }
    if (const FunctionComponent* component = std::get_if<FunctionComponent>(&left)) {
      return *component == std::get<FunctionComponent>(right);
    }
    return true;
  };

  if ((old_vnode && is_null(*old_vnode) && key.empty()) ||
      (matched && key == old_vnode->key && same_type(type, old_vnode->type))) {
    return skewed_index;
  } else if (should_search) {
    int backward_index = skewed_index - 1;
    int forward_index = skewed_index + 1;
    while (backward_index >= 0 || forward_index < old_children_length) {
      const int child_index = backward_index >= 0 ? backward_index-- : forward_index++;
      old_vnode = &old_children[child_index];
      if (!is_null(*old_vnode) && (old_vnode->flags & vnode_flag::matched) == 0 &&
          key == old_vnode->key && same_type(type, old_vnode->type)) {
        return child_index;
      }
    }
  }

  return -1;
}

inline DomNode last_dom(const VNode& node) {
  if (is_text(node) || is_element(node)) return node.dom;
  const std::vector<VNode>* children_list =
      is_component(node) ? (node.component ? &node.component->rendered : nullptr)
                         : &node.children;
  if (!children_list) return null_dom_node;
  for (auto child = children_list->rbegin(); child != children_list->rend(); ++child) {
    DomNode found = last_dom(*child);
    if (found != null_dom_node) return found;
  }
  return null_dom_node;
}

inline DomNode construct_new_children_array(Host& host, VNode& new_parent_vnode,
                                               std::vector<VNode>& old_children,
                                               DomNode old_dom) {
  std::vector<VNode>& new_children = new_parent_vnode.children;
  const int new_children_length = static_cast<int>(new_children.size());
  const int old_children_length = static_cast<int>(old_children.size());
  int remaining_old_children = old_children_length;

  int skew = 0;

  for (int i = 0; i < new_children_length; ++i) {
    VNode& child_vnode = new_children[i];

    if (is_null(child_vnode)) continue;

    if (is_fragment(child_vnode)) {
      child_vnode =
          create_vnode(Fragment, Object{}, std::string(), {}, std::move(child_vnode.children));
    }

    const int skewed_index = i + skew;
    child_vnode.depth = new_parent_vnode.depth + 1;

    const int matching_index = child_vnode.index =
        find_matching_index(child_vnode, old_children, skewed_index, remaining_old_children);

    VNode* old_vnode = nullptr;
    if (matching_index != -1) {
      old_vnode = &old_children[matching_index];
      remaining_old_children--;
      if (!is_null(*old_vnode)) {
        old_vnode->flags |= vnode_flag::matched;
      }
    }

    if (old_vnode == nullptr || is_null(*old_vnode)) {
      if (matching_index == -1) {
        if (new_children_length > old_children_length) {
          skew--;
        } else if (new_children_length < old_children_length) {
          skew++;
        }
      }

      if (!is_component(child_vnode)) {
        child_vnode.flags |= vnode_flag::insert_vnode;
      }
    } else if (matching_index != skewed_index) {
      if (matching_index == skewed_index - 1) {
        skew--;
      } else if (matching_index == skewed_index + 1) {
        skew++;
      } else {
        if (matching_index > skewed_index) {
          skew--;
        } else {
          skew++;
        }

        child_vnode.flags |= vnode_flag::insert_vnode;
      }
    }
  }

  if (remaining_old_children) {
    for (int i = 0; i < old_children_length; ++i) {
      VNode& old_vnode = old_children[i];
      if (!is_null(old_vnode) && (old_vnode.flags & vnode_flag::matched) == 0) {
        if (first_dom(old_vnode) == old_dom) {
          DomNode last = last_dom(old_vnode);
          old_dom = last != null_dom_node ? host.next_sibling(last) : null_dom_node;
        }

        unmount(host, old_vnode);
      }
    }
  }

  return old_dom;
}

inline DomNode insert(Host& host, VNode& parent_vnode, DomNode old_dom,
                         DomNode parent_dom, bool should_place) {
  if (is_component(parent_vnode)) {
    std::vector<VNode>* children_list =
        parent_vnode.component ? &parent_vnode.component->rendered : nullptr;
    if (children_list) {
      for (VNode& child : *children_list) {
        if (!is_null(child)) {
          old_dom = insert(host, child, old_dom, parent_dom, should_place);
        }
      }
    }

    return old_dom;
  } else if (parent_vnode.dom != old_dom) {
    if (should_place) {
      if (old_dom != null_dom_node && is_element(parent_vnode) &&
          host.parent_node(old_dom) == null_dom_node) {
        old_dom = null_dom_node;
      }
      host.insert_before(parent_dom, parent_vnode.dom, old_dom);
    }
    old_dom = parent_vnode.dom;
  }

  old_dom = old_dom != null_dom_node ? host.next_sibling(old_dom) : null_dom_node;

  return old_dom;
}

inline DomNode diff_children(Host& host, DomNode parent_dom,
                                VNode& new_parent_vnode, std::vector<VNode>& old_children,
                                const GlobalContext& global_context, CommitQueue& commit_queue,
                                DomNode old_dom, ReferenceQueue& reference_queue) {
  DomNode new_dom = null_dom_node;
  DomNode first_child_dom = null_dom_node;

  const int new_children_length = static_cast<int>(new_parent_vnode.children.size());

  old_dom = construct_new_children_array(host, new_parent_vnode, old_children, old_dom);

  for (int i = 0; i < new_children_length; ++i) {
    VNode& child_vnode = new_parent_vnode.children[i];
    if (is_null(child_vnode)) continue;

    VNode empty_old{};
    VNode& old_vnode = child_vnode.index != -1 && !is_null(old_children[child_vnode.index])
                           ? old_children[child_vnode.index]
                           : empty_old;

    child_vnode.index = i;

    DomNode result = diff(host, parent_dom, child_vnode, old_vnode, global_context,
                             commit_queue, old_dom, reference_queue);

    new_dom = child_vnode.dom;
    if (has_reference(child_vnode.ref) && !(old_vnode.ref == child_vnode.ref)) {
      if (has_reference(old_vnode.ref)) {
        apply_reference(old_vnode.ref, null_dom_node);
      }
      reference_queue.push_back({child_vnode.ref, new_dom});
    }

    if (first_child_dom == null_dom_node && new_dom != null_dom_node) {
      first_child_dom = new_dom;
    }

    const bool should_place = (child_vnode.flags & vnode_flag::insert_vnode) != 0;
    if (should_place) {
      old_dom = insert(host, child_vnode, old_dom, parent_dom, should_place);

      if (old_vnode.dom != null_dom_node) {
        old_vnode.dom = null_dom_node;
      }
    } else if (is_component(child_vnode)) {
      old_dom = result;
    } else if (new_dom != null_dom_node) {
      old_dom = host.next_sibling(new_dom);
    }

    child_vnode.flags &= static_cast<std::uint8_t>(~(vnode_flag::insert_vnode | vnode_flag::matched));
  }

  new_parent_vnode.dom = first_child_dom;

  return old_dom;
}

inline void to_child_array(std::vector<VNode>& children, std::vector<VNode>& out) {
  for (VNode& child : children) {
    if (is_null(child)) continue;
    if (is_fragment(child)) {
      to_child_array(child.children, out);
    } else {
      out.push_back(std::move(child));
    }
  }
}

inline std::vector<VNode> to_child_array(std::vector<VNode> children) {
  std::vector<VNode> out;
  to_child_array(children, out);
  return out;
}

}
