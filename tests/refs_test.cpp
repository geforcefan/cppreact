#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/hooks/hooks.hpp"
#include "cppreact/component/portals.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"
#include "cppreact/vnode/helpers.hpp"
#include "cppreact/component/memo.hpp"

using namespace cppreact;
using namespace cppreact::tags;

namespace {

struct HarnessProps {};

struct OpenProps {
    bool open = false;
};

struct HeaderVisibleProps {
    bool header_visible = false;
};

struct ClassProps {
    std::string class_name{};
};

struct MovedProps {
    bool moved = false;
};

struct InnerProps {
    Reference ref{};

    bool operator==(const InnerProps&) const = default;
};

}

static hosts::HtmlStringHost* renderer_pointer = nullptr;
static std::vector<std::string> events;
static std::vector<std::string> calls;
static DomNode seen_node = null_dom_node;
static int ref_calls = 0;
static int ref2_calls = 0;
static DomNode ref2_node = null_dom_node;
static bool use_first = true;
static DomNode current_ref = null_dom_node;
static StateSetter<bool> set_show;
static StateSetter<int> set_phase;
static ReferenceObject* reference_pointer = nullptr;
static FunctionComponent<InnerProps> child_component = nullptr;
static int memo_render_count = 0;

static Callback logging_ref(const std::string& name) {
    return Callback{[name](DomNode node) {
        if (node != null_dom_node) {
            calls.push_back("adding ref to " + name);
        } else {
            calls.push_back("removing ref from " + name);
        }
    }};
}

static Callback tag_logging_ref() {
    return Callback{[](DomNode node) {
        events.push_back("called with " +
                         (node != null_dom_node ? renderer_pointer->tag_name(node) : "null"));
    }};
}

TEST_CASE("refs") {
    hosts::HtmlStringHost renderer;
    renderer_pointer = &renderer;
    Container scratch = renderer.create_container();

    SECTION("should invoke refs in render") {
        ref_calls = 0;
        seen_node = null_dom_node;

        render(View({.ref = Callback{[](DomNode node) {
                    ++ref_calls;
                    seen_node = node;
                }}}),
               scratch);

        REQUIRE(ref_calls == 1);
        REQUIRE(seen_node == renderer.find_first("view"));
    }

    SECTION("should support createRef") {
        ReferenceObject reference{};
        REQUIRE(reference.current() == null_dom_node);

        render(View({.ref = reference}), scratch);
        REQUIRE(reference.current() == renderer.find_first("view"));
    }

    SECTION("should not call stale refs") {
        ref_calls = 0;
        ref2_calls = 0;
        seen_node = null_dom_node;
        ref2_node = null_dom_node;
        use_first = true;

        const FunctionComponent App = [](const HarnessProps&) -> VNode {
            Callback first = Callback{[](DomNode node) {
                ++ref_calls;
                seen_node = node;
            }};
            Callback second = Callback{[](DomNode node) {
                ++ref2_calls;
                ref2_node = node;
            }};
            return View({.ref = use_first ? std::move(first) : std::move(second)});
        };

        render(App({}), scratch);
        REQUIRE(ref_calls == 1);
        REQUIRE(seen_node == renderer.find_first("view"));

        use_first = false;
        render(App({}), scratch);
        REQUIRE(ref_calls == 2);
        REQUIRE(seen_node == null_dom_node);
        REQUIRE(ref2_calls == 1);
        REQUIRE(ref2_node == renderer.find_first("view"));
    }

    SECTION("should have a consistent order") {
        events.clear();

        const FunctionComponent App = [](const HarnessProps&) -> VNode {
            return View({.ref = tag_logging_ref(),
                         .children = {Text({.ref = tag_logging_ref(), .children = {"hi"}})}});
        };

        render(App({}), scratch);
        render(App({}), scratch);

        REQUIRE(events == std::vector<std::string>{
                              "called with text",
                              "called with view",
                              "called with null",
                              "called with null",
                              "called with text",
                              "called with view"});
    }

    SECTION("should null and re-invoke refs when swapping component root element type") {
        events.clear();

        const FunctionComponent Child = [](const HarnessProps&) -> VNode {
            auto [show, set_state] = use_state<bool>(false);
            set_show = set_state;
            if (!show) return View({.ref = tag_logging_ref()});
            return Text({.ref = tag_logging_ref(), .children = {"some test content"}});
        };
        static const FunctionComponent child = Child;

        const FunctionComponent App = [](const HarnessProps&) -> VNode {
            return View({.children = {child({})}});
        };

        render(App({}), scratch);
        REQUIRE(events == std::vector<std::string>{"called with view"});
        events.clear();

        set_show(true);
        flush();
        REQUIRE(events == std::vector<std::string>{"called with null", "called with text"});
        events.clear();

        set_show(false);
        flush();
        REQUIRE(events == std::vector<std::string>{"called with null", "called with view"});
    }

    SECTION("should correctly set nested child refs") {
        current_ref = null_dom_node;

        Callback reference = Callback{[](DomNode node) { current_ref = node; }};
        static Callback* reference_holder = nullptr;
        reference_holder = &reference;

        const FunctionComponent App = [](const OpenProps& props) -> VNode {
            if (props.open) {
                return View({.class_name = "open",
                             .key = "open",
                             .children = {View({.ref = *reference_holder})}});
            }
            return View({.class_name = "closed",
                         .key = "closed",
                         .children = {View({.ref = *reference_holder})}});
        };

        render(App({}), scratch);
        REQUIRE(current_ref != null_dom_node);

        render(App({.open = true}), scratch);
        REQUIRE(current_ref != null_dom_node);
    }

    SECTION("should correctly call child refs for un-keyed children on re-render") {
        current_ref = null_dom_node;

        const FunctionComponent App = [](const HeaderVisibleProps& props) -> VNode {
            return View({.children = {
                             when(props.header_visible, [] { return View({.children = {"foo"}}); }),
                             View({.ref = Callback{[](DomNode node) {
                                       if (node != null_dom_node) current_ref = node;
                                   }},
                                   .children = {"bar"}})}});
        };

        render(App({.header_visible = true}), scratch);
        REQUIRE(current_ref != null_dom_node);

        render(App({}), scratch);
        REQUIRE(current_ref != null_dom_node);
    }

    SECTION("should first clean-up refs and after apply them") {
        calls.clear();

        const FunctionComponent App = [](const HarnessProps&) -> VNode {
            auto [phase, set_state] = use_state<int>(1);
            set_phase = set_state;
            if (phase == 1) {
                return View({.children = {
                                 View({.ref = logging_ref("two"), .children = {"Element two"}}),
                                 View({.ref = logging_ref("three"),
                                       .children = {"Element three"}})}});
            }
            return View({.class_name = "outer",
                         .children = {
                             View({.ref = logging_ref("one"), .children = {"Element one"}}),
                             View({.class_name = "wrapper",
                                   .children = {View({.ref = logging_ref("two"),
                                                      .children = {"Element two"}}),
                                                View({.ref = logging_ref("three"),
                                                      .children = {"Element three"}})}})}});
        };

        render(App({}), scratch);
        REQUIRE(calls == std::vector<std::string>{"adding ref to two", "adding ref to three"});
        calls.clear();

        set_phase(2);
        flush();
        REQUIRE(calls == std::vector<std::string>{"removing ref from two", "adding ref to one",
                                                  "adding ref to two", "adding ref to three"});
    }

    SECTION("should not re-invoke a stable object ref on re-render") {
        ReferenceObject reference{};
        reference_pointer = &reference;

        const FunctionComponent App = [](const ClassProps& props) -> VNode {
            return View({.class_name = props.class_name, .ref = *reference_pointer});
        };

        render(App({.class_name = "first"}), scratch);
        REQUIRE(reference.current() == renderer.find_first("view"));

        *reference.dom = null_dom_node;
        render(App({.class_name = "second"}), scratch);
        REQUIRE(reference.current() == null_dom_node);
    }

    SECTION("should not re-invoke a stable callback ref on re-render") {
        ref_calls = 0;
        static Callback stable_reference{};
        stable_reference = Callback{[](DomNode) { ++ref_calls; }};

        const FunctionComponent App = [](const ClassProps& props) -> VNode {
            return View({.class_name = props.class_name, .ref = stable_reference});
        };

        render(App({.class_name = "first"}), scratch);
        REQUIRE(ref_calls == 1);

        render(App({.class_name = "second"}), scratch);
        REQUIRE(ref_calls == 1);
    }

    SECTION("should keep an object ref current when its previous holder unmounts") {
        ReferenceObject reference{};
        reference_pointer = &reference;

        const FunctionComponent App = [](const MovedProps& props) -> VNode {
            if (!props.moved) return View({.ref = *reference_pointer});
            return Portal({.container = renderer_pointer->document(),
                           .children = {Text({.ref = *reference_pointer})}});
        };

        render(App({}), scratch);
        REQUIRE(reference.current() == renderer.find_first("view"));

        render(App({.moved = true}), scratch);
        REQUIRE(reference.current() != null_dom_node);
        REQUIRE(reference.current() == renderer.find_first("text"));
    }

    SECTION("should not remove refs for memoized components unkeyed") {
        memo_render_count = 0;
        ReferenceObject reference{};
        reference_pointer = &reference;

        const FunctionComponent Inner = [](const InnerProps& props) -> VNode {
            ++memo_render_count;
            return View({.ref = props.ref});
        };
        child_component = memo(FunctionComponent{Inner});

        const FunctionComponent App = [](const ClassProps& props) -> VNode {
            return View({.class_name = props.class_name,
                         .children = {child_component({.ref = *reference_pointer})}});
        };

        render(App({.class_name = "first"}), scratch);
        REQUIRE(memo_render_count == 1);
        REQUIRE(reference.current() != null_dom_node);

        render(App({.class_name = "second"}), scratch);
        REQUIRE(memo_render_count == 1);
        REQUIRE(reference.current() != null_dom_node);
    }
}
