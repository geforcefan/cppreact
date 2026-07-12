#include <functional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/hooks/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"
#include "cppreact/vnode/helpers.hpp"

using namespace cppreact;
using namespace cppreact::tags;

struct CountState {
    int count = 0;
    bool operator==(const CountState&) const = default;
};

struct CountAction {
    std::string type;
    int by = 0;
};

struct MessageState {
    std::string inner_message;
    bool operator==(const MessageState&) const = default;
};

struct MessageAction {
    std::string payload;
};

struct MessageContextValue {
    MessageState state;
    std::function<void(MessageAction)> dispatch;
};

static std::vector<CountState> states;
static std::function<void(CountAction)> dispatch_action;
static std::function<void(CountAction)> first_dispatch;
static FunctionComponent child_component = nullptr;
static Context<MessageContextValue> bad_context = create_context(MessageContextValue{});

static CountState count_reducer(const CountState& state, const CountAction& action) {
    if (action.type == "increment") return CountState{state.count + action.by};
    return state;
}

TEST_CASE("useReducer") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("rerenders when dispatching an action") {
        states.clear();

        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            auto [state, dispatch] = use_reducer<CountState, CountAction>(count_reducer, CountState{0});
            dispatch_action = dispatch;
            states.push_back(state);
            return fragment();
        };

        render(TestComponent({}), scratch);

        dispatch_action({"increment", 10});
        flush();

        REQUIRE(states == std::vector<CountState>{{0}, {10}});
    }

    SECTION("can be dispatched by another component") {
        const FunctionComponent DispatchComponent = [](const Object& props) -> VNode {
            return Input({{"on_click", props.get<Callback>("increment",Callback{})}},
                     "Increment");
        };
        child_component = DispatchComponent;

        const FunctionComponent ReducerComponent = [](const Object&) -> VNode {
            auto [state, dispatch] = use_reducer<CountState, CountAction>(count_reducer, CountState{0});
            return View({},
                     Text({}, "Count: " + std::to_string(state.count)),
                     child_component(
                       {{"increment", Callback{[dispatch = dispatch](const Event&) {
                           dispatch({"increment", 10});
                       }}}}));
        };

        render(ReducerComponent({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>Count: 0</text><input>Increment</input></view>");

        DomNode button = renderer.find_first("input");
        renderer.dispatch_event(button, "click");

        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Count: 10</text><input>Increment</input></view>");
    }

    SECTION("can lazily initialize its state with an action") {
        states.clear();

        const FunctionComponent TestComponent = [](const Object& props) -> VNode {
            int initial_count = static_cast<int>(props.get<double>("initial_count",0));
            auto [state, dispatch] = use_reducer<CountState, CountAction>(
                count_reducer, [initial_count] { return CountState{initial_count}; });
            dispatch_action = dispatch;
            states.push_back(state);
            return fragment();
        };

        render(TestComponent({{"initial_count", 10.0}}), scratch);

        dispatch_action({"increment", 10});
        flush();

        REQUIRE(states == std::vector<CountState>{{10}, {20}});
    }

    SECTION("provides a stable reference for dispatch") {
        states.clear();

        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            auto [state, dispatch] = use_reducer<CountState, CountAction>(count_reducer, CountState{0});
            if (!first_dispatch) first_dispatch = dispatch;
            states.push_back(state);
            return fragment();
        };

        first_dispatch = nullptr;
        render(TestComponent({}), scratch);
        render(TestComponent({}), scratch);

        first_dispatch({"increment", 10});
        flush();

        REQUIRE(states == std::vector<CountState>{{0}, {0}, {10}});
    }

    SECTION("uses latest reducer") {
        states.clear();

        const FunctionComponent TestComponent = [](const Object& props) -> VNode {
            int increment = static_cast<int>(props.get<double>("increment",0));
            auto [state, dispatch] = use_reducer<CountState, CountAction>(
                [increment](const CountState& state, const CountAction& action) {
                    if (action.type == "increment") return CountState{state.count + increment};
                    return state;
                },
                CountState{0});
            dispatch_action = dispatch;
            states.push_back(state);
            return fragment();
        };

        render(TestComponent({{"increment", 10.0}}), scratch);
        render(TestComponent({{"increment", 20.0}}), scratch);

        dispatch_action({"increment"});
        flush();

        REQUIRE(states == std::vector<CountState>{{0}, {0}, {20}});
    }

    SECTION("should not mutate the hookState") {
        const FunctionComponent ContextMessage = [](const Object&) -> VNode {
            const MessageContextValue& value = use_context(bad_context);
            use_effect(
                [dispatch = value.dispatch]() -> CleanupFunction {
                    if (dispatch) dispatch({"message"});
                    return {};
                },
                {});

            return when(!value.state.inner_message.empty(),
                        [&] { return Text({}, value.state.inner_message); });
        };
        child_component = ContextMessage;

        const FunctionComponent Wrapper = [](const Object&) -> VNode {
            return View({}, children());
        };
        static FunctionComponent wrapper_component = Wrapper;

        const FunctionComponent Abstraction = [](const Object&) -> VNode {
            auto [state, dispatch] = use_reducer<MessageState, MessageAction>(
                [](const MessageState& state, const MessageAction& action) {
                    MessageState next = state;
                    next.inner_message = action.payload;
                    return next;
                },
                MessageState{});
            return provide(bad_context, MessageContextValue{state, dispatch},
                           wrapper_component({}, children()));
        };
        static FunctionComponent abstraction_component = Abstraction;

        const FunctionComponent App = [](const Object&) -> VNode {
            return abstraction_component({}, child_component({}));
        };

        render(App({}), scratch);
        flush();

        REQUIRE(renderer.inner_html() == "<view><text>message</text></view>");
    }
}
