#pragma once

#include <string_view>

#include "../value/raw_payload.hpp"

namespace cppreact {

struct NativeElement {
  virtual ~NativeElement() = default;

  virtual void set_native_property(std::string_view name, const RawPayload& value) {
    (void)name;
    (void)value;
  }

  virtual RawPayload native_reference() { return {}; }
};

}
