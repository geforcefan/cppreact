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

static int foo_calls = 0;
static int comparer_calls = 0;
static bool comparer_saw_empty = false;
static bool previous_foo = false;
static bool next_foo = true;
static std::function<void()> update;
static FunctionComponent memoized = nullptr;
static StateSetter<std::string> set_selected;

TEST_CASE("memo") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("should work with function components") {
        foo_calls = 0;

        memoized = memo([](const Object&) -> VNode {
            ++foo_calls;
            return Text({}, "Hello World");
        });

        const FunctionComponent App = [](const Object&) -> VNode {
            auto [version, set_version] = use_state<int>(0);
            update = [set_version = set_version, version = version] { set_version(version + 1); };
            return memoized({});
        };

        render(App({}), scratch);
        REQUIRE(foo_calls == 1);

        update();
        flush();

        REQUIRE(foo_calls == 1);
    }

    SECTION("should support custom comparer functions") {
        comparer_calls = 0;
        comparer_saw_empty = false;

        memoized = memo([](const Object&) -> VNode { return Text({}, "Hello World"); },
                        [](const Object& previous, const Object& next) {
                            ++comparer_calls;
                            comparer_saw_empty = previous.size() == 0 && next.size() == 0;
                            return true;
                        });

        const FunctionComponent App = [](const Object&) -> VNode {
            auto [version, set_version] = use_state<int>(0);
            update = [set_version = set_version, version = version] { set_version(version + 1); };
            return memoized({});
        };

        render(App({}), scratch);

        update();
        flush();

        REQUIRE(comparer_calls == 1);
        REQUIRE(comparer_saw_empty);
    }

    SECTION("should rerender when custom comparer returns false") {
        foo_calls = 0;

        const FunctionComponent App = memo(
            [](const Object&) -> VNode {
                ++foo_calls;
                return Text({}, "Hello World");
            },
            [](const Object&, const Object&) { return false; });

        render(App({}), scratch);
        REQUIRE(foo_calls == 1);

        render(App({{"foo", "bar"}}), scratch);
        REQUIRE(foo_calls == 2);
    }

    SECTION("should pass props and nextProps to the comparer") {
        previous_foo = false;
        next_foo = true;

        const FunctionComponent App = memo(
            [](const Object&) -> VNode { return View({}, "foo"); },
            [](const Object& previous, const Object& next) {
                previous_foo = previous.get<bool>("foo",false);
                next_foo = next.get<bool>("foo",false);
                return false;
            });

        render(App({{"foo", true}}), scratch);
        render(App({{"foo", false}}), scratch);

        REQUIRE(previous_foo == true);
        REQUIRE(next_foo == false);
    }

    SECTION("should nest without errors") {
        const FunctionComponent App =
            memo(memo([](const Object&) -> VNode { return View({}, "foo"); }));

        render(App({}), scratch);
        REQUIRE(renderer.inner_html() == "<view>foo</view>");
    }

    SECTION("should not unnecessarily reorder children #2895") {
        memoized = memo([](const Object& props) -> VNode {
            std::string name = props.get<std::string>("name","");
            bool is_selected = props.get<bool>("is_selected",false);
            Callback handle_click{[name](const Event&) { set_selected(name); }};
            if (is_selected) {
                return View({{"class", "selected"}, {"on_click", handle_click}}, name);
            }
            return View({{"on_click", handle_click}}, name);
        });

        const FunctionComponent List = [](const Object&) -> VNode {
            auto [selected, set_state] = use_state<std::string>("");
            set_selected = set_state;
            std::vector<std::string> names{"A", "B", "C", "D"};
            std::vector<VNode> items;
            for (const std::string& name : names) {
                items.push_back(memoized({{"key", name},
                                          {"name", name},
                                          {"is_selected", name == selected}}));
            }
            return View({}, std::move(items));
        };

        render(List({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><view>A</view><view>B</view><view>C</view><view>D</view></view>");

        DomNode third = renderer.parent_node(renderer.find_by_text("C"));
        renderer.dispatch_event(third, "click");
        flush();

        REQUIRE(renderer.inner_html() ==
                "<view><view>A</view><view>B</view><view class=\"selected\">C</view><view>D</view></view>");
    }
}
