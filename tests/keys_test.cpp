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

struct StatefulProps {
    std::string name{};
    int position = 0;
    std::string key{};
};

struct ConditionProps {
    bool condition = false;
};

struct KeyedProps {
    bool keyed = false;
};

struct MovedProps {
    bool moved = false;
};

}

static std::vector<std::string> ops;
static int next_serial = 0;
static int seen_serial[3] = {0, 0, 0};

static VNode list(const std::vector<std::string>& values) {
    std::vector<VNode> items;
    for (const std::string& value : values) items.push_back(View({.key = value, .children = {value}}));
    return View({.children = std::move(items)});
}

static std::string list_html(const std::vector<std::string>& values) {
    std::string html = "<view>";
    for (const std::string& value : values) html += "<view>" + value + "</view>";
    return html + "</view>";
}

static long count_log(const std::vector<std::string>& log, const std::string& prefix,
                      std::size_t start) {
    long total = 0;
    for (std::size_t position = start; position < log.size(); ++position) {
        if (log[position].rfind(prefix, 0) == 0) ++total;
    }
    return total;
}

static const FunctionComponent Stateful = [](const StatefulProps& props) -> VNode {
    int& serial = use_ref<int>(0);
    if (serial == 0) serial = ++next_serial;
    seen_serial[props.position] = serial;
    use_effect(
        [name = props.name]() -> CleanupFunction {
            ops.push_back("Mount " + name);
            return [name] { ops.push_back("Unmount " + name); };
        },
        {});
    return View({.children = {props.name}});
};

TEST_CASE("keys") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("should update in-place keyed DOM nodes") {
        render(View({.children = {View({.key = "0", .children = {"a"}}),
                                  View({.key = "1", .children = {"b"}}),
                                  View({.key = "2", .children = {"c"}})}}),
               scratch);
        REQUIRE(renderer.inner_html() == "<view><view>a</view><view>b</view><view>c</view></view>");

        std::size_t start = renderer.log.size();
        render(View({.children = {View({.key = "0", .children = {"x"}}),
                                  View({.key = "1", .children = {"y"}}),
                                  View({.key = "2", .children = {"z"}})}}),
               scratch);
        REQUIRE(renderer.inner_html() == "<view><view>x</view><view>y</view><view>z</view></view>");
        REQUIRE(count_log(renderer.log, "create", start) == 0);
    }

    SECTION("should remove orphaned keyed nodes") {
        render(View({.children = {View({.children = {1}}),
                                  View({.key = "a", .children = {"a"}}),
                                  View({.key = "b", .children = {"b"}})}}),
               scratch);

        render(View({.children = {View({.children = {2}}),
                                  View({.key = "b", .children = {"b"}}),
                                  View({.key = "c", .children = {"c"}})}}),
               scratch);

        REQUIRE(renderer.inner_html() == "<view><view>2</view><view>b</view><view>c</view></view>");
    }

    SECTION("should append new keyed elements") {
        std::vector<std::string> values{"a", "b"};

        render(list(values), scratch);
        REQUIRE(renderer.inner_html() == list_html(values));

        values.push_back("c");
        std::size_t start = renderer.log.size();

        render(list(values), scratch);
        REQUIRE(renderer.inner_html() == list_html(values));
        REQUIRE(count_log(renderer.log, "create element", start) == 1);
        REQUIRE(count_log(renderer.log, "remove", start) == 0);
    }

    SECTION("should remove keyed elements from the end") {
        std::vector<std::string> values{"a", "b", "c", "d"};

        render(list(values), scratch);
        REQUIRE(renderer.inner_html() == list_html(values));

        values.pop_back();
        std::size_t start = renderer.log.size();

        render(list(values), scratch);
        REQUIRE(renderer.inner_html() == list_html(values));
        REQUIRE(count_log(renderer.log, "remove", start) == 1);
        REQUIRE(count_log(renderer.log, "create", start) == 0);
    }

    SECTION("should prepend keyed elements to the beginning") {
        std::vector<std::string> values{"b", "c"};

        render(list(values), scratch);
        REQUIRE(renderer.inner_html() == list_html(values));

        values.insert(values.begin(), "a");
        std::size_t start = renderer.log.size();

        render(list(values), scratch);
        REQUIRE(renderer.inner_html() == list_html(values));
        REQUIRE(count_log(renderer.log, "create element", start) == 1);
    }

    SECTION("should remove keyed elements from the beginning") {
        std::vector<std::string> values{"z", "a", "b", "c"};

        render(list(values), scratch);
        REQUIRE(renderer.inner_html() == list_html(values));

        values.erase(values.begin());
        std::size_t start = renderer.log.size();

        render(list(values), scratch);
        REQUIRE(renderer.inner_html() == list_html(values));
        REQUIRE(count_log(renderer.log, "remove", start) == 1);
        REQUIRE(count_log(renderer.log, "create", start) == 0);
    }

    SECTION("should insert new keyed children in the middle") {
        std::vector<std::string> values{"a", "c"};

        render(list(values), scratch);
        REQUIRE(renderer.inner_html() == list_html(values));

        values.insert(values.begin() + 1, "b");
        std::size_t start = renderer.log.size();

        render(list(values), scratch);
        REQUIRE(renderer.inner_html() == list_html(values));
        REQUIRE(count_log(renderer.log, "create element", start) == 1);
    }

    SECTION("should remove keyed children from the middle") {
        std::vector<std::string> values{"a", "b", "x", "y", "z", "c", "d"};

        render(list(values), scratch);
        REQUIRE(renderer.inner_html() == list_html(values));

        values.erase(values.begin() + 2, values.begin() + 5);
        std::size_t start = renderer.log.size();

        render(list(values), scratch);
        REQUIRE(renderer.inner_html() == list_html(values));
        REQUIRE(count_log(renderer.log, "remove", start) == 3);
        REQUIRE(count_log(renderer.log, "create", start) == 0);
    }

    SECTION("should move keyed children to the beginning") {
        render(list({"b", "c", "d", "a"}), scratch);
        std::size_t start = renderer.log.size();

        render(list({"a", "b", "c", "d"}), scratch);
        REQUIRE(renderer.inner_html() == list_html({"a", "b", "c", "d"}));
        REQUIRE(count_log(renderer.log, "create", start) == 0);
        REQUIRE(count_log(renderer.log, "remove", start) == 0);
    }

    SECTION("should move multiple keyed children to the beginning") {
        render(list({"c", "d", "e", "a", "b"}), scratch);
        std::size_t start = renderer.log.size();

        render(list({"a", "b", "c", "d", "e"}), scratch);
        REQUIRE(renderer.inner_html() == list_html({"a", "b", "c", "d", "e"}));
        REQUIRE(count_log(renderer.log, "create", start) == 0);
        REQUIRE(count_log(renderer.log, "remove", start) == 0);
    }

    SECTION("should swap keyed children efficiently") {
        render(list({"a", "b"}), scratch);
        std::size_t start = renderer.log.size();

        render(list({"b", "a"}), scratch);
        REQUIRE(renderer.inner_html() == list_html({"b", "a"}));
        REQUIRE(count_log(renderer.log, "create", start) == 0);
        REQUIRE(count_log(renderer.log, "remove", start) == 0);
    }

    SECTION("should swap existing keyed children in the middle of a list efficiently") {
        render(list({"a", "b", "c", "d"}), scratch);
        std::size_t start = renderer.log.size();

        render(list({"a", "c", "b", "d"}), scratch);
        REQUIRE(renderer.inner_html() == list_html({"a", "c", "b", "d"}));
        REQUIRE(count_log(renderer.log, "create", start) == 0);

        start = renderer.log.size();
        render(list({"a", "b", "c", "d"}), scratch);
        REQUIRE(renderer.inner_html() == list_html({"a", "b", "c", "d"}));
        REQUIRE(count_log(renderer.log, "create", start) == 0);
    }

    SECTION("should move keyed children to the end of the list") {
        render(list({"a", "b", "c", "d"}), scratch);
        std::size_t start = renderer.log.size();

        render(list({"b", "c", "d", "a"}), scratch);
        REQUIRE(renderer.inner_html() == list_html({"b", "c", "d", "a"}));
        REQUIRE(count_log(renderer.log, "create", start) == 0);

        start = renderer.log.size();
        render(list({"a", "b", "c", "d"}), scratch);
        REQUIRE(renderer.inner_html() == list_html({"a", "b", "c", "d"}));
        REQUIRE(count_log(renderer.log, "create", start) == 0);
    }

    SECTION("should move keyed children to the beginning on longer list") {
        render(list({"a", "b", "c", "d", "e", "f"}), scratch);
        std::size_t start = renderer.log.size();

        render(list({"a", "e", "b", "c", "d", "f"}), scratch);
        REQUIRE(renderer.inner_html() == list_html({"a", "e", "b", "c", "d", "f"}));
        REQUIRE(count_log(renderer.log, "create", start) == 0);
    }

    SECTION("should move keyed children to the end on longer list") {
        render(list({"a", "b", "c", "d", "e", "f"}), scratch);
        std::size_t start = renderer.log.size();

        render(list({"a", "c", "d", "e", "b", "f"}), scratch);
        REQUIRE(renderer.inner_html() == list_html({"a", "c", "d", "e", "b", "f"}));
        REQUIRE(count_log(renderer.log, "create", start) == 0);
    }

    SECTION("should reverse keyed children effectively") {
        std::vector<std::string> values{"a", "b", "c", "d", "e", "f", "g", "h", "i", "j"};
        render(list(values), scratch);
        std::size_t start = renderer.log.size();

        std::vector<std::string> reversed(values.rbegin(), values.rend());
        render(list(reversed), scratch);
        REQUIRE(renderer.inner_html() == list_html(reversed));
        REQUIRE(count_log(renderer.log, "create", start) == 0);
        REQUIRE(count_log(renderer.log, "remove", start) == 0);
    }

    SECTION("should not preserve state when a component's keys are different") {
        ops.clear();
        next_serial = 0;

        const FunctionComponent Foo = [](const ConditionProps& props) -> VNode {
            if (props.condition) return Stateful({.name = "Stateful", .key = "a"});
            return Stateful({.name = "Stateful", .key = "b"});
        };

        render(Foo({.condition = true}), scratch);
        REQUIRE(renderer.inner_html() == "<view>Stateful</view>");
        REQUIRE(ops == std::vector<std::string>{"Mount Stateful"});

        ops.clear();
        render(Foo({.condition = false}), scratch);
        REQUIRE(renderer.inner_html() == "<view>Stateful</view>");
        REQUIRE(ops == std::vector<std::string>{"Unmount Stateful", "Mount Stateful"});

        ops.clear();
        render(Foo({.condition = true}), scratch);
        REQUIRE(renderer.inner_html() == "<view>Stateful</view>");
        REQUIRE(ops == std::vector<std::string>{"Unmount Stateful", "Mount Stateful"});
    }

    SECTION("should not preserve state between an unkeyed and keyed component") {
        ops.clear();
        next_serial = 0;

        const FunctionComponent Foo = [](const KeyedProps& props) -> VNode {
            if (props.keyed) return Stateful({.name = "Stateful", .key = "a"});
            return Stateful({.name = "Stateful"});
        };

        render(Foo({.keyed = true}), scratch);
        REQUIRE(renderer.inner_html() == "<view>Stateful</view>");
        REQUIRE(ops == std::vector<std::string>{"Mount Stateful"});

        ops.clear();
        render(Foo({.keyed = false}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Unmount Stateful", "Mount Stateful"});

        ops.clear();
        render(Foo({.keyed = true}), scratch);
        REQUIRE(ops == std::vector<std::string>{"Unmount Stateful", "Mount Stateful"});
    }

    SECTION("should not preserve state when keys change with multiple children") {
        ops.clear();
        next_serial = 0;

        const FunctionComponent Foo = [](const MovedProps& props) -> VNode {
            if (props.moved) {
                return View({.children = {View({.children = {1}}),
                                          Stateful({.name = "Stateful1", .position = 1, .key = "c"}),
                                          View({.children = {2}}),
                                          Stateful({.name = "Stateful2", .position = 2, .key = "d"})}});
            }
            return View({.children = {View({.children = {1}}),
                                      Stateful({.name = "Stateful1", .position = 1, .key = "a"}),
                                      View({.children = {2}}),
                                      Stateful({.name = "Stateful2", .position = 2, .key = "b"})}});
        };

        const std::string expected_html =
            "<view><view>1</view><view>Stateful1</view><view>2</view><view>Stateful2</view></view>";

        render(Foo({.moved = false}), scratch);
        REQUIRE(renderer.inner_html() == expected_html);
        REQUIRE(ops == std::vector<std::string>{"Mount Stateful1", "Mount Stateful2"});
        int first_serial1 = seen_serial[1];
        int first_serial2 = seen_serial[2];

        ops.clear();
        render(Foo({.moved = true}), scratch);
        REQUIRE(renderer.inner_html() == expected_html);
        REQUIRE(ops == std::vector<std::string>{"Unmount Stateful1", "Unmount Stateful2",
                                                "Mount Stateful1", "Mount Stateful2"});
        REQUIRE(seen_serial[1] != first_serial1);
        REQUIRE(seen_serial[2] != first_serial2);
    }

    SECTION("should preserve state when moving keyed children components") {
        ops.clear();
        next_serial = 0;

        const FunctionComponent Foo = [](const MovedProps& props) -> VNode {
            if (props.moved) {
                return View({.children = {View({.children = {1}}),
                                          Stateful({.name = "Stateful2", .position = 2, .key = "b"}),
                                          View({.children = {2}}),
                                          Stateful({.name = "Stateful1", .position = 1, .key = "a"})}});
            }
            return View({.children = {View({.children = {1}}),
                                      Stateful({.name = "Stateful1", .position = 1, .key = "a"}),
                                      View({.children = {2}}),
                                      Stateful({.name = "Stateful2", .position = 2, .key = "b"})}});
        };

        const std::string html_for_false =
            "<view><view>1</view><view>Stateful1</view><view>2</view><view>Stateful2</view></view>";
        const std::string html_for_true =
            "<view><view>1</view><view>Stateful2</view><view>2</view><view>Stateful1</view></view>";

        render(Foo({.moved = false}), scratch);
        REQUIRE(renderer.inner_html() == html_for_false);
        REQUIRE(ops == std::vector<std::string>{"Mount Stateful1", "Mount Stateful2"});
        int first_serial1 = seen_serial[1];
        int first_serial2 = seen_serial[2];

        ops.clear();
        render(Foo({.moved = true}), scratch);
        REQUIRE(renderer.inner_html() == html_for_true);
        REQUIRE(ops == std::vector<std::string>{});
        REQUIRE(seen_serial[1] == first_serial1);
        REQUIRE(seen_serial[2] == first_serial2);

        ops.clear();
        render(Foo({.moved = false}), scratch);
        REQUIRE(renderer.inner_html() == html_for_false);
        REQUIRE(ops == std::vector<std::string>{});
        REQUIRE(seen_serial[1] == first_serial1);
        REQUIRE(seen_serial[2] == first_serial2);
    }
}
