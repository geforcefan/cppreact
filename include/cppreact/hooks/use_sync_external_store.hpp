#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include "use_effect.hpp"
#include "use_layout_effect.hpp"
#include "use_state.hpp"

namespace cppreact {

template <class Subscribe, class GetSnapshot>
auto use_sync_external_store(Subscribe subscribe, GetSnapshot get_snapshot) {
  using Snapshot = std::decay_t<decltype(get_snapshot())>;

  struct StoreInstance {
    Snapshot value;
    std::function<Snapshot()> get_snapshot;
  };

  struct StoreReference {
    std::shared_ptr<StoreInstance> instance{};
  };

  Snapshot value = get_snapshot();

  auto [store, force_update] = use_state<StoreReference>(
      StoreReference{std::make_shared<StoreInstance>(StoreInstance{value, get_snapshot})});

  std::shared_ptr<StoreInstance> store_instance = store.instance;
  auto did_snapshot_change = [](const std::shared_ptr<StoreInstance>& instance) -> bool {
    return !state_values_equal(instance->value, instance->get_snapshot());
  };

  use_layout_effect(
      [store_instance, value, get_snapshot, force_update,
       did_snapshot_change]() -> CleanupFunction {
        store_instance->value = value;
        store_instance->get_snapshot = get_snapshot;

        if (did_snapshot_change(store_instance)) {
          force_update(StoreReference{store_instance});
        }
        return {};
      },
      Dependencies::every_render());

  use_effect(
      [store_instance, force_update, did_snapshot_change, subscribe]() -> CleanupFunction {
        if (did_snapshot_change(store_instance)) {
          force_update(StoreReference{store_instance});
        }

        return subscribe([store_instance, force_update, did_snapshot_change]() {
          if (did_snapshot_change(store_instance)) {
            force_update(StoreReference{store_instance});
          }
        });
      },
      {});

  return value;
}

}
