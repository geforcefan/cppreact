#pragma once

#include <memory>

namespace cppreact {

struct RawPayload {
  std::shared_ptr<void> data{};

  bool operator==(const RawPayload& other) const { return data == other.data; }
};

}
