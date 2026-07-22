#pragma once

#include <concepts>
#include <memory>
#include <utility>
#include <vector>

namespace cppreact {

namespace payload_detail {

template <class Stored>
inline constexpr char type_tag = 0;

template <class Stored>
inline constexpr bool comparable = std::equality_comparable<Stored>;

template <class Element, class Allocator>
inline constexpr bool comparable<std::vector<Element, Allocator>> = comparable<Element>;

template <class Stored>
bool stored_equal(const void* left, const void* right) {
  return *static_cast<const Stored*>(left) == *static_cast<const Stored*>(right);
}

}

struct Payload {
  std::shared_ptr<void> data{};
  const void* type = nullptr;
  bool (*equals)(const void*, const void*) = nullptr;

  bool operator==(const Payload& other) const {
    if (data == other.data) return true;
    if (!data || !other.data || type != other.type) return false;
    return equals && equals(data.get(), other.data.get());
  }
};

template <class Stored>
Payload make_payload(std::shared_ptr<Stored> payload) {
  Payload result{std::move(payload), &payload_detail::type_tag<Stored>, nullptr};
  if constexpr (payload_detail::comparable<Stored>) {
    result.equals = &payload_detail::stored_equal<Stored>;
  }
  return result;
}

template <class Stored>
Payload make_payload(Stored payload) {
  return make_payload(std::make_shared<Stored>(std::move(payload)));
}

template <class Stored>
std::shared_ptr<const Stored> payload_as(const Payload& payload) {
  if (!payload.data || payload.type != &payload_detail::type_tag<Stored>) return {};
  return std::static_pointer_cast<const Stored>(payload.data);
}

}
