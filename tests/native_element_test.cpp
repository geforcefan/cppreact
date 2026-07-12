#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/host/native.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/create.hpp"

using namespace cppreact;

struct TestNativeElement : NativeElement {
    int prop_calls = 0;
    std::shared_ptr<std::string> last_model{};
    std::shared_ptr<std::string> handle = std::make_shared<std::string>("cube-handle");

    void set_native_property(std::string_view name, const RawPayload& value) override {
        if (name != "model") return;
        prop_calls++;
        last_model = std::static_pointer_cast<std::string>(value.data);
    }

    RawPayload native_reference() override { return RawPayload{handle}; }
};

static RawPayload current_model{};

static const FunctionComponent App = [](const Object&) -> VNode { return h("cube", {{"model", current_model}}); };

TEST_CASE("native elements") {
    hosts::HtmlStringHost renderer;
    TestNativeElement cube;
    renderer.register_native("cube", &cube);

    Container scratch = renderer.create_container();
    current_model = RawPayload{std::make_shared<std::string>("alpha")};

    SECTION("a RawPayload prop routes to the native element, not an attribute") {
        render(App({}), scratch);

        DomNode node = renderer.find_first("cube");
        REQUIRE(node != null_dom_node);
        REQUIRE(cube.prop_calls == 1);
        REQUIRE((cube.last_model && *cube.last_model == "alpha"));
    }

    SECTION("native_reference returns the imperative handle") {
        render(App({}), scratch);

        RawPayload handle = renderer.native_reference(renderer.find_first("cube"));
        REQUIRE((handle.data && *std::static_pointer_cast<std::string>(handle.data) == "cube-handle"));
    }

    SECTION("an unchanged payload is not re-delivered") {
        render(App({}), scratch);
        render(App({}), scratch);

        REQUIRE(cube.prop_calls == 1);
    }

    SECTION("a changed payload is delivered again") {
        render(App({}), scratch);

        current_model = RawPayload{std::make_shared<std::string>("beta")};
        render(App({}), scratch);

        REQUIRE(cube.prop_calls == 2);
        REQUIRE(*cube.last_model == "beta");
    }
}
