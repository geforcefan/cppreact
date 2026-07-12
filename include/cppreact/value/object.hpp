#pragma once

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "value.hpp"

namespace cppreact {

class Object {
public:
  struct Entry {
    std::string name;
    Value value;

    Entry(std::string name, Value value);
    Entry(std::string name, std::initializer_list<Entry> entries);
  };

  Object() = default;
  Object(std::initializer_list<Entry> initial);

  const Value* get(std::string_view name) const;
  Value& set(std::string name, Value value);
  Value take(std::string_view name);
  std::size_t size() const;

  template <class T>
  std::optional<detail::typed_value<T>> get(std::string_view name) const {
    const Value* found = get(name);
    if (!found) return std::nullopt;
    if (const auto* typed = value_as<T>(*found)) return *typed;
    return std::nullopt;
  }

  template <class T>
  detail::typed_value<T> get(std::string_view name, detail::typed_value<T> fallback) const {
    const Value* found = get(name);
    if (!found) return fallback;
    if (const auto* typed = value_as<T>(*found)) return *typed;
    return fallback;
  }

  std::vector<Entry>::const_iterator begin() const;
  std::vector<Entry>::const_iterator end() const;

private:
  std::vector<Entry> entries{};
};

inline Object::Object(std::initializer_list<Entry> initial) : entries(initial) {}

inline Object::Entry::Entry(std::string name, Value value)
    : name(std::move(name)), value(std::move(value)) {}

inline Object::Entry::Entry(std::string name, std::initializer_list<Entry> entries)
    : name(std::move(name)), value(std::make_shared<Object>(entries)) {}

inline const Value* Object::get(std::string_view name) const {
  for (const Entry& entry : entries) {
    if (entry.name == name) return &entry.value;
  }
  return nullptr;
}

inline Value& Object::set(std::string name, Value value) {
  for (Entry& entry : entries) {
    if (entry.name == name) {
      entry.value = std::move(value);
      return entry.value;
    }
  }
  entries.emplace_back(std::move(name), std::move(value));
  return entries.back().value;
}

inline Value Object::take(std::string_view name) {
  for (std::size_t position = 0; position < entries.size(); ++position) {
    if (entries[position].name == name) {
      Value value = std::move(entries[position].value);
      entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(position));
      return value;
    }
  }
  return std::monostate{};
}

inline std::size_t Object::size() const { return entries.size(); }

inline std::vector<Object::Entry>::const_iterator Object::begin() const { return entries.begin(); }

inline std::vector<Object::Entry>::const_iterator Object::end() const { return entries.end(); }

inline std::shared_ptr<Object> object(Object entries) {
  return std::make_shared<Object>(std::move(entries));
}

}
