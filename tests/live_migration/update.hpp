#pragma once

#include "state.hpp"

namespace fixture {
inline bool check_cache(const CacheV2& cache) {
  return cache.max_entries >= 1024;
}

inline TaggedV2 move_tag(const TaggedV1& old) {
  TaggedV2 state;
  state.kind = old.kind;
  if (old.kind == TaggedV1::Kind::count) {
    state.count = old.count;
  } else {
    state.fraction = old.fraction;
  }
  return state;
}

inline TaggedV1 reverse_tag(const TaggedV2& old) {
  TaggedV1 state;
  state.kind = old.kind;
  if (old.kind == TaggedV1::Kind::count) {
    state.count = old.count;
  } else {
    state.fraction = old.fraction;
  }
  return state;
}
} // namespace fixture

extern "C" const fixture::managed::Recipe* clang_tracer_managed_update_v1() {
  using namespace fixture;
  using namespace fixture::managed;
  constexpr Transform forward = preserve<CacheV1, CacheV2, Preserve<&CacheV1::name, &CacheV2::name>,
                                         Preserve<&CacheV1::entries, &CacheV2::entries>>;
  constexpr Transform reverse = preserve<CacheV2, CacheV1, Preserve<&CacheV2::name, &CacheV1::name>,
                                         Preserve<&CacheV2::entries, &CacheV1::entries>>;
  static constexpr Schema before{layout<CacheV1>("cache.1", "fixture::CacheV1"),
                                 create_from<CacheV1, Seed, make_v1>};
  static constexpr Schema after{layout<CacheV2>("cache.2", "fixture::CacheV2"),
                                create_from<CacheV2, Seed, make_v2>,
                                validate<CacheV2, check_cache>};
  static constexpr Change changes[]{
      {"Cache", before.layout, after, forward, reverse},
      {"Singleton", before.layout, after, forward, reverse},
      {"UninitializedSingleton", before.layout, after, forward, reverse},
      {"Tagged",
       layout<TaggedV1>("tagged.1", "fixture::TaggedV1"),
       {layout<TaggedV2>("tagged.2", "fixture::TaggedV2"), create_default<TaggedV2>},
       transform<TaggedV1, TaggedV2, move_tag>,
       transform<TaggedV2, TaggedV1, reverse_tag>},
  };
  static constexpr Recipe recipe{changes, sizeof(changes) / sizeof(changes[0])};
  return &recipe;
}
