#include "context_journal.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>

#include "context_transport.pb.h"
#include "tracer/compat.hpp"

namespace tracer::context_delivery {
namespace {
constexpr std::uint32_t kMagic = 0x4a435458; // XCTJ, little endian on disk.
constexpr std::size_t kHeaderSize = 24;

std::uint32_t checksum(std::string_view bytes) {
  std::uint32_t value = 2166136261u;
  for (const unsigned char byte : bytes) value = (value ^ byte) * 16777619u;
  return value;
}
void put32(std::array<char, kHeaderSize> &h, std::size_t at, std::uint32_t v) {
  for (unsigned i = 0; i < 4; ++i) h[at + i] = static_cast<char>(v >> (8 * i));
}
void put64(std::array<char, kHeaderSize> &h, std::size_t at, std::uint64_t v) {
  for (unsigned i = 0; i < 8; ++i) h[at + i] = static_cast<char>(v >> (8 * i));
}
std::uint32_t get32(const std::array<char, kHeaderSize> &h, std::size_t at) {
  std::uint32_t v = 0;
  for (unsigned i = 0; i < 4; ++i) v |= std::uint32_t(static_cast<unsigned char>(h[at+i])) << (8*i);
  return v;
}
std::uint64_t get64(const std::array<char, kHeaderSize> &h, std::size_t at) {
  std::uint64_t v = 0;
  for (unsigned i = 0; i < 8; ++i) v |= std::uint64_t(static_cast<unsigned char>(h[at+i])) << (8*i);
  return v;
}
bool sync_file(const std::filesystem::path &path) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) return false;
  const bool ok = ::fsync(fd) == 0;
  ::close(fd);
  return ok;
}
std::string budget_key_for(const std::filesystem::path &path) {
  std::error_code ec;
  const auto absolute = std::filesystem::absolute(path, ec);
  return (ec ? path : absolute).lexically_normal().string();
}
}

JournalLimits journal_limits_for_packet(std::uint32_t packet_bytes,
                                      std::uint64_t schema_bytes) {
  JournalLimits limits;
  // The producer persists the event together with schema announcements.
  limits.segment_limit = std::max(limits.segment_limit,
                                  packet_bytes + schema_bytes + 4096);
  limits.data_budget = std::max(limits.data_budget, limits.segment_limit);
  return limits;
}

bool JournalBudget::claim(std::string key, std::uint64_t bytes) {
  std::lock_guard lock(mutex_);
  const auto found = charges_.find(key);
  const auto old = found == charges_.end() ? 0 : found->second;
  if (bytes > old && bytes - old > limit_ - used_) return false;
  used_ = used_ - old + bytes;
  if (bytes) charges_[std::move(key)] = bytes;
  else charges_.erase(key);
  return true;
}
bool JournalBudget::reserve(const std::string &key, std::uint64_t bytes) {
  std::lock_guard lock(mutex_);
  if (bytes > limit_ - used_) return false;
  charges_[key] += bytes; used_ += bytes; return true;
}
void JournalBudget::release(const std::string &key, std::uint64_t bytes) noexcept {
  std::lock_guard lock(mutex_);
  const auto found = charges_.find(key);
  if (found == charges_.end()) return;
  const auto count = std::min(bytes, found->second);
  found->second -= count; used_ -= count;
  if (!found->second) charges_.erase(found);
}
std::uint64_t JournalBudget::used() const noexcept { std::lock_guard lock(mutex_); return used_; }
bool JournalBudget::admit_control(const std::string &key) {
  std::lock_guard lock(mutex_);
  if (::tracer::compat::contains(control_keys_, key)) {
    return true;
  }
  if (kControlOperationLimit > kControlReserve - control_used_) return false;
  control_keys_.emplace(key, true); control_used_ += kControlOperationLimit; return true;
}
bool JournalBudget::claim_control(const std::string &key) { return admit_control(key); }
std::uint64_t JournalBudget::control_used() const noexcept { std::lock_guard lock(mutex_); return control_used_; }
void JournalBudget::ensure_data_capacity(std::uint64_t minimum) noexcept {
  std::lock_guard lock(mutex_);
  limit_ = std::max(limit_, minimum);
}
std::shared_ptr<JournalBudget> process_journal_budget(std::uint64_t minimum) {
  static auto budget = std::make_shared<JournalBudget>();
  budget->ensure_data_capacity(minimum);
  return budget;
}

ContextJournal::ContextJournal(std::filesystem::path directory, JournalLimits limits,
                              std::shared_ptr<JournalBudget> budget)
    : directory_(std::move(directory)),
      state_path_(directory_ / "checkpoint.bin"), limits_(limits),
      budget_(budget ? std::move(budget) : process_journal_budget(limits.data_budget)), budget_key_(budget_key_for(directory_)) {
  std::error_code ec;
  std::filesystem::create_directories(directory_, ec);
  if (ec) { fail("create journal directory: " + ec.message()); return; }
  recover();
}

std::filesystem::path ContextJournal::segment_path(std::uint64_t index) const {
  std::ostringstream name;
  name << "segment-" << std::setw(8) << std::setfill('0') << index << ".bin";
  return directory_ / name.str();
}

bool ContextJournal::healthy() const noexcept { std::lock_guard lock(mutex_); return error_.empty(); }
std::string ContextJournal::error() const { std::lock_guard lock(mutex_); return error_; }
JournalState ContextJournal::state() const { std::lock_guard lock(mutex_); return state_; }
std::uint64_t ContextJournal::committed_prefix() const noexcept { std::lock_guard lock(mutex_); return records_.empty() ? state_.acknowledged : records_.back().position; }
std::uint64_t ContextJournal::data_bytes() const noexcept { std::lock_guard lock(mutex_); return data_bytes_; }

void ContextJournal::fail(std::string message) noexcept { std::lock_guard lock(mutex_); if (error_.empty()) error_ = std::move(message); }

bool ContextJournal::persist_state(const JournalState &next) {
  if (limits_.fail_operation && limits_.fail_operation("checkpoint")) return false;
  const auto temp = state_path_.string() + ".tmp";
  std::ofstream out(temp, std::ios::binary | std::ios::trunc);
  constexpr std::uint32_t magic = 0x53545843;
  const std::uint32_t id_size = static_cast<std::uint32_t>(next.recording_id.size());
  const std::uint32_t sink_size = static_cast<std::uint32_t>(next.sink_id.size());
  out.write(reinterpret_cast<const char *>(&magic), sizeof(magic));
  out.write(reinterpret_cast<const char *>(&id_size), sizeof(id_size));
  out.write(reinterpret_cast<const char *>(&sink_size), sizeof(sink_size));
  out.write(reinterpret_cast<const char *>(&next.acknowledged), sizeof(next.acknowledged));
  const char confirmed = next.creation_confirmed ? 1 : 0;
  out.write(&confirmed, 1);
  out.write(next.recording_id.data(), next.recording_id.size());
  out.write(next.sink_id.data(), next.sink_id.size());
  out.flush();
  if (!out) return false;
  out.close();
  if (!sync_file(temp)) return false;
  std::error_code ec;
  std::filesystem::rename(temp, state_path_, ec);
  if (ec) return false;
  const int dfd = ::open(directory_.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  const bool synced = dfd >= 0 && ::fsync(dfd) == 0;
  if (dfd >= 0) ::close(dfd);
  return synced;
}

bool ContextJournal::begin_create(std::string id, std::string sink) {
  std::lock_guard lock(mutex_);
  if (!error_.empty() || id.empty() || sink.empty() || id.size() > 1024 || sink.size() > 1024) return false;
  if (!state_.recording_id.empty()) return state_.recording_id == id && state_.sink_id == sink;
  if (!budget_->admit_control(budget_key_)) return false;
  JournalState next{std::move(id), std::move(sink), 0, false};
  if (!persist_state(next)) { error_ = "persist CREATE intent failed"; return false; }
  state_ = std::move(next);
  return true;
}

bool ContextJournal::confirm_create(std::string_view id, std::uint64_t prefix) {
  std::lock_guard lock(mutex_);
  const std::uint64_t committed = records_.empty() ? state_.acknowledged : records_.back().position;
  if (!error_.empty() || state_.recording_id != id || prefix > committed) return false;
  JournalState next = state_;
  next.creation_confirmed = true;
  next.acknowledged = prefix;
  if (!persist_state(next)) { error_ = "persist CREATE confirmation failed"; return false; }
  state_ = std::move(next);
  return true;
}

bool ContextJournal::append(const clangtracer::context::v1::ContextFrame &frame, std::uint64_t &position) {
  if (!frame.has_event()) return false;
  clangtracer::context::v1::ContextBatch batch;
  *batch.mutable_event() = frame.event();
  if (frame.has_schema()) *batch.add_schemas() = frame.schema();
  return append(batch, position);
}

bool ContextJournal::append(const clangtracer::context::v1::ContextBatch &batch,
                            std::uint64_t &position,
                            std::size_t max_replay_bytes) {
  std::lock_guard lock(mutex_);
  if (!error_.empty()) return false;
  if (limits_.fail_operation && limits_.fail_operation("record_write")) return false;
  if (!batch.has_event()) return false;
  const auto current_prefix = records_.empty() ? state_.acknowledged : records_.back().position;
  if (current_prefix == std::numeric_limits<std::uint64_t>::max()) return false;
  const std::uint64_t next_pos = current_prefix + 1;
  auto committed_batch = batch;
  for (auto &schema : *committed_batch.mutable_schemas()) schema.clear_epoch_schema_id();
  auto *event = committed_batch.mutable_event();
  if (event->recording_position() != 0 && event->recording_position() != next_pos) return false;
  event->set_recording_position(next_pos);
  for (auto &state : *event->mutable_states()) state.clear_epoch_schema_id();
  std::string payload;
  if (!committed_batch.SerializeToString(&payload) || payload.empty() ||
      payload.size() > limits_.segment_limit - kHeaderSize ||
      payload.size() > max_replay_bytes) return false;
  const std::uint64_t charge = kHeaderSize + payload.size();
  if (charge > limits_.data_budget || data_bytes_ > limits_.data_budget - charge) return false;
  if (!budget_->reserve(budget_key_, charge)) return false;
  if (current_segment_bytes_ + charge > limits_.segment_limit) {
    ++current_segment_; current_segment_bytes_ = 0;
  }
  std::array<char, kHeaderSize> h{};
  put32(h, 0, kMagic); put32(h, 4, static_cast<std::uint32_t>(payload.size()));
  put32(h, 8, checksum(payload)); put64(h, 12, next_pos); put32(h, 20, 1);
  const auto path = segment_path(current_segment_);
  std::ofstream out(path, std::ios::binary | std::ios::app);
  out.write(h.data(), h.size()); out.write(payload.data(), payload.size()); out.flush();
  if (!out) { budget_->release(budget_key_, charge); error_ = "append journal record failed"; return false; }
  out.close();
  if (!sync_file(path)) { error_ = "flush journal record failed"; return false; }
  records_.push_back({next_pos, std::move(payload), current_segment_});
  data_bytes_ += charge;
  current_segment_bytes_ += charge;
  position = next_pos;
  return true;
}

std::vector<JournalRecord> ContextJournal::read_after(std::uint64_t position, std::size_t max_records, std::size_t max_bytes) const {
  std::lock_guard lock(mutex_);
  std::vector<JournalRecord> out;
  if (!state_.creation_confirmed || !error_.empty()) return out;
  std::size_t bytes = 0;
  for (const auto &record : records_) {
    if (record.position <= position) continue;
    if (out.size() >= max_records || record.serialized_frame.size() > max_bytes - bytes) break;
    bytes += record.serialized_frame.size(); out.push_back(record);
  }
  return out;
}

bool ContextJournal::save_ack(std::string_view id, std::uint64_t prefix) {
  std::lock_guard lock(mutex_);
  if (!error_.empty() || !state_.creation_confirmed || state_.recording_id != id || prefix < state_.acknowledged) return false;
  const auto committed = records_.empty() ? state_.acknowledged : records_.back().position;
  if (prefix > committed) return false;
  JournalState next = state_; next.acknowledged = prefix;
  if (!persist_state(next)) { error_ = "persist ACK checkpoint failed"; return false; }
  state_ = std::move(next);
  reclaim_acknowledged();
  return true;
}

bool ContextJournal::recover() {
  std::lock_guard lock(mutex_);
  if (std::filesystem::exists(state_path_)) {
    if (!budget_->claim_control(budget_key_)) { error_ = "global journal control reserve exhausted"; return false; }
    std::ifstream in(state_path_, std::ios::binary);
    std::uint32_t magic = 0, idn = 0, sn = 0; char confirmed = 0;
    in.read(reinterpret_cast<char *>(&magic), 4); in.read(reinterpret_cast<char *>(&idn), 4);
    in.read(reinterpret_cast<char *>(&sn), 4); in.read(reinterpret_cast<char *>(&state_.acknowledged), 8); in.read(&confirmed, 1);
    if (!in || magic != 0x53545843 || idn > 1024 || sn > 1024) { error_ = "invalid journal checkpoint"; return false; }
    state_.recording_id.resize(idn); state_.sink_id.resize(sn); state_.creation_confirmed = confirmed != 0;
    in.read(state_.recording_id.data(), idn); in.read(state_.sink_id.data(), sn);
    if (!in) { error_ = "truncated journal checkpoint"; return false; }
  }
  std::vector<std::pair<std::uint64_t, std::filesystem::path>> segments;
  for (const auto &entry : std::filesystem::directory_iterator(directory_)) {
    const auto name = entry.path().filename().string();
    if (!::tracer::compat::starts_with(name, "segment-") ||
        !::tracer::compat::ends_with(name, ".bin")) {
      continue;
    }
    try { segments.emplace_back(std::stoull(name.substr(8, name.size() - 12)), entry.path()); }
    catch (...) { error_ = "invalid journal segment name"; return false; }
  }
  std::sort(segments.begin(), segments.end());
  for (const auto &[segment, path] : segments) {
    std::ifstream in(path, std::ios::binary);
    std::uint64_t valid_end = 0;
    while (true) {
      std::array<char, kHeaderSize> h{}; in.read(h.data(), h.size());
      if (in.gcount() == 0) break;
      if (in.gcount() != static_cast<std::streamsize>(h.size()) || get32(h,0) != kMagic || get32(h,20) != 1) break;
      const auto size = get32(h,4);
      const auto pos = get64(h,12);
      if (size == 0) break;
      if (size > limits_.segment_limit - kHeaderSize) {
        error_ = "journal record exceeds configured segment capacity";
        return false;
      }
      std::string payload(size, '\0'); in.read(payload.data(), size);
      if (!in || checksum(payload) != get32(h,8)) break;
      clangtracer::context::v1::ContextBatch batch;
      if (!batch.ParseFromString(payload) || !batch.has_event() ||
          batch.event().recording_position() != pos) break;
      records_.push_back({pos, std::move(payload), segment});
      data_bytes_ += kHeaderSize + size; valid_end += kHeaderSize + size;
      if (data_bytes_ > limits_.data_budget || valid_end > limits_.segment_limit) { error_ = "journal segment or data budget exceeded"; return false; }
    }
    std::error_code ec; const auto file_size = std::filesystem::file_size(path, ec);
    if (!ec && file_size != valid_end) std::filesystem::resize_file(path, valid_end, ec);
    if (ec) { error_ = "truncate incomplete journal tail: " + ec.message(); return false; }
    if (segment >= current_segment_) { current_segment_ = segment; current_segment_bytes_ = valid_end; }
  }
  if (!records_.empty() && records_.back().position < state_.acknowledged) { error_ = "checkpoint exceeds committed journal prefix"; return false; }
  for (std::size_t i = 1; i < records_.size(); ++i)
    if (records_[i].position != records_[i-1].position + 1) { error_ = "journal position gap"; return false; }
  reclaim_acknowledged();
  if (!budget_->claim(budget_key_, data_bytes_)) { error_ = "global journal data budget exhausted"; return false; }
  return true;
}

bool ContextJournal::reclaim_acknowledged() {
  std::vector<std::uint64_t> reclaim;
  for (const auto &record : records_) {
    if (record.position > state_.acknowledged) continue;
    if (reclaim.empty() || reclaim.back() != record.segment_index) reclaim.push_back(record.segment_index);
  }
  for (auto segment : reclaim) {
    const bool has_unacked = std::any_of(records_.begin(), records_.end(), [&](const JournalRecord &record) {
      return record.segment_index == segment && record.position > state_.acknowledged;
    });
    if (has_unacked) continue;
    std::error_code ec;
    const auto path = segment_path(segment);
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec) continue;
    std::filesystem::remove(path, ec);
    if (ec) continue;
    data_bytes_ -= bytes;
    budget_->release(budget_key_, bytes);
    ::tracer::compat::erase_if(
        records_, [&](const JournalRecord& record) { return record.segment_index == segment; });
    if (segment == current_segment_) current_segment_bytes_ = 0;
  }
  if (!records_.empty()) return true;
  // Keep the next identity monotonic after all acknowledged segments are gone.
  return true;
}

ContextCollectorStore::ContextCollectorStore(std::filesystem::path directory, JournalLimits limits,
                                             std::shared_ptr<JournalBudget> budget)
    : directory_(std::move(directory)), checkpoint_path_(directory_ / "collector-checkpoint.bin"), limits_(limits),
      budget_(budget ? std::move(budget) : process_journal_budget(limits.data_budget)), budget_key_(budget_key_for(directory_)) {
  std::error_code ec;
  std::filesystem::create_directories(directory_, ec);
  if (ec) error_ = "create collector directory: " + ec.message();
  else recover();
}

std::filesystem::path ContextCollectorStore::segment_path(std::uint64_t index) const {
  std::ostringstream name;
  name << "collector-segment-" << std::setw(8) << std::setfill('0') << index << ".bin";
  return directory_ / name.str();
}

std::uint64_t ContextCollectorStore::durable_prefix() const noexcept {
  std::lock_guard lock(mutex_); return prefix_;
}

bool ContextCollectorStore::persist_checkpoint() {
  if (limits_.fail_operation && limits_.fail_operation("checkpoint")) return false;
  const auto temp = checkpoint_path_.string() + ".tmp";
  std::ofstream out(temp, std::ios::binary | std::ios::trunc);
  const std::uint32_t magic = 0x43545843;
  const std::uint32_t rn = static_cast<std::uint32_t>(recording_id_.size());
  const std::uint32_t sn = static_cast<std::uint32_t>(sink_id_.size());
  out.write(reinterpret_cast<const char *>(&magic), 4);
  out.write(reinterpret_cast<const char *>(&rn), 4);
  out.write(reinterpret_cast<const char *>(&sn), 4);
  out.write(reinterpret_cast<const char *>(&prefix_), 8);
  out.write(recording_id_.data(), recording_id_.size());
  out.write(sink_id_.data(), sink_id_.size());
  out.flush();
  if (!out) return false;
  out.close();
  if (!sync_file(temp)) return false;
  std::error_code ec; std::filesystem::rename(temp, checkpoint_path_, ec);
  if (ec) return false;
  const int dfd = ::open(directory_.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  const bool ok = dfd >= 0 && ::fsync(dfd) == 0;
  if (dfd >= 0) ::close(dfd);
  return ok;
}

CollectorOpenResult ContextCollectorStore::open(bool create, std::string recording_id,
    std::string sink_id, std::uint64_t sender_ack) {
  std::lock_guard lock(mutex_);
  if (!error_.empty()) return {false, prefix_, error_};
  if (recording_id.empty() || sink_id.empty() || recording_id.size() > 1024 || sink_id.size() > 1024)
    return {false, prefix_, "invalid durable recording identity"};
  if (create) {
    if (recording_id_.empty()) {
      if (!budget_->admit_control(budget_key_)) return {false, 0, "global collector control reserve exhausted"};
      recording_id_ = std::move(recording_id); sink_id_ = std::move(sink_id); prefix_ = 0;
      if (!persist_checkpoint()) { error_ = "persist CREATE checkpoint zero failed"; return {false, 0, error_}; }
      return {true, 0, {}};
    }
    if (recording_id_ != recording_id || sink_id_ != sink_id)
      return {false, prefix_, "CREATE identity conflicts with existing recording"};
    return {true, prefix_, {}}; // Idempotent retry; never resets a checkpoint.
  }
  if (recording_id_ != recording_id || sink_id_ != sink_id)
    return {false, prefix_, "RESUME checkpoint missing or incompatible"};
  if (sender_ack > prefix_) return {false, prefix_, "sender checkpoint exceeds collector prefix"};
  return {true, prefix_, {}};
}

CollectorOpenResult ContextCollectorStore::persist(
    const clangtracer::context::v1::ContextFrame &frame, std::uint64_t position) {
  std::lock_guard lock(mutex_);
  if (!error_.empty() || recording_id_.empty()) return {false, prefix_, "recording is not created"};
  if (limits_.fail_operation && limits_.fail_operation("record_write"))
    return {false, prefix_, "injected collector record write failure"};
  auto durable_frame = frame;
  if ((!durable_frame.has_schema() && !durable_frame.has_event()) ||
      (durable_frame.has_schema() && position != 0) ||
      (durable_frame.has_event() && position == 0))
    return {false, prefix_, "frame kind does not match durable position"};
  if (durable_frame.has_schema()) durable_frame.mutable_schema()->set_epoch_schema_id(0);
  if (durable_frame.has_event())
    for (auto &state : *durable_frame.mutable_event()->mutable_states()) state.set_epoch_schema_id(0);
  std::string payload;
  constexpr std::uint64_t header_size = 24;
  if (!durable_frame.SerializeToString(&payload) || payload.empty() || payload.size() > limits_.segment_limit - header_size)
    return {false, prefix_, "invalid or oversized durable frame"};
  std::uint64_t hash = 1469598103934665603ull;
  for (const unsigned char c : payload) hash = (hash ^ c) * 1099511628211ull;
  if (position != 0 && position <= prefix_) {
    if (position > hashes_.size() || hashes_[position - 1] != hash)
      return {false, prefix_, "conflicting reuse of durable recording position"};
    return {true, prefix_, {}};
  }
  if (position != 0 && position != prefix_ + 1)
    return {false, prefix_, "durable recording position has a gap"};
  const std::uint64_t charge = payload.size() + header_size;
  if (charge > limits_.data_budget || bytes_ > limits_.data_budget - charge)
    return {false, prefix_, "collector durable data budget exhausted"};
  if (!budget_->reserve(budget_key_, charge)) return {false, prefix_, "global durable data budget exhausted"};
  if (current_segment_bytes_ + charge > limits_.segment_limit) {
    ++current_segment_; current_segment_bytes_ = 0;
  }
  std::ofstream out(segment_path(current_segment_), std::ios::binary | std::ios::app);
  const std::uint32_t magic = 0x52585443, size = static_cast<std::uint32_t>(payload.size());
  const std::uint32_t check = checksum(payload);
  out.write(reinterpret_cast<const char *>(&magic), 4);
  out.write(reinterpret_cast<const char *>(&size), 4);
  out.write(reinterpret_cast<const char *>(&check), 4);
  out.write(reinterpret_cast<const char *>(&position), 8);
  const std::uint32_t version = 1;
  out.write(reinterpret_cast<const char *>(&version), 4);
  out.write(payload.data(), payload.size()); out.flush();
  if (!out) { budget_->release(budget_key_, charge); error_ = "append collector record failed"; return {false, prefix_, error_}; }
  out.close();
  if (!sync_file(segment_path(current_segment_))) { error_ = "flush collector record failed"; return {false, prefix_, error_}; }
  bytes_ += charge;
  current_segment_bytes_ += charge;
  if (position != 0) {
    hashes_.push_back(hash);
    const auto prior_prefix = prefix_;
    prefix_ = position;
    if (!persist_checkpoint()) {
      prefix_ = prior_prefix;
      error_ = "persist collector durable prefix failed";
      return {false, prefix_, error_};
    }
  }
  return {true, prefix_, {}};
}

bool ContextCollectorStore::recover() {
  std::lock_guard lock(mutex_);
  if (std::filesystem::exists(checkpoint_path_)) {
    if (!budget_->claim_control(budget_key_)) { error_ = "global collector control reserve exhausted"; return false; }
    std::ifstream in(checkpoint_path_, std::ios::binary);
    std::uint32_t magic = 0, rn = 0, sn = 0; std::uint64_t saved = 0;
    in.read(reinterpret_cast<char *>(&magic), 4); in.read(reinterpret_cast<char *>(&rn), 4);
    in.read(reinterpret_cast<char *>(&sn), 4); in.read(reinterpret_cast<char *>(&saved), 8);
    if (!in || magic != 0x43545843 || rn > 1024 || sn > 1024) { error_ = "invalid collector checkpoint"; return false; }
    recording_id_.resize(rn); sink_id_.resize(sn);
    in.read(recording_id_.data(), rn); in.read(sink_id_.data(), sn);
    if (!in) { error_ = "truncated collector checkpoint"; return false; }
    prefix_ = saved;
  }
  std::vector<std::pair<std::uint64_t, std::filesystem::path>> segments;
  for (const auto &entry : std::filesystem::directory_iterator(directory_)) {
    const auto name = entry.path().filename().string();
    if (!::tracer::compat::starts_with(name, "collector-segment-") ||
        !::tracer::compat::ends_with(name, ".bin")) {
      continue;
    }
    try { segments.emplace_back(std::stoull(name.substr(18, name.size() - 22)), entry.path()); }
    catch (...) { error_ = "invalid collector segment name"; return false; }
  }
  std::sort(segments.begin(), segments.end());
  std::uint64_t recovered_prefix = 0;
  for (const auto &[segment, path] : segments) {
    std::ifstream in(path, std::ios::binary);
    std::uint64_t valid_end = 0;
    while (true) {
      std::uint32_t magic = 0, size = 0, check = 0, version = 0; std::uint64_t position = 0;
      in.read(reinterpret_cast<char *>(&magic), 4);
      if (in.gcount() == 0) break;
      in.read(reinterpret_cast<char *>(&size), 4); in.read(reinterpret_cast<char *>(&check), 4);
      in.read(reinterpret_cast<char *>(&position), 8); in.read(reinterpret_cast<char *>(&version), 4);
      if (!in || magic != 0x52585443 || version != 1 || size == 0) break;
      if (size > limits_.segment_limit - 24) {
        error_ = "collector record exceeds configured segment capacity";
        return false;
      }
      std::string payload(size, '\0'); in.read(payload.data(), size);
      clangtracer::context::v1::ContextFrame frame;
      if (!in || checksum(payload) != check || !frame.ParseFromString(payload)) break;
      if ((position == 0 && !frame.has_schema()) || (position != 0 && !frame.has_event())) break;
      const auto charge = size + 24;
      if (charge > limits_.data_budget || bytes_ > limits_.data_budget - charge) { error_ = "collector journal budget exceeded"; return false; }
      bytes_ += charge; valid_end += charge;
      if (position != 0) {
        std::uint64_t hash = 1469598103934665603ull;
        for (const unsigned char c : payload) hash = (hash ^ c) * 1099511628211ull;
        if (position != recovered_prefix + 1) { error_ = "collector journal position gap"; return false; }
        hashes_.push_back(hash); recovered_prefix = position;
      }
    }
    std::error_code ec; const auto full = std::filesystem::file_size(path, ec);
    if (!ec && full != valid_end) std::filesystem::resize_file(path, valid_end, ec);
    if (ec) { error_ = "truncate collector journal tail: " + ec.message(); return false; }
    if (valid_end > limits_.segment_limit) { error_ = "collector segment exceeded capacity"; return false; }
    if (segment >= current_segment_) { current_segment_ = segment; current_segment_bytes_ = valid_end; }
  }
  if (!budget_->claim(budget_key_, bytes_)) { error_ = "global collector data budget exhausted"; return false; }
  if (prefix_ > recovered_prefix) { error_ = "collector checkpoint exceeds committed records"; return false; }
  if (recovered_prefix > prefix_) {
    prefix_ = recovered_prefix;
    if (!persist_checkpoint()) { error_ = "recover collector prefix checkpoint failed"; return false; }
  }
  return true;
}
} // namespace tracer::context_delivery
