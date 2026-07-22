#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/hooks/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"
#include "cppreact/vnode/helpers.hpp"
#include "cppreact/component/memo.hpp"

using namespace cppreact;
using namespace cppreact::tags;

namespace {

struct HarnessProps {
    bool operator==(const HarnessProps&) const = default;
};

struct ValueProps {
    int value = 0;
};

struct DefaultState {
    std::string state;
    bool operator==(const DefaultState&) const = default;
};

}

static std::vector<int> values;
static std::vector<std::pair<int, int>> pair_values;
static int inner_calls = 0;
static int unmount_calls = 0;
static Context<int> number_context;
static Context<int> foo_context;
static Context<int> bar_context;
static Context<DefaultState> state_context;
static FunctionComponent<HarnessProps> consumer_component = nullptr;
static FunctionComponent<HarnessProps> no_update_function = nullptr;
static StateSetter<int> change_value;
static StateSetter<bool> toggle_consumer;
static StateSetter<DefaultState> set;

TEST_CASE("useContext") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("gets values from context") {
        values.clear();
        number_context = create_context(13);

        const FunctionComponent TestComponent = [](const HarnessProps&) -> VNode {
            int value = use_context(number_context);
            values.push_back(value);
            return fragment();
        };

        render(TestComponent({}), scratch);
        render(number_context.Provider({.value = 42, .children = {TestComponent({})}}), scratch);
        render(number_context.Provider({.value = 69, .children = {TestComponent({})}}), scratch);

        REQUIRE(values == std::vector<int>{13, 42, 69});
    }

    SECTION("should use the value of the nearest Provider") {
        values.clear();
        number_context = create_context(0);

        const FunctionComponent App = [](const HarnessProps&) -> VNode {
            values.push_back(use_context(number_context));
            return View({.children = {"bar"}});
        };

        render(number_context.Provider(
                   {.value = 1,
                    .children = {number_context.Provider(
                        {.value = 2, .children = {App({})}})}}),
               scratch);

        REQUIRE(values == std::vector<int>{2});
    }

    SECTION("should use default value") {
        values.clear();
        foo_context = create_context(42);

        const FunctionComponent App = [](const HarnessProps&) -> VNode {
            values.push_back(use_context(foo_context));
            return View({});
        };

        render(App({}), scratch);
        REQUIRE(values == std::vector<int>{42});
    }

    SECTION("should update when value changes with nonUpdating Component on top") {
        values.clear();
        number_context = create_context(0);

        const FunctionComponent TestComponent = [](const HarnessProps&) -> VNode {
            int value = use_context(number_context);
            values.push_back(value);
            return Text({.children = {value}});
        };
        consumer_component = TestComponent;

        no_update_function = memo(FunctionComponent{[](const HarnessProps&) -> VNode {
            return consumer_component({});
        }});

        const FunctionComponent App = [](const ValueProps& props) -> VNode {
            return number_context.Provider(
                {.value = props.value, .children = {no_update_function({})}});
        };

        render(App({.value = 0}), scratch);
        REQUIRE(values == std::vector<int>{0});

        render(App({.value = 1}), scratch);
        flush();

        REQUIRE(values == std::vector<int>{0, 1});
    }

    SECTION("should only update when value has changed") {
        values.clear();
        number_context = create_context(0);

        const FunctionComponent TestComponent = [](const HarnessProps&) -> VNode {
            int value = use_context(number_context);
            values.push_back(value);
            return Text({.children = {value}});
        };
        consumer_component = TestComponent;

        const FunctionComponent App = [](const ValueProps& props) -> VNode {
            return number_context.Provider(
                {.value = props.value, .children = {consumer_component({})}});
        };

        render(App({.value = 0}), scratch);
        REQUIRE(values == std::vector<int>{0});

        render(App({.value = 1}), scratch);
        REQUIRE(values == std::vector<int>{0, 1});

        flush();
        REQUIRE(values == std::vector<int>{0, 1});
    }

    SECTION("should allow multiple context hooks at the same time") {
        pair_values.clear();
        unmount_calls = 0;
        foo_context = create_context(0);
        bar_context = create_context(10);

        const FunctionComponent TestComponent = [](const HarnessProps&) -> VNode {
            int foo = use_context(foo_context);
            int bar = use_context(bar_context);
            pair_values.push_back({foo, bar});
            use_effect([]() -> CleanupFunction { return [] { ++unmount_calls; }; }, {});
            return View({});
        };

        render(foo_context.Provider(
                   {.value = 0,
                    .children = {bar_context.Provider(
                        {.value = 10, .children = {TestComponent({})}})}}),
               scratch);

        REQUIRE(pair_values == std::vector<std::pair<int, int>>{{0, 10}});

        render(foo_context.Provider(
                   {.value = 11,
                    .children = {bar_context.Provider(
                        {.value = 42, .children = {TestComponent({})}})}}),
               scratch);

        REQUIRE(pair_values == std::vector<std::pair<int, int>>{{0, 10}, {11, 42}});
        REQUIRE(unmount_calls == 0);
    }

    SECTION("should not rerender consumers that have been unmounted") {
        inner_calls = 0;
        number_context = create_context(0);

        const FunctionComponent Inner = [](const HarnessProps&) -> VNode {
            ++inner_calls;
            int value = use_context(number_context);
            return View({.children = {value}});
        };
        consumer_component = Inner;

        const FunctionComponent App = [](const HarnessProps&) -> VNode {
            auto [value, set_value] = use_state<int>(0);
            auto [show, set_show] = use_state<bool>(true);
            change_value = set_value;
            toggle_consumer = set_show;
            return number_context.Provider(
                {.value = value,
                 .children = {View({.children = {when(
                                        show, [] { return consumer_component({}); })}})}});
        };

        render(App({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><view>0</view></view>");
        REQUIRE(inner_calls == 1);

        change_value(1);
        flush();
        REQUIRE(renderer.inner_html() == "<view><view>1</view></view>");
        REQUIRE(inner_calls == 2);

        toggle_consumer(false);
        flush();
        REQUIRE(renderer.inner_html() == "<view></view>");
        REQUIRE(inner_calls == 2);

        change_value(2);
        flush();
        REQUIRE(renderer.inner_html() == "<view></view>");
        REQUIRE(inner_calls == 2);
    }

    SECTION("should rerender when reset to defaultValue") {
        state_context = create_context(DefaultState{"hi"});

        const FunctionComponent Consumer = [](const HarnessProps&) -> VNode {
            const DefaultState& context_value = use_context(state_context);
            return Text({.children = {context_value.state}});
        };
        consumer_component = Consumer;

        no_update_function = memo(FunctionComponent{[](const HarnessProps&) -> VNode {
            return consumer_component({});
        }});

        const FunctionComponent Provider = [](const HarnessProps&) -> VNode {
            auto [state, set_state] = use_state<DefaultState>(DefaultState{"hi"});
            set = set_state;
            return state_context.Provider({.value = state, .children = {no_update_function({})}});
        };

        render(Provider({}), scratch);
        REQUIRE(renderer.inner_html() == "<text>hi</text>");

        set(DefaultState{"bye"});
        flush();
        REQUIRE(renderer.inner_html() == "<text>bye</text>");

        set(DefaultState{"hi"});
        flush();
        REQUIRE(renderer.inner_html() == "<text>hi</text>");
    }
}
