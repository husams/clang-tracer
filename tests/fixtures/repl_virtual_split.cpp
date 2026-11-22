// Built at -O2 by the GCC variant of the test only: the throw path makes GCC move part of the
// body to a cold section, so the subprogram is described by DW_AT_ranges instead of one
// low/high pc pair. The method has a body, whatever a probe can do with it.
#include <stdexcept>
#include <string>

namespace virt {
class Split {
public:
  Split();
  virtual ~Split();
  virtual long post(long amount);

private:
  long total_ = 0;
};
Split::Split() = default;
Split::~Split() = default;
__attribute__((noinline)) long Split::post(long amount) {
  if (amount < 0)
    throw std::invalid_argument("negative amount " + std::to_string(amount));
  total_ += amount;
  return total_;
}
long split_once(long amount) {
  static Split split;
  return split.post(amount);
}
} // namespace virt
