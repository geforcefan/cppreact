#include <functional>
#include <string>

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

static DomNode portal_target = null_dom_node;
static std::function<void()> set_false;
static std::function<void()> toggle;
static std::function<void()> bump;
static StateSetter<std::string> set;
static int unmount_calls = 0;
static FunctionComponent modal_component = nullptr;
static FunctionComponent child_component = nullptr;
static Context<std::string> label_context;
static std::string document_key;
static std::function<void()> toggle_second;
static StateSetter<DomNode> set_container;
static StateSetter<bool> set_visible;
static ReferenceObject mounted_element_reference;
static int child_mount_calls = 0;
static int parent_mount_calls = 0;
static int render_counter = 0;
static DomNode secondary_scratch = null_dom_node;
static FunctionComponent parent_component = nullptr;
static FunctionComponent modal_button_component = nullptr;
static FunctionComponent nested_portal_host_component = nullptr;
static std::vector<std::string> effect_call_order;

TEST_CASE("createPortal") {
    hosts::HtmlStringHost renderer;
    portal_target = renderer.create_element("layer", Object{});
    renderer.insert_before(renderer.document(), portal_target, null_dom_node);
    Container scratch = renderer.create_container();

    SECTION("should render into a different root node") {
        const FunctionComponent Foo = [](const Object&) -> VNode {
            return View({}, portal(portal_target, children()));
        };

        render(Foo({}, "foobar"), scratch);

        REQUIRE(renderer.inner_html(portal_target) == "foobar");
    }

    SECTION("should insert the portal") {
        const FunctionComponent Foo = [](const Object&) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(true);
            set_false = [set_mounted = set_mounted] { set_mounted(false); };
            return View({},
                     Text({}, "Hello"),
                     when(mounted, [] { return portal(portal_target, children()); }));
        };

        render(Foo({}, "foobar"), scratch);
        REQUIRE(renderer.inner_html(portal_target) == "foobar");

        set_false();
        flush();
        REQUIRE(renderer.inner_html(portal_target) == "");
    }

    SECTION("should order portal children well") {
        const FunctionComponent Modal = [](const Object&) -> VNode {
            auto [state, set_state] = use_state<int>(0);
            bump = [set_state = set_state] { set_state(1); };
            return fragment(when(state == 1, [] { return View({}, "top"); }),
                            View({}, "middle"),
                            View({}, "bottom"));
        };
        modal_component = Modal;

        const FunctionComponent Foo = [](const Object&) -> VNode {
            return portal(portal_target, modal_component({}));
        };

        render(Foo({}), scratch);
        REQUIRE(renderer.inner_html(portal_target) == "<view>middle</view><view>bottom</view>");

        bump();
        flush();
        REQUIRE(renderer.inner_html(portal_target) ==
                "<view>top</view><view>middle</view><view>bottom</view>");
    }

    SECTION("should toggle the portal") {
        const FunctionComponent Foo = [](const Object&) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(true);
            toggle = [set_mounted = set_mounted] {
                set_mounted(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({},
                     Text({}, "Hello"),
                     when(mounted, [] { return portal(portal_target, children()); }));
        };

        render(Foo({}, View({}, "foobar")), scratch);
        REQUIRE(renderer.inner_html(portal_target) == "<view>foobar</view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html(portal_target) == "");

        toggle();
        flush();
        REQUIRE(renderer.inner_html(portal_target) == "<view>foobar</view>");
    }

    SECTION("should notice prop changes on the portal") {
        const FunctionComponent Foo = [](const Object&) -> VNode {
            auto [extra, set_extra] = use_state<std::string>("red");
            set = set_extra;
            VNode content = extra.empty() ? Text({}, "Foo")
                                          : Text({{"class", extra}}, "Foo");
            return View({},
                     Text({}, "Hello"),
                     portal(portal_target, std::move(content)));
        };

        render(Foo({}), scratch);
        REQUIRE(renderer.inner_html(portal_target) == "<text class=\"red\">Foo</text>");

        set("");
        flush();
        REQUIRE(renderer.inner_html(portal_target) == "<text>Foo</text>");
    }

    SECTION("should not unmount the portal component") {
        unmount_calls = 0;

        const FunctionComponent Child = [](const Object&) -> VNode {
            use_effect([]() -> CleanupFunction { return [] { ++unmount_calls; }; }, {});
            return fragment(children());
        };
        child_component = Child;

        const FunctionComponent Foo = [](const Object&) -> VNode {
            auto [extra, set_extra] = use_state<std::string>("red");
            set = set_extra;
            return View({},
                     Text({}, "Hello"),
                     portal(portal_target, child_component({{"class", extra}}, "Foo")));
        };

        render(Foo({}), scratch);
        REQUIRE(unmount_calls == 0);

        set("");
        flush();
        REQUIRE(unmount_calls == 0);
    }

    SECTION("should not render <undefined> for Portal nodes") {
        DomNode root = renderer.create_element("div", Object{});
        renderer.insert_before(renderer.document(), root, null_dom_node);

        const FunctionComponent Dialog = [](const Object&) -> VNode {
            return View({}, "Dialog content");
        };
        child_component = Dialog;

        const FunctionComponent App = [](const Object&) -> VNode {
            return View({}, portal(portal_target, child_component({})));
        };

        Container app_container{.host = &renderer, .mount = root};
        render(App({}), app_container);

        REQUIRE(renderer.inner_html(root) == "<view></view>");
    }

    SECTION("should unmount Portal") {
        DomNode root = renderer.create_element("div", Object{});
        renderer.insert_before(renderer.document(), root, null_dom_node);

        const FunctionComponent Dialog = [](const Object&) -> VNode {
            return View({}, "Dialog content");
        };
        child_component = Dialog;

        const FunctionComponent App = [](const Object&) -> VNode {
            return View({}, portal(portal_target, child_component({})));
        };

        Container app_container{.host = &renderer, .mount = root};
        render(App({}), app_container);
        REQUIRE(renderer.inner_html(portal_target) == "<view>Dialog content</view>");

        render(fragment(), app_container);
        REQUIRE(renderer.inner_html(portal_target) == "");
    }

    SECTION("should not unmount when parent renders") {
        DomNode root = renderer.create_element("div", Object{});
        renderer.insert_before(renderer.document(), root, null_dom_node);

        child_mount_calls = 0;
        parent_mount_calls = 0;

        const FunctionComponent Child = [](const Object&) -> VNode {
            use_effect([]() -> CleanupFunction {
                ++child_mount_calls;
                return {};
            }, {});
            ReferenceObject ref = use_ref(ReferenceObject{});
            mounted_element_reference = ref;
            return View({{"id", "child"}, {"ref", ref}}, "child");
        };
        child_component = Child;

        const FunctionComponent App = [](const Object&) -> VNode {
            use_effect([]() -> CleanupFunction {
                ++parent_mount_calls;
                return {};
            }, {});
            return View({}, portal(portal_target, child_component({})));
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
        const FunctionComponent Foo = [](const Object&) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(false);
            auto [mounted_second, set_mounted_second] = use_state<bool>(true);
            toggle = [set_mounted = set_mounted] {
                set_mounted(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            toggle_second = [set_mounted_second = set_mounted_second] {
                set_mounted_second(
                    std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({},
                     when(mounted, [] { return portal(portal_target, children()); }),
                     when(mounted_second, [] { return Text({}, "Hello"); }));
        };

        render(Foo({}, View({}, "foobar")), scratch);
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
        const FunctionComponent Foo = [](const Object&) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(false);
            auto [mounted_second, set_mounted_second] = use_state<bool>(false);
            toggle = [set_mounted = set_mounted] {
                set_mounted(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            toggle_second = [set_mounted_second = set_mounted_second] {
                set_mounted_second(
                    std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({},
                     Text({}, "Hello"),
                     when(mounted, [] { return portal(portal_target, children()); }),
                     when(mounted_second, [] { return portal(portal_target, View({}, "foobar2")); }));
        };

        render(Foo({}, View({}, "foobar")), scratch);
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
        const FunctionComponent Foo = [](const Object&) -> VNode {
            auto [container, set_container_state] = use_state<DomNode>(portal_target);
            set_container = set_container_state;
            ReferenceObject ref = use_ref(ReferenceObject{});
            mounted_element_reference = ref;
            return View({{"ref", ref}},
                     Text({}, "Hello"),
                     portal(container, children()));
        };

        render(Foo({}, View({}, "foobar")), scratch);
        REQUIRE(renderer.inner_html() == "<view>foobar</view><view><text>Hello</text></view>");

        set_container(mounted_element_reference.current());
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text><view>foobar</view></view>");
    }

    SECTION("should work with replacing placeholder portals") {
        const FunctionComponent Foo = [](const Object&) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(false);
            auto [mounted_second, set_mounted_second] = use_state<bool>(false);
            toggle = [set_mounted = set_mounted] {
                set_mounted(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            toggle_second = [set_mounted_second = set_mounted_second] {
                set_mounted_second(
                    std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({},
                     Text({}, "Hello"),
                     portal(portal_target, mounted ? children() : std::vector<VNode>{}),
                     portal(portal_target, mounted_second ? children() : std::vector<VNode>{}));
        };

        render(Foo({}, View({}, "foobar")), scratch);
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
        const FunctionComponent Foo = [](const Object&) -> VNode {
            auto [root, set_root] = use_state<DomNode>(portal_target);
            ReferenceObject ref = use_ref(ReferenceObject{});
            toggle = [set_root = set_root, ref] { set_root(ref.current()); };
            return View({{"ref", ref}},
                     Text({}, "Hello"),
                     portal(portal_target, children()),
                     portal(root, children()));
        };

        render(Foo({}, View({}, "foobar")), scratch);
        REQUIRE(renderer.inner_html() ==
                "<view>foobar</view><view>foobar</view><view><text>Hello</text></view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view>foobar</view><view><text>Hello</text><view>foobar</view></view>");
    }

    SECTION("should support nested portals") {
        const FunctionComponent Bar = [](const Object&) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(false);
            toggle_second = [set_mounted = set_mounted] {
                set_mounted(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            ReferenceObject ref = use_ref(ReferenceObject{});
            return View({{"ref", ref}},
                     Text({}, "Inner"),
                     when(mounted, [] { return portal(portal_target, Text({}, "hiFromBar")); }),
                     when(mounted, [ref] { return portal(ref.current(), Text({}, "innerPortal")); }));
        };
        child_component = Bar;

        const FunctionComponent Foo = [](const Object&) -> VNode {
            auto [mounted, set_mounted] = use_state<bool>(false);
            toggle = [set_mounted = set_mounted] {
                set_mounted(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({},
                     Text({}, "Hello"),
                     when(mounted, [] { return portal(portal_target, child_component({})); }));
        };

        render(Foo({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view>");

        toggle();
        flush();
        REQUIRE(renderer.inner_html() == "<view><text>Hello</text></view><view><text>Inner</text></view>");

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

        const FunctionComponent NestedPortalHost = [](const Object& props) -> VNode {
            bool show = props.get<bool>("show",false);
            VNode inner_vnode = View({{"id", "inner"}}, std::to_string(render_counter));
            ++render_counter;
            std::vector<VNode> outer_children;
            outer_children.push_back(std::to_string(render_counter));
            if (show) outer_children.push_back(portal(portal_target, std::move(inner_vnode)));
            VNode outer_vnode = View({{"id", "outer"}}, std::move(outer_children));
            ++render_counter;
            return portal(portal_target, std::move(outer_vnode));
        };
        nested_portal_host_component = NestedPortalHost;

        const FunctionComponent App = [](const Object&) -> VNode {
            auto [visible, set_visible_state] = use_state<bool>(true);
            set_visible = set_visible_state;
            return View({{"id", "app"}}, "test", nested_portal_host_component({{"show", visible}}));
        };

        render(App({}), scratch);
        REQUIRE(renderer.inner_html() ==
                "<view id=\"inner\">0</view><view id=\"outer\">1</view><view id=\"app\">test</view>");

        set_visible(false);
        flush();
        REQUIRE(renderer.inner_html() == "<view id=\"outer\">3</view><view id=\"app\">test</view>");

        set_visible(true);
        flush();
        REQUIRE(renderer.inner_html() ==
                "<view id=\"outer\">5</view><view id=\"app\">test</view><view id=\"inner\">4</view>");

        set_visible(false);
        flush();
        REQUIRE(renderer.inner_html() == "<view id=\"outer\">7</view><view id=\"app\">test</view>");
    }

    SECTION("should switch between non portal and portal node (Modal as lastChild)") {
        const FunctionComponent Modal = [](const Object& props) -> VNode {
            bool open = props.get<bool>("open",false);
            if (open) return portal(portal_target, View({}, children()));
            return View({}, "Closed");
        };
        modal_component = Modal;

        const FunctionComponent App = [](const Object&) -> VNode {
            auto [open, set_open] = use_state<bool>(false);
            set_visible = set_open;
            toggle = [set_visible = set_visible] {
                set_visible(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({},
                     View({}, "Show"),
                     open ? std::string("Open") : std::string("Closed"),
                     modal_component({{"open", open}}, "Hello"));
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
        const FunctionComponent Modal = [](const Object& props) -> VNode {
            bool open = props.get<bool>("open",false);
            if (open) return portal(portal_target, View({}, children()));
            return View({}, "Closed");
        };
        modal_component = Modal;

        const FunctionComponent App = [](const Object&) -> VNode {
            auto [open, set_open] = use_state<bool>(false);
            set_visible = set_open;
            toggle = [set_visible = set_visible] {
                set_visible(std::function<bool(const bool&)>([](const bool& state) { return !state; }));
            };
            return View({},
                     modal_component({{"open", open}}, "Hello"),
                     View({}, "Show"),
                     open ? std::string("Open") : std::string("Closed"));
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

        const FunctionComponent ModalButton = [](const Object& props) -> VNode {
            std::string index = props.get<std::string>("index","");
            use_effect([index]() -> CleanupFunction {
                effect_call_order.push_back("Button " + index);
                return {};
            }, {});
            return View({}, "Action");
        };
        modal_button_component = ModalButton;

        const FunctionComponent Modal = [](const Object&) -> VNode {
            use_effect([]() -> CleanupFunction {
                effect_call_order.push_back("Modal");
                return {};
            }, {});
            return portal(portal_target, View({{"class", "modal"}}, children()));
        };
        modal_component = Modal;

        const FunctionComponent App = [](const Object&) -> VNode {
            use_effect([]() -> CleanupFunction {
                effect_call_order.push_back("App");
                return {};
            }, {});
            return modal_component({},
                     modal_button_component({{"index", "1"}}),
                     modal_button_component({{"index", "2"}}));
        };

        render(App({}), scratch);

        REQUIRE(effect_call_order == std::vector<std::string>{"Button 1", "Button 2", "Modal", "App"});
    }

    SECTION("should order complex effects well") {
        effect_call_order.clear();
        secondary_scratch = renderer.create_element("div", Object{});
        renderer.insert_before(renderer.document(), secondary_scratch, null_dom_node);

        const FunctionComponent Child = [](const Object& props) -> VNode {
            std::string index = props.get<std::string>("index","");
            bool is_portal = props.get<bool>("is_portal",false);
            use_effect([index, is_portal]() -> CleanupFunction {
                effect_call_order.push_back((is_portal ? std::string("Portal ") : std::string()) +
                                            "Child " + index);
                return {};
            }, {index, is_portal});
            return View({}, index);
        };
        child_component = Child;

        const FunctionComponent Parent = [](const Object& props) -> VNode {
            bool is_portal = props.get<bool>("is_portal",false);
            use_effect([is_portal]() -> CleanupFunction {
                effect_call_order.push_back((is_portal ? std::string("Portal ") : std::string()) +
                                            "Parent");
                return {};
            }, {is_portal});
            return View({}, children());
        };
        parent_component = Parent;

        const FunctionComponent PortalHost = [](const Object&) -> VNode {
            use_effect([]() -> CleanupFunction {
                effect_call_order.push_back("Portal");
                return {};
            }, {});
            return portal(secondary_scratch,
                     parent_component({{"is_portal", true}},
                         child_component({{"index", "1"}, {"is_portal", true}}),
                         child_component({{"index", "2"}, {"is_portal", true}}),
                         child_component({{"index", "3"}, {"is_portal", true}})));
        };
        nested_portal_host_component = PortalHost;

        const FunctionComponent App = [](const Object&) -> VNode {
            return fragment(
                     parent_component({},
                         child_component({{"index", "1"}}),
                         child_component({{"index", "2"}}),
                         child_component({{"index", "3"}})),
                     nested_portal_host_component({}));
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
    portal_target = renderer.create_element("layer", Object{});
    renderer.insert_before(renderer.document(), portal_target, null_dom_node);
    Container scratch = renderer.create_container();

    SECTION("reads context across the portal boundary") {
        label_context = create_context<std::string>("default");

        const FunctionComponent Ported = [](const Object&) -> VNode {
            return Text({{"class", "ported"}}, use_context(label_context));
        };
        child_component = Ported;

        const FunctionComponent App = [](const Object&) -> VNode {
            return provide(label_context, std::string("from-app"),
                           View({{"class", "inline"}}),
                           portal(portal_target, child_component({})));
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

        const FunctionComponent Listener = [](const Object&) -> VNode {
            use_document_event("key_down", [](const Event& event) { document_key = event.key; });
            return View({{"class", "listener"}});
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
