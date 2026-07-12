#include <functional>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/value/object.hpp"

using namespace cppreact;

TEST_CASE("props") {
    SECTION("set overwrites, find locates, take removes") {
        Object props{{"label", "Save"}, {"count", 3.0}};
        REQUIRE(props.get("label"));
        REQUIRE(props.get("missing") == nullptr);

        props.set("label", std::string("Rename"));
        REQUIRE(props.get<std::string>("label","") == "Rename");

        props.take("label");
        REQUIRE_FALSE(props.get("label"));
        REQUIRE(props.size() == 1);
    }

    SECTION("typed get returns the stored form or nothing") {
        Object props{{"count", 3.0}, {"active", true}};
        REQUIRE(props.get<double>("count",0) == 3.0);
        REQUIRE(props.get<bool>("active",false));
        REQUIRE_FALSE(props.get<std::string>("count").has_value());
        REQUIRE_FALSE(props.get<bool>("missing").has_value());
    }

    SECTION("get with a function signature unpacks the callback typed") {
        double received = 0;
        Object props{{"on_change", Callback{[&received](double next) { received = next; }}}};

        std::function<void(double)> on_change =
            props.get<void(double)>("on_change",nullptr);
        REQUIRE(on_change != nullptr);
        on_change(0.5);
        REQUIRE(received == 0.5);

        REQUIRE_FALSE(props.get<void(std::string)>("on_change").has_value());
        REQUIRE_FALSE(props.get<void(double)>("missing").has_value());

        std::function<void(double)> named =
            props.get<std::function<void(double)>>("on_change",nullptr);
        REQUIRE(named != nullptr);
    }
}
