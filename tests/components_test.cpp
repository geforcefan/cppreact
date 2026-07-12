#include <functional>
#include <memory>
#include <optional>
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

static std::vector<std::string> ops;

static int outer_render_calls = 0;
static int inner_render_calls = 0;
static int inner_render_serial = 0;

static FunctionComponent nested_inner_component = nullptr;
static FunctionComponent nested_second_inner_component = nullptr;
static FunctionComponent nested_intermediate_component = nullptr;

static StateSetter<bool> root_alt_setter;
static std::function<void()> add_entry_callback;
static std::function<void()> add_entry_twice_callback;
static std::function<void()> reset_entries_callback;
static FunctionComponent entry_component = nullptr;
static FunctionComponent paint_something_component = nullptr;

static FunctionComponent recyclable_component = nullptr;
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

static const FunctionComponent MixedArrayFoo = [](const Object&) -> VNode { return "d"; };

static std::vector<VNode> mixed_array() {
    std::vector<VNode> items;
    items.push_back(VNode(0));
    items.push_back(text("a"));
    items.push_back(text("b"));
    items.push_back(Text({}, "c"));
    items.push_back(MixedArrayFoo({}));
    items.push_back(fragment());
    items.push_back(fragment());
    items.push_back(fragment());
    items.push_back(fragment(text("e"), text("f")));
    items.push_back(VNode(1));
    return items;
}

static const std::string mixed_array_html = "0ab<text>c</text>def1";

TEST_CASE("Components") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("should render functional components") {
        outer_render_calls = 0;

        const FunctionComponent C3 = [](const Object& props) -> VNode {
            ++outer_render_calls;
            std::string foo = props.get<std::string>("foo","");
            Callback on_baz = props.get<Callback>("on_baz",Callback{});
            return View({{"foo", foo}, {"on_baz", on_baz}});
        };

        render(C3({{"foo", "bar"}, {"on_baz", Callback{[](const Event&) {}}}}), scratch);

        REQUIRE(outer_render_calls == 1);
        REQUIRE(renderer.inner_html() == "<view foo=\"bar\"></view>");
    }

    SECTION("should render components with props") {
        const FunctionComponent C2 = [](const Object& props) -> VNode {
            std::string foo = props.get<std::string>("foo","");
            return View({{"foo", foo}});
        };

        render(C2({{"foo", "bar"}}), scratch);

        REQUIRE(renderer.inner_html() == "<view foo=\"bar\"></view>");
    }

    SECTION("should render string") {
        const FunctionComponent StringComponent = [](const Object&) -> VNode {
            return text("Hi there");
        };

        render(StringComponent({}), scratch);
        REQUIRE(renderer.inner_html() == "Hi there");
    }

    SECTION("should render number as string") {
        const FunctionComponent NumberComponent = [](const Object&) -> VNode {
            return VNode(42);
        };

        render(NumberComponent({}), scratch);
        REQUIRE(renderer.inner_html() == "42");
    }

    SECTION("should remove orphaned elements replaced by Components") {
        const FunctionComponent SpanComponent = [](const Object&) -> VNode {
            return Text({}, "span in a component");
        };

        render(SpanComponent({}), scratch);
        render(View({}, "just a div"), scratch);
        render(SpanComponent({}), scratch);

        REQUIRE(renderer.inner_html() == "<text>span in a component</text>");
    }

    SECTION("should remove children when root changes to text node") {
        const FunctionComponent RootSwitch = [](const Object&) -> VNode {
            auto [alt, set_alt] = use_state<bool>(false);
            root_alt_setter = set_alt;
            if (alt) return text("asdf");
            return View({}, "test");
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
        const FunctionComponent Entry = [](const Object&) -> VNode {
            return View({}, children());
        };
        entry_component = Entry;

        const FunctionComponent App = [](const Object&) -> VNode {
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
            for (const std::string& value : values) entries.push_back(entry_component({}, value));
            entries.push_back(View({}, "First Button"));
            entries.push_back(View({}, "Second Button"));
            entries.push_back(View({}, "Third Button"));
            return View({}, std::move(entries));
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

        const FunctionComponent Comp = [](const Object&) -> VNode {
            bool& mounted = use_ref<bool>(false);
            std::string& message = use_ref<std::string>("");
            if (!mounted) {
                message = messages[recyclable_message_index++ % 8];
                ops.push_back("Mount " + message);
                mounted = true;
            }
            return View({}, message);
        };
        recyclable_component = Comp;

        const FunctionComponent GoodContainer = [](const Object&) -> VNode {
            auto [alt, set_alt] = use_state<bool>(false);
            good_container_alt_setter = set_alt;
            return View({},
                       when(!alt, [] { return recyclable_component({{"key", "1"}}); }),
                       when(!alt, [] { return recyclable_component({{"key", "2"}}); }),
                       when(alt, [] { return recyclable_component({{"key", "3"}}); }));
        };

        const FunctionComponent BadContainer = [](const Object&) -> VNode {
            auto [alt, set_alt] = use_state<bool>(false);
            bad_container_alt_setter = set_alt;
            return View({},
                       when(!alt, [] { return recyclable_component({}); }),
                       when(!alt, [] { return recyclable_component({}); }),
                       when(alt, [] { return recyclable_component({}); }));
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
        render(View({}, mixed_array()), scratch);
        DomNode root = renderer.find_first("view");
        REQUIRE(renderer.inner_html(root) == mixed_array_html);
    }

    SECTION("should render Component's array children") {
        const FunctionComponent MixedArrayComponent = [](const Object&) -> VNode {
            return fragment(mixed_array());
        };

        render(MixedArrayComponent({}), scratch);
        REQUIRE(renderer.inner_html() == mixed_array_html);
    }

    SECTION("should render Fragment's array children") {
        const FunctionComponent MixedArrayFragmentComponent = [](const Object&) -> VNode {
            return fragment(mixed_array());
        };

        render(MixedArrayFragmentComponent({}), scratch);
        REQUIRE(renderer.inner_html() == mixed_array_html);
    }

    SECTION("should render sibling array children") {
        const FunctionComponent Todo = [](const Object&) -> VNode {
            std::vector<std::string> first_pair{"a", "b"};
            std::vector<std::string> second_pair{"c", "d"};
            return View({},
                     View({}, "A header"),
                     fragment(map(first_pair, [](const std::string& value) { return View({}, value); })),
                     View({}, "A divider"),
                     fragment(map(second_pair, [](const std::string& value) { return View({}, value); })),
                     View({}, "A footer"));
        };

        render(Todo({}), scratch);

        REQUIRE(renderer.inner_html() ==
                "<view><view>A header</view><view>a</view><view>b</view><view>A divider</view>"
                "<view>c</view><view>d</view><view>A footer</view></view>");
    }

    SECTION("should render wrapper HOCs") {
        static Context<std::string> bob_ross_context = create_context(std::string(""));
        static std::string bob_ross_text =
            "We'll throw some happy little limbs on this tree.";

        const FunctionComponent PaintSomething = [](const Object&) -> VNode {
            const std::string& value = use_context(bob_ross_context);
            return View({}, value);
        };
        paint_something_component = PaintSomething;

        const FunctionComponent Paint = [](const Object&) -> VNode {
            return provide(bob_ross_context, bob_ross_text, paint_something_component({}));
        };

        render(Paint({}), scratch);
        REQUIRE(renderer.inner_html() == "<view>" + bob_ross_text + "</view>");
    }

    SECTION("should render nested functional components") {
        outer_render_calls = 0;
        inner_render_calls = 0;
        static std::string last_inner_foo;

        const FunctionComponent Inner = [](const Object& props) -> VNode {
            ++inner_render_calls;
            std::string foo = props.get<std::string>("foo","");
            last_inner_foo = foo;
            return View({{"foo", foo}}, "inner");
        };
        nested_inner_component = Inner;

        const FunctionComponent Outer = [](const Object& props) -> VNode {
            ++outer_render_calls;
            std::string foo = props.get<std::string>("foo","");
            return nested_inner_component({{"foo", foo}});
        };

        render(Outer({{"foo", "bar"}, {"on_baz", Callback{[](const Event&) {}}}}), scratch);

        REQUIRE(outer_render_calls == 1);
        REQUIRE(inner_render_calls == 1);
        REQUIRE(last_inner_foo == "bar");
        REQUIRE(renderer.inner_html() == "<view foo=\"bar\">inner</view>");
    }

    SECTION("should re-render nested functional components") {
        inner_render_calls = 0;
        inner_render_serial = 0;

        const FunctionComponent Inner = [](const Object& props) -> VNode {
            ++inner_render_calls;
            ++inner_render_serial;
            int i = static_cast<int>(props.get<double>("i",0));
            std::string foo = props.get<std::string>("foo","");
            return View({{"j", static_cast<double>(inner_render_serial)},
                       {"i", static_cast<double>(i)},
                       {"foo", foo}},
                      "inner");
        };
        nested_inner_component = Inner;

        const FunctionComponent Outer = [](const Object& props) -> VNode {
            auto [i, set_i] = use_state<int>(1);
            outer_update_callback = [set_i, i] { set_i(i + 1); };
            std::string foo = props.get<std::string>("foo","");
            return nested_inner_component({{"i", static_cast<double>(i)}, {"foo", foo}});
        };

        render(Outer({{"foo", "bar"}}), scratch);

        outer_update_callback();
        flush();

        REQUIRE(inner_render_calls == 2);
        REQUIRE(renderer.inner_html() == "<view foo=\"bar\" i=\"2\" j=\"2\">inner</view>");

        outer_update_callback();
        flush();

        REQUIRE(inner_render_calls == 3);
        REQUIRE(renderer.inner_html() == "<view foo=\"bar\" i=\"3\" j=\"3\">inner</view>");
    }

    SECTION("should re-render nested components") {
        ops.clear();
        inner_render_calls = 0;
        inner_render_serial = 0;
        outer_alt_flag = false;

        const FunctionComponent Inner = [](const Object& props) -> VNode {
            ++inner_render_calls;
            ++inner_render_serial;
            use_effect(
                []() -> CleanupFunction {
                    ops.push_back("Mount Inner");
                    return [] { ops.push_back("Unmount Inner"); };
                },
                {});
            int i = static_cast<int>(props.get<double>("i",0));
            std::string foo = props.get<std::string>("foo","");
            return View({{"j", static_cast<double>(inner_render_serial)},
                       {"i", static_cast<double>(i)},
                       {"foo", foo}},
                      "inner");
        };
        nested_inner_component = Inner;

        const FunctionComponent Outer = [](const Object& props) -> VNode {
            auto [i, set_i] = use_state<int>(1);
            outer_update_callback = [set_i, i] { set_i(i + 1); };
            if (outer_alt_flag) return View({{"is-alt", true}});
            std::string foo = props.get<std::string>("foo","");
            return nested_inner_component({{"i", static_cast<double>(i)}, {"foo", foo}});
        };

        render(Outer({{"foo", "bar"}}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Mount Inner"});

        outer_update_callback();
        flush();
        REQUIRE(inner_render_calls == 2);
        REQUIRE(renderer.inner_html() == "<view foo=\"bar\" i=\"2\" j=\"2\">inner</view>");

        outer_update_callback();
        flush();
        REQUIRE(inner_render_calls == 3);
        REQUIRE(renderer.inner_html() == "<view foo=\"bar\" i=\"3\" j=\"3\">inner</view>");

        outer_alt_flag = true;
        outer_update_callback();
        flush();
        REQUIRE(ops == std::vector<std::string>{"Mount Inner", "Unmount Inner"});
        REQUIRE(renderer.inner_html() == "<view is-alt=\"true\"></view>");

        outer_alt_flag = false;
        outer_update_callback();
        flush();
        REQUIRE(renderer.inner_html() == "<view foo=\"bar\" i=\"5\" j=\"4\">inner</view>");
    }

    SECTION("should resolve intermediary functional component") {
        ops.clear();

        const FunctionComponent Inner = [](const Object&) -> VNode {
            use_effect(
                []() -> CleanupFunction {
                    ops.push_back("Mount Inner");
                    return [] { ops.push_back("Unmount Inner"); };
                },
                {});
            return View({}, "inner");
        };
        nested_inner_component = Inner;

        const FunctionComponent Intermediate = [](const Object&) -> VNode {
            return nested_inner_component({});
        };
        nested_intermediate_component = Intermediate;

        const FunctionComponent Root = [](const Object&) -> VNode {
            return nested_intermediate_component({});
        };

        render(Root({}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Mount Inner"});

        render(h("asdf", {}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Mount Inner", "Unmount Inner"});
    }

    SECTION("should unmount children of high-order components without unmounting parent") {
        outer_mount_count = 0;
        inner_first_render_calls = 0;
        inner_first_mount_count = 0;
        inner_second_render_calls = 0;
        inner_second_mount_count = 0;

        const FunctionComponent InnerFirst = [](const Object&) -> VNode {
            ++inner_first_render_calls;
            use_effect([]() -> CleanupFunction {
                            ++inner_first_mount_count;
                            return [] {};
                        },
                        {});
            return View({}, "a");
        };

        const FunctionComponent InnerSecond = [](const Object&) -> VNode {
            ++inner_second_render_calls;
            auto [tick, set_tick] = use_state<int>(0);
            inner_second_tick_setter = set_tick;
            (void)tick;
            use_effect([]() -> CleanupFunction {
                            ++inner_second_mount_count;
                            return [] {};
                        },
                        {});
            return View({}, "b");
        };

        nested_inner_component = InnerFirst;
        nested_second_inner_component = InnerSecond;

        const FunctionComponent Outer = [](const Object&) -> VNode {
            auto [use_second, set_use_second] = use_state<bool>(false);
            outer_child_setter = set_use_second;
            use_effect([]() -> CleanupFunction {
                            ++outer_mount_count;
                            return [] {};
                        },
                        {});
            if (use_second) return nested_second_inner_component({});
            return nested_inner_component({});
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

        const FunctionComponent Inner = [](const Object&) -> VNode {
            use_effect(
                []() -> CleanupFunction {
                    ops.push_back("Mount Inner");
                    return [] { ops.push_back("Unmount Inner"); };
                },
                {});
            return View({{"class", "inner"}}, "foo");
        };
        nested_inner_component = Inner;

        const FunctionComponent InnerFunction = [](const Object&) -> VNode {
            return View({{"class", "inner-function"}}, "bar");
        };
        nested_second_inner_component = InnerFunction;

        const FunctionComponent Outer = [](const Object& props) -> VNode {
            bool use_function_child = props.get<bool>("use_function_child",false);
            if (use_function_child) return nested_second_inner_component({});
            return nested_inner_component({});
        };

        render(Outer({{"use_function_child", false}}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Mount Inner"});

        render(Outer({{"use_function_child", true}}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Mount Inner", "Unmount Inner"});

        render(Outer({{"use_function_child", false}}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Mount Inner", "Unmount Inner", "Mount Inner"});
    }

    SECTION("should render components by depth") {
        depth_child_render_calls = 0;
        depth_parent_render_count = 0;

        const FunctionComponent Child = [](const Object& props) -> VNode {
            ++depth_child_render_calls;
            int items = static_cast<int>(props.get<double>("items",0));
            auto [tick, set_tick] = use_state<int>(0);
            std::optional<RawPayload> update_payload = props.get<RawPayload>("update");
            depth_update_callback = [update_payload, set_tick, tick] {
                if (update_payload && update_payload->data) {
                    auto parent_update =
                        std::static_pointer_cast<std::function<void()>>(update_payload->data);
                    (*parent_update)();
                }
                set_tick(tick + 1);
            };
            std::string joined;
            for (int index = 0; index < items; ++index) {
                if (index > 0) joined += ",";
                joined += std::to_string(index);
            }
            return View({}, joined);
        };
        nested_inner_component = Child;

        const FunctionComponent Parent = [](const Object&) -> VNode {
            auto [version, set_version] = use_state<int>(0);
            std::function<void()> parent_update = [set_version, version] {
                set_version(version + 1);
            };
            ++depth_parent_render_count;
            RawPayload update_payload{std::make_shared<std::function<void()>>(parent_update)};
            return nested_inner_component(
                {{"items", static_cast<double>(depth_parent_render_count)}, {"update", update_payload}});
        };

        render(Parent({}), scratch);
        REQUIRE(depth_child_render_calls == 1);

        depth_update_callback();
        flush();
        REQUIRE(depth_child_render_calls == 2);
    }
}
