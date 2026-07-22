#include <functional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/hooks/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"
#include "cppreact/component/memo.hpp"

using namespace cppreact;
using namespace cppreact::tags;

namespace {

struct HarnessProps {};

struct GreetingProps {
    std::string words{};

    bool operator==(const GreetingProps&) const = default;
};

struct ItemProps {
    std::string name{};
    bool is_selected = false;
    std::string key{};

    bool operator==(const ItemProps&) const = default;
};

}

static int foo_calls = 0;
static int comparer_calls = 0;
static std::function<void()> update;
static StateSetter<std::string> set_selected;
static FunctionComponent<ItemProps> memoized_item = nullptr;

TEST_CASE("memo") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("equal props skip the render, changed props render") {
        foo_calls = 0;

        const FunctionComponent Greeting =
            memo(FunctionComponent{[](const GreetingProps& props) -> VNode {
                ++foo_calls;
                return Text({.children = {props.words}});
            }});

        const FunctionComponent App = [Greeting](const HarnessProps&) -> VNode {
            auto [version, set_version] = use_state<int>(0);
            update = [set_version = set_version, version = version] { set_version(version + 1); };
            return Greeting({.words = "Hello World"});
        };

        render(App({}), scratch);
        REQUIRE(foo_calls == 1);

        update();
        flush();

        REQUIRE(foo_calls == 1);
        REQUIRE(renderer.inner_html() == "<text>Hello World</text>");
    }

    SECTION("a custom comparer sees previous and next props") {
        comparer_calls = 0;
        std::string previous_words;
        std::string next_words;

        const FunctionComponent Greeting = memo(
            FunctionComponent{[](const GreetingProps& props) -> VNode {
                return Text({.children = {props.words}});
            }},
            [&comparer = comparer_calls, &previous_words, &next_words](
                const GreetingProps& previous, const GreetingProps& next) {
                ++comparer;
                previous_words = previous.words;
                next_words = next.words;
                return false;
            });

        const FunctionComponent App = [Greeting](const HarnessProps&) -> VNode {
            auto [version, set_version] = use_state<int>(0);
            update = [set_version = set_version, version = version] { set_version(version + 1); };
            return Greeting({.words = version == 0 ? "first" : "second"});
        };

        render(App({}), scratch);
        update();
        flush();

        REQUIRE(comparer_calls == 1);
        REQUIRE(previous_words == "first");
        REQUIRE(next_words == "second");
        REQUIRE(renderer.inner_html() == "<text>second</text>");
    }

    SECTION("should nest without errors") {
        const FunctionComponent App =
            memo(memo(FunctionComponent{[](const GreetingProps& props) -> VNode {
                return View({.children = {props.words}});
            }}));

        render(App({.words = "foo"}), scratch);
        REQUIRE(renderer.inner_html() == "<view>foo</view>");
    }

    SECTION("should not unnecessarily reorder children #2895") {
        memoized_item = memo(FunctionComponent{[](const ItemProps& props) -> VNode {
            EventCallback handle_click = [name = props.name](const Event&) { set_selected(name); };
            if (props.is_selected) {
                return View({.class_name = "selected",
                             .on_click = handle_click,
                             .children = {props.name}});
            }
            return View({.on_click = handle_click, .children = {props.name}});
        }});

        const FunctionComponent List = [](const HarnessProps&) -> VNode {
            auto [selected, set_state] = use_state<std::string>("");
            set_selected = set_state;
            std::vector<std::string> names{"A", "B", "C", "D"};
            std::vector<VNode> items;
            for (const std::string& name : names) {
                items.push_back(memoized_item({.name = name,
                                               .is_selected = name == selected,
                                               .key = name}));
            }
            return View({.children = std::move(items)});
        };

        render(List({}), scratch);
        REQUIRE(renderer.inner_html() ==
                "<view><view>A</view><view>B</view><view>C</view><view>D</view></view>");

        DomNode third = renderer.parent_node(renderer.find_by_text("C"));
        renderer.dispatch_event(third, "click");
        flush();

        REQUIRE(renderer.inner_html() ==
                "<view><view>A</view><view>B</view><view class=\"selected\">C</view><view>D</view></view>");
    }
}
