#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/hooks/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/create.hpp"

using namespace cppreact;

namespace {

struct HarnessProps {};

}

static std::vector<int> values;
static std::vector<std::optional<int>> optional_values;

TEST_CASE("useRef") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("provides a stable reference") {
        values.clear();

        const FunctionComponent TestComponent = [](const HarnessProps&) -> VNode {
            int& ref = use_ref<int>(1);
            values.push_back(ref);
            ref = 2;
            return fragment();
        };

        render(TestComponent({}), scratch);
        render(TestComponent({}), scratch);

        REQUIRE(values == std::vector<int>{1, 2});
    }

    SECTION("defaults to an empty value") {
        optional_values.clear();

        const FunctionComponent TestComponent = [](const HarnessProps&) -> VNode {
            std::optional<int>& ref = use_ref<std::optional<int>>(std::nullopt);
            optional_values.push_back(ref);
            ref = 2;
            return fragment();
        };

        render(TestComponent({}), scratch);
        render(TestComponent({}), scratch);

        REQUIRE(optional_values == std::vector<std::optional<int>>{std::nullopt, 2});
    }
}
