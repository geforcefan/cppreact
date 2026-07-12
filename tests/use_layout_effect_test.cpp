#include <memory>
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
static std::string expected_html;
static FunctionComponent grand_child_component = nullptr;
static FunctionComponent child_component = nullptr;
static hosts::HtmlStringHost* renderer_pointer = nullptr;
static int expected_layout_updates = 0;

static Value callback_ref(const std::string& name) {
    return Callback{[name](DomNode node) {
        if (node != null_dom_node) calls.push_back(name);
    }};
}

TEST_CASE("useLayoutEffect") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();
    renderer_pointer = &renderer;

    SECTION("performs the effect after every render by default") {
        callback_calls = 0;

        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            use_layout_effect([]() -> CleanupFunction {
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
            use_layout_effect(
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
            use_layout_effect(
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
            use_layout_effect([]() -> CleanupFunction {
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
            use_layout_effect([]() -> CleanupFunction {
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
            use_layout_effect([value]() -> CleanupFunction {
                values.push_back(value);
                return {};
            });
            return fragment();
        };

        render(TestComponent({{"value", 1.0}}), scratch);
        render(TestComponent({{"value", 2.0}}), scratch);

        REQUIRE(values == std::vector<int>{1, 2});
    }

    SECTION("calls the effect immediately after render") {
        callback_calls = 0;
        cleanup_calls = 0;

        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            use_layout_effect([]() -> CleanupFunction {
                ++callback_calls;
                return [] { ++cleanup_calls; };
            });
            return fragment();
        };

        render(TestComponent({}), scratch);
        render(TestComponent({}), scratch);

        REQUIRE(cleanup_calls == 1);
        REQUIRE(callback_calls == 2);

        render(TestComponent({}), scratch);

        REQUIRE(cleanup_calls == 2);
        REQUIRE(callback_calls == 3);
    }

    SECTION("works on a nested component") {
        callback_calls = 0;

        const FunctionComponent Child = [](const Object&) -> VNode {
            use_layout_effect([]() -> CleanupFunction {
                ++callback_calls;
                return {};
            });
            return fragment();
        };
        child_component = Child;

        const FunctionComponent Parent = [](const Object&) -> VNode {
            return View({}, child_component({}));
        };

        render(Parent({}), scratch);

        REQUIRE(callback_calls == 1);
    }

    SECTION("should execute multiple layout effects in same component in the right order") {
        const FunctionComponent App = [](const Object& props) -> VNode {
            double i = props.get<double>("i",0);
            execution_order.clear();
            use_layout_effect(
                []() -> CleanupFunction {
                    execution_order.push_back("action1");
                    return [] { execution_order.push_back("cleanup1"); };
                },
                {i});
            use_layout_effect(
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

    SECTION("should correctly display DOM") {
        const FunctionComponent AutoResizeTextareaLayoutEffect = [](const Object& props) -> VNode {
            std::string value = props.get<std::string>("value","");
            use_layout_effect([]() -> CleanupFunction {
                REQUIRE(renderer_pointer->inner_html() == expected_html);
                return {};
            });
            return fragment(Text({}, value), Textarea({}));
        };
        child_component = AutoResizeTextareaLayoutEffect;

        const FunctionComponent App = [](const Object& props) -> VNode {
            std::string value = props.get<std::string>("value","");
            return View({{"class", value}}, child_component({{"value", value}}));
        };

        expected_html = "<view class=\"hi\"><text>hi</text><textarea></textarea></view>";
        render(App({{"value", "hi"}}), scratch);

        expected_html = "<view class=\"hii\"><text>hii</text><textarea></textarea></view>";
        render(App({{"value", "hii"}}), scratch);
    }

    SECTION("should invoke layout effects after subtree is fully connected") {
        callback_calls = 0;

        const FunctionComponent Inner = [](const Object&) -> VNode {
            ReferenceObject ref = use_ref(ReferenceObject{});
            use_layout_effect([ref]() -> CleanupFunction {
                ++callback_calls;
                bool connected = false;
                for (DomNode current = ref.current(); current != null_dom_node;
                     current = renderer_pointer->parent_node(current)) {
                    if (current == renderer_pointer->document()) {
                        connected = true;
                        break;
                    }
                }
                REQUIRE(connected);
                return {};
            });
            return fragment(Textarea({{"ref", ref}}), Text({}, "hello"));
        };
        child_component = Inner;

        const FunctionComponent Outer = [](const Object&) -> VNode {
            return View({}, child_component({}));
        };

        render(Outer({}), scratch);
        REQUIRE(callback_calls == 1);
    }

    SECTION("orders effects effectively") {
        calls.clear();

        const FunctionComponent GrandChild = [](const Object& props) -> VNode {
            std::string id = props.get<std::string>("id","");
            use_layout_effect(
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
            use_layout_effect(
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
            use_layout_effect(
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

            use_layout_effect(
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

            use_layout_effect(
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

            use_layout_effect(
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

    SECTION("updates the renderer layout once per commit before layout effects run") {
        callback_calls = 0;
        renderer.layout_updates = 0;

        const FunctionComponent Child = [](const Object&) -> VNode {
            use_layout_effect([]() -> CleanupFunction {
                REQUIRE(renderer_pointer->layout_updates == expected_layout_updates);
                ++callback_calls;
                return {};
            });
            return fragment();
        };
        child_component = Child;

        const FunctionComponent Parent = [](const Object&) -> VNode {
            use_layout_effect([]() -> CleanupFunction {
                ++callback_calls;
                return {};
            });
            return View({}, child_component({}), child_component({}));
        };

        expected_layout_updates = 1;
        render(Parent({}), scratch);
        REQUIRE(callback_calls == 3);
        REQUIRE(renderer.layout_updates == 1);

        expected_layout_updates = 2;
        render(Parent({}), scratch);
        REQUIRE(callback_calls == 6);
        REQUIRE(renderer.layout_updates == 2);

        render(fragment(), scratch);
        REQUIRE(renderer.layout_updates == 2);
    }

    SECTION("does not update the renderer layout without pending layout effects") {
        renderer.layout_updates = 0;

        const FunctionComponent Plain = [](const Object&) -> VNode {
            use_effect([]() -> CleanupFunction { return {}; });
            return Text({}, "plain");
        };

        render(Plain({}), scratch);
        REQUIRE(renderer.layout_updates == 0);
    }

    SECTION("should run layout effects after all refs are invoked") {
        calls.clear();

        const FunctionComponent App = [](const Object&) -> VNode {
            ReferenceObject ref = use_ref(ReferenceObject{});
            use_layout_effect([ref]() -> CleanupFunction {
                REQUIRE(ref.current() == renderer_pointer->find_first("text"));
                calls.push_back("doing effect");
                return [] { calls.push_back("cleaning up"); };
            });

            return View({{"ref", callback_ref("callback ref outer")}},
                     Text({{"ref", ref}},
                       Text({{"ref", callback_ref("callback ref inner")}}, "Hi")));
        };

        render(App({}), scratch);

        REQUIRE(calls == std::vector<std::string>{"callback ref inner", "callback ref outer",
                                                  "doing effect"});
    }
}
