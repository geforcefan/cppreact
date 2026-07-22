#include <functional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/context/create_context.hpp"
#include "cppreact/hooks/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"
#include "cppreact/vnode/helpers.hpp"

using namespace cppreact;
using namespace cppreact::tags;

namespace {

struct HarnessProps {};

struct LabelProps {
    std::string label{};
};

struct LabelIterationProps {
    std::string label{};
    int iteration = 0;
};

struct EntryProps {
    Children children{};
};

struct RecyclableProps {
    std::string key{};
};

struct ToggleChildProps {
    bool use_function_child = false;
};

struct DepthChildProps {
    int items = 0;
    std::function<void()> update{};
};

}

static const FunctionComponent MixedArrayFoo =
    [](const HarnessProps&) -> VNode { return text("d"); };

static std::vector<VNode> mixed_array() {
    std::vector<VNode> items;
    items.push_back(VNode(0));
    items.push_back(text("a"));
    items.push_back(text("b"));
    items.push_back(Text({.children = {"c"}}));
    items.push_back(MixedArrayFoo({}));
    items.push_back(fragment());
    items.push_back(fragment());
    items.push_back(fragment());
    items.push_back(fragment(text("e"), text("f")));
    items.push_back(VNode(1));
    return items;
}

static const std::string mixed_array_html = "0ab<text>c</text>def1";

static std::vector<std::string> ops;

static int outer_render_calls = 0;
static int inner_render_calls = 0;
static int inner_render_serial = 0;

static StateSetter<bool> root_alt_setter;
static std::function<void()> add_entry_callback;
static std::function<void()> add_entry_twice_callback;
static std::function<void()> reset_entries_callback;

static StateSetter<bool> good_container_alt_setter;
static StateSetter<bool> bad_container_alt_setter;
static int recyclable_message_index = 0;

static std::function<void()> outer_update_callback;
static bool outer_alt_flag = false;

static StateSetter<bool> outer_child_setter;
static StateSetter<int> inner_second_tick_setter;
static int outer_mount_count = 0;
static int inner_first_render_calls = 0;
static int inner_first_mount_count = 0;
static int inner_second_render_calls = 0;
static int inner_second_mount_count = 0;

static std::function<void()> depth_update_callback;
static int depth_child_render_calls = 0;
static int depth_parent_render_count = 0;

TEST_CASE("Components") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("should render functional components") {
        outer_render_calls = 0;

        const FunctionComponent Panel = [](const LabelProps& props) -> VNode {
            ++outer_render_calls;
            return View({.class_name = props.label});
        };

        render(Panel({.label = "bar"}), scratch);

        REQUIRE(outer_render_calls == 1);
        REQUIRE(renderer.inner_html() == "<view class=\"bar\"></view>");
    }

    SECTION("should render components with props") {
        const FunctionComponent Panel = [](const LabelProps& props) -> VNode {
            return View({.class_name = props.label});
        };

        render(Panel({.label = "bar"}), scratch);

        REQUIRE(renderer.inner_html() == "<view class=\"bar\"></view>");
    }

    SECTION("should render string") {
        const FunctionComponent StringComponent = [](const HarnessProps&) -> VNode {
            return text("Hi there");
        };

        render(StringComponent({}), scratch);
        REQUIRE(renderer.inner_html() == "Hi there");
    }

    SECTION("should render number as string") {
        const FunctionComponent NumberComponent = [](const HarnessProps&) -> VNode {
            return VNode(42);
        };

        render(NumberComponent({}), scratch);
        REQUIRE(renderer.inner_html() == "42");
    }

    SECTION("should remove orphaned elements replaced by Components") {
        const FunctionComponent SpanComponent = [](const HarnessProps&) -> VNode {
            return Text({.children = {"span in a component"}});
        };

        render(SpanComponent({}), scratch);
        render(View({.children = {"just a div"}}), scratch);
        render(SpanComponent({}), scratch);

        REQUIRE(renderer.inner_html() == "<text>span in a component</text>");
    }

    SECTION("should remove children when root changes to text node") {
        const FunctionComponent RootSwitch = [](const HarnessProps&) -> VNode {
            auto [alternate, set_alternate] = use_state<bool>(false);
            root_alt_setter = set_alternate;
            if (alternate) return text("asdf");
            return View({.children = {"test"}});
        };

        render(RootSwitch({}), scratch);

        root_alt_setter(true);
        flush();
        REQUIRE(renderer.inner_html() == "asdf");

        root_alt_setter(false);
        flush();
        REQUIRE(renderer.inner_html() == "<view>test</view>");

        root_alt_setter(true);
        flush();
        REQUIRE(renderer.inner_html() == "asdf");
    }

    SECTION("should maintain order when setting state (that inserts dom-elements)") {
        const FunctionComponent Entry = [](const EntryProps& props) -> VNode {
            return View({.children = props.children});
        };

        const FunctionComponent App = [Entry](const HarnessProps&) -> VNode {
            auto [values, set_values] = use_state(std::vector<std::string>{"abc"});
            add_entry_callback = [values, set_values] {
                std::vector<std::string> next = values;
                next.push_back("def");
                set_values(next);
            };
            add_entry_twice_callback = [values, set_values] {
                std::vector<std::string> next = values;
                next.push_back("def");
                next.push_back("ghi");
                set_values(next);
            };
            reset_entries_callback = [set_values] { set_values(std::vector<std::string>{"abc"}); };

            std::vector<VNode> entries;
            for (const std::string& value : values) entries.push_back(Entry({.children = {value}}));
            entries.push_back(View({.children = {"First Button"}}));
            entries.push_back(View({.children = {"Second Button"}}));
            entries.push_back(View({.children = {"Third Button"}}));
            return View({.children = std::move(entries)});
        };

        render(App({}), scratch);
        REQUIRE(renderer.inner_html() ==
                "<view><view>abc</view><view>First Button</view>"
                "<view>Second Button</view><view>Third Button</view></view>");

        add_entry_callback();
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view><view>abc</view><view>def</view><view>First Button</view>"
                "<view>Second Button</view><view>Third Button</view></view>");

        add_entry_callback();
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view><view>abc</view><view>def</view><view>def</view><view>First Button</view>"
                "<view>Second Button</view><view>Third Button</view></view>");

        reset_entries_callback();
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view><view>abc</view><view>First Button</view>"
                "<view>Second Button</view><view>Third Button</view></view>");

        add_entry_twice_callback();
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view><view>abc</view><view>def</view><view>ghi</view><view>First Button</view>"
                "<view>Second Button</view><view>Third Button</view></view>");
    }

    SECTION("should not recycle common class children with different keys") {
        ops.clear();
        recyclable_message_index = 0;
        static const std::string messages[8] = {"A", "B", "C", "D", "E", "F", "G", "H"};

        const FunctionComponent Comp = [](const RecyclableProps&) -> VNode {
            bool& mounted = use_ref<bool>(false);
            std::string& message = use_ref<std::string>("");
            if (!mounted) {
                message = messages[recyclable_message_index++ % 8];
                ops.push_back("Mount " + message);
                mounted = true;
            }
            return View({.children = {message}});
        };

        const FunctionComponent GoodContainer = [Comp](const HarnessProps&) -> VNode {
            auto [alternate, set_alternate] = use_state<bool>(false);
            good_container_alt_setter = set_alternate;
            return View({.children = {
                            when(!alternate, [Comp] { return Comp({.key = "1"}); }),
                            when(!alternate, [Comp] { return Comp({.key = "2"}); }),
                            when(alternate, [Comp] { return Comp({.key = "3"}); })}});
        };

        const FunctionComponent BadContainer = [Comp](const HarnessProps&) -> VNode {
            auto [alternate, set_alternate] = use_state<bool>(false);
            bad_container_alt_setter = set_alternate;
            return View({.children = {
                            when(!alternate, [Comp] { return Comp({}); }),
                            when(!alternate, [Comp] { return Comp({}); }),
                            when(alternate, [Comp] { return Comp({}); })}});
        };

        render(GoodContainer({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><view>A</view><view>B</view></view>");
        REQUIRE(ops == std::vector<std::string>{"Mount A", "Mount B"});

        ops.clear();
        good_container_alt_setter(true);
        flush();
        REQUIRE(renderer.inner_html() == "<view><view>C</view></view>");
        REQUIRE(ops == std::vector<std::string>{"Mount C"});

        ops.clear();
        render(BadContainer({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><view>D</view><view>E</view></view>");
        REQUIRE(ops == std::vector<std::string>{"Mount D", "Mount E"});

        ops.clear();
        bad_container_alt_setter(true);
        flush();
        REQUIRE(renderer.inner_html() == "<view><view>F</view></view>");
        REQUIRE(ops == std::vector<std::string>{"Mount F"});
    }

    SECTION("should render DOM element's array children") {
        render(View({.children = mixed_array()}), scratch);
        DomNode root = renderer.find_first("view");
        REQUIRE(renderer.inner_html(root) == mixed_array_html);
    }

    SECTION("should render Component's array children") {
        const FunctionComponent MixedArrayComponent = [](const HarnessProps&) -> VNode {
            return fragment(mixed_array());
        };

        render(MixedArrayComponent({}), scratch);
        REQUIRE(renderer.inner_html() == mixed_array_html);
    }

    SECTION("should render Fragment's array children") {
        const FunctionComponent MixedArrayFragmentComponent = [](const HarnessProps&) -> VNode {
            return fragment(mixed_array());
        };

        render(MixedArrayFragmentComponent({}), scratch);
        REQUIRE(renderer.inner_html() == mixed_array_html);
    }

    SECTION("should render sibling array children") {
        const FunctionComponent Todo = [](const HarnessProps&) -> VNode {
            std::vector<std::string> first_pair{"a", "b"};
            std::vector<std::string> second_pair{"c", "d"};
            return View({.children = {
                            View({.children = {"A header"}}),
                            fragment(map(first_pair,
                                         [](const std::string& value) {
                                             return View({.children = {value}});
                                         })),
                            View({.children = {"A divider"}}),
                            fragment(map(second_pair,
                                         [](const std::string& value) {
                                             return View({.children = {value}});
                                         })),
                            View({.children = {"A footer"}})}});
        };

        render(Todo({}), scratch);

        REQUIRE(renderer.inner_html() ==
                "<view><view>A header</view><view>a</view><view>b</view><view>A divider</view>"
                "<view>c</view><view>d</view><view>A footer</view></view>");
    }

    SECTION("should render wrapper HOCs") {
        Context<std::string> bob_ross_context = create_context(std::string(""));
        const std::string bob_ross_text =
            "We'll throw some happy little limbs on this tree.";

        const FunctionComponent PaintSomething = [bob_ross_context](const HarnessProps&) -> VNode {
            const std::string& value = use_context(bob_ross_context);
            return View({.children = {value}});
        };

        const FunctionComponent Paint =
            [bob_ross_context, PaintSomething, bob_ross_text](const HarnessProps&) -> VNode {
            return bob_ross_context.Provider(
                {.value = bob_ross_text, .children = {PaintSomething({})}});
        };

        render(Paint({}), scratch);
        REQUIRE(renderer.inner_html() == "<view>" + bob_ross_text + "</view>");
    }

    SECTION("should render nested functional components") {
        outer_render_calls = 0;
        inner_render_calls = 0;
        static std::string last_inner_label;

        const FunctionComponent Inner = [](const LabelProps& props) -> VNode {
            ++inner_render_calls;
            last_inner_label = props.label;
            return View({.class_name = props.label, .children = {"inner"}});
        };

        const FunctionComponent Outer = [Inner](const LabelProps& props) -> VNode {
            ++outer_render_calls;
            return Inner({.label = props.label});
        };

        render(Outer({.label = "bar"}), scratch);

        REQUIRE(outer_render_calls == 1);
        REQUIRE(inner_render_calls == 1);
        REQUIRE(last_inner_label == "bar");
        REQUIRE(renderer.inner_html() == "<view class=\"bar\">inner</view>");
    }

    SECTION("should re-render nested functional components") {
        inner_render_calls = 0;
        inner_render_serial = 0;

        const FunctionComponent Inner = [](const LabelIterationProps& props) -> VNode {
            ++inner_render_calls;
            ++inner_render_serial;
            return View({.class_name = props.label,
                         .children = {"iteration=" + std::to_string(props.iteration) +
                                      " serial=" + std::to_string(inner_render_serial)}});
        };

        const FunctionComponent Outer = [Inner](const LabelProps& props) -> VNode {
            auto [iteration, set_iteration] = use_state<int>(1);
            outer_update_callback = [set_iteration, iteration] { set_iteration(iteration + 1); };
            return Inner({.label = props.label, .iteration = iteration});
        };

        render(Outer({.label = "bar"}), scratch);

        outer_update_callback();
        flush();

        REQUIRE(inner_render_calls == 2);
        REQUIRE(renderer.inner_html() == "<view class=\"bar\">iteration=2 serial=2</view>");

        outer_update_callback();
        flush();

        REQUIRE(inner_render_calls == 3);
        REQUIRE(renderer.inner_html() == "<view class=\"bar\">iteration=3 serial=3</view>");
    }

    SECTION("should re-render nested components") {
        ops.clear();
        inner_render_calls = 0;
        inner_render_serial = 0;
        outer_alt_flag = false;

        const FunctionComponent Inner = [](const LabelIterationProps& props) -> VNode {
            ++inner_render_calls;
            ++inner_render_serial;
            use_effect(
                []() -> CleanupFunction {
                    ops.push_back("Mount Inner");
                    return [] { ops.push_back("Unmount Inner"); };
                },
                {});
            return View({.class_name = props.label,
                         .children = {"iteration=" + std::to_string(props.iteration) +
                                      " serial=" + std::to_string(inner_render_serial)}});
        };

        const FunctionComponent Outer = [Inner](const LabelProps& props) -> VNode {
            auto [iteration, set_iteration] = use_state<int>(1);
            outer_update_callback = [set_iteration, iteration] { set_iteration(iteration + 1); };
            if (outer_alt_flag) return View({.class_name = "alt"});
            return Inner({.label = props.label, .iteration = iteration});
        };

        render(Outer({.label = "bar"}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Mount Inner"});

        outer_update_callback();
        flush();
        REQUIRE(inner_render_calls == 2);
        REQUIRE(renderer.inner_html() == "<view class=\"bar\">iteration=2 serial=2</view>");

        outer_update_callback();
        flush();
        REQUIRE(inner_render_calls == 3);
        REQUIRE(renderer.inner_html() == "<view class=\"bar\">iteration=3 serial=3</view>");

        outer_alt_flag = true;
        outer_update_callback();
        flush();
        REQUIRE(ops == std::vector<std::string>{"Mount Inner", "Unmount Inner"});
        REQUIRE(renderer.inner_html() == "<view class=\"alt\"></view>");

        outer_alt_flag = false;
        outer_update_callback();
        flush();
        REQUIRE(renderer.inner_html() == "<view class=\"bar\">iteration=5 serial=4</view>");
    }

    SECTION("should resolve intermediary functional component") {
        ops.clear();

        const FunctionComponent Inner = [](const HarnessProps&) -> VNode {
            use_effect(
                []() -> CleanupFunction {
                    ops.push_back("Mount Inner");
                    return [] { ops.push_back("Unmount Inner"); };
                },
                {});
            return View({.children = {"inner"}});
        };

        const FunctionComponent Intermediate = [Inner](const HarnessProps&) -> VNode {
            return Inner({});
        };

        const FunctionComponent Root = [Intermediate](const HarnessProps&) -> VNode {
            return Intermediate({});
        };

        render(Root({}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Mount Inner"});

        render(View({}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Mount Inner", "Unmount Inner"});
    }

    SECTION("should unmount children of high-order components without unmounting parent") {
        outer_mount_count = 0;
        inner_first_render_calls = 0;
        inner_first_mount_count = 0;
        inner_second_render_calls = 0;
        inner_second_mount_count = 0;

        const FunctionComponent InnerFirst = [](const HarnessProps&) -> VNode {
            ++inner_first_render_calls;
            use_effect([]() -> CleanupFunction {
                            ++inner_first_mount_count;
                            return [] {};
                        },
                        {});
            return View({.children = {"a"}});
        };

        const FunctionComponent InnerSecond = [](const HarnessProps&) -> VNode {
            ++inner_second_render_calls;
            auto [tick, set_tick] = use_state<int>(0);
            inner_second_tick_setter = set_tick;
            (void)tick;
            use_effect([]() -> CleanupFunction {
                            ++inner_second_mount_count;
                            return [] {};
                        },
                        {});
            return View({.children = {"b"}});
        };

        const FunctionComponent Outer =
            [InnerFirst, InnerSecond](const HarnessProps&) -> VNode {
            auto [use_second, set_use_second] = use_state<bool>(false);
            outer_child_setter = set_use_second;
            use_effect([]() -> CleanupFunction {
                            ++outer_mount_count;
                            return [] {};
                        },
                        {});
            if (use_second) return InnerSecond({});
            return InnerFirst({});
        };

        render(Outer({}), scratch);
        flush();
        REQUIRE(outer_mount_count == 1);
        REQUIRE(inner_first_mount_count == 1);

        outer_child_setter(true);
        flush();
        REQUIRE(inner_second_render_calls == 1);
        REQUIRE(outer_mount_count == 1);
        REQUIRE(inner_second_mount_count == 1);

        inner_second_tick_setter(1);
        flush();
        REQUIRE(inner_second_render_calls == 2);
        REQUIRE(outer_mount_count == 1);
        REQUIRE(inner_second_mount_count == 1);
    }

    SECTION("should remount when swapping between HOC child types") {
        ops.clear();

        const FunctionComponent Inner = [](const HarnessProps&) -> VNode {
            use_effect(
                []() -> CleanupFunction {
                    ops.push_back("Mount Inner");
                    return [] { ops.push_back("Unmount Inner"); };
                },
                {});
            return View({.class_name = "inner", .children = {"foo"}});
        };

        const FunctionComponent InnerFunction = [](const HarnessProps&) -> VNode {
            return View({.class_name = "inner-function", .children = {"bar"}});
        };

        const FunctionComponent Outer =
            [Inner, InnerFunction](const ToggleChildProps& props) -> VNode {
            if (props.use_function_child) return InnerFunction({});
            return Inner({});
        };

        render(Outer({.use_function_child = false}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Mount Inner"});

        render(Outer({.use_function_child = true}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Mount Inner", "Unmount Inner"});

        render(Outer({.use_function_child = false}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Mount Inner", "Unmount Inner", "Mount Inner"});
    }

    SECTION("should render components by depth") {
        depth_child_render_calls = 0;
        depth_parent_render_count = 0;

        const FunctionComponent Child = [](const DepthChildProps& props) -> VNode {
            ++depth_child_render_calls;
            int items = props.items;
            auto [tick, set_tick] = use_state<int>(0);
            std::function<void()> parent_update = props.update;
            depth_update_callback = [parent_update, set_tick, tick] {
                if (parent_update) parent_update();
                set_tick(tick + 1);
            };
            std::string joined;
            for (int index = 0; index < items; ++index) {
                if (index > 0) joined += ",";
                joined += std::to_string(index);
            }
            return View({.children = {joined}});
        };

        const FunctionComponent Parent = [Child](const HarnessProps&) -> VNode {
            auto [version, set_version] = use_state<int>(0);
            std::function<void()> parent_update = [set_version, version] {
                set_version(version + 1);
            };
            ++depth_parent_render_count;
            return Child({.items = depth_parent_render_count, .update = parent_update});
        };

        render(Parent({}), scratch);
        REQUIRE(depth_child_render_calls == 1);

        depth_update_callback();
        flush();
        REQUIRE(depth_child_render_calls == 2);
    }
}
