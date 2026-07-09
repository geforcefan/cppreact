#pragma once

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cppreact {

struct Event;

using EventHandler = std::function<void(const Event&)>;

struct Payload {
  std::shared_ptr<void> data{};

  bool operator==(const Payload& other) const;
};

using Style = std::vector<std::pair<std::string, std::string>>;

using Value =
    std::variant<std::monostate, bool, double, std::string, EventHandler, Payload, Style>;

bool is_event_handler(const Value& value);

bool values_equal(const Value& left, const Value& right);
bool value_lists_equal(const std::vector<Value>& left, const std::vector<Value>& right);

class Props {
public:
  using Entry = std::pair<std::string, Value>;

  Props() = default;
  Props(std::initializer_list<Entry> entries);

  const Value* find(std::string_view name) const;
  Value& set(std::string name, Value value);
  Value take(std::string_view name);
  bool has(std::string_view name) const;
  std::size_t size() const;

  template <class T>
  std::optional<T> get(std::string_view name) const {
    const Value* found = find(name);
    if (found) {
      if (const T* typed = std::get_if<T>(found)) return *typed;
    }
    return std::nullopt;
  }

  std::vector<Entry>::const_iterator begin() const;
  std::vector<Entry>::const_iterator end() const;

private:
  std::vector<Entry> entries{};
};

inline bool Payload::operator==(const Payload& other) const { return data == other.data; }

inline bool is_event_handler(const Value& value) {
  return std::holds_alternative<EventHandler>(value);
}

inline bool values_equal(const Value& left, const Value& right) {
  if (left.index() != right.index()) return false;
  if (is_event_handler(left)) return false;
  return std::visit(
      [&right](const auto& stored) -> bool {
        using Stored = std::decay_t<decltype(stored)>;
        if constexpr (std::is_same_v<Stored, EventHandler>) {
          return false;
        } else {
          return stored == std::get<Stored>(right);
        }
      },
      left);
}

inline bool value_lists_equal(const std::vector<Value>& left, const std::vector<Value>& right) {
  if (left.size() != right.size()) return false;
  for (std::size_t position = 0; position < left.size(); ++position) {
    if (!values_equal(left[position], right[position])) return false;
  }
  return true;
}

inline Props::Props(std::initializer_list<Entry> initial) : entries(initial) {}

inline const Value* Props::find(std::string_view name) const {
  for (const Entry& entry : entries) {
    if (entry.first == name) return &entry.second;
  }
  return nullptr;
}

inline Value& Props::set(std::string name, Value value) {
  for (Entry& entry : entries) {
    if (entry.first == name) {
      entry.second = std::move(value);
      return entry.second;
    }
  }
  entries.emplace_back(std::move(name), std::move(value));
  return entries.back().second;
}

inline Value Props::take(std::string_view name) {
  for (std::size_t position = 0; position < entries.size(); ++position) {
    if (entries[position].first == name) {
      Value value = std::move(entries[position].second);
      entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(position));
      return value;
    }
  }
  return std::monostate{};
}

inline bool Props::has(std::string_view name) const { return find(name) != nullptr; }

inline std::size_t Props::size() const { return entries.size(); }

inline std::vector<Props::Entry>::const_iterator Props::begin() const { return entries.begin(); }

inline std::vector<Props::Entry>::const_iterator Props::end() const { return entries.end(); }

}
