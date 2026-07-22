#pragma once

#include <string_view>

#include "../value/payload.hpp"

namespace cppreact {

struct NativeElement {
  virtual ~NativeElement() = default;

  virtual void set_native_property(std::string_view name, const Payload& value) {
    (void)name;
    (void)value;
  }

  virtual Payload native_reference() { return {}; }
};

}
