#pragma once

#include <string_view>

#include "value.hpp"

namespace cppreact {

struct NativeElement {
  virtual ~NativeElement() = default;

  virtual void set_native_prop(std::string_view name, const Payload& value) {
    (void)name;
    (void)value;
  }

  virtual Payload native_reference() { return {}; }
};

}
