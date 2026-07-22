#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "../diff/diff.hpp"

namespace cppreact {

namespace detail {

inline std::size_t& context_id_counter() {
  static std::size_t counter = 0;
  return counter;
}

}

template <class T>
struct ProviderProps {
  T value{};
  Children children{};
};

template <class T>
struct Context {
  std::size_t id = 0;
  std::shared_ptr<T> default_value{};
  FunctionComponent<ProviderProps<T>> Provider{};
};

template <class T>
Context<T> create_context(T default_value) {
  Context<T> context;
  context.id = ++detail::context_id_counter();
  context.default_value = std::make_shared<T>(std::move(default_value));

  const std::size_t id = context.id;
  context.Provider =
      FunctionComponent<ProviderProps<T>>([id](const ProviderProps<T>& props) -> VNode {
        ComponentInstance* provider = detail::current_rendering;
        if (provider && !provider->get_child_context) {
          auto subscribers =
              std::make_shared<std::vector<std::weak_ptr<ComponentInstance>>>();
          std::weak_ptr<ComponentInstance> self = provider->weak_from_this();

          provider->get_child_context = [id, self]() -> GlobalContext {
            GlobalContext entries;
            if (std::shared_ptr<ComponentInstance> locked = self.lock()) {
              entries[id] = locked;
            }
            return entries;
          };

          provider->component_will_unmount = [subscribers]() { subscribers->clear(); };

          provider->should_component_update = [self, subscribers](
                                                  const Payload& next_props) -> bool {
            std::shared_ptr<ComponentInstance> locked = self.lock();
            if (locked) {
              bool value_changed = true;
              if constexpr (std::equality_comparable<T>) {
                std::shared_ptr<const ProviderProps<T>> stored =
                    payload_as<ProviderProps<T>>(locked->props);
                std::shared_ptr<const ProviderProps<T>> next =
                    payload_as<ProviderProps<T>>(next_props);
                if (stored && next) value_changed = !(stored->value == next->value);
              }
              if (value_changed) {
                for (const std::weak_ptr<ComponentInstance>& entry : *subscribers) {
                  std::shared_ptr<ComponentInstance> subscriber = entry.lock();
                  if (!subscriber) continue;
                  subscriber->flags |= component_flag::force;
                  enqueue_render(subscriber);
                }
              }
            }
            return true;
          };

          provider->sub = [subscribers](const std::shared_ptr<ComponentInstance>& consumer) {
            for (const std::weak_ptr<ComponentInstance>& entry : *subscribers) {
              if (!entry.owner_before(consumer) && !consumer.owner_before(entry)) return;
            }
            subscribers->push_back(consumer);

            std::function<void()> old_unmount = consumer->component_will_unmount;
            std::weak_ptr<ComponentInstance> weak_consumer = consumer;
            consumer->component_will_unmount = [subscribers, weak_consumer, old_unmount]() {
              std::erase_if(*subscribers,
                            [&weak_consumer](const std::weak_ptr<ComponentInstance>& entry) {
                              return entry.expired() || (!entry.owner_before(weak_consumer) &&
                                                         !weak_consumer.owner_before(entry));
                            });
              if (old_unmount) old_unmount();
            };
          };
        }

        return props.children;
      });

  return context;
}

}
