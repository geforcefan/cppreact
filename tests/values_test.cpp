#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/event/synthetic_event.hpp"
#include "cppreact/value/object.hpp"
#include "cppreact/value/value.hpp"

using namespace cppreact;

TEST_CASE("values") {
    SECTION("float and double spellings land in the one number alternative") {
        REQUIRE(values_equal(Value{5.0}, Value{5.0f}));
    }

    SECTION("number compare is exact, no epsilon") {
        REQUIRE(values_equal(Value{0.5}, Value{0.5}));
        REQUIRE_FALSE(values_equal(Value{0.1 + 0.2}, Value{0.3}));
    }

    SECTION("a float-sourced fraction is not the double-sourced fraction") {
        REQUIRE_FALSE(values_equal(Value{0.1f}, Value{0.1}));
    }

    SECTION("NaN equals NaN, unlike raw double compare") {
        const double quiet = std::numeric_limits<double>::quiet_NaN();
        REQUIRE(values_equal(Value{quiet}, Value{std::nan("")}));
        REQUIRE_FALSE(values_equal(Value{quiet}, Value{1.0}));
    }

    SECTION("infinities compare by value, zero signs are one zero") {
        const double infinite = std::numeric_limits<double>::infinity();
        REQUIRE(values_equal(Value{infinite}, Value{infinite}));
        REQUIRE_FALSE(values_equal(Value{infinite}, Value{-infinite}));
        REQUIRE(values_equal(Value{0.0}, Value{-0.0}));
    }

    SECTION("mismatched alternatives are never equal") {
        REQUIRE_FALSE(values_equal(Value{true}, Value{1.0}));
        REQUIRE_FALSE(values_equal(Value{std::string("1")}, Value{1.0}));
        REQUIRE_FALSE(values_equal(Value{}, Value{0.0}));
    }

    SECTION("absent, bool, and string compare by value") {
        REQUIRE(values_equal(Value{}, Value{}));
        REQUIRE(values_equal(Value{true}, Value{true}));
        REQUIRE_FALSE(values_equal(Value{true}, Value{false}));
        REQUIRE(values_equal(Value{std::string("steel")}, Value{"steel"}));
        REQUIRE_FALSE(values_equal(Value{std::string("steel")}, Value{"wood"}));
    }

    SECTION("objects compare by identity, not content") {
        std::shared_ptr<Object> style = object({{"left", "4px"}});
        REQUIRE(values_equal(Value{style}, Value{style}));
        REQUIRE_FALSE(values_equal(Value{style}, Value{object({{"left", "4px"}})}));
    }

    SECTION("raw payloads compare by identity, not content") {
        RawPayload first{std::make_shared<int>(7)};
        RawPayload second{std::make_shared<int>(7)};
        REQUIRE(values_equal(Value{first}, Value{first}));
        REQUIRE_FALSE(values_equal(Value{first}, Value{second}));
    }

    SECTION("callbacks compare by identity, any signature") {
        Callback handler{[](const Event&) {}};
        Callback copy = handler;
        REQUIRE(values_equal(Value{handler}, Value{copy}));
        REQUIRE_FALSE(values_equal(Value{handler}, Value{Callback{[](const Event&) {}}}));
        REQUIRE(values_equal(Value{Callback{}}, Value{Callback{}}));

        Callback node_callback{[](DomNode) {}};
        REQUIRE(values_equal(Value{node_callback}, Value{node_callback}));
    }

    SECTION("object references compare by identity") {
        ReferenceObject reference{};
        ReferenceObject shared_copy = reference;
        REQUIRE(values_equal(Value{reference}, Value{shared_copy}));
        REQUIRE_FALSE(values_equal(Value{reference}, Value{ReferenceObject{}}));
    }
}
