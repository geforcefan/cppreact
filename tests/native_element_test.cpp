#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/host/native.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/element.hpp"
#include "cppreact/host/appliers.hpp"

using namespace cppreact;

namespace cube_element {

struct CubeProps {
  Payload model{};
};

inline void apply_props_to_dom(Host& host, DomNode dom, const CubeProps& next,
                               const CubeProps* old) {
  apply_native_property_to_dom(host, dom, "model", next.model, old ? &old->model : nullptr);
}

inline const Element<CubeProps> Cube{"cube"};

}

namespace {

struct TestNativeElement : NativeElement {
    int prop_calls = 0;
    std::shared_ptr<const std::string> last_model{};
    std::shared_ptr<std::string> handle = std::make_shared<std::string>("cube-handle");

    void set_native_property(std::string_view name, const Payload& value) override {
        if (name != "model") return;
        prop_calls++;
        last_model = payload_as<std::string>(value);
    }

    Payload native_reference() override { return make_payload(handle); }
};

struct AppProps {};

Payload current_model{};

const FunctionComponent App = [](const AppProps&) -> VNode {
    return cube_element::Cube({.model = current_model});
};

}

TEST_CASE("native elements") {
    hosts::HtmlStringHost renderer;
    TestNativeElement cube;
    renderer.register_native("cube", &cube);

    Container scratch = renderer.create_container();
    current_model = make_payload(std::string("alpha"));

    SECTION("a payload prop routes to the native element, not an attribute") {
        render(App({}), scratch);

        DomNode node = renderer.find_first("cube");
        REQUIRE(node != null_dom_node);
        REQUIRE(cube.prop_calls == 1);
        REQUIRE((cube.last_model && *cube.last_model == "alpha"));
    }

    SECTION("native_reference returns the imperative handle") {
        render(App({}), scratch);

        Payload handle = renderer.native_reference(renderer.find_first("cube"));
        std::shared_ptr<const std::string> named = payload_as<std::string>(handle);
        REQUIRE((named && *named == "cube-handle"));
    }

    SECTION("an equal payload is not re-delivered") {
        render(App({}), scratch);
        render(App({}), scratch);

        REQUIRE(cube.prop_calls == 1);
    }

    SECTION("a changed payload is delivered again") {
        render(App({}), scratch);

        current_model = make_payload(std::string("beta"));
        render(App({}), scratch);

        REQUIRE(cube.prop_calls == 2);
        REQUIRE(*cube.last_model == "beta");
    }
}
