<h1 align="center">c++react</h1>

<p align="center">React in C++, probably for your games, CAD editors or other crazy stuff, like a GUI inside Unreal Engine 5 (I did). Function components, hooks, and a virtual DOM that renders into whatever host you give it. Because your CAD software also deserved a pretty GUI.</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-alpha-orange" alt="alpha">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue" alt="C++20">
  <img src="https://img.shields.io/badge/deps-none-brightgreen" alt="no dependencies">
  <img src="https://img.shields.io/badge/exceptions%20%2F%20rtti-off-black" alt="no exceptions or rtti">
  <img src="https://img.shields.io/badge/license-MIT-black" alt="MIT license">
</p>

<p align="center">
  <img src="demo/demo.gif" alt="the todo demo app, driven by c++react through RmlUi" width="480">
</p>

> **Alpha.** c++react is experimental and not recommended for production use yet. Try it, break it,
> and help me sort out the bugs: issues and pull requests are very welcome.

c++react is a C++ library for building user interfaces, with the React model: components, hooks, and
a virtual DOM that updates only what changed. It does not render on its own. You point it at a
host and it drives that, so the same components can run on a browser, a game engine, or anything
else. The [demo](#demo) below uses [RmlUi](https://github.com/mikke89/RmlUi), the great HTML and CSS
like library for C++ that draws through OpenGL and many other targets (and yes, I implemented an
Unreal Engine 5 backend for RmlUi, stay tuned, that will be open sourced too).

What you get:

- typed props: components and elements take plain C++ structs, a typo is a compile error
- the hooks you expect (`use_state`, `use_effect`, `use_memo` and friends, all listed below)
- keyed lists, context, portals, synthetic events
- native elements for the parts that draw their own pixels
- batched rendering: `set_state` queues, `flush` drains it
- no dependencies, C++20, builds with `-fno-exceptions -fno-rtti`

## Using it

c++react is header only. Fetch it and link the target:

```cmake
include(FetchContent)
FetchContent_Declare(cppreact GIT_REPOSITORY https://github.com/geforcefan/cppreact.git)
FetchContent_MakeAvailable(cppreact)

target_link_libraries(your_app PRIVATE cppreact::cppreact)
```

Or copy the `include/` directory into your project, there is nothing to compile.

```cpp
#include "cppreact/cppreact.hpp"
#include "cppreact/hosts/rml.hpp"   // the host you render through
```

## Demo

A demo says more than a thousand words. `demo/` is the todo app from the animation at the top, a
complete desktop app:

- a GLFW window with OpenGL
- RmlUi on top as the host for c++react
- CMake fetches GLFW and RmlUi
- FreeType comes from the system (`brew install freetype` on macOS, `apt install libfreetype-dev` on Linux)

```
cmake -S demo -B demo/build
cmake --build demo/build
./demo/build/todo
```

The demo fetches [my RmlUi fork](https://github.com/geforcefan/RmlUi) instead of upstream: it
carries a couple of bug fixes of mine that are not merged yet (open PRs
[#962](https://github.com/mikke89/RmlUi/pull/962) and
[#965](https://github.com/mikke89/RmlUi/pull/965)).

## Components

A component is a props struct and a function `VNode(const Props&)`. Wrap the function in
`FunctionComponent` and call it with designated initializers:

```cpp
#include "cppreact/cppreact.hpp"

using namespace cppreact;
using namespace cppreact::tags;

struct CounterProps {
  std::string label = "count";
  std::optional<double> maximum{};
};

const FunctionComponent Counter = [](const CounterProps& props) -> VNode {
  auto [count, set_count] = use_state<int>(0);

  auto increment = [=, maximum = props.maximum](const Event&) {
    if (maximum && count >= *maximum) return;
    set_count(count + 1);
  };

  return View({.class_name = "counter",
               .children = {Text({.children = {props.label, ": ", count}}),
                            View({.on_click = increment, .children = {"increment"}})}});
};
```

A click runs the handler and `set_count` re-renders. A misspelled field or a wrong type does not
compile. Components go inside components:

```cpp
struct AppProps {};

const FunctionComponent App = [](const AppProps&) -> VNode {
  return View({.class_name = "app",
               .children = {Counter({.label = "left"}),
                            Counter({.label = "right", .maximum = 10})}});
};
```

Children are a field like any other. Declare `Children children{};` and the component decides
where they land:

```cpp
struct CardProps {
  Children children{};
};

const FunctionComponent Card = [](const CardProps& props) -> VNode {
  return View({.class_name = "card", .children = {props.children}});
};

Card({.children = {Text({.children = {"inside"}})}});
```

A struct can hold as many children slots as it wants: a `Children aside{}` next to
`Children children{}` is two named slots, something the single JSX children list cannot say.

## Props

Props are your struct, so a field is whatever C++ type you need: an enum, a callback, a math
vector, another struct. Defaults live in the field initializers, `std::optional` marks the truly
optional ones, and three field names have built-in meaning when you declare them:

- `key`: list identity, see [Lists](#lists)
- `ref`: attaches to the created node, see [References](#references)
- `children`: the nodes passed by the caller

Give the struct a defaulted `operator==` and [memo](#memo) can compare it by value:

```cpp
struct RowProps {
  std::string label{};
  int count = 0;
  std::string key{};
  Children children{};

  bool operator==(const RowProps&) const = default;
};
```

`Children` compares as equal only when both sides are empty, so a memoized component with real
children re-renders, exactly like React. A `Callback` field compares by identity: the same one
again changed nothing, a fresh one did.

## Elements

These elements exist by default: `View`, `Text`, `Input`, `Textarea`, `Image`. Each one is a
props struct too, and the struct is the contract: `class_name`, `style`, the event handlers the
element actually supports, and on the form controls `value`, `checked`, `disabled`, `type`.

Children mix freely: strings and numbers turn into text nodes, elements and components nest,
`nullptr` renders nothing.

```cpp
View({.class_name = "row",
      .children = {"hello",
                   42,
                   6.5,
                   Text({.children = {"an element"}}),
                   Counter({.label = "a component"}),
                   nullptr}});
```

`style` is per-node styling, name and value strings of your host's styling system:

```cpp
View({.style = {{"width", "50%"}, {"color", "red"}}});
```

An input with a `value` set is controlled: c++react holds it to the props value against user
edits, leave `value` unset for an uncontrolled input:

```cpp
Input({.type = "text",
       .value = draft,
       .on_change = [=](const Event& event) { set_draft(event.value); }});
```

If you need your own element, define it: a props struct, an `apply_props_to_dom` overload that
maps fields to host calls, and an `Element`. That is the whole mechanism the built-in five use, see
[Native elements](#native-elements) for the typical case.

## Hooks

Call them at the top of a component, same order every render. Dependencies are a brace list of
`Value`s:

- `{a, b}` reruns when one changed
- `{}` runs once, on mount
- no argument runs every render

### use_state

```cpp
auto [count, set_count] = use_state<int>(0);

set_count(count + 1);
set_count([](const int& value) { return value + 1; });

auto [rows, set_rows] = use_state<Rows>([] { return load_rows(); });
```

The setter takes a value or an updater lambda. It re-renders the component and skips the render
when nothing changed. A call after unmount is ignored. Pass a function as the initial value and it
runs once, on mount.

### use_reducer

A store: the reducer takes the current state and an action and returns the next state, `dispatch`
feeds it one:

```cpp
struct IncrementAction { int amount; };
struct ResetAction {};
using CounterAction = std::variant<IncrementAction, ResetAction>;

int counter_reducer(const int& count, const CounterAction& action) {
  if (const IncrementAction* increment = std::get_if<IncrementAction>(&action)) {
    return count + increment->amount;
  }
  if (std::get_if<ResetAction>(&action)) {
    return 0;
  }
  return count;
}

struct CounterProps {};

const FunctionComponent Counter = [](const CounterProps&) -> VNode {
  auto [count, dispatch] = use_reducer<int, CounterAction>(counter_reducer, 0);

  return View({.children = {
    Text({.children = {count}}),
    View({.on_click = [=](const Event&) { dispatch(IncrementAction{10}); },
          .children = {"+10"}}),
    View({.on_click = [=](const Event&) { dispatch(ResetAction{}); },
          .children = {"reset"}})}});
};
```

`dispatch` skips the render when the reducer returns an equal state. As with `use_state`, a
function as the initial value runs once, on mount.

### use_effect

```cpp
use_effect([id]() -> CleanupFunction {
  subscribe(id);
  return [id] { unsubscribe(id); };
}, {id});
```

Runs after commit when a dependency changed. The effect returns its cleanup, which runs before the
next run and on unmount. Return an empty `CleanupFunction` when there is nothing to clean up.

### use_layout_effect

Same signature as `use_effect`, runs first in the commit, before the passive effects. For reading
layout right after the tree changed: layout is settled when a layout effect runs, read it without
forcing anything yourself.

### use_memo

```cpp
auto total = use_memo<int>([rows] { return sum(rows); }, {static_cast<double>(rows.size())});
```

Recomputes only when a dependency changed.

### use_callback

```cpp
auto select = use_callback(Callback{[id](const Event&) { open(id); }}, {id});
```

Returns the same `Callback` until a dependency changes, so dependency lists and `memo` see a
stable identity.

### use_ref

```cpp
int& render_count = use_ref<int>(0);
render_count += 1;
```

A mutable value that survives re-renders and does not trigger one.

### use_context

```cpp
const std::string& value = use_context(theme);
```

Reads the nearest provided value, see [Context](#context).

### use_sync_external_store

```cpp
auto paused = use_sync_external_store(
  [](auto on_change) { return store.subscribe(on_change); },
  [] { return store.paused(); });
```

Subscribes to an external store and re-renders when the snapshot changes.
`subscribe(on_change)` returns the unsubscribe.

### References

References come in two forms, and the `ref` field takes either one directly. The object form is
`ReferenceObject`: copies all point at the same node, `current()` reads it. Hold it in
`use_ref` so it survives re-renders:

```cpp
ReferenceObject reference = use_ref(ReferenceObject{});

return View({.ref = reference, .class_name = "panel", .children = {"..."}});
```

The function form is a `Callback` taking a `DomNode`. It is called with the node on attach
and with `null_dom_node` on detach:

```cpp
View({.ref = Callback{[](DomNode node) { ... }}});
```

References compare by identity: a stable reference of either form never re-fires across
re-renders and lets `memo` bail, an inline one re-fires every render. On unmount an object
reference is only nulled while it still points at the dying node.

`reference.current()` reaches the created node. Anything host-specific, measuring a box,
reading the display density, imperative animation, goes through the node the reference hands you:
resolve it with `Host::native_element(node)` and talk to your toolkit directly.

On an element, `ref` attaches to the created node. On a component, declare a `Reference ref{}`
field and copy it onto whichever element you want to expose, without caring which form it holds:

```cpp
struct FieldProps {
  Reference ref{};
};

const FunctionComponent Field = [](const FieldProps& props) -> VNode {
  return Input({.class_name = "field", .type = "text", .ref = props.ref});
};

ReferenceObject reference = use_ref(ReferenceObject{});
Field({.ref = reference});
```

A component without a `ref` field attaches nothing, and its type says so.

### use_document_event

```cpp
use_document_event("key_down", [cancel](const Event& event) {
  if (event.key == "escape") cancel();
});
```

An event listener on the document. Bound once, always calls the latest handler, removed on unmount.

## Context

```cpp
const auto theme = create_context<std::string>("light");

theme.Provider({.value = "dark", .children = {App({})}});

const std::string& value = use_context(theme);
```

The `Provider` publishes a value for its subtree, `use_context` reads the nearest one. A consumer
subscribes to the provider, so when the value changes every consumer re-renders, even behind a
memoized component that skips its own render. Change means value change: a context type with an
`operator==` only notifies its consumers when the compared value actually differs.

## Lists

Give items a `key`. Add, remove and reorder then keep the matching instances and their state, moving
only what actually moved. The `map` helper turns a range into a list of nodes, and a built vector
moves straight into a children field:

```cpp
View({.class_name = "list",
      .children = map(items, [](const auto& item) {
        return Row({.label = item.name, .key = item.id});
      })});
```

## Conditionals

`when` renders a node only when the condition holds. Pass a node, or a lambda to build it lazily and
skip the work when it is false:

```cpp
View({.class_name = "row",
      .children = {Text({.children = {label}}),
                   when(expanded, [] {
                     return View({.class_name = "detail", .children = {"more"}});
                   })}});
```

## Events

A handler is `void(const Event&)`, wired through the element's handler field. Each element
declares the events it supports; the full vocabulary:

- `on_click`, `on_double_click`
- `on_mouse_down`, `on_mouse_up`, `on_mouse_move`, `on_mouse_over`, `on_mouse_out`
- `on_key_down`, `on_key_up`
- `on_wheel`
- `on_focus`, `on_blur`
- `on_change` (form controls)

Append `_capture` to listen in the capture phase:

```cpp
View({.on_click = bubbled,
      .on_click_capture = first,
      .children = {View({.on_click = pressed, .children = {"ok"}})}});
```

Capture handlers run on the way down to the target, before it: a click on the inner element runs
`first`, then `pressed`, then `bubbled`.

The `Event` is synthetic and the same on every host. The fields you will actually reach for:

```cpp
struct Event {
  std::string type;
  DomNode target, current_target;
  double client_x, client_y;
  int button;
  bool shift_key, ctrl_key, alt_key, meta_key;
  std::string key;
  std::string value;
  double delta_x, delta_y;
  Payload native;

  void prevent_default() const;
  void stop_propagation() const;
};
```

`key` is the pressed key:

- bare characters: `"a"`, `"7"`
- `"enter"`, `"escape"`, `"tab"`, `"delete"`, `"backspace"`
- space is `" "`
- `"arrow_left"`, `"arrow_right"`, `"arrow_up"`, `"arrow_down"`
- `"shift"`, `"control"`, `"alt"`, `"meta"`

`value` is a control's value on change, and `native` carries the raw host event for anything the
fields above do not cover. The rest of the host event surface is there too (`buttons`,
`movement_x`, `code`, `location`, `repeat`, `delta_z`, `delta_mode`, `data`, `related_target`,
`event_phase`, `get_modifier_state`), a host fills what it can.

```cpp
EventCallback zoom = [set_scale](const Event& event) {
  event.prevent_default();
  set_scale([delta = event.delta_y](double scale) { return scale - delta * 0.001; });
};

return View({.class_name = "graph", .on_wheel = zoom, .children = {props.children}});
```

For a listener on the document instead of an element, see
[use_document_event](#use_document_event).

## Portals

`Portal` renders a subtree into a different container:

```cpp
return View({.class_name = "row",
             .children = {Text({.children = {"row"}}),
                          Portal({.container = overlay_layer,
                                  .children = {Tooltip({.text = "on top of everything"})}})}});
```

Context still flows from where the `Portal` sits, not from the target.

## Hosts

c++react renders through a `Host`. One ships with the library, `hosts/html_string.hpp`: an
in-memory tree with `inner_html` and `dispatch_event`, so components can be tested without a UI
toolkit.

```cpp
#include "cppreact/hosts/html_string.hpp"

hosts::HtmlStringHost host;
Container root = host.create_container();
render(View({.class_name = "x", .children = {"hi"}}), root);

host.inner_html();   // <view class="x">hi</view>
```

The RmlUi host ships too, in `hosts/rml.hpp`. Construction is the only difference, it mounts
into an element of the document:

```cpp
#include "cppreact/hosts/rml.hpp"

hosts::RmlHost host;
Container root = host.create_container(rml_element);
render(App({}), root);
```

Each host realizes the five tags in its own vocabulary: the RmlUi host maps `image` to RmlUi's
`img` and instances the rest under the library's names, so RCSS selects `view` and `text`
directly.

`set_state` queues a re-render; nothing happens until you call `flush()`, which batches the queued
updates into one render. Call it wherever it fits, a render loop calls it once per frame.

To write your own host, implement `Host`. `hosts/html_string.hpp` is the reference to read.

## Native elements

Some nodes draw their own pixels and have no children to diff: a canvas, a chart. Such an element
implements `NativeElement`, and the host registers its tag as native:

```cpp
struct NativeElement {
  virtual void set_native_property(std::string_view name, const Payload& value);
  virtual Payload native_reference();
};
```

On the component side it is a custom element with a typed field. A `Payload` carries any C++
value type-erased, compares by value when the type can, and c++react never looks inside:

```cpp
struct ChartProps {
  Payload samples{};
};

inline void apply_props_to_dom(Host& host, DomNode dom, const ChartProps& next,
                               const ChartProps* old) {
  apply_native_property_to_dom(host, dom, "samples", next.samples, old ? &old->samples : nullptr);
}

inline const Element<ChartProps> Chart{"chart"};

Chart({.samples = make_payload(curve_samples)});
```

Read a handle back through a `ref` and `native_reference` when the drawing side exposes one.

## License

MIT, see [LICENSE](LICENSE).
