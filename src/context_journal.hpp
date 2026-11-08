#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <vector>

namespace clangtracer::context::v1 { class ContextFrame; class ContextBatch; }

namespace tracer::context_delivery {

inline constexpr std::uint64_t kJournalBudget = 1024ull * 1024 * 1024;
inline constexpr std::uint64_t kControlReserve = 16ull * 1024 * 1024;
inline constexpr std::uint64_t kDataBudget = kJournalBudget - kControlReserve;
inline constexpr std::uint64_t kSegmentLimit = 64ull * 1024 * 1024;
inline constexpr std::uint64_t kControlOperationLimit = 256ull * 1024;

struct JournalRecord {
  std::uint64_t position = 0;
  std::string serialized_frame;
  std::uint64_t segment_index = 0;
};

struct JournalState {
  std::string recording_id;
  std::string sink_id;
  std::uint64_t acknowledged = 0;
  bool creation_confirmed = false;
};

struct JournalLimits {
  std::uint64_t data_budget = kDataBudget;
  std::uint64_t segment_limit = kSegmentLimit;
  std::function<bool(std::string_view)> fail_operation;
};

JournalLimits journal_limits_for_packet(std::uint32_t packet_bytes,
                                      std::uint64_t schema_bytes);

class JournalBudget final {
public:
  explicit JournalBudget(std::uint64_t limit = kDataBudget) : limit_(limit) {}
  bool claim(std::string key, std::uint64_t bytes);
  bool reserve(const std::string &key, std::uint64_t bytes);
  void release(const std::string &key, std::uint64_t bytes) noexcept;
  bool admit_control(const std::string &key);
  bool claim_control(const std::string &key);
  std::uint64_t used() const noexcept;
  std::uint64_t control_used() const noexcept;
  void ensure_data_capacity(std::uint64_t minimum) noexcept;
private:
  std::uint64_t limit_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::uint64_t> charges_;
  std::uint64_t used_ = 0;
  std::unordered_map<std::string, bool> control_keys_;
  std::uint64_t control_used_ = 0;
};
std::shared_ptr<JournalBudget> process_journal_budget(std::uint64_t minimum = kDataBudget);

struct CollectorOpenResult {
  bool accepted = false;
  std::uint64_t durable_prefix = 0;
  std::string detail;
};

// Durable collector-side CREATE/RESUME and position-deduplicating store.
class ContextCollectorStore final {
public:
  explicit ContextCollectorStore(std::filesystem::path directory,
                                 JournalLimits limits = {},
                                 std::shared_ptr<JournalBudget> budget = {});
  CollectorOpenResult open(bool create, std::string recording_id,
                           std::string sink_id, std::uint64_t sender_ack);
  CollectorOpenResult persist(const clangtracer::context::v1::ContextFrame &frame,
                              std::uint64_t recording_position);
  std::uint64_t durable_prefix() const noexcept;
private:
  bool recover();
  bool persist_checkpoint();
  std::filesystem::path segment_path(std::uint64_t index) const;
  const std::filesystem::path directory_, checkpoint_path_;
  const JournalLimits limits_;
  const std::shared_ptr<JournalBudget> budget_;
  const std::string budget_key_;
  mutable std::mutex mutex_;
  std::string recording_id_, sink_id_, error_;
  std::uint64_t prefix_ = 0, bytes_ = 0;
  std::uint64_t current_segment_ = 0, current_segment_bytes_ = 0;
  std::vector<std::uint64_t> hashes_;
};

class ContextJournal final {
public:
  explicit ContextJournal(std::filesystem::path directory, JournalLimits limits = {},
                          std::shared_ptr<JournalBudget> budget = {});
  ContextJournal(const ContextJournal &) = delete;
  ContextJournal &operator=(const ContextJournal &) = delete;

  bool healthy() const noexcept;
  std::string error() const;
  JournalState state() const;
  // CREATE intent is persisted before a CREATE packet may be sent.
  bool begin_create(std::string recording_id, std::string sink_id);
  // Confirmation is persisted before any journal record is eligible to send.
  bool confirm_create(std::string_view recording_id, std::uint64_t prefix);
  bool append(const clangtracer::context::v1::ContextFrame &frame,
              std::uint64_t &position);
  bool append(const clangtracer::context::v1::ContextBatch &batch,
              std::uint64_t &position);
  std::vector<JournalRecord> read_after(std::uint64_t position,
                                        std::size_t max_records,
                                        std::size_t max_bytes) const;
  // Save the durable prefix before reclaiming any committed bytes.
  bool save_ack(std::string_view recording_id, std::uint64_t prefix);
  std::uint64_t committed_prefix() const noexcept;
  std::uint64_t data_bytes() const noexcept;

private:
  bool recover();
  bool reclaim_acknowledged();
  std::filesystem::path segment_path(std::uint64_t index) const;
  bool persist_state(const JournalState &next);
  void fail(std::string message) noexcept;

  const std::filesystem::path directory_;
  const std::filesystem::path state_path_;
  const JournalLimits limits_;
  const std::shared_ptr<JournalBudget> budget_;
  const std::string budget_key_;
  mutable std::mutex mutex_;
  JournalState state_;
  std::vector<JournalRecord> records_;
  std::uint64_t data_bytes_ = 0;
  std::uint64_t current_segment_ = 0;
  std::uint64_t current_segment_bytes_ = 0;
  std::string error_;
};

} // namespace tracer::context_delivery
