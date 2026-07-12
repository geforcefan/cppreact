#include <cmath>
#include <functional>
#include <optional>
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

using namespace cppreact;
using namespace cppreact::tags;

static VNode reconcile_list(const std::vector<std::string>& values) {
    std::vector<VNode> items;
    for (const std::string& value : values) items.push_back(View({{"key", value}}, value));
    return View({}, std::move(items));
}

static std::string reconcile_list_html(const std::vector<std::string>& values) {
    std::string html = "<view>";
    for (const std::string& value : values) html += "<view>" + value + "</view>";
    return html + "</view>";
}

static VNode shuffled_list(const std::vector<std::string>& keys) {
    std::vector<VNode> items;
    for (const std::string& key : keys) items.push_back(View({{"key", key}}, key));
    return View({}, std::move(items));
}

static std::string shuffled_list_html(const std::vector<std::string>& keys) {
    std::string html = "<view>";
    for (const std::string& key : keys) html += "<view>" + key + "</view>";
    return html + "</view>";
}

static const FunctionComponent MixedFoo = [](const Object&) -> VNode { return "d"; };

static const FunctionComponent RenderedItem = [](const Object& props) -> VNode {
    bool render_as_null = props.get<bool>("render_as_null",false);
    if (render_as_null) return fragment();
    return View({}, props.get<std::string>("item_id",""));
};

static VNode shrink_list(const std::vector<std::pair<std::string, bool>>& items) {
    std::vector<VNode> nodes;
    for (const auto& [item_id, render_as_null] : items) {
        nodes.push_back(RenderedItem(
            {{"key", item_id}, {"item_id", item_id}, {"render_as_null", render_as_null}}));
    }
    return View({}, std::move(nodes));
}

static std::function<void()> update_reuse_x;
static std::function<void()> update_reuse_app;

static const FunctionComponent ReuseX = [](const Object&) -> VNode {
    auto [i, set_state] = use_state<int>(0);
    update_reuse_x = [set_state = set_state, i = i] { set_state(i + 1); };
    return View({}, i);
};

static const FunctionComponent ReuseApp = [](const Object&) -> VNode {
    auto [i, set_state] = use_state<int>(0);
    update_reuse_app = [set_state = set_state, i = i] { set_state(i ^ 1); };
    return View({}, when(i == 0, [] { return ReuseX({}); }), ReuseX({}));
};

static const FunctionComponent RetainX = [](const Object& props) -> VNode {
    std::string& name = use_ref<std::string>(props.get<std::string>("name",""));
    return Text({}, name);
};

static const FunctionComponent RetainFoo = [](const Object& props) -> VNode {
    bool condition = props.get<bool>("condition",false);
    if (condition) return View({}, Text({}), RetainX({{"name", std::string("B")}}));
    return View({}, RetainX({{"name", std::string("A")}}));
};

static int stale_render_count = 0;
static std::function<void()> update_stale_app;
static std::function<void()> update_stale_parent;

static const FunctionComponent StaleChild = [](const Object& props) -> VNode {
    int count = static_cast<int>(props.get<double>("count",0));
    return when(count >= 3, [] { return View({}, "foo"); });
};

static const FunctionComponent StaleParent = [](const Object&) -> VNode {
    auto [tick, set_tick] = use_state<int>(0);
    update_stale_parent = [set_tick = set_tick, tick = tick] { set_tick(tick + 1); };
    ++stale_render_count;
    return StaleChild({{"count", static_cast<double>(stale_render_count)}});
};

static const FunctionComponent StaleApp = [](const Object&) -> VNode {
    auto [tick, set_tick] = use_state<int>(0);
    update_stale_app = [set_tick = set_tick, tick = tick] { set_tick(tick + 1); };
    return StaleParent({});
};

static std::vector<std::string> falsy_actions;

static const FunctionComponent FalsyComponent = [](const Object& props) -> VNode {
    int index = static_cast<int>(props.get<double>("index",0));
    use_effect(
        [index]() -> CleanupFunction {
            falsy_actions.push_back("mounted " + std::to_string(index));
            return {};
        },
        {});
    return View({}, "Hello");
};

static const FunctionComponent FalsyApp = [](const Object& props) -> VNode {
    std::string y = props.get<std::string>("y","");
    VNode first = y == "1" ? FalsyComponent({{"index", 1.0}}) : View({});
    return View({}, std::move(first), fragment(), FalsyComponent({{"index", 2.0}}),
               FalsyComponent({{"index", 3.0}}));
};

TEST_CASE("render()") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("should rerender when value from \"\" to 0") {
        render("", scratch);
        REQUIRE(renderer.inner_html() == "");

        render(0, scratch);
        REQUIRE(renderer.inner_html() == "0");
    }

    SECTION("should render an empty text node given an empty string") {
        render("", scratch);

        DomNode empty_text = renderer.find_by_text("");
        REQUIRE(empty_text != null_dom_node);
        REQUIRE(renderer.parent_node(empty_text) == scratch.mount);
        REQUIRE(renderer.inner_html() == "");
    }

    SECTION("should allow node type change with content") {
        render(Text({}, "Bad"), scratch);
        render(View({}, "Good"), scratch);
        REQUIRE(renderer.inner_html() == "<view>Good</view>");
    }

    SECTION("should create empty nodes (<* />)") {
        render(View({}), scratch);
        REQUIRE(renderer.inner_html() == "<view></view>");

        hosts::HtmlStringHost renderer2;
        Container scratch2 = renderer2.create_container();
        render(Text({}), scratch2);
        REQUIRE(renderer2.inner_html() == "<text></text>");
    }

    SECTION("should support custom tag names") {
        render(h("foo"), scratch);
        REQUIRE(renderer.inner_html() == "<foo></foo>");

        hosts::HtmlStringHost renderer2;
        Container scratch2 = renderer2.create_container();
        render(h("x-bar"), scratch2);
        REQUIRE(renderer2.inner_html() == "<x-bar></x-bar>");
    }

    SECTION("should merge new elements when called multiple times") {
        render(View({}), scratch);
        REQUIRE(renderer.inner_html() == "<view></view>");

        render(Text({}), scratch);
        REQUIRE(renderer.inner_html() == "<text></text>");

        render(Text({{"class", "hello"}}, "Hello!"), scratch);
        REQUIRE(renderer.inner_html() == "<text class=\"hello\">Hello!</text>");
    }

    SECTION("should nest empty nodes") {
        render(View({}, Text({}), h("foo"), h("x-bar")), scratch);
        REQUIRE(renderer.inner_html() == "<view><text></text><foo></foo><x-bar></x-bar></view>");
    }

    SECTION("should render NaN as text content") {
        render(NAN, scratch);
        REQUIRE(renderer.inner_html() == "NaN");
    }

    SECTION("should render numbers (0) as text content") {
        render(0, scratch);
        REQUIRE(renderer.inner_html() == "0");
    }

    SECTION("should render numbers (42) as text content") {
        render(42, scratch);
        REQUIRE(renderer.inner_html() == "42");
    }

    SECTION("should render strings as text content") {
        render(std::string("Testing, huh! How is it going?"), scratch);
        REQUIRE(renderer.inner_html() == "Testing, huh! How is it going?");
    }

    SECTION("should render arrays of mixed elements") {
        std::vector<VNode> nested;
        nested.push_back("e");
        nested.push_back("f");

        std::vector<VNode> mixed;
        mixed.push_back(0);
        mixed.push_back("a");
        mixed.push_back("b");
        mixed.push_back(Text({}, "c"));
        mixed.push_back(MixedFoo({}));
        mixed.push_back(fragment());
        mixed.push_back(fragment());
        mixed.push_back(fragment());
        mixed.push_back(fragment(std::move(nested)));
        mixed.push_back(1);

        render(fragment(std::move(mixed)), scratch);
        REQUIRE(renderer.inner_html() == "0ab<text>c</text>def1");
    }

    SECTION("should apply string attributes") {
        render(View({{"foo", "bar"}, {"data-foo", "databar"}}), scratch);
        REQUIRE(renderer.inner_html() == "<view data-foo=\"databar\" foo=\"bar\"></view>");
    }

    SECTION("should apply class as String") {
        render(View({{"class", "foo"}}), scratch);
        REQUIRE(renderer.inner_html() == "<view class=\"foo\"></view>");
    }

    SECTION("should remove class attributes") {
        const FunctionComponent RemoveClassApp = [](const Object& props) -> VNode {
            std::optional<std::string> class_name = props.get<std::string>("class");
            Object div_props;
            if (class_name) div_props.set("class", *class_name);
            return View(div_props, Text({}, "Bye"));
        };

        render(RemoveClassApp({{"class", std::string("hi")}}), scratch);
        REQUIRE(renderer.inner_html() == "<view class=\"hi\"><text>Bye</text></view>");

        render(RemoveClassApp({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>Bye</text></view>");
    }

    SECTION("should reorder child pairs") {
        render(View({}, Text({}, "a"), Text({}, "b")), scratch);

        DomNode a = renderer.find_first("a");
        DomNode b = renderer.find_first("b");

        render(View({}, Text({}, "b"), Text({}, "a")), scratch);

        REQUIRE(renderer.find_first("a") == a);
        REQUIRE(renderer.find_first("b") == b);
    }

    SECTION("should not reuse unkeyed components") {
        render(ReuseApp({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><view>0</view><view>0</view></view>");

        update_reuse_x();
        update_reuse_app();
        flush();
        REQUIRE(renderer.inner_html() == "<view><view>1</view></view>");

        update_reuse_app();
        flush();
        REQUIRE(renderer.inner_html() == "<view><view>0</view><view>1</view></view>");
    }

    SECTION("should reconcile children in right order") {
        render(reconcile_list({"A", "B", "C", "D", "E"}), scratch);
        REQUIRE(renderer.inner_html() == reconcile_list_html({"A", "B", "C", "D", "E"}));

        render(reconcile_list({"B", "E", "C", "D"}), scratch);
        REQUIRE(renderer.inner_html() == reconcile_list_html({"B", "E", "C", "D"}));
    }

    SECTION("should reconcile children in right order #2") {
        render(reconcile_list({"A", "B", "C", "D", "E"}), scratch);
        REQUIRE(renderer.inner_html() == reconcile_list_html({"A", "B", "C", "D", "E"}));

        render(reconcile_list({"B", "E", "D", "C"}), scratch);
        REQUIRE(renderer.inner_html() == reconcile_list_html({"B", "E", "D", "C"}));
    }

    SECTION("should reconcile children in right order #3") {
        render(View({}, Text({}, "_A1"), Text({}, "_A2"), View({}, "_A3"), Text({}, "_A4"), View({}, "_A5"),
                     Text({}, "_A6"), View({}, "_A7"), Text({}, "_A8")),
               scratch);

        render(View({}, Text({}, "_B1"), Text({}, "_B2"), Text({}, "_B3"), View({}, "_B4"), Text({}, "_B5"),
                     Text({}, "_B6"), View({}, "_B7"), Text({}, "_B8")),
               scratch);

        REQUIRE(renderer.inner_html() ==
                "<view><text>_B1</text><text>_B2</text><text>_B3</text><view>_B4</view><text>_B5</text><text>_B6</text>"
                "<view>_B7</view><text>_B8</text></view>");
    }

    SECTION("should reconcile children in right order #4") {
        render(View({}, Text({}, "_A1"), Text({}, "_A2"), View({}, "_A3"), View({}, "_A4"), Text({}, "_A5"),
                     View({}, "_A6"), View({}, "_A7"), Text({}, "_A8"), View({}, "_A9"), View({}, "_A10"),
                     Text({}, "_A11"), View({}, "_A12")),
               scratch);

        render(View({}, Text({}, "_B1"), Text({}, "_B2"), Text({}, "_B3"), View({}, "_B4"), Text({}, "_B5"),
                     Text({}, "_B6"), Text({}, "_B7"), View({}, "_B8"), Text({}, "_B9"), Text({}, "_B10"),
                     Text({}, "_B11"), Text({}, "_B12"), View({}, "_B13")),
               scratch);

        REQUIRE(renderer.inner_html() ==
                "<view><text>_B1</text><text>_B2</text><text>_B3</text><view>_B4</view><text>_B5</text><text>_B6</text><text>_B7</text>"
                "<view>_B8</view><text>_B9</text><text>_B10</text><text>_B11</text><text>_B12</text><view>_B13</view></view>");
    }

    SECTION("should shrink lists") {
        render(shrink_list({{"One", false}, {"Two", false}, {"Three", false}, {"Four", false}}),
               scratch);
        REQUIRE(renderer.inner_html() ==
                "<view><view>One</view><view>Two</view><view>Three</view><view>Four</view></view>");

        render(shrink_list({{"One", false}, {"Four", true}, {"Six", false}, {"Seven", false}}),
               scratch);
        REQUIRE(renderer.inner_html() == "<view><view>One</view><view>Six</view><view>Seven</view></view>");
    }

    SECTION("handles shuffled child-ordering") {
        std::vector<std::string> a{"0", "1", "2", "3", "4", "5", "6"};
        std::vector<std::string> b{"1", "3", "5", "2", "6", "4", "0"};
        std::vector<std::string> c{"11", "3", "1", "4", "6", "2", "5", "0", "9", "10"};

        render(shuffled_list(a), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(a));

        render(shuffled_list(b), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(b));

        render(shuffled_list(c), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(c));

        render(shuffled_list(a), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(a));
    }

    SECTION("handle shuffled array children (moving to the front)") {
        std::vector<std::string> a{"0", "2", "7", "6", "1", "3", "5", "4"};
        std::vector<std::string> b{"1", "0", "6", "7", "5", "2", "4", "3"};
        std::vector<std::string> c{"0", "7", "2", "1", "3", "5", "6", "4"};

        render(shuffled_list(a), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(a));

        render(shuffled_list(b), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(b));

        render(shuffled_list(c), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(c));
    }

    SECTION("should retain state for inserted children") {
        render(RetainFoo({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>A</text></view>");

        render(RetainFoo({{"condition", true}}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text></text><text>A</text></view>");

        render(RetainFoo({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>A</text></view>");
    }

    SECTION("should not lead to stale DOM nodes") {
        stale_render_count = 0;

        render(StaleApp({}), scratch);

        update_stale_app();
        flush();
        update_stale_parent();
        flush();
        update_stale_app();
        flush();

        REQUIRE(renderer.inner_html() == "<view>foo</view>");
    }

    SECTION("should not remount components when replacing a component with a falsy value in-between") {
        falsy_actions.clear();

        render(FalsyApp({{"y", std::string("1")}}), scratch);
        REQUIRE(falsy_actions == std::vector<std::string>{"mounted 1", "mounted 2", "mounted 3"});

        render(FalsyApp({{"y", std::string("2")}}), scratch);
        REQUIRE(falsy_actions == std::vector<std::string>{"mounted 1", "mounted 2", "mounted 3"});
    }
}
