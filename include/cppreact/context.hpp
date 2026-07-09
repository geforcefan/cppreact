#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "value.hpp"

namespace cppreact {

struct ContextCell {
  Payload value{};
  std::vector<std::pair<std::uint32_t, std::function<void()>>> subscribers{};
  std::uint32_t next_subscription = 1;
};

inline std::uint32_t subscribe_context(ContextCell& cell, std::function<void()> notify) {
  std::uint32_t token = cell.next_subscription++;
  cell.subscribers.push_back({token, std::move(notify)});
  return token;
}

inline void unsubscribe_context(ContextCell& cell, std::uint32_t token) {
  cell.subscribers.erase(
      std::remove_if(cell.subscribers.begin(), cell.subscribers.end(),
                     [token](const auto& entry) { return entry.first == token; }),
      cell.subscribers.end());
}

inline void notify_context(ContextCell& cell) {
  auto subscribers = cell.subscribers;
  for (auto& [token, notify] : subscribers) {
    if (notify) notify();
  }
}

struct ContextNode {
  std::uint32_t id = 0;
  std::shared_ptr<ContextCell> cell{};
  std::shared_ptr<const ContextNode> parent{};
};

using ContextChain = std::shared_ptr<const ContextNode>;

template <class T>
bool payload_values_equal(const Payload& left, const Payload& right) {
  if (!left.data || !right.data) return left.data == right.data;
  if constexpr (requires(const T& a, const T& b) { a == b; }) {
    return *static_cast<const T*>(left.data.get()) == *static_cast<const T*>(right.data.get());
  } else {
    return left.data == right.data;
  }
}

struct ContextBinding {
  std::uint32_t id = 0;
  Payload value{};
  bool (*values_match)(const Payload&, const Payload&) = nullptr;
};

inline std::shared_ptr<ContextCell> lookup_context_cell(const ContextChain& chain,
                                                        std::uint32_t id) {
  for (const ContextNode* node = chain.get(); node != nullptr; node = node->parent.get()) {
    if (node->id == id) return node->cell;
  }
  return nullptr;
}

inline ContextChain extend_context(const ContextChain& parent, std::uint32_t id,
                                   std::shared_ptr<ContextCell> cell) {
  return std::make_shared<const ContextNode>(ContextNode{id, std::move(cell), parent});
}

inline std::uint32_t next_context_id() {
  static std::uint32_t counter = 0;
  return ++counter;
}

template <class T>
struct Context {
  std::uint32_t id = 0;
  std::shared_ptr<T> default_value{};
};

template <class T>
Context<T> create_context(T value) {
  return Context<T>{next_context_id(), std::make_shared<T>(std::move(value))};
}

}
