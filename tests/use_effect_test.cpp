#include <limits>
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

static int callback_calls = 0;
static int cleanup_calls = 0;
static std::vector<std::string> sequence;
static std::vector<std::string> execution_order;
static std::vector<std::string> calls;
static std::vector<int> values;
static StateSetter<std::string> set;
static StateSetter<int> set_val;
static FunctionComponent grand_child_component = nullptr;
static FunctionComponent child_component = nullptr;
static Container* scratch_pointer = nullptr;

TEST_CASE("useEffect") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("performs the effect after every render by default") {
        callback_calls = 0;

        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            use_effect([]() -> CleanupFunction {
                ++callback_calls;
                return {};
            });
            return fragment();
        };

        render(TestComponent({}), scratch);
        REQUIRE(callback_calls == 1);

        render(TestComponent({}), scratch);
        REQUIRE(callback_calls == 2);
    }

    SECTION("performs the effect only if one of the inputs changed") {
        callback_calls = 0;

        const FunctionComponent TestComponent = [](const Object& props) -> VNode {
            double a = props.get<double>("a",0);
            double b = props.get<double>("b",0);
            use_effect(
                []() -> CleanupFunction {
                    ++callback_calls;
                    return {};
                },
                {a, b});
            return fragment();
        };

        render(TestComponent({{"a", 1.0}, {"b", 2.0}}), scratch);
        REQUIRE(callback_calls == 1);

        render(TestComponent({{"a", 1.0}, {"b", 2.0}}), scratch);
        REQUIRE(callback_calls == 1);

        render(TestComponent({{"a", 2.0}, {"b", 2.0}}), scratch);
        REQUIRE(callback_calls == 2);

        render(TestComponent({{"a", 2.0}, {"b", 2.0}}), scratch);
        REQUIRE(callback_calls == 2);
    }

    SECTION("performs the effect at mount time and never again if an empty input Array is passed") {
        callback_calls = 0;

        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            use_effect(
                []() -> CleanupFunction {
                    ++callback_calls;
                    return {};
                },
                {});
            return fragment();
        };

        render(TestComponent({}), scratch);
        render(TestComponent({}), scratch);
        REQUIRE(callback_calls == 1);

        render(TestComponent({}), scratch);
        REQUIRE(callback_calls == 1);
    }

    SECTION("calls the cleanup function followed by the effect after each render") {
        sequence.clear();

        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            use_effect([]() -> CleanupFunction {
                sequence.push_back("callback");
                return [] { sequence.push_back("cleanup"); };
            });
            return fragment();
        };

        render(TestComponent({}), scratch);
        REQUIRE(sequence == std::vector<std::string>{"callback"});

        render(TestComponent({}), scratch);
        REQUIRE(sequence == std::vector<std::string>{"callback", "cleanup", "callback"});
    }

    SECTION("cleanups the effect when the component get unmounted if the effect was called before") {
        callback_calls = 0;
        cleanup_calls = 0;

        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            use_effect([]() -> CleanupFunction {
                ++callback_calls;
                return [] { ++cleanup_calls; };
            });
            return fragment();
        };

        render(TestComponent({}), scratch);
        REQUIRE(callback_calls == 1);

        render(fragment(), scratch);
        REQUIRE(cleanup_calls == 1);
    }

    SECTION("works with closure effect callbacks capturing props") {
        values.clear();

        const FunctionComponent TestComponent = [](const Object& props) -> VNode {
            int value = static_cast<int>(props.get<double>("value",0));
            use_effect([value]() -> CleanupFunction {
                values.push_back(value);
                return {};
            });
            return fragment();
        };

        render(TestComponent({{"value", 1.0}}), scratch);
        render(TestComponent({{"value", 2.0}}), scratch);

        REQUIRE(values == std::vector<int>{1, 2});
    }

    SECTION("should execute multiple effects in same component in the right order") {
        const FunctionComponent App = [](const Object& props) -> VNode {
            double i = props.get<double>("i",0);
            execution_order.clear();
            use_effect(
                []() -> CleanupFunction {
                    execution_order.push_back("action1");
                    return [] { execution_order.push_back("cleanup1"); };
                },
                {i});
            use_effect(
                []() -> CleanupFunction {
                    execution_order.push_back("action2");
                    return [] { execution_order.push_back("cleanup2"); };
                },
                {i});
            return Text({}, "Test");
        };

        render(App({{"i", 0.0}}), scratch);
        render(App({{"i", 2.0}}), scratch);

        REQUIRE(execution_order ==
                std::vector<std::string>{"cleanup1", "cleanup2", "action1", "action2"});
    }

    SECTION("orders effects effectively") {
        calls.clear();

        const FunctionComponent GrandChild = [](const Object& props) -> VNode {
            std::string id = props.get<std::string>("id","");
            use_effect(
                [id]() -> CleanupFunction {
                    calls.push_back(id + " - Effect");
                    return [id] { calls.push_back(id + " - Cleanup"); };
                },
                {id});
            return Text({}, id);
        };
        grand_child_component = GrandChild;

        const FunctionComponent Child = [](const Object& props) -> VNode {
            std::string id = props.get<std::string>("id","");
            use_effect(
                [id]() -> CleanupFunction {
                    calls.push_back(id + " - Effect");
                    return [id] { calls.push_back(id + " - Cleanup"); };
                },
                {id});
            return fragment(grand_child_component({{"id", id + "-GrandChild-1"}}),
                            grand_child_component({{"id", id + "-GrandChild-2"}}));
        };
        child_component = Child;

        const FunctionComponent Parent = [](const Object&) -> VNode {
            use_effect(
                []() -> CleanupFunction {
                    calls.push_back("Parent - Effect");
                    return [] { calls.push_back("Parent - Cleanup"); };
                },
                {});
            return View({{"class", "App"}},
                     child_component({{"id", "Child-1"}}),
                     View({}, child_component({{"id", "Child-2"}})),
                     child_component({{"id", "Child-3"}}));
        };

        render(Parent({}), scratch);

        REQUIRE(calls == std::vector<std::string>{
                             "Child-1-GrandChild-1 - Effect",
                             "Child-1-GrandChild-2 - Effect",
                             "Child-1 - Effect",
                             "Child-2-GrandChild-1 - Effect",
                             "Child-2-GrandChild-2 - Effect",
                             "Child-2 - Effect",
                             "Child-3-GrandChild-1 - Effect",
                             "Child-3-GrandChild-2 - Effect",
                             "Child-3 - Effect",
                             "Parent - Effect"});
    }

    SECTION("should cancel effects from a disposed render") {
        calls.clear();

        const FunctionComponent App = [](const Object&) -> VNode {
            auto [greeting, set_greeting] = use_state<std::string>("bye");

            use_effect(
                [greeting = greeting]() -> CleanupFunction {
                    calls.push_back("doing effect" + greeting);
                    return [greeting] { calls.push_back("cleaning up" + greeting); };
                },
                {greeting});

            if (greeting == "bye") {
                set_greeting("hi");
            }

            return Text({}, greeting);
        };

        render(App({}), scratch);
        flush();

        REQUIRE(calls == std::vector<std::string>{"doing effecthi"});
    }

    SECTION("should not rerun committed effects") {
        calls.clear();

        const FunctionComponent App = [](const Object& props) -> VNode {
            double i = props.get<double>("i",0);
            auto [greeting, set_greeting] = use_state<std::string>("hi");

            use_effect(
                [greeting = greeting]() -> CleanupFunction {
                    calls.push_back("doing effect" + greeting);
                    return [greeting] { calls.push_back("cleaning up" + greeting); };
                },
                {});

            if (i == 2) {
                set_greeting("bye");
            }

            return Text({}, greeting);
        };

        render(App({}), scratch);
        flush();
        REQUIRE(calls == std::vector<std::string>{"doing effecthi"});

        render(App({{"i", 2.0}}), scratch);
        flush();
        REQUIRE(calls == std::vector<std::string>{"doing effecthi"});
    }

    SECTION("should not schedule effects that have no change") {
        calls.clear();

        const FunctionComponent App = [](const Object&) -> VNode {
            auto [greeting, set_greeting] = use_state<std::string>("hi");
            set = set_greeting;

            use_effect(
                [greeting = greeting]() -> CleanupFunction {
                    calls.push_back("doing effect" + greeting);
                    return [greeting] { calls.push_back("cleaning up" + greeting); };
                },
                {greeting});

            if (greeting == "bye") {
                set_greeting("hi");
            }

            return Text({}, greeting);
        };

        render(App({}), scratch);
        flush();
        REQUIRE(calls == std::vector<std::string>{"doing effecthi"});

        set("bye");
        flush();
        REQUIRE(calls == std::vector<std::string>{"doing effecthi"});
    }

    SECTION("should not crash when the component is unmounted by a root render during flush") {
        scratch_pointer = &scratch;

        const FunctionComponent App = [](const Object&) -> VNode {
            auto [val, set_value] = use_state<int>(0);
            set_val = set_value;
            use_effect(
                [val = val]() -> CleanupFunction {
                    if (val == 1) {
                        render(fragment(), *scratch_pointer);
                    }
                    return {};
                },
                {static_cast<double>(val)});
            return View({}, "val: ", val);
        };

        render(App({}), scratch);
        flush();

        set_val(1);
        flush();
        REQUIRE(renderer.inner_html() == "");
    }

    SECTION("should not rerun when receiving NaN on subsequent renders") {
        calls.clear();

        const FunctionComponent TestComponent = [](const Object& props) -> VNode {
            double value = props.get<double>("value",0);
            auto [count, set_count] = use_state<int>(0);
            use_effect(
                [count = count, set_count = set_count]() -> CleanupFunction {
                    calls.push_back("doing effect" + std::to_string(count));
                    set_count(count + 1);
                    return [count] { calls.push_back("cleaning up" + std::to_string(count)); };
                },
                {value});
            return Text({}, count);
        };
        grand_child_component = TestComponent;

        const FunctionComponent App = [](const Object&) -> VNode {
            return grand_child_component(
                     {{"value", std::numeric_limits<double>::quiet_NaN()}});
        };

        render(App({}), scratch);
        flush();

        REQUIRE(calls == std::vector<std::string>{"doing effect0"});
    }
}
