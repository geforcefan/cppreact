#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static VNode keyed_list(const std::vector<std::string>& keys) {
    std::vector<VNode> items;
    for (const std::string& key : keys) items.push_back(h("li", {{"key", key}}, key));
    return h("ul", {}, std::move(items));
}

static long count_creates(const std::vector<std::string>& log) {
    long total = 0;
    for (const std::string& line : log) {
        if (line.rfind("create element", 0) == 0) ++total;
    }
    return total;
}

int main() {
    {
        renderers::TestRenderer renderer;
        NodeHandle container = renderer.document();
        Container root = renderer.create_container();

        render(keyed_list({"a", "b", "c"}), root);
        assert(renderer.serialize(container) == "<ul><li>a</li><li>b</li><li>c</li></ul>");

        long creates_after_mount = count_creates(renderer.log);

        render(keyed_list({"a", "c"}), root);
        assert(renderer.serialize(container) == "<ul><li>a</li><li>c</li></ul>");

        render(keyed_list({"c", "a", "b"}), root);
        assert(renderer.serialize(container) == "<ul><li>c</li><li>a</li><li>b</li></ul>");

        long creates_total = count_creates(renderer.log);
        assert(creates_total - creates_after_mount == 1);
        std::printf("PASS keyed list reuses nodes across remove, insert and reorder\n");
    }

    {
        renderers::TestRenderer renderer;
        NodeHandle container = renderer.document();
        Container root = renderer.create_container();

        render(h("p", {}, std::string{"one"}), root);
        assert(renderer.serialize(container) == "<p>one</p>");
        NodeHandle text_node = renderer.find_by_text("one");

        render(h("p", {}, std::string{"two"}), root);
        assert(renderer.serialize(container) == "<p>two</p>");
        assert(renderer.find_by_text("two") == text_node);
        std::printf("PASS text updates in place\n");
    }

    {
        renderers::TestRenderer renderer;
        NodeHandle container = renderer.document();
        Container root = renderer.create_container();

        render(h("div", {{"class", std::string{"on"}}, {"title", std::string{"x"}}}), root);
        assert(renderer.serialize(container) == "<div class=\"on\" title=\"x\"></div>");

        render(h("div", {{"class", std::string{"off"}}}), root);
        assert(renderer.serialize(container) == "<div class=\"off\"></div>");
        std::printf("PASS props update and removed props are cleared\n");
    }

    std::printf("ALL diff tests passed\n");
    return 0;
}
