#include <functional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/context/create_context.hpp"
#include "cppreact/hooks/hooks.hpp"
#include "cppreact/component/portals.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"
#include "cppreact/vnode/helpers.hpp"

using namespace cppreact;
using namespace cppreact::tags;

namespace {

struct HarnessProps {};

struct ChildrenProps {
    Children children{};
};

struct ModalOpenProps {
    bool open = false;
    Children children{};
};

struct ShowProps {
    bool show = false;
};

struct IndexProps {
    std::string index{};
};

struct ComplexChildProps {
    std::string index{};
    bool is_portal = false;
};

struct ComplexParentProps {
    bool is_portal = false;
    Children children{};
};

}

static DomNode portal_target = null_dom_node;
static std::function<void()> set_false;
static std::function<void()> toggle;
static std::function<void()> bump;
static StateSetter<std::string> set;
static int unmount_calls = 0;
static std::string document_key;
static std::function<void()> toggle_second;
static StateSetter<DomNode> set_container;
static StateSetter<bool> set_visible;
static ReferenceObject mounted_element_reference;
static int child_mount_calls = 0;
static int parent_mount_calls = 0;
static int render_counter = 0;
static DomNode secondary_scratch = null_dom_node;
static std::vector<std::string> effect_call_order;

TEST_CASE("createPortal") {
    hosts::HtmlStringHost renderer;
    portal_target = renderer.create_element("layer");
    renderer.insert_before(renderer.document(), portal_target, null_dom_node);
    Container scratch = renderer.create_container();

    SECTION("should render into a different root node") {
        const FunctionComponent Foo = [](const ChildrenProps& props) -> VNode {
            return View({.children = {
                            Portal({.container = portal_target, .children = props.children})}});
        };

        render(Foo({.children = {"foobar"}}), scratch);

        REQUIRE(renderer.inner_html(portal_target) == "foobar");
    }

    SECTION("should insert the portal") {
        const FunctionComponent Foo = [](const ChildrenProps& props) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(true);
            set_false = [set_mounted] { set_mounted(false); };
            return View({.children = {
                            Text({.children = {"Hello"}}),
                            when(mounted, [&props] {
                                return Portal(
                                    {.container = portal_target, .children = props.children});
                            })}});
        };

        render(Foo({.children = {"foobar"}}), scratch);
        REQUIRE(renderer.inner_html(portal_target) == "foobar");

        set_false();
        flush();
        REQUIRE(renderer.inner_html(portal_target) == "");
    }

    SECTION("should order portal children well") {
        const FunctionComponent Modal = [](const HarnessProps&) -> VNode {
            auto [state, set_state] = use_state<int>(0);
            bump = [set_state] { set_state(1); };
            return fragment(when(state == 1, [] { return View({.children = {"top"}}); }),
                            View({.children = {"middle"}}),
                            View({.children = {"bottom"}}));
        };

        const FunctionComponent Foo = [Modal](const HarnessProps&) -> VNode {
            return Portal({.container = portal_target, .children = {Modal({})}});
        };

        render(Foo({}), scratch);
        REQUIRE(renderer.inner_html(portal_target) == "<view>middle</view><view>bottom</view>");

        bump();
        flush();
        REQUIRE(renderer.inner_html(portal_target) ==
                "<view>top</view><view>middle</view><view>bottom</view>");
    }

    SECTION("should toggle the portal") {
        const FunctionComponent Foo = [](const ChildrenProps& props) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(true);
            toggle = [set_mounted] {
                set_mounted(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({.children = {
                            Text({.children = {"Hello"}}),
                            when(mounted, [&props] {
                                return Portal(
                                    {.container = portal_target, .children = props.children});
                            })}});
        };

        render(Foo({.children = {View({.children = {"foobar"}})}}), scratch);
        REQUIRE(renderer.inner_html(portal_target) == "<view>foobar</view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html(portal_target) == "");

        toggle();
        flush();
        REQUIRE(renderer.inner_html(portal_target) == "<view>foobar</view>");
    }

    SECTION("should notice prop changes on the portal") {
        const FunctionComponent Foo = [](const HarnessProps&) -> VNode {
            auto [extra, set_extra] = use_state<std::string>("red");
            set = set_extra;
            VNode content = extra.empty() ? Text({.children = {"Foo"}})
                                          : Text({.class_name = extra, .children = {"Foo"}});
            return View({.children = {
                            Text({.children = {"Hello"}}),
                            Portal({.container = portal_target,
                                    .children = {std::move(content)}})}});
        };

        render(Foo({}), scratch);
        REQUIRE(renderer.inner_html(portal_target) == "<text class=\"red\">Foo</text>");

        set("");
        flush();
        REQUIRE(renderer.inner_html(portal_target) == "<text>Foo</text>");
    }

    SECTION("should not unmount the portal component") {
        unmount_calls = 0;

        const FunctionComponent Child = [](const ChildrenProps& props) -> VNode {
            use_effect([]() -> CleanupFunction { return [] { ++unmount_calls; }; }, {});
            return props.children;
        };

        const FunctionComponent Foo = [Child](const HarnessProps&) -> VNode {
            auto [extra, set_extra] = use_state<std::string>("red");
            set = set_extra;
            return View({.children = {
                            Text({.children = {"Hello"}}),
                            Portal({.container = portal_target,
                                    .children = {Child({.children = {extra}})}})}});
        };

        render(Foo({}), scratch);
        REQUIRE(unmount_calls == 0);

        set("");
        flush();
        REQUIRE(unmount_calls == 0);
    }

    SECTION("should not render <undefined> for Portal nodes") {
        DomNode root = renderer.create_element("div");
        renderer.insert_before(renderer.document(), root, null_dom_node);

        const FunctionComponent Dialog = [](const HarnessProps&) -> VNode {
            return View({.children = {"Dialog content"}});
        };

        const FunctionComponent App = [Dialog](const HarnessProps&) -> VNode {
            return View({.children = {
                            Portal({.container = portal_target, .children = {Dialog({})}})}});
        };

        Container app_container{.host = &renderer, .mount = root};
        render(App({}), app_container);

        REQUIRE(renderer.inner_html(root) == "<view></view>");
    }

    SECTION("should unmount Portal") {
        DomNode root = renderer.create_element("div");
        renderer.insert_before(renderer.document(), root, null_dom_node);

        const FunctionComponent Dialog = [](const HarnessProps&) -> VNode {
            return View({.children = {"Dialog content"}});
        };

        const FunctionComponent App = [Dialog](const HarnessProps&) -> VNode {
            return View({.children = {
                            Portal({.container = portal_target, .children = {Dialog({})}})}});
        };

        Container app_container{.host = &renderer, .mount = root};
        render(App({}), app_container);
        REQUIRE(renderer.inner_html(portal_target) == "<view>Dialog content</view>");

        render(fragment(), app_container);
        REQUIRE(renderer.inner_html(portal_target) == "");
    }

    SECTION("should not unmount when parent renders") {
        DomNode root = renderer.create_element("div");
        renderer.insert_before(renderer.document(), root, null_dom_node);

        child_mount_calls = 0;
        parent_mount_calls = 0;

        const FunctionComponent Child = [](const HarnessProps&) -> VNode {
            use_effect([]() -> CleanupFunction {
                ++child_mount_calls;
                return {};
            }, {});
            ReferenceObject ref = use_ref(ReferenceObject{});
            mounted_element_reference = ref;
            return View({.class_name = "child", .ref = ref, .children = {"child"}});
        };

        const FunctionComponent App = [Child](const HarnessProps&) -> VNode {
            use_effect([]() -> CleanupFunction {
                ++parent_mount_calls;
                return {};
            }, {});
            return View({.children = {
                            Portal({.container = portal_target, .children = {Child({})}})}});
        };

        Container app_container{.host = &renderer, .mount = root};
        render(App({}), app_container);

        DomNode mounted_child = mounted_element_reference.current();
        REQUIRE(parent_mount_calls == 1);
        REQUIRE(child_mount_calls == 1);

        render(App({}), app_container);
        render(App({}), app_container);

        DomNode remounted_child = mounted_element_reference.current();
        REQUIRE(mounted_child == remounted_child);
        REQUIRE(parent_mount_calls == 1);
        REQUIRE(child_mount_calls == 1);
    }
}

TEST_CASE("portal into shared container") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();
    portal_target = renderer.document();

    SECTION("should leave a working root after the portal") {
        const FunctionComponent Foo = [](const ChildrenProps& props) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(false);
            auto [mounted_second, set_mounted_second] = use_state<bool>(true);
            toggle = [set_mounted] {
                set_mounted(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            toggle_second = [set_mounted_second] {
                set_mounted_second(
                    std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({.children = {
                            when(mounted, [&props] {
                                return Portal(
                                    {.container = portal_target, .children = props.children});
                            }),
                            when(mounted_second, [] { return Text({.children = {"Hello"}}); })}});
        };

        render(Foo({.children = {View({.children = {"foobar"}})}}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view><view>foobar</view>");

        toggle_second();
        flush();
        REQUIRE(renderer.inner_html() == "<view></view><view>foobar</view>");

        toggle_second();
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view><view>foobar</view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view>");

        toggle_second();
        flush();
        REQUIRE(renderer.inner_html() == "<view></view>");
    }

    SECTION("should work with stacking portals") {
        const FunctionComponent Foo = [](const ChildrenProps& props) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(false);
            auto [mounted_second, set_mounted_second] = use_state<bool>(false);
            toggle = [set_mounted] {
                set_mounted(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            toggle_second = [set_mounted_second] {
                set_mounted_second(
                    std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({.children = {
                            Text({.children = {"Hello"}}),
                            when(mounted, [&props] {
                                return Portal(
                                    {.container = portal_target, .children = props.children});
                            }),
                            when(mounted_second, [] {
                                return Portal({.container = portal_target,
                                               .children = {View({.children = {"foobar2"}})}});
                            })}});
        };

        render(Foo({.children = {View({.children = {"foobar"}})}}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view><view>foobar</view>");

        toggle_second();
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view><text>Hello</text></view><view>foobar</view><view>foobar2</view>");

        toggle_second();
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view><view>foobar</view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view>");
    }

    SECTION("should work with changing the container") {
        const FunctionComponent Foo = [](const ChildrenProps& props) -> VNode {
            auto [container, set_container_state] = use_state<DomNode>(portal_target);
            set_container = set_container_state;
            ReferenceObject ref = use_ref(ReferenceObject{});
            mounted_element_reference = ref;
            return View({.ref = ref,
                         .children = {
                             Text({.children = {"Hello"}}),
                             Portal({.container = container, .children = props.children})}});
        };

        render(Foo({.children = {View({.children = {"foobar"}})}}), scratch);
        REQUIRE(renderer.inner_html() == "<view>foobar</view><view><text>Hello</text></view>");

        set_container(mounted_element_reference.current());
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text><view>foobar</view></view>");
    }

    SECTION("should work with replacing placeholder portals") {
        const FunctionComponent Foo = [](const ChildrenProps& props) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(false);
            auto [mounted_second, set_mounted_second] = use_state<bool>(false);
            toggle = [set_mounted] {
                set_mounted(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            toggle_second = [set_mounted_second] {
                set_mounted_second(
                    std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({.children = {
                            Text({.children = {"Hello"}}),
                            Portal({.container = portal_target,
                                    .children = mounted ? props.children : Children{}}),
                            Portal({.container = portal_target,
                                    .children = mounted_second ? props.children : Children{}})}});
        };

        render(Foo({.children = {View({.children = {"foobar"}})}}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view><view>foobar</view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view>");

        toggle_second();
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view><view>foobar</view>");

        toggle_second();
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view>");
    }

    SECTION("should work with removing an element from stacked container to new one") {
        const FunctionComponent Foo = [](const ChildrenProps& props) -> VNode {
            auto [root, set_root] = use_state<DomNode>(portal_target);
            ReferenceObject ref = use_ref(ReferenceObject{});
            toggle = [set_root, ref] { set_root(ref.current()); };
            return View({.ref = ref,
                         .children = {
                             Text({.children = {"Hello"}}),
                             Portal({.container = portal_target, .children = props.children}),
                             Portal({.container = root, .children = props.children})}});
        };

        render(Foo({.children = {View({.children = {"foobar"}})}}), scratch);
        REQUIRE(renderer.inner_html() ==
                "<view>foobar</view><view>foobar</view><view><text>Hello</text></view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view>foobar</view><view><text>Hello</text><view>foobar</view></view>");
    }

    SECTION("should support nested portals") {
        const FunctionComponent Bar = [](const HarnessProps&) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(false);
            toggle_second = [set_mounted] {
                set_mounted(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            ReferenceObject ref = use_ref(ReferenceObject{});
            return View({.ref = ref,
                         .children = {
                             Text({.children = {"Inner"}}),
                             when(mounted, [] {
                                 return Portal({.container = portal_target,
                                                .children = {Text({.children = {"hiFromBar"}})}});
                             }),
                             when(mounted, [ref] {
                                 return Portal({.container = ref.current(),
                                                .children = {Text({.children = {"innerPortal"}})}});
                             })}});
        };

        const FunctionComponent Foo = [Bar](const HarnessProps&) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(false);
            toggle = [set_mounted] {
                set_mounted(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({.children = {
                            Text({.children = {"Hello"}}),
                            when(mounted, [Bar] {
                                return Portal({.container = portal_target,
                                               .children = {Bar({})}});
                            })}});
        };

        render(Foo({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view><text>Hello</text></view><view><text>Inner</text></view>");

        toggle_second();
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view><text>Hello</text></view><view><text>Inner</text><text>innerPortal</text></view><text>hiFromBar</text>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view>");
    }

    SECTION("should support nested portals remounting #2669") {
        render_counter = 0;

        const FunctionComponent NestedPortalHost = [](const ShowProps& props) -> VNode {
            VNode inner_vnode =
                View({.class_name = "inner", .children = {std::to_string(render_counter)}});
            ++render_counter;
            std::vector<VNode> outer_children;
            outer_children.push_back(std::to_string(render_counter));
            if (props.show)
                outer_children.push_back(Portal(
                    {.container = portal_target, .children = {std::move(inner_vnode)}}));
            VNode outer_vnode =
                View({.class_name = "outer", .children = std::move(outer_children)});
            ++render_counter;
            return Portal({.container = portal_target, .children = {std::move(outer_vnode)}});
        };

        const FunctionComponent App = [NestedPortalHost](const HarnessProps&) -> VNode {
            auto [visible, set_visible_state] = use_state<bool>(true);
            set_visible = set_visible_state;
            return View({.class_name = "app",
                         .children = {"test", NestedPortalHost({.show = visible})}});
        };

        render(App({}), scratch);
        REQUIRE(renderer.inner_html() ==
                "<view class=\"inner\">0</view><view class=\"outer\">1</view><view class=\"app\">test</view>");

        set_visible(false);
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view class=\"outer\">3</view><view class=\"app\">test</view>");

        set_visible(true);
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view class=\"outer\">5</view><view class=\"app\">test</view><view class=\"inner\">4</view>");

        set_visible(false);
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view class=\"outer\">7</view><view class=\"app\">test</view>");
    }

    SECTION("should switch between non portal and portal node (Modal as lastChild)") {
        const FunctionComponent Modal = [](const ModalOpenProps& props) -> VNode {
            if (props.open)
                return Portal({.container = portal_target,
                               .children = {View({.children = props.children})}});
            return View({.children = {"Closed"}});
        };

        const FunctionComponent App = [Modal](const HarnessProps&) -> VNode {
            auto [open, set_open] = use_state<bool>(false);
            toggle = [set_open] {
                set_open(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({.children = {
                            View({.children = {"Show"}}),
                            open ? std::string("Open") : std::string("Closed"),
                            Modal({.open = open, .children = {"Hello"}})}});
        };

        render(App({}), scratch);
        REQUIRE(renderer.inner_html() ==
                "<view><view>Show</view>Closed<view>Closed</view></view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view><view>Show</view>Open</view><view>Hello</view>");
    }

    SECTION("should switch between non portal and portal node (Modal as firstChild)") {
        const FunctionComponent Modal = [](const ModalOpenProps& props) -> VNode {
            if (props.open)
                return Portal({.container = portal_target,
                               .children = {View({.children = props.children})}});
            return View({.children = {"Closed"}});
        };

        const FunctionComponent App = [Modal](const HarnessProps&) -> VNode {
            auto [open, set_open] = use_state<bool>(false);
            toggle = [set_open] {
                set_open(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({.children = {
                            Modal({.open = open, .children = {"Hello"}}),
                            View({.children = {"Show"}}),
                            open ? std::string("Open") : std::string("Closed")}});
        };

        render(App({}), scratch);
        REQUIRE(renderer.inner_html() ==
                "<view><view>Closed</view><view>Show</view>Closed</view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view><view>Show</view>Open</view><view>Hello</view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view><view>Closed</view><view>Show</view>Closed</view>");
    }

    SECTION("should order effects well") {
        effect_call_order.clear();

        const FunctionComponent ModalButton = [](const IndexProps& props) -> VNode {
            std::string index = props.index;
            use_effect([index]() -> CleanupFunction {
                effect_call_order.push_back("Button " + index);
                return {};
            }, {});
            return View({.children = {"Action"}});
        };

        const FunctionComponent Modal = [](const ChildrenProps& props) -> VNode {
            use_effect([]() -> CleanupFunction {
                effect_call_order.push_back("Modal");
                return {};
            }, {});
            return Portal({.container = portal_target,
                           .children = {View({.class_name = "modal", .children = props.children})}});
        };

        const FunctionComponent App = [Modal, ModalButton](const HarnessProps&) -> VNode {
            use_effect([]() -> CleanupFunction {
                effect_call_order.push_back("App");
                return {};
            }, {});
            return Modal({.children = {ModalButton({.index = "1"}), ModalButton({.index = "2"})}});
        };

        render(App({}), scratch);

        REQUIRE(effect_call_order ==
                std::vector<std::string>{"Button 1", "Button 2", "Modal", "App"});
    }

    SECTION("should order complex effects well") {
        effect_call_order.clear();
        secondary_scratch = renderer.create_element("div");
        renderer.insert_before(renderer.document(), secondary_scratch, null_dom_node);

        const FunctionComponent Child = [](const ComplexChildProps& props) -> VNode {
            std::string index = props.index;
            bool is_portal = props.is_portal;
            use_effect([index, is_portal]() -> CleanupFunction {
                effect_call_order.push_back((is_portal ? std::string("Portal ") : std::string()) +
                                            "Child " + index);
                return {};
            }, {index, is_portal});
            return View({.children = {index}});
        };

        const FunctionComponent Parent = [](const ComplexParentProps& props) -> VNode {
            bool is_portal = props.is_portal;
            use_effect([is_portal]() -> CleanupFunction {
                effect_call_order.push_back((is_portal ? std::string("Portal ") : std::string()) +
                                            "Parent");
                return {};
            }, {is_portal});
            return View({.children = props.children});
        };

        const FunctionComponent PortalHost = [Parent, Child](const HarnessProps&) -> VNode {
            use_effect([]() -> CleanupFunction {
                effect_call_order.push_back("Portal");
                return {};
            }, {});
            return Portal({.container = secondary_scratch,
                           .children = {Parent(
                               {.is_portal = true,
                                .children = {Child({.index = "1", .is_portal = true}),
                                             Child({.index = "2", .is_portal = true}),
                                             Child({.index = "3", .is_portal = true})}})}});
        };

        const FunctionComponent App = [Parent, Child, PortalHost](const HarnessProps&) -> VNode {
            return fragment(
                Parent({.children = {Child({.index = "1"}), Child({.index = "2"}),
                                     Child({.index = "3"})}}),
                PortalHost({}));
        };

        render(App({}), scratch);

        REQUIRE(effect_call_order == std::vector<std::string>{
            "Child 1", "Child 2", "Child 3", "Parent",
            "Portal Child 1", "Portal Child 2", "Portal Child 3", "Portal Parent",
            "Portal"});
    }
}

TEST_CASE("portal context") {
    hosts::HtmlStringHost renderer;
    portal_target = renderer.create_element("layer");
    renderer.insert_before(renderer.document(), portal_target, null_dom_node);
    Container scratch = renderer.create_container();

    SECTION("reads context across the portal boundary") {
        Context<std::string> label_context = create_context<std::string>("default");

        const FunctionComponent Ported = [label_context](const HarnessProps&) -> VNode {
            return Text({.class_name = "ported", .children = {use_context(label_context)}});
        };

        const FunctionComponent App = [label_context, Ported](const HarnessProps&) -> VNode {
            return label_context.Provider(
                {.value = std::string("from-app"),
                 .children = {View({.class_name = "inline"}),
                              Portal({.container = portal_target, .children = {Ported({})}})}});
        };

        render(App({}), scratch);

        REQUIRE(renderer.inner_html(portal_target) == "<text class=\"ported\">from-app</text>");
    }
}

TEST_CASE("use_document_event") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("delivers a document event to the handler and unsubscribes on unmount") {
        document_key.clear();

        const FunctionComponent Listener = [](const HarnessProps&) -> VNode {
            use_document_event("key_down", [](const Event& event) { document_key = event.key; });
            return View({.class_name = "listener"});
        };

        render(Listener({}), scratch);

        renderer.dispatch_event(renderer.document(), "key_down", Event{.key = "escape"});
        REQUIRE(document_key == "escape");

        render(fragment(), scratch);
        document_key.clear();
        renderer.dispatch_event(renderer.document(), "key_down", Event{.key = "enter"});
        REQUIRE(document_key == "");
    }
}
