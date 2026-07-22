#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/value/payload.hpp"
#include "cppreact/value/style.hpp"

using namespace cppreact;

namespace {

struct Force {
  double value = 0;

  bool operator==(const Force&) const = default;
};

struct Roll {
  double value = 0;

  bool operator==(const Roll&) const = default;
};

struct Opaque {
  std::shared_ptr<int> handle{};
};

}

TEST_CASE("payload") {
    SECTION("typed payloads read back only as their own type") {
        Payload payload = make_payload(Force{2.5});
        std::shared_ptr<const Force> force = payload_as<Force>(payload);
        REQUIRE(force);
        REQUIRE(force->value == 2.5);
        REQUIRE_FALSE(payload_as<Roll>(payload));
    }

    SECTION("comparable payloads compare by value") {
        REQUIRE(make_payload(Force{1.5}) == make_payload(Force{1.5}));
        REQUIRE_FALSE(make_payload(Force{1.5}) == make_payload(Force{2.5}));
    }

    SECTION("payloads of different types never compare equal") {
        REQUIRE_FALSE(make_payload(Force{1}) == make_payload(Roll{1}));
    }

    SECTION("non comparable payloads fall back to identity") {
        Payload first = make_payload(Opaque{});
        Payload copy = first;
        REQUIRE(first == copy);
        REQUIRE_FALSE(first == make_payload(Opaque{}));
    }

    SECTION("empty payloads are equal, empty never equals stored") {
        REQUIRE(Payload{} == Payload{});
        REQUIRE_FALSE(Payload{} == make_payload(Force{}));
    }

    SECTION("a shared payload keeps its stored value alive") {
        std::shared_ptr<const Force> survivor;
        {
            Payload payload = make_payload(Force{4.5});
            survivor = payload_as<Force>(payload);
        }
        REQUIRE(survivor->value == 4.5);
    }
}

TEST_CASE("style") {
    SECTION("style holds named string properties in order") {
        Style style{{"width", "3rem"}, {"opacity", "0.5"}};
        REQUIRE(style.get("width"));
        REQUIRE(*style.get("width") == "3rem");
        REQUIRE(style.get("missing") == nullptr);
    }

    SECTION("styles compare by content") {
        REQUIRE(Style{{"width", "3rem"}} == Style{{"width", "3rem"}});
        REQUIRE_FALSE(Style{{"width", "3rem"}} == Style{{"width", "4rem"}});
        REQUIRE(Style{} == Style{});
    }
}
