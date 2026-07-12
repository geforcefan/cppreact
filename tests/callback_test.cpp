#include <functional>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/value/callback.hpp"
#include "cppreact/event/synthetic_event.hpp"
#include "cppreact/value/value.hpp"

using namespace cppreact;

TEST_CASE("callbacks") {
    SECTION("deduces the signature from the lambda and unpacks only with it") {
        Callback on_change{[](double) {}};
        REQUIRE(on_change.as<void(double)>() != nullptr);
        REQUIRE(on_change.as<std::function<void(double)>>() != nullptr);
        REQUIRE(on_change.as<void(float)>() == nullptr);
        REQUIRE(on_change.as<EventCallback>() == nullptr);
    }

    SECTION("the named signatures unpack through their aliases") {
        Callback on_click{[](const Event&) {}};
        REQUIRE(on_click.as<EventCallback>() != nullptr);

        Callback reference{[](DomNode) {}};
        REQUIRE(reference.as<ReferenceCallback>() != nullptr);
        REQUIRE(reference.as<EventCallback>() == nullptr);
    }

    SECTION("carries any signature including return values") {
        Callback add_one{[](double count) { return static_cast<int>(count) + 1; }};
        const std::function<int(double)>* function = add_one.as<int(double)>();
        REQUIRE(function != nullptr);
        REQUIRE((*function)(2.0) == 3);
    }

    SECTION("accepts a std::function and keeps its signature") {
        std::function<void(std::string)> report = [](std::string) {};
        Callback wrapped{report};
        REQUIRE(wrapped.as<void(std::string)>() != nullptr);
        REQUIRE(wrapped.as<void(double)>() == nullptr);
    }

    SECTION("copies share identity, fresh constructions never do") {
        Callback handler{[](const Event&) {}};
        Callback copy = handler;
        REQUIRE(handler == copy);
        REQUIRE_FALSE(handler == Callback{[](const Event&) {}});
    }

    SECTION("an empty callback is falsy and unpacks to nothing") {
        REQUIRE_FALSE(static_cast<bool>(Callback{}));
        REQUIRE(Callback{}.as<void(double)>() == nullptr);
        REQUIRE(Callback{} == Callback{});
        REQUIRE_FALSE(static_cast<bool>(Callback{nullptr}));
    }
}
