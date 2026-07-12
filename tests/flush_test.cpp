#include <functional>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/hooks/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"

using namespace cppreact;
using namespace cppreact::tags;

static std::function<void(int)> counter_set;
static int parent_renders = 0;
static int child_renders = 0;
static std::function<void(int)> parent_set;
static std::function<void(int)> child_set;
static FunctionComponent child_component = nullptr;

TEST_CASE("flush") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("set_state queues and one flush drains it") {
        const FunctionComponent Counter = [](const Object&) -> VNode {
            auto [count, set_count] = use_state<int>(0);
            counter_set = [set_count = set_count](int value) { set_count(value); };
            return View({}, count);
        };

        render(Counter({}), scratch);
        REQUIRE(renderer.inner_html() == "<view>0</view>");

        counter_set(7);
        REQUIRE(renderer.inner_html() == "<view>0</view>");

        flush();
        REQUIRE(renderer.inner_html() == "<view>7</view>");
    }

    SECTION("the parent renders first and absorbs the queued child") {
        parent_renders = 0;
        child_renders = 0;

        const FunctionComponent Child = [](const Object&) -> VNode {
            auto [value, set_value] = use_state<int>(0);
            ++child_renders;
            child_set = [set_value = set_value](int next) { set_value(next); };
            return Text({}, value);
        };
        child_component = Child;

        const FunctionComponent Parent = [](const Object&) -> VNode {
            auto [value, set_value] = use_state<int>(0);
            ++parent_renders;
            parent_set = [set_value = set_value](int next) { set_value(next); };
            return View({}, child_component({}));
        };

        render(Parent({}), scratch);
        REQUIRE(parent_renders == 1);
        REQUIRE(child_renders == 1);

        child_set(1);
        parent_set(1);
        flush();
        REQUIRE(parent_renders == 2);
        REQUIRE(child_renders == 2);
    }
}
