#pragma once

#include <initializer_list>
#include <string>
#include <vector>

namespace cppreact {

struct StyleProperty {
  std::string name;
  std::string value;

  bool operator==(const StyleProperty&) const = default;
};

struct Style {
  std::vector<StyleProperty> properties{};

  Style() = default;
  Style(std::initializer_list<StyleProperty> initial) : properties(initial) {}

  auto begin() const { return properties.begin(); }
  auto end() const { return properties.end(); }

  const std::string* get(const std::string& name) const {
    for (const StyleProperty& property : properties) {
      if (property.name == name) return &property.value;
    }
    return nullptr;
  }

  bool operator==(const Style&) const = default;
};

}
