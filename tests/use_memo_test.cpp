#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/hooks/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"

using namespace cppreact;
using namespace cppreact::tags;

namespace {

struct HarnessProps {};

struct PairProps {
    double a = 0;
    double b = 0;

    bool operator==(const PairProps&) const = default;
};

struct ToggleProps {
    bool all = false;

    bool operator==(const ToggleProps&) const = default;
};

}

static int memo_function_calls = 0;
static std::vector<int> results;
static std::vector<std::string> calls;
static StateSetter<std::string> set;
static StateSetter<int> update;

TEST_CASE("useMemo") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("only recomputes the result when inputs change") {
        memo_function_calls = 0;
        results.clear();

        const FunctionComponent TestComponent = [](const PairProps& props) -> VNode {
            int result = use_memo<int>(
                [a = props.a, b = props.b]() {
                    ++memo_function_calls;
                    return static_cast<int>(a + b);
                },
                {props.a, props.b});
            results.push_back(result);
            return fragment();
        };

        render(TestComponent({.a = 1, .b = 1}), scratch);
        render(TestComponent({.a = 1, .b = 1}), scratch);

        REQUIRE(results == std::vector<int>{2, 2});
        REQUIRE(memo_function_calls == 1);

        render(TestComponent({.a = 1, .b = 2}), scratch);
        render(TestComponent({.a = 1, .b = 2}), scratch);

        REQUIRE(results == std::vector<int>{2, 2, 3, 3});
        REQUIRE(memo_function_calls == 2);
    }

    SECTION("should rerun when the dependency list length changes") {
        memo_function_calls = 0;

        const FunctionComponent TestComponent = [](const ToggleProps& props) -> VNode {
            Dependencies dependency_list = props.all ? Dependencies{1.0, 2.0} : Dependencies{1.0};
            use_memo<int>(
                []() {
                    ++memo_function_calls;
                    return 1 + 2;
                },
                dependency_list);
            return fragment();
        };

        render(TestComponent({.all = true}), scratch);
        REQUIRE(memo_function_calls == 1);
        render(TestComponent({.all = false}), scratch);
        REQUIRE(memo_function_calls == 2);
    }

    SECTION("should not commit memoization from a skipped render") {
        calls.clear();

        const FunctionComponent App = [](const HarnessProps&) -> VNode {
            auto [greeting, set_greeting] = use_state<std::string>("hi");
            set = set_greeting;

            std::string value = use_memo<std::string>(
                [greeting = greeting]() {
                    calls.push_back("doing memo");
                    return greeting;
                },
                {greeting});
            calls.push_back("render " + value);

            if (greeting == "bye") {
                set_greeting("hi");
            }

            return Text({.children = {value}});
        };

        render(App({}), scratch);
        REQUIRE(calls == std::vector<std::string>{"doing memo", "render hi"});

        set("bye");
        flush();
        REQUIRE(calls == std::vector<std::string>{"doing memo", "render hi", "doing memo",
                                                  "render bye", "doing memo", "render hi"});
    }

    SECTION("should promote falsy value after a skipped render") {
        const FunctionComponent App = [](const HarnessProps&) -> VNode {
            auto [v, set_value] = use_state<int>(0);
            update = set_value;
            int res = use_memo<int>([]() { return 0; }, {v > 1});

            if (v == 0) {
                set_value(v + 1);
            }
            return Text({.children = {res}});
        };

        render(App({}), scratch);
        flush();
        REQUIRE(renderer.inner_html() == "<text>0</text>");

        update([](const int& v) { return v + 1; });
        flush();
        update([](const int& v) { return v + 1; });
        flush();

        REQUIRE(renderer.inner_html() == "<text>0</text>");
    }
}
