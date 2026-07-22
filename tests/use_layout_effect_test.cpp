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

namespace {

struct HarnessProps {};

struct PairProps {
    double a = 0;
    double b = 0;
};

struct LayoutValueProps {
    int value = 0;
};

struct IndexProps {
    double index = 0;
};

struct ValueTextProps {
    std::string value{};
};

struct IdProps {
    std::string id{};
};

}

static int callback_calls = 0;
static int cleanup_calls = 0;
static std::vector<std::string> sequence;
static std::vector<std::string> execution_order;
static std::vector<std::string> calls;
static std::vector<int> values;
static StateSetter<std::string> set;
static std::string expected_html;
static FunctionComponent<IdProps> id_grand_child_component = nullptr;
static FunctionComponent<IdProps> id_child_component = nullptr;
static FunctionComponent<HarnessProps> nested_child_component = nullptr;
static FunctionComponent<ValueTextProps> display_child_component = nullptr;
static hosts::HtmlStringHost* renderer_pointer = nullptr;

static Callback callback_ref(const std::string& name) {
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

        const FunctionComponent TestComponent = [](const HarnessProps&) -> VNode {
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

        const FunctionComponent TestComponent = [](const PairProps& props) -> VNode {
            use_layout_effect(
                []() -> CleanupFunction {
                    ++callback_calls;
                    return {};
                },
                {props.a, props.b});
            return fragment();
        };

        render(TestComponent({.a = 1, .b = 2}), scratch);
        REQUIRE(callback_calls == 1);

        render(TestComponent({.a = 1, .b = 2}), scratch);
        REQUIRE(callback_calls == 1);

        render(TestComponent({.a = 2, .b = 2}), scratch);
        REQUIRE(callback_calls == 2);

        render(TestComponent({.a = 2, .b = 2}), scratch);
        REQUIRE(callback_calls == 2);
    }

    SECTION("performs the effect at mount time and never again if an empty input Array is passed") {
        callback_calls = 0;

        const FunctionComponent TestComponent = [](const HarnessProps&) -> VNode {
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

        const FunctionComponent TestComponent = [](const HarnessProps&) -> VNode {
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

        const FunctionComponent TestComponent = [](const HarnessProps&) -> VNode {
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

        const FunctionComponent TestComponent = [](const LayoutValueProps& props) -> VNode {
            int value = props.value;
            use_layout_effect([value]() -> CleanupFunction {
                values.push_back(value);
                return {};
            });
            return fragment();
        };

        render(TestComponent({.value = 1}), scratch);
        render(TestComponent({.value = 2}), scratch);

        REQUIRE(values == std::vector<int>{1, 2});
    }

    SECTION("calls the effect immediately after render") {
        callback_calls = 0;
        cleanup_calls = 0;

        const FunctionComponent TestComponent = [](const HarnessProps&) -> VNode {
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

        const FunctionComponent Child = [](const HarnessProps&) -> VNode {
            use_layout_effect([]() -> CleanupFunction {
                ++callback_calls;
                return {};
            });
            return fragment();
        };
        nested_child_component = Child;

        const FunctionComponent Parent = [](const HarnessProps&) -> VNode {
            return View({.children = {nested_child_component({})}});
        };

        render(Parent({}), scratch);

        REQUIRE(callback_calls == 1);
    }

    SECTION("should execute multiple layout effects in same component in the right order") {
        const FunctionComponent App = [](const IndexProps& props) -> VNode {
            execution_order.clear();
            use_layout_effect(
                []() -> CleanupFunction {
                    execution_order.push_back("action1");
                    return [] { execution_order.push_back("cleanup1"); };
                },
                {props.index});
            use_layout_effect(
                []() -> CleanupFunction {
                    execution_order.push_back("action2");
                    return [] { execution_order.push_back("cleanup2"); };
                },
                {props.index});
            return Text({.children = {"Test"}});
        };

        render(App({.index = 0}), scratch);
        render(App({.index = 2}), scratch);

        REQUIRE(execution_order ==
                std::vector<std::string>{"cleanup1", "cleanup2", "action1", "action2"});
    }

    SECTION("should correctly display DOM") {
        const FunctionComponent AutoResizeTextareaLayoutEffect =
            [](const ValueTextProps& props) -> VNode {
            use_layout_effect([]() -> CleanupFunction {
                REQUIRE(renderer_pointer->inner_html() == expected_html);
                return {};
            });
            return fragment(Text({.children = {props.value}}), Textarea({}));
        };
        display_child_component = AutoResizeTextareaLayoutEffect;

        const FunctionComponent App = [](const ValueTextProps& props) -> VNode {
            return View({.class_name = props.value,
                         .children = {display_child_component({.value = props.value})}});
        };

        expected_html = "<view class=\"hi\"><text>hi</text><textarea></textarea></view>";
        render(App({.value = "hi"}), scratch);

        expected_html = "<view class=\"hii\"><text>hii</text><textarea></textarea></view>";
        render(App({.value = "hii"}), scratch);
    }

    SECTION("should invoke layout effects after subtree is fully connected") {
        callback_calls = 0;

        const FunctionComponent Inner = [](const HarnessProps&) -> VNode {
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
            return fragment(Textarea({.ref = ref}), Text({.children = {"hello"}}));
        };
        nested_child_component = Inner;

        const FunctionComponent Outer = [](const HarnessProps&) -> VNode {
            return View({.children = {nested_child_component({})}});
        };

        render(Outer({}), scratch);
        REQUIRE(callback_calls == 1);
    }

    SECTION("orders effects effectively") {
        calls.clear();

        const FunctionComponent GrandChild = [](const IdProps& props) -> VNode {
            std::string id = props.id;
            use_layout_effect(
                [id]() -> CleanupFunction {
                    calls.push_back(id + " - Effect");
                    return [id] { calls.push_back(id + " - Cleanup"); };
                },
                {id});
            return Text({.children = {id}});
        };
        id_grand_child_component = GrandChild;

        const FunctionComponent Child = [](const IdProps& props) -> VNode {
            std::string id = props.id;
            use_layout_effect(
                [id]() -> CleanupFunction {
                    calls.push_back(id + " - Effect");
                    return [id] { calls.push_back(id + " - Cleanup"); };
                },
                {id});
            return fragment(id_grand_child_component({.id = id + "-GrandChild-1"}),
                            id_grand_child_component({.id = id + "-GrandChild-2"}));
        };
        id_child_component = Child;

        const FunctionComponent Parent = [](const HarnessProps&) -> VNode {
            use_layout_effect(
                []() -> CleanupFunction {
                    calls.push_back("Parent - Effect");
                    return [] { calls.push_back("Parent - Cleanup"); };
                },
                {});
            return View({.class_name = "App",
                         .children = {id_child_component({.id = "Child-1"}),
                                      View({.children = {id_child_component({.id = "Child-2"})}}),
                                      id_child_component({.id = "Child-3"})}});
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

        const FunctionComponent App = [](const HarnessProps&) -> VNode {
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

            return Text({.children = {greeting}});
        };

        render(App({}), scratch);
        flush();

        REQUIRE(calls == std::vector<std::string>{"doing effecthi"});
    }

    SECTION("should not rerun committed effects") {
        calls.clear();

        const FunctionComponent App = [](const IndexProps& props) -> VNode {
            auto [greeting, set_greeting] = use_state<std::string>("hi");

            use_layout_effect(
                [greeting = greeting]() -> CleanupFunction {
                    calls.push_back("doing effect" + greeting);
                    return [greeting] { calls.push_back("cleaning up" + greeting); };
                },
                {});

            if (props.index == 2) {
                set_greeting("bye");
            }

            return Text({.children = {greeting}});
        };

        render(App({}), scratch);
        flush();
        REQUIRE(calls == std::vector<std::string>{"doing effecthi"});

        render(App({.index = 2}), scratch);
        flush();
        REQUIRE(calls == std::vector<std::string>{"doing effecthi"});
    }

    SECTION("should not schedule effects that have no change") {
        calls.clear();

        const FunctionComponent App = [](const HarnessProps&) -> VNode {
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

            return Text({.children = {greeting}});
        };

        render(App({}), scratch);
        flush();
        REQUIRE(calls == std::vector<std::string>{"doing effecthi"});

        set("bye");
        flush();
        REQUIRE(calls == std::vector<std::string>{"doing effecthi"});
    }

    SECTION("should run layout effects after all refs are invoked") {
        calls.clear();

        const FunctionComponent App = [](const HarnessProps&) -> VNode {
            ReferenceObject ref = use_ref(ReferenceObject{});
            use_layout_effect([ref]() -> CleanupFunction {
                REQUIRE(ref.current() == renderer_pointer->find_first("text"));
                calls.push_back("doing effect");
                return [] { calls.push_back("cleaning up"); };
            });

            return View({.ref = callback_ref("callback ref outer"),
                         .children = {Text({.ref = ref,
                                            .children = {Text({.ref = callback_ref(
                                                                   "callback ref inner"),
                                                               .children = {"Hi"}})}})}});
        };

        render(App({}), scratch);

        REQUIRE(calls == std::vector<std::string>{"callback ref inner", "callback ref outer",
                                                  "doing effect"});
    }
}
