// HFW::ItemV2 is declared by the prepared adapter before this encoder is parsed.
namespace tracer_snapshot {

struct V2Projection {
  int summary;
};

V2Projection encode(const HFW::ItemV2 &item) noexcept {
  return {static_cast<int>(item.generation) + 1000};
}

} // namespace tracer_snapshot
