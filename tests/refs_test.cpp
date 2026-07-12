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

static hosts::HtmlStringHost* renderer_pointer = nullptr;
static std::vector<std::string> events;
static std::vector<std::string> calls;
static DomNode seen_node = null_dom_node;
static int ref_calls = 0;
static int ref2_calls = 0;
static DomNode ref2_node = null_dom_node;
static bool use_first = true;
static ReferenceObject seen_reference;
static DomNode current_ref = null_dom_node;
static StateSetter<bool> set_show;
static StateSetter<int> set_phase;
static ReferenceObject* reference_pointer = nullptr;
static FunctionComponent child_component = nullptr;
static int memo_render_count = 0;

static Value logging_ref(const std::string& name) {
    return Callback{[name](DomNode node) {
        if (node != null_dom_node) {
            calls.push_back("adding ref to " + name);
        } else {
            calls.push_back("removing ref from " + name);
        }
    }};
}

static Value tag_logging_ref() {
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

        render(View({{"ref", Callback{[](DomNode node) {
                    ++ref_calls;
                    seen_node = node;
                }}}}),
               scratch);

        REQUIRE(ref_calls == 1);
        REQUIRE(seen_node == renderer.find_first("view"));
    }

    SECTION("should support createRef") {
        ReferenceObject reference{};
        REQUIRE(reference.current() == null_dom_node);

        render(View({{"ref", reference}}), scratch);
        REQUIRE(reference.current() == renderer.find_first("view"));
    }

    SECTION("should not call stale refs") {
        ref_calls = 0;
        ref2_calls = 0;
        seen_node = null_dom_node;
        ref2_node = null_dom_node;
        use_first = true;

        const FunctionComponent App = [](const Object&) -> VNode {
            Value first = Callback{[](DomNode node) {
                ++ref_calls;
                seen_node = node;
            }};
            Value second = Callback{[](DomNode node) {
                ++ref2_calls;
                ref2_node = node;
            }};
            return View({{"ref", use_first ? std::move(first) : std::move(second)}});
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

    SECTION("should pass component refs in props") {
        seen_reference = ReferenceObject{};

        const FunctionComponent Foo = [](const Object& props) -> VNode {
            seen_reference = props.get<ReferenceObject>("ref",ReferenceObject{});
            return View({});
        };

        ReferenceObject passed_reference{};
        render(Foo({{"ref", passed_reference}}), scratch);

        REQUIRE(seen_reference == passed_reference);
    }

    SECTION("should have a consistent order") {
        events.clear();

        const FunctionComponent App = [](const Object&) -> VNode {
            return View({{"ref", tag_logging_ref()}},
                     Text({{"ref", tag_logging_ref()}}, "hi"));
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

        const FunctionComponent Child = [](const Object&) -> VNode {
            auto [show, set_state] = use_state<bool>(false);
            set_show = set_state;
            if (!show) return View({{"id", "div"}, {"ref", tag_logging_ref()}});
            return Text({{"id", "span"}, {"ref", tag_logging_ref()}}, "some test content");
        };
        static const FunctionComponent child = Child;

        const FunctionComponent App = [](const Object&) -> VNode {
            return View({}, child({}));
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

        Value ref = Callback{[](DomNode node) { current_ref = node; }};
        static Value* ref_pointer = nullptr;
        ref_pointer = &ref;

        const FunctionComponent App = [](const Object& props) -> VNode {
            bool open = props.get<bool>("open",false);
            if (open) {
                return View({{"class", "open"}, {"key", "open"}},
                         View({{"ref", *ref_pointer}}));
            }
            return View({{"class", "closed"}, {"key", "closed"}},
                     View({{"ref", *ref_pointer}}));
        };

        render(App({}), scratch);
        REQUIRE(current_ref != null_dom_node);

        render(App({{"open", true}}), scratch);
        REQUIRE(current_ref != null_dom_node);
    }

    SECTION("should correctly call child refs for un-keyed children on re-render") {
        current_ref = null_dom_node;

        const FunctionComponent App = [](const Object& props) -> VNode {
            bool header_visible = props.get<bool>("header_visible",false);
            return View({},
                     when(header_visible, [] { return View({}, "foo"); }),
                     View({{"ref", Callback{[](DomNode node) {
                          if (node != null_dom_node) current_ref = node;
                      }}}},
                       "bar"));
        };

        render(App({{"header_visible", true}}), scratch);
        REQUIRE(current_ref != null_dom_node);

        render(App({}), scratch);
        REQUIRE(current_ref != null_dom_node);
    }

    SECTION("should first clean-up refs and after apply them") {
        calls.clear();

        const FunctionComponent App = [](const Object&) -> VNode {
            auto [phase, set_state] = use_state<int>(1);
            set_phase = set_state;
            if (phase == 1) {
                return View({},
                         View({{"ref", logging_ref("two")}}, "Element two"),
                         View({{"ref", logging_ref("three")}}, "Element three"));
            }
            return View({{"class", "outer"}},
                     View({{"ref", logging_ref("one")}}, "Element one"),
                     View({{"class", "wrapper"}},
                       View({{"ref", logging_ref("two")}}, "Element two"),
                       View({{"ref", logging_ref("three")}}, "Element three")));
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

        const FunctionComponent App = [](const Object& props) -> VNode {
            return View({{"class", props.get<std::string>("class","")},
                        {"ref", *reference_pointer}});
        };

        render(App({{"class", "first"}}), scratch);
        REQUIRE(reference.current() == renderer.find_first("view"));

        *reference.dom = null_dom_node;
        render(App({{"class", "second"}}), scratch);
        REQUIRE(reference.current() == null_dom_node);
    }

    SECTION("should not re-invoke a stable callback ref on re-render") {
        ref_calls = 0;
        static Callback stable_reference{};
        stable_reference = Callback{[](DomNode) { ++ref_calls; }};

        const FunctionComponent App = [](const Object& props) -> VNode {
            return View({{"class", props.get<std::string>("class","")},
                        {"ref", stable_reference}});
        };

        render(App({{"class", "first"}}), scratch);
        REQUIRE(ref_calls == 1);

        render(App({{"class", "second"}}), scratch);
        REQUIRE(ref_calls == 1);
    }

    SECTION("should keep an object ref current when its previous holder unmounts") {
        ReferenceObject reference{};
        reference_pointer = &reference;

        const FunctionComponent App = [](const Object& props) -> VNode {
            bool moved = props.get<bool>("moved",false);
            if (!moved) return View({{"id", "holder"}, {"ref", *reference_pointer}});
            return portal(renderer_pointer->document(),
                          Text({{"id", "target"}, {"ref", *reference_pointer}}));
        };

        render(App({}), scratch);
        REQUIRE(reference.current() == renderer.find_first("view"));

        render(App({{"moved", true}}), scratch);
        REQUIRE(reference.current() != null_dom_node);
        REQUIRE(reference.current() == renderer.find_first("text"));
    }

    SECTION("should not remove refs for memoized components unkeyed") {
        memo_render_count = 0;
        ReferenceObject reference{};
        reference_pointer = &reference;

        const FunctionComponent Inner = [](const Object& props) -> VNode {
            ++memo_render_count;
            ReferenceObject target =
                props.get<ReferenceObject>("ref",ReferenceObject{});
            return View({{"id", "inner"}, {"ref", target}});
        };
        child_component = memo(Inner);

        const FunctionComponent App = [](const Object& props) -> VNode {
            return View({{"class", props.get<std::string>("class","")}},
                     child_component({{"ref", *reference_pointer}}));
        };

        render(App({{"class", "first"}}), scratch);
        REQUIRE(memo_render_count == 1);
        REQUIRE(reference.current() != null_dom_node);

        render(App({{"class", "second"}}), scratch);
        REQUIRE(memo_render_count == 1);
        REQUIRE(reference.current() != null_dom_node);
    }
}
