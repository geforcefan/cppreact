#include <functional>
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

struct AState {
    int a = 0;
    bool operator==(const AState&) const = default;
};

static std::vector<AState> state_history;
static int component_calls = 0;
static int last_state = 0;
static StateSetter<int> do_set_state;
static std::string argument;
static std::string scoped_thing;
static std::function<void()> simulate_click;
static std::function<void()> open_widget;
static std::function<void()> close_widget;
static StateSetter<std::string> set_parent;
static StateSetter<bool> set_child;
static int initialize_state_calls = 0;
static std::vector<std::string> calls;

static std::pair<std::string, StateSetter<std::string>> use_something() {
    return use_state<std::string>([] {
        argument = scoped_thing;
        return scoped_thing;
    });
}

TEST_CASE("useState") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("serves the same state across render calls") {
        state_history.clear();

        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            auto [state, set_state] = use_state<AState>(AState{1});
            state_history.push_back(state);
            return fragment();
        };

        render(TestComponent({}), scratch);
        render(TestComponent({}), scratch);

        REQUIRE(state_history == std::vector<AState>{{1}, {1}});
    }

    SECTION("can initialize the state via a function") {
        initialize_state_calls = 0;

        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            use_state<int>([] {
                ++initialize_state_calls;
                return 1;
            });
            return fragment();
        };

        render(TestComponent({}), scratch);
        render(TestComponent({}), scratch);

        REQUIRE(initialize_state_calls == 1);
    }

    SECTION("does not rerender on equal state") {
        component_calls = 0;

        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            auto [state, set_state] = use_state<int>(0);
            ++component_calls;
            last_state = state;
            do_set_state = set_state;
            return fragment();
        };

        render(TestComponent({}), scratch);
        REQUIRE(last_state == 0);
        REQUIRE(component_calls == 1);

        do_set_state(0);
        flush();
        REQUIRE(last_state == 0);
        REQUIRE(component_calls == 1);

        do_set_state([](const int&) { return 0; });
        flush();
        REQUIRE(last_state == 0);
        REQUIRE(component_calls == 1);
    }

    SECTION("rerenders when setting the state") {
        component_calls = 0;

        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            auto [state, set_state] = use_state<int>(0);
            ++component_calls;
            last_state = state;
            do_set_state = set_state;
            return fragment();
        };

        render(TestComponent({}), scratch);
        REQUIRE(last_state == 0);
        REQUIRE(component_calls == 1);

        do_set_state(1);
        flush();
        REQUIRE(last_state == 1);
        REQUIRE(component_calls == 2);

        do_set_state([](const int& current) { return current * 10; });
        flush();
        REQUIRE(last_state == 10);
        REQUIRE(component_calls == 3);
    }

    SECTION("can be set by another component") {
        const FunctionComponent Increment = [](const Object& props) -> VNode {
            return Input({{"on_click", props.get<Callback>("increment",Callback{})}},
                     "Increment");
        };

        static FunctionComponent increment_child = Increment;
        const FunctionComponent StateContainer = [](const Object&) -> VNode {
            auto [count, set_count] = use_state<int>(0);
            return View({},
                     Text({}, "Count: " + std::to_string(count)),
                     increment_child(
                       {{"increment", Callback{[set_count = set_count](const Event&) {
                           set_count([](const int& c) { return c + 10; });
                       }}}}));
        };

        render(StateContainer({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>Count: 0</text><input>Increment</input></view>");

        DomNode button = renderer.find_first("input");
        renderer.dispatch_event(button, "click");

        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Count: 10</text><input>Increment</input></view>");
    }

    SECTION("should correctly initialize") {
        scoped_thing = "hi";
        argument.clear();

        const FunctionComponent App = [](const Object&) -> VNode {
            auto [state, set_state] = use_something();
            return Text({}, state);
        };

        render(App({}), scratch);

        REQUIRE(argument == "hi");
        REQUIRE(renderer.inner_html() == "<text>hi</text>");
    }

    SECTION("should handle queued useState") {
        const FunctionComponent Message = [](const Object& props) -> VNode {
            std::string message = props.get<std::string>("message","");
            auto [is_visible, set_visible] = use_state<bool>(!message.empty());
            auto [previous_message, set_previous_message] = use_state<std::string>(message);

            if (message != previous_message) {
                set_previous_message(message);
                set_visible(!message.empty());
            }

            if (!is_visible) {
                return fragment();
            }
            return Text({{"on_click", props.get<Callback>("on_close",Callback{})}},
                     message);
        };

        static FunctionComponent message_child = Message;
        const FunctionComponent App = [](const Object&) -> VNode {
            auto [message, set_message] = use_state<std::string>("Click Here!!");
            return message_child(
                     {{"on_close", Callback{[set_message = set_message](const Event&) {
                         set_message("");
                     }}},
                      {"message", message}});
        };

        render(App({}), scratch);
        REQUIRE(renderer.inner_html() == "<text>Click Here!!</text>");
        DomNode text = renderer.find_first("text");
        renderer.dispatch_event(text, "click");
        flush();
        REQUIRE(renderer.inner_html() == "");
    }

    SECTION("should render a second time when the render function updates state") {
        calls.clear();

        const FunctionComponent App = [](const Object&) -> VNode {
            auto [greeting, set_greeting] = use_state<std::string>("bye");

            if (greeting == "bye") {
                set_greeting("hi");
            }

            calls.push_back(greeting);

            return Text({}, greeting);
        };

        render(App({}), scratch);
        flush();
        REQUIRE(calls.size() == 2);
        REQUIRE(calls == std::vector<std::string>{"bye", "hi"});
        REQUIRE(renderer.inner_html() == "<text>hi</text>");
    }

    SECTION("correctly updates with multiple state updates") {
        const FunctionComponent TestWidget = [](const Object&) -> VNode {
            auto [saved, set_saved] = use_state<bool>(false);
            auto [saving, set_saving] = use_state<bool>(false);

            simulate_click = [set_saving = set_saving, set_saved = set_saved] {
                set_saving(true);
                set_saved(true);
                set_saving(false);
            };

            return View({}, saved ? "Saved!" : "Unsaved!");
        };

        render(TestWidget({}), scratch);
        REQUIRE(renderer.inner_html() == "<view>Unsaved!</view>");

        simulate_click();
        flush();

        REQUIRE(renderer.inner_html() == "<view>Saved!</view>");
    }

    SECTION("iterates over all hooks") {
        const FunctionComponent TestWidget = [](const Object&) -> VNode {
            auto [counter, set_counter] = use_state<int>(0);
            auto [is_open, set_open] = use_state<bool>(false);

            open_widget = [set_counter = set_counter, set_open = set_open] {
                set_counter(42);
                set_open(true);
            };

            close_widget = [set_open = set_open] {
                set_open(false);
            };

            return View({}, is_open ? "open" : "closed");
        };

        render(TestWidget({}), scratch);
        REQUIRE(renderer.inner_html() == "<view>closed</view>");

        open_widget();
        flush();
        REQUIRE(renderer.inner_html() == "<view>open</view>");

        close_widget();
        flush();
        REQUIRE(renderer.inner_html() == "<view>closed</view>");
    }

    SECTION("respects updates initiated from the parent") {
        const FunctionComponent Child = [](const Object& props) -> VNode {
            auto [state, set_state] = use_state<bool>(false);
            set_child = set_state;
            return Text({}, props.get<std::string>("text",""));
        };

        static FunctionComponent child_component = Child;
        const FunctionComponent Parent = [](const Object&) -> VNode {
            auto [state, set_state] = use_state<std::string>("hello world");
            set_parent = set_state;
            return child_component({{"text", state}});
        };

        render(Parent({}), scratch);
        REQUIRE(renderer.inner_html() == "<text>hello world</text>");

        set_parent("hello world!!!");
        set_child(true);
        set_child(false);
        flush();
        REQUIRE(renderer.inner_html() == "<text>hello world!!!</text>");
    }
}
