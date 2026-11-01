#include "context_receiver.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <functional>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace tracer::context_engine {
struct SchemaEntry {
  StateFamilyKey family;
  std::string root, descriptor_bytes, identity;
  std::shared_ptr<const SchemaRecord> record;
  std::size_t charge = 0;
  google::protobuf::DescriptorPool pool;
  mutable google::protobuf::DynamicMessageFactory factory{&pool};
  const google::protobuf::Descriptor *descriptor = nullptr;
  std::shared_ptr<void> global_lease;
};
struct ReceiverLedger::State {
  mutable std::mutex mutex;
  ReceiverLimits limits;
  std::size_t schema_bytes = 0, decoded_bytes = 0;
};
struct ContextReceiver::Mapping {
  std::shared_ptr<const SchemaEntry> entry;
  std::shared_ptr<void> epoch_lease;
};

ReceiverSchemaView::ReceiverSchemaView(std::shared_ptr<const SchemaEntry> entry)
    : entry_(std::move(entry)) {}

const SchemaRecord &ReceiverSchemaView::record() const noexcept {
  return *entry_->record;
}

const google::protobuf::Descriptor *
ReceiverSchemaView::root_descriptor() const noexcept {
  return entry_->descriptor;
}

const StateFamilyKey &ReceiverSchemaView::family() const noexcept {
  return entry_->family;
}

std::string_view ReceiverSchemaView::root_type() const noexcept {
  return entry_->root;
}

std::string_view ReceiverSchemaView::revision_id() const noexcept {
  return entry_->identity;
}

std::span<const std::string>
ReceiverSchemaView::container_roles() const noexcept {
  return entry_->record->container_roles;
}

namespace {
std::uint64_t next_legacy_epoch_nonce() {
  static std::atomic<std::uint64_t> sequence{1};
  const auto clock = static_cast<std::uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const auto serial = sequence.fetch_add(1, std::memory_order_relaxed);
  return clock ^ (serial + 0x9e3779b97f4a7c15ULL + (clock << 6) + (clock >> 2));
}
std::shared_ptr<void>
charge_token(const std::shared_ptr<ReceiverLedger::State> &s, std::size_t n,
             bool schema) {
  // Caller reserves under State::mutex; destruction returns the exact charge.
  return std::shared_ptr<void>(new std::size_t(n), [s, schema](void *p) {
    auto *n = static_cast<std::size_t *>(p);
    {
      std::lock_guard lock(s->mutex);
      auto &used = schema ? s->schema_bytes : s->decoded_bytes;
      used -= std::min(used, *n);
    }
    delete n;
  });
}
bool validate_set(const google::protobuf::FileDescriptorSet &set,
                  const ReceiverLimits &l, std::string &err,
                  std::size_t &field_count, std::size_t &message_count) {
  std::size_t fields = 0, messages = 0;
  if (set.file_size() > static_cast<int>(l.files)) {
    err = "descriptor file limit exceeded";
    return false;
  }
  for (const auto &f : set.file()) {
    if (f.message_type_size() < 0) {
      err = "invalid descriptor";
      return false;
    }
    for (const auto &m : f.message_type()) {
      std::vector<
          std::pair<const google::protobuf::DescriptorProto *, std::size_t>>
          todo{{&m, 1}};
      while (!todo.empty()) {
        auto [cur, d] = todo.back();
        todo.pop_back();
        if (++messages > l.messages) {
          err = "descriptor message limit exceeded";
          return false;
        }
        if (d > l.depth) {
          err = "descriptor nesting limit exceeded";
          return false;
        }
        fields += cur->field_size();
        if (fields > l.fields) {
          err = "descriptor field limit exceeded";
          return false;
        }
        for (const auto &n : cur->nested_type())
          todo.emplace_back(&n, d + 1);
      }
    }
  }
  field_count = fields;
  message_count = messages;
  return true;
}
bool validate_descriptor_shape(const google::protobuf::FileDescriptorSet &set,
                               const std::string &root, std::string &err) {
  std::unordered_set<std::string> files, messages, enums;
  std::vector<const google::protobuf::FieldDescriptorProto *> fields;
  std::function<void(const google::protobuf::DescriptorProto &,
                     const std::string &)>
      add_message;
  add_message = [&](const auto &m, const std::string &parent) {
    const auto full = parent.empty() ? m.name() : parent + "." + m.name();
    if (m.name().empty() || !messages.insert(full).second) {
      err = "duplicate or empty message name";
      return;
    }
    std::unordered_set<int> numbers;
    for (const auto &f : m.field()) {
      if (f.name().empty() || f.number() <= 0 || f.number() >= (1 << 29) ||
          (f.number() >= 19000 && f.number() <= 19999) ||
          !numbers.insert(f.number()).second) {
        err = "invalid or duplicate descriptor field";
        return;
      }
      if (!f.has_type() || !f.has_label()) {
        err = "descriptor field has no type or label";
        return;
      }
      fields.push_back(&f);
    }
    for (const auto &e : m.enum_type()) {
      const auto en = full + "." + e.name();
      if (e.name().empty() || !enums.insert(en).second) {
        err = "duplicate or empty enum name";
        return;
      }
    }
    for (const auto &n : m.nested_type())
      add_message(n, full);
  };
  for (const auto &file : set.file()) {
    if (file.name().empty() || !files.insert(file.name()).second) {
      err = "duplicate or empty descriptor file name";
      return false;
    }
    const auto package = file.package();
    for (const auto &m : file.message_type())
      add_message(m, package);
    for (const auto &e : file.enum_type()) {
      const auto full = package.empty() ? e.name() : package + "." + e.name();
      if (e.name().empty() || !enums.insert(full).second) {
        err = "duplicate or empty top-level enum name";
        return false;
      }
    }
  }
  if (!err.empty())
    return false;
  for (const auto &file : set.file())
    for (const auto &dependency : file.dependency())
      if (!files.contains(dependency)) {
        err = "descriptor dependency closure is incomplete";
        return false;
      }
  if (!messages.contains(root)) {
    err = "root message absent from descriptor bundle";
    return false;
  }
  for (const auto *f : fields) {
    using F = google::protobuf::FieldDescriptorProto;
    const auto t = f->type();
    if (t == F::TYPE_GROUP) {
      err = "group fields are unsupported";
      return false;
    }
    if (t == F::TYPE_MESSAGE || t == F::TYPE_ENUM) {
      auto name = f->type_name();
      if (!name.empty() && name.front() == '.')
        name.erase(name.begin());
      if ((t == F::TYPE_ENUM && !enums.contains(name)) ||
          (t == F::TYPE_MESSAGE && !messages.contains(name))) {
        err = "descriptor field references an absent type";
        return false;
      }
    } else if (!f->type_name().empty()) {
      err = "scalar descriptor field has a type name";
      return false;
    }
  }
  return true;
}
bool read_varint(std::span<const std::byte> b, std::size_t &pos,
                 std::uint64_t &v) {
  v = 0;
  for (unsigned i = 0; i < 10 && pos < b.size(); ++i) {
    auto x = std::to_integer<unsigned char>(b[pos++]);
    if (i == 9 && (x & 0xfe))
      return false;
    v |= std::uint64_t(x & 0x7f) << (7 * i);
    if (!(x & 0x80))
      return true;
  }
  return false;
}
bool valid_utf8(std::string_view s) {
  for (std::size_t i = 0; i < s.size();) {
    const auto c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) {
      ++i;
      continue;
    }
    unsigned n = 0;
    std::uint32_t cp = 0;
    if ((c & 0xe0) == 0xc0) {
      n = 1;
      cp = c & 0x1f;
      if (cp < 2)
        return false;
    } else if ((c & 0xf0) == 0xe0) {
      n = 2;
      cp = c & 0x0f;
    } else if ((c & 0xf8) == 0xf0) {
      n = 3;
      cp = c & 7;
    } else
      return false;
    if (i + n >= s.size())
      return false;
    for (unsigned j = 1; j <= n; ++j) {
      auto x = static_cast<unsigned char>(s[i + j]);
      if ((x & 0xc0) != 0x80)
        return false;
      cp = (cp << 6) | (x & 0x3f);
    }
    if ((n == 2 && cp < 0x800) || (n == 3 && cp < 0x10000) || cp > 0x10ffff ||
        (cp >= 0xd800 && cp <= 0xdfff))
      return false;
    i += n + 1;
  }
  return true;
}
bool validate_role_order(std::span<const std::string> roles, std::string &err) {
  for (std::size_t i = 1; i < roles.size(); ++i) {
    if (roles[i - 1] == roles[i]) {
      err = "duplicate container role";
      return false;
    }
    if (roles[i - 1] > roles[i]) {
      err = "container roles are not in canonical order";
      return false;
    }
  }
  return true;
}
bool canonical_decimal(std::string_view digits) {
  return !digits.empty() && (digits.size() == 1 || digits.front() != '0');
}
struct PayloadScan {
  std::size_t fields = 0, items = 0, charge = 0;
};
bool scan_message(const google::protobuf::Descriptor *d,
                  std::span<const std::byte> b, std::size_t depth,
                  PayloadScan &s, std::string &err) {
  if (depth > 32) {
    err = "payload nesting limit exceeded";
    return false;
  }
  std::size_t p = 0;
  while (p < b.size()) {
    std::uint64_t tag = 0;
    if (!read_varint(b, p, tag) || !tag || (tag >> 3) > ((1u << 29) - 1)) {
      err = "malformed protobuf tag";
      return false;
    }
    const auto number = static_cast<int>(tag >> 3);
    const auto wire = static_cast<unsigned>(tag & 7);
    if (number >= 19000 && number <= 19999) {
      err = "reserved protobuf field number";
      return false;
    }
    const auto *f = d->FindFieldByNumber(number);
    if (!f) {
      err = "payload contains a field outside its descriptor";
      return false;
    }
    if (++s.fields > 16384) {
      err = "payload visit limit exceeded";
      return false;
    }
    const bool packed =
        f->is_repeated() && wire == 2 &&
        f->type() != google::protobuf::FieldDescriptor::TYPE_STRING &&
        f->type() != google::protobuf::FieldDescriptor::TYPE_BYTES &&
        f->type() != google::protobuf::FieldDescriptor::TYPE_MESSAGE;
    if (f->is_repeated() && !packed && ++s.items > 4096) {
      err = "payload item limit exceeded";
      return false;
    }
    unsigned expected = 0;
    switch (f->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      expected = 1;
      break;
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      expected = 5;
      break;
    case google::protobuf::FieldDescriptor::TYPE_STRING:
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      expected = 2;
      break;
    default:
      expected = 0;
    }
    if (wire != expected && !packed) {
      err = "payload wire type does not match descriptor";
      return false;
    }
    if (wire == 0) {
      std::uint64_t v;
      if (!read_varint(b, p, v)) {
        err = "malformed protobuf varint";
        return false;
      }
    } else if (wire == 1 || wire == 5) {
      const std::size_t n = wire == 1 ? 8 : 4;
      if (n > b.size() - p) {
        err = "truncated fixed-width value";
        return false;
      }
      p += n;
    } else if (wire == 2) {
      std::uint64_t n;
      if (!read_varint(b, p, n) || n > b.size() - p) {
        err = "truncated length-delimited value";
        return false;
      }
      auto sub = b.subspan(p, static_cast<std::size_t>(n));
      p += static_cast<std::size_t>(n);
      if (packed) {
        std::size_t q = 0;
        const auto fixed_width =
            (f->type() == google::protobuf::FieldDescriptor::TYPE_DOUBLE ||
             f->type() == google::protobuf::FieldDescriptor::TYPE_FIXED64 ||
             f->type() == google::protobuf::FieldDescriptor::TYPE_SFIXED64)
                ? 8u
            : (f->type() == google::protobuf::FieldDescriptor::TYPE_FLOAT ||
               f->type() == google::protobuf::FieldDescriptor::TYPE_FIXED32 ||
               f->type() == google::protobuf::FieldDescriptor::TYPE_SFIXED32)
                ? 4u
                : 0u;
        std::size_t count = 0;
        while (q < sub.size()) {
          if (fixed_width) {
            if (fixed_width > sub.size() - q) {
              err = "truncated packed scalar";
              return false;
            }
            q += fixed_width;
          } else {
            std::uint64_t v;
            if (!read_varint(sub, q, v)) {
              err = "malformed packed scalar";
              return false;
            }
          }
          ++count;
        }
        s.items += count;
        if (s.items > 4096) {
          err = "payload item limit exceeded";
          return false;
        }
        s.fields += count;
        if (s.fields > 16384) {
          err = "payload visit limit exceeded";
          return false;
        }
      } else if (f->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
        if (!scan_message(f->message_type(), sub, depth + 1, s, err))
          return false;
      } else if (n > 64 * 1024) {
        err = "string/bytes value exceeds 64 KiB";
        return false;
      } else
        s.charge += static_cast<std::size_t>(n);
    }
    s.charge += 48;
  }
  return true;
}
bool scalar_matches_key(const google::protobuf::FieldDescriptor *f,
                        CapturePathSelector selector) {
  using F = google::protobuf::FieldDescriptor;
  switch (selector) {
  case CapturePathSelector::string_key:
    return f->type() == F::TYPE_STRING;
  case CapturePathSelector::bool_key:
    return f->type() == F::TYPE_BOOL;
  case CapturePathSelector::signed_key:
    return f->type() == F::TYPE_INT32 || f->type() == F::TYPE_INT64 ||
           f->type() == F::TYPE_SINT32 || f->type() == F::TYPE_SINT64 ||
           f->type() == F::TYPE_SFIXED32 || f->type() == F::TYPE_SFIXED64 ||
           f->type() == F::TYPE_ENUM;
  case CapturePathSelector::unsigned_key:
    return f->type() == F::TYPE_UINT32 || f->type() == F::TYPE_UINT64 ||
           f->type() == F::TYPE_FIXED32 || f->type() == F::TYPE_FIXED64 ||
           f->type() == F::TYPE_ENUM;
  default:
    return false;
  }
}
bool valid_capture_path(const google::protobuf::Descriptor *root,
                        std::span<const CapturePathStep> path) {
  const auto *current = root;
  const google::protobuf::FieldDescriptor *pending = nullptr;
  for (const auto &step : path) {
    if (step.selector == CapturePathSelector::field_number) {
      if (pending) {
        if (pending->is_repeated() ||
            pending->type() != google::protobuf::FieldDescriptor::TYPE_MESSAGE)
          return false;
        current = pending->message_type();
        pending = nullptr;
      }
      if (!current)
        return false;
      pending = current->FindFieldByNumber(static_cast<int>(step.number));
      if (!pending)
        return false;
      continue;
    }
    if (!pending || !pending->is_repeated())
      return false;
    if (step.selector == CapturePathSelector::list_index) {
      if (pending->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE)
        current = pending->message_type();
      else
        current = nullptr;
    } else {
      if (pending->type() != google::protobuf::FieldDescriptor::TYPE_MESSAGE)
        return false;
      current = pending->message_type();
      const auto *key = current->FindFieldByNumber(1);
      if (!key || !scalar_matches_key(key, step.selector))
        return false;
    }
    pending = nullptr;
  }
  return true;
}
bool validate_container_roles(const google::protobuf::Descriptor *root,
                              std::span<const std::string> roles,
                              std::string &err) {
  if (roles.size() > 8192) {
    err = "container role count exceeds limit";
    return false;
  }
  if (!validate_role_order(roles, err))
    return false;
  std::unordered_set<std::string> seen;
  for (const auto &entry : roles) {
    if (entry.size() > 4096 || !valid_utf8(entry) ||
        !seen.insert(entry).second) {
      err = "invalid or duplicate container role";
      return false;
    }
    const auto eq = entry.find('=');
    if (eq == std::string::npos || eq == 0 || eq + 1 == entry.size()) {
      err = "malformed container role";
      return false;
    }
    const auto path = std::string_view(entry).substr(0, eq),
               role = std::string_view(entry).substr(eq + 1);
    const bool optional = role == "optional",
               map = role == "map" || role == "unordered_map";
    const bool simple = role == "optional" || role == "vector" ||
                        role == "deque" || role == "list" || role == "set" ||
                        role == "unordered_set" || role == "map" ||
                        role == "unordered_map";
    std::size_t array_count = 0;
    if (!simple && role.rfind("array:", 0) == 0) {
      for (char c : role.substr(6)) {
        if (c < '0' || c > '9' || array_count > 4096) {
          err = "invalid array role cap";
          return false;
        }
        array_count = array_count * 10 + (c - '0');
      }
      if (array_count > 4096) {
        err = "array role cap exceeds limit";
        return false;
      }
    } else if (!simple) {
      err = "unknown container role";
      return false;
    }
    std::size_t pos = 0;
    const google::protobuf::FieldDescriptor *pending = nullptr;
    const google::protobuf::Descriptor *current = root;
    bool first = true;
    while (pos < path.size()) {
      if (path[pos] == '.' || path[pos] == '/') {
        const char sepchar = path[pos++];
        const auto begin = pos;
        while (pos < path.size() &&
               ((sepchar == '/' &&
                 std::isalpha(static_cast<unsigned char>(path[pos]))) ||
                (sepchar == '.' &&
                 std::isdigit(static_cast<unsigned char>(path[pos])))))
          ++pos;
        if (begin == pos) {
          err = "malformed container role path: " + entry;
          return false;
        }
        if (sepchar == '/') {
          const auto marker = path.substr(begin, pos - begin);
          if (marker != "value" && marker != "key") {
            err = "unknown container role path marker";
            return false;
          }
          if (!pending || pending->type() !=
                              google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
            err = "container role path traverses a scalar";
            return false;
          }
          const auto *child = pending->message_type();
          const auto *selected =
              child->FindFieldByName(marker == "value" ? "value" : "key");
          if (!selected) {
            if (marker == "key") {
              err = "container role key selector does not match descriptor";
              return false;
            }
            selected = child->FindFieldByNumber(1);
            if (!selected) {
              err = "container role wrapper has no value field";
              return false;
            }
          }
          pending = selected;
          current = selected->type() ==
                            google::protobuf::FieldDescriptor::TYPE_MESSAGE
                        ? selected->message_type()
                        : nullptr;
        } else {
          int n = 0;
          for (char c : path.substr(begin, pos - begin)) {
            if (n > (1 << 29) / 10) {
              err = "container role path number exceeds limit";
              return false;
            }
            n = n * 10 + c - '0';
          }
          if (!n || n >= (1 << 29) || !current ||
              (pending &&
               pending->type() !=
                   google::protobuf::FieldDescriptor::TYPE_MESSAGE)) {
            err = "invalid container role field path";
            return false;
          }
          if (pending)
            current = pending->message_type();
          pending = current->FindFieldByNumber(n);
          if (!pending) {
            err = "container role path field is absent from descriptor";
            return false;
          }
          current =
              pending->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE
                  ? pending->message_type()
                  : nullptr;
        }
      } else if (first) {
        int n = 0;
        while (pos < path.size() &&
               std::isdigit(static_cast<unsigned char>(path[pos]))) {
          if (n > (1 << 29) / 10) {
            err = "container role root number exceeds limit";
            return false;
          }
          n = n * 10 + path[pos++] - '0';
        }
        if (!n || n >= (1 << 29)) {
          err = "invalid container role root number";
          return false;
        }
        pending = root->FindFieldByNumber(n);
        if (!pending) {
          err = "container role root field is absent from descriptor";
          return false;
        }
        current =
            pending->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE
                ? pending->message_type()
                : nullptr;
        first = false;
      } else {
        err = "malformed container role path";
        return false;
      }
    }
    if (first || !pending) {
      err = "empty container role path";
      return false;
    }
    const auto *field = pending;
    if ((optional &&
         (field->is_repeated() ||
          field->type() != google::protobuf::FieldDescriptor::TYPE_MESSAGE)) ||
        (map &&
         (!field->is_repeated() ||
          field->type() != google::protobuf::FieldDescriptor::TYPE_MESSAGE)) ||
        (!optional && !map && !field->is_repeated() &&
         field->type() != google::protobuf::FieldDescriptor::TYPE_MESSAGE)) {
      err = "container role does not match descriptor field shape: " + entry;
      return false;
    }
  }
  return true;
}
bool validate_container_roles_proto(
    const google::protobuf::FileDescriptorSet &set, const std::string &root,
    std::span<const std::string> roles, std::string &err) {
  using DP = google::protobuf::DescriptorProto;
  using FP = google::protobuf::FieldDescriptorProto;
  std::unordered_map<std::string, const DP *> messages;
  std::function<void(const DP &, const std::string &)> collect =
      [&](const DP &m, const std::string &prefix) {
        const auto name = prefix.empty() ? m.name() : prefix + "." + m.name();
        messages.emplace(name, &m);
        for (const auto &n : m.nested_type())
          collect(n, name);
      };
  for (const auto &file : set.file()) {
    for (const auto &m : file.message_type())
      collect(m, file.package());
  }
  const auto root_it = messages.find(root);
  if (root_it == messages.end()) {
    err = "container role root is absent from descriptor bundle";
    return false;
  }
  if (roles.size() > 8192) {
    err = "container role count exceeds limit";
    return false;
  }
  if (!validate_role_order(roles, err))
    return false;
  std::unordered_set<std::string> seen;
  std::unordered_set<const FP *> role_targets;
  auto message_for = [&](std::string name) -> const DP * {
    if (!name.empty() && name.front() == '.')
      name.erase(name.begin());
    auto i = messages.find(name);
    return i == messages.end() ? nullptr : i->second;
  };
  for (const auto &entry : roles) {
    if (entry.size() > 4096 || !valid_utf8(entry) ||
        !seen.insert(entry).second) {
      err = "invalid or duplicate container role";
      return false;
    }
    const auto eq = entry.find('=');
    if (eq == std::string::npos || eq == 0 || eq + 1 == entry.size()) {
      err = "malformed container role";
      return false;
    }
    const std::string_view path(entry.data(), eq),
        role(entry.data() + eq + 1, entry.size() - eq - 1);
    const bool optional = role == "optional",
               map = role == "map" || role == "unordered_map";
    const bool simple = optional || role == "vector" || role == "deque" ||
                        role == "list" || role == "set" ||
                        role == "unordered_set" || map;
    std::size_t cap = 0;
    if (!simple && role.rfind("array:", 0) == 0) {
      const auto digits = role.substr(6);
      if (!canonical_decimal(digits)) {
        err = "invalid array role cap";
        return false;
      }
      for (char c : digits) {
        if (c < '0' || c > '9' || cap > (4096 - (c - '0')) / 10) {
          err = "invalid array role cap";
          return false;
        }
        cap = cap * 10 + c - '0';
      }
      if (cap > 4096) {
        err = "array cap exceeds limit";
        return false;
      }
    } else if (!simple) {
      err = "unknown container role";
      return false;
    }
    const DP *current = root_it->second;
    const FP *pending = nullptr;
    std::size_t pos = 0;
    bool first = true;
    while (pos < path.size()) {
      if (path[pos] == '.' || path[pos] == '/') {
        const char sep = path[pos++];
        const auto begin = pos;
        while (pos < path.size() &&
               ((sep == '/' &&
                 std::isalpha(static_cast<unsigned char>(path[pos]))) ||
                (sep == '.' &&
                 std::isdigit(static_cast<unsigned char>(path[pos])))))
          ++pos;
        if (begin == pos) {
          err = "malformed role path";
          return false;
        }
        if (sep == '/') {
          const auto marker = path.substr(begin, pos - begin);
          if (marker != "value" && marker != "key") {
            err = "unknown role path marker";
            return false;
          }
          if (!pending || pending->type() != FP::TYPE_MESSAGE) {
            err = "role path traverses scalar";
            return false;
          }
          current = message_for(pending->type_name());
          if (!current) {
            err = "role path message type is absent";
            return false;
          }
          const FP *selected = nullptr;
          for (const auto &f : current->field())
            if (f.name() == marker) {
              selected = &f;
              break;
            }
          if (!selected && marker == "value")
            for (const auto &f : current->field())
              if (f.number() == 1) {
                selected = &f;
                break;
              }
          if (!selected) {
            err = "role path selector is absent";
            return false;
          }
          pending = selected;
          current = pending->type() == FP::TYPE_MESSAGE
                        ? message_for(pending->type_name())
                        : nullptr;
        } else {
          int number = 0;
          const auto digits = path.substr(begin, pos - begin);
          if (!canonical_decimal(digits)) {
            err = "noncanonical role path field number";
            return false;
          }
          for (char c : digits) {
            if (number > (1 << 29) / 10) {
              err = "role path field number too large";
              return false;
            }
            number = number * 10 + c - '0';
          }
          if (!number || !current ||
              (pending && pending->type() != FP::TYPE_MESSAGE)) {
            err = "invalid role path field";
            return false;
          }
          if (pending) {
            current = message_for(pending->type_name());
            if (!current) {
              err = "role path message type is absent";
              return false;
            }
          }
          pending = nullptr;
          for (const auto &f : current->field())
            if (f.number() == number) {
              pending = &f;
              break;
            }
          if (!pending) {
            err = "role path field is absent";
            return false;
          }
          current = pending->type() == FP::TYPE_MESSAGE
                        ? message_for(pending->type_name())
                        : nullptr;
        }
      } else if (first) {
        int number = 0;
        const auto begin = pos;
        while (pos < path.size() &&
               std::isdigit(static_cast<unsigned char>(path[pos]))) {
          if (number > (1 << 29) / 10) {
            err = "role root field number too large";
            return false;
          }
          number = number * 10 + path[pos++] - '0';
        }
        if (!canonical_decimal(path.substr(begin, pos - begin)) || !number) {
          err = "invalid role root field";
          return false;
        }
        pending = nullptr;
        for (const auto &f : current->field())
          if (f.number() == number) {
            pending = &f;
            break;
          }
        if (!pending) {
          err = "role root field is absent";
          return false;
        }
        current = pending->type() == FP::TYPE_MESSAGE
                      ? message_for(pending->type_name())
                      : nullptr;
        first = false;
      } else {
        err = "malformed role path";
        return false;
      }
    }
    if (!pending) {
      err = "role path is empty";
      return false;
    }
    const bool repeated = pending->label() == FP::LABEL_REPEATED;
    const bool message = pending->type() == FP::TYPE_MESSAGE;
    if ((optional && (repeated || !message)) ||
        (map && (!repeated || !message)) || (!optional && !map && !repeated)) {
      err = "container role does not match descriptor field shape";
      return false;
    }
    if (optional) {
      const auto *wrapper = message_for(pending->type_name());
      if (!wrapper || wrapper->field_size() != 1 ||
          wrapper->field(0).number() != 1 ||
          wrapper->field(0).name() != "value") {
        err = "optional role does not match its value wrapper";
        return false;
      }
    } else if (map) {
      const auto *entry_message = message_for(pending->type_name());
      if (!entry_message || entry_message->field_size() != 2) {
        err = "map role does not match its entry wrapper";
        return false;
      }
      const FP *key = nullptr, *value = nullptr;
      for (const auto &field : entry_message->field()) {
        if (field.number() == 1 && field.name() == "key")
          key = &field;
        if (field.number() == 2 && field.name() == "value")
          value = &field;
      }
      if (!key || !value || key->label() == FP::LABEL_REPEATED ||
          key->type() == FP::TYPE_MESSAGE || key->type() == FP::TYPE_GROUP ||
          key->type() == FP::TYPE_BYTES) {
        err = "map role does not match its key/value entry shape";
        return false;
      }
    }
    if (!role_targets.insert(pending).second) {
      err = "multiple container roles select the same descriptor field";
      return false;
    }
  }
  std::unordered_set<const DP *> visited_messages;
  std::function<bool(const DP *)> check_repeated = [&](const DP *message) {
    if (!message || !visited_messages.insert(message).second)
      return true;
    for (const auto &field : message->field()) {
      if (field.label() == FP::LABEL_REPEATED &&
          !role_targets.contains(&field)) {
        err =
            "repeated descriptor field has no container role: " + field.name();
        return false;
      }
      if (field.type() == FP::TYPE_MESSAGE &&
          !check_repeated(message_for(field.type_name())))
        return false;
    }
    return true;
  };
  if (!check_repeated(root_it->second))
    return false;
  return true;
}
} // namespace
ReceiverLedger::ReceiverLedger(ReceiverLimits l)
    : state_(std::make_shared<State>()) {
  state_->limits = l;
}
std::size_t ReceiverLedger::retained_schema_bytes() const noexcept {
  std::lock_guard l(state_->mutex);
  return state_->schema_bytes;
}
std::size_t ReceiverLedger::retained_decoded_bytes() const noexcept {
  std::lock_guard l(state_->mutex);
  return state_->decoded_bytes;
}
const SchemaRecord *DecodedState::schema() const noexcept {
  return entry_ ? entry_->record.get() : nullptr;
}
ContextReceiver::ContextReceiver(std::shared_ptr<ReceiverLedger> l,
                                 ReceiverLimits limits)
    : ledger_(std::move(l)), limits_(limits),
      legacy_epoch_nonce_(next_legacy_epoch_nonce()) {}
ContextReceiver::~ContextReceiver() { close_epoch(); }
bool ContextReceiver::register_schema(
    std::uint64_t id, const google::protobuf::FileDescriptorSet &set,
    const std::string &root, StateFamilyKey family, std::string *error,
    std::span<const std::string> container_roles) {
  SchemaRegistration registration;
  registration.id = id;
  registration.descriptor_set = set;
  registration.root = root;
  registration.family = std::move(family);
  registration.container_roles.assign(container_roles.begin(),
                                      container_roles.end());
  return register_schema_set(
      std::span<const SchemaRegistration>(&registration, 1), error);
}
bool ContextReceiver::register_schema_set(
    std::span<const SchemaRegistration> registrations, std::string *error) {
  auto fail = [&](std::string s) {
    if (error)
      *error = std::move(s);
    return false;
  };
  if (registrations.empty())
    return true;
  if (registrations.size() > limits_.schemas_per_epoch)
    return fail("schema set exceeds epoch schema count limit");
  struct Prepared {
    const SchemaRegistration *input = nullptr;
    std::string raw;
    std::size_t charge = 0;
    bool already_present = false;
    std::shared_ptr<void> global_lease;
  };
  std::vector<Prepared> prepared;
  prepared.reserve(registrations.size());
  std::unordered_set<std::uint64_t> ids;
  std::size_t total_charge = 0, new_count = 0;
  std::shared_ptr<void> epoch_token;
  std::shared_ptr<std::atomic_bool> epoch_active;
  for (const auto &registration : registrations) {
    if (!registration.id || !ids.insert(registration.id).second)
      return fail("schema set contains a zero or duplicate ID");
    const auto &set = registration.descriptor_set;
    const auto &family = registration.family;
    if (family.cpp_state_type.empty() || family.cpp_scope_type.empty() ||
        family.cpp_state_type.size() > 4096 ||
        family.cpp_scope_type.size() > 4096)
      return fail("family name missing or exceeds 4 KiB limit");
    if (!valid_utf8(family.cpp_state_type) ||
        !valid_utf8(family.cpp_scope_type))
      return fail("family type name is not valid UTF-8");
    if (std::none_of(family.source_lineage_id.begin(),
                     family.source_lineage_id.end(),
                     [](auto b) { return b != 0; }))
      return fail("missing source lineage UUID");
    std::string why;
    std::size_t field_count = 0, message_count = 0;
    if (!validate_set(set, limits_, why, field_count, message_count) ||
        !validate_descriptor_shape(set, registration.root, why))
      return fail(std::move(why));
    if (registration.container_roles.size() > limits_.fields)
      return fail("container role count exceeds descriptor field budget");
    if (!validate_container_roles_proto(set, registration.root,
                                        registration.container_roles, why))
      return fail(std::move(why));
    std::string raw;
    if (!set.SerializeToString(&raw))
      return fail("descriptor serialization failed");
    std::size_t role_bytes = 0;
    for (const auto &role : registration.container_roles) {
      if (role.size() > 4096 || role.size() > limits_.descriptor_bytes ||
          role_bytes > limits_.descriptor_bytes - role.size())
        return fail("container role metadata exceeds byte budget");
      role_bytes += role.size();
    }
    if (raw.size() > limits_.descriptor_bytes ||
        role_bytes > limits_.descriptor_bytes - raw.size())
      return fail("descriptor and role bytes exceed limit");
    const auto charge = raw.size() + role_bytes + family.cpp_state_type.size() +
                        family.cpp_scope_type.size() + 16 +
                        set.file_size() * 512 + message_count * 256 +
                        field_count * 96;
    if (charge > std::numeric_limits<std::size_t>::max() - total_charge)
      return fail("schema-set byte accounting overflow");
    total_charge += charge;
    prepared.push_back({&registration, std::move(raw), charge, false});
  }
  {
    std::lock_guard lock(mutex_);
    if (closed_)
      return fail("epoch is closed");
    for (auto &item : prepared) {
      auto i = schemas_.find(item.input->id);
      if (i == schemas_.end()) {
        ++new_count;
        continue;
      }
      const auto &entry = *i->second.entry;
      if (entry.descriptor_bytes != item.raw ||
          entry.root != item.input->root ||
          entry.family != item.input->family ||
          entry.record->container_roles != item.input->container_roles)
        return fail("schema ID redefined");
      item.already_present = true;
      total_charge -= item.charge;
    }
    if (schemas_.size() + new_count > limits_.schemas_per_epoch ||
        total_charge > limits_.epoch_schema_bytes -
                           std::min(epoch_bytes_, limits_.epoch_schema_bytes))
      return fail("epoch schema capacity exceeded");
    if (!total_charge)
      return true;
    std::vector<std::shared_ptr<std::atomic_bool>> active_flags;
    active_flags.reserve(new_count);
    try {
      epoch_active = std::make_shared<std::atomic_bool>(false);
      epoch_token = std::shared_ptr<void>(
          new std::size_t(total_charge), [this, epoch_active](void *p) {
            auto *n = static_cast<std::size_t *>(p);
            if (epoch_active->load(std::memory_order_relaxed)) {
              std::lock_guard lock(mutex_);
              epoch_bytes_ -= std::min(epoch_bytes_, *n);
            }
            delete n;
          });
      for (auto &item : prepared)
        if (!item.already_present) {
          auto active = std::make_shared<std::atomic_bool>(false);
          item.global_lease = std::shared_ptr<void>(
              new std::size_t(item.charge),
              [state = ledger_->state_, active](void *p) {
                auto *n = static_cast<std::size_t *>(p);
                if (active->load(std::memory_order_relaxed)) {
                  std::lock_guard lock(state->mutex);
                  state->schema_bytes -= std::min(state->schema_bytes, *n);
                }
                delete n;
              });
          active_flags.push_back(std::move(active));
        }
    } catch (...) {
      return fail("schema-set reservation allocation failed");
    }
    std::lock_guard global(ledger_->state_->mutex);
    auto &used = ledger_->state_->schema_bytes;
    const auto cap = ledger_->state_->limits.global_schema_bytes;
    if (total_charge > cap - std::min(used, cap))
      return fail("global schema capacity exceeded");
    used += total_charge;
    epoch_bytes_ += total_charge;
    for (auto &active : active_flags)
      active->store(true, std::memory_order_relaxed);
    epoch_active->store(true, std::memory_order_relaxed);
  }
  if (!total_charge)
    return true;
  std::vector<std::pair<std::uint64_t, Mapping>> staged;
  staged.reserve(new_count);
  for (const auto &item : prepared) {
    if (item.already_present)
      continue;
    const auto &registration = *item.input;
    auto entry = std::make_shared<SchemaEntry>();
    entry->family = registration.family;
    entry->root = registration.root;
    entry->descriptor_bytes = item.raw;
    entry->charge = item.charge;
    entry->global_lease = item.global_lease;
    for (const auto &file : registration.descriptor_set.file())
      if (!entry->pool.BuildFile(file))
        return fail("invalid descriptor dependency closure");
    entry->descriptor = entry->pool.FindMessageTypeByName(entry->root);
    if (!entry->descriptor)
      return fail("root message absent from descriptor bundle");
    std::string why;
    if (!validate_container_roles(entry->descriptor,
                                  registration.container_roles, why))
      return fail(std::move(why));
    entry->identity = compute_schema_revision_id(
        item.raw, entry->family, entry->root, registration.container_roles);
    entry->record = std::make_shared<const SchemaRecord>(
        entry->family, entry->root, entry->identity, item.raw, nullptr,
        registration.container_roles);
    staged.emplace_back(registration.id,
                        Mapping{std::move(entry), epoch_token});
  }
  {
    std::lock_guard lock(mutex_);
    if (closed_)
      return fail("epoch closed during schema-set construction");
    for (const auto &item : staged)
      if (schemas_.contains(item.first))
        return fail("schema ID concurrently published");
    try {
      schemas_.reserve(schemas_.size() + staged.size());
      for (const auto &item : staged)
        schemas_.emplace(item.first, item.second);
    } catch (...) {
      for (const auto &item : staged)
        schemas_.erase(item.first);
      return fail("atomic schema-set publication failed");
    }
  }
  return true;
}
bool ContextReceiver::register_legacy_schema(
    std::uint64_t id, const google::protobuf::FileDescriptorSet &set,
    const std::string &root, std::string *error) {
  std::string raw;
  if (!set.SerializeToString(&raw)) {
    if (error)
      *error = "legacy descriptor serialization failed";
    return false;
  }
  std::uint64_t digest = 1469598103934665603ULL;
  for (unsigned char c : raw + root + std::to_string(id)) {
    digest ^= c;
    digest *= 1099511628211ULL;
  }
  StateFamilyKey family;
  std::ostringstream state, scope;
  state << "__tracer_legacy_schema_" << std::hex << digest;
  scope << "__tracer_legacy_epoch_" << std::hex << legacy_epoch_nonce_;
  family.cpp_state_type = state.str();
  family.cpp_scope_type = scope.str();
  for (unsigned i = 0; i < 8; ++i) {
    family.source_lineage_id[i] =
        static_cast<std::uint8_t>(legacy_epoch_nonce_ >> (i * 8));
    family.source_lineage_id[i + 8] =
        static_cast<std::uint8_t>(digest >> (i * 8));
  }
  return register_schema(id, set, root, std::move(family), error, {});
}
std::optional<DecodedState>
ContextReceiver::decode(std::uint64_t id, std::span<const std::byte> bytes,
                        std::string *error) {
  return decode(id, bytes, std::span<const CaptureLimitView>{}, error);
}
std::optional<DecodedState>
ContextReceiver::decode(std::uint64_t id, std::span<const std::byte> bytes,
                        std::span<const CaptureLimitView> limits,
                        std::string *error) {
  auto fail = [&](std::string s) -> std::optional<DecodedState> {
    if (error)
      *error = std::move(s);
    return std::nullopt;
  };
  if (bytes.size() > limits_.event_bytes)
    return fail("payload exceeds event limit");
  if (limits.size() > limits_.truncation_records)
    return fail("truncation record limit exceeded");
  std::size_t metadata_bytes = 0;
  for (const auto &limit : limits) {
    if (limit.total_items <= limit.captured_items ||
        limit.captured_items > 4096)
      return fail("invalid capture truncation counts");
    if (limit.path.empty() || limit.path.size() > limits_.path_depth)
      return fail("invalid capture truncation path depth");
    metadata_bytes += sizeof(DecodedCaptureLimit) +
                      limit.path.size() * sizeof(DecodedPathStep);
    for (const auto &step : limit.path) {
      if (step.selector == CapturePathSelector::field_number &&
          (step.number == 0 || step.number >= (1u << 29) ||
           (step.number >= 19000 && step.number <= 19999)))
        return fail("invalid field number in truncation path");
      if (step.selector == CapturePathSelector::list_index &&
          step.number > 4096)
        return fail("list index exceeds capture limit");
      if (step.selector == CapturePathSelector::string_key) {
        if (step.string_value.size() > 64 * 1024 ||
            !valid_utf8(step.string_value))
          return fail("invalid string selector in truncation path");
        metadata_bytes += step.string_value.size();
      }
      if (step.selector != CapturePathSelector::field_number &&
          step.selector != CapturePathSelector::list_index &&
          step.selector != CapturePathSelector::string_key &&
          step.selector != CapturePathSelector::signed_key &&
          step.selector != CapturePathSelector::unsigned_key &&
          step.selector != CapturePathSelector::bool_key)
        return fail("unknown truncation selector");
    }
  }
  if (metadata_bytes > limits_.event_bytes - bytes.size())
    return fail("event truncation metadata exceeds byte limit");
  std::shared_ptr<const SchemaEntry> entry;
  {
    std::lock_guard lock(mutex_);
    if (closed_)
      return fail("epoch is closed");
    auto i = schemas_.find(id);
    if (i == schemas_.end())
      return fail("unknown schema ID");
    entry = i->second.entry;
  }
  for (const auto &limit : limits)
    if (!valid_capture_path(entry->descriptor, limit.path))
      return fail("truncation path does not match announced schema");
  PayloadScan scan;
  scan.charge = bytes.size() + metadata_bytes;
  std::string scan_error;
  if (!scan_message(entry->descriptor, bytes, 0, scan, scan_error))
    return fail(std::move(scan_error));
  scan.charge += scan.fields * 16;
  {
    std::lock_guard lock(ledger_->state_->mutex);
    auto &used = ledger_->state_->decoded_bytes;
    const auto cap = ledger_->state_->limits.decoded_bytes;
    if (scan.charge > cap - std::min(used, cap))
      return fail("decoded-event capacity exceeded");
    used += scan.charge;
  }
  auto lease = charge_token(ledger_->state_, scan.charge, false);
  const auto *prototype = entry->factory.GetPrototype(entry->descriptor);
  if (!prototype)
    return fail("dynamic message prototype unavailable");
  std::shared_ptr<google::protobuf::Message> message(prototype->New());
  if (bytes.size() >
          static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
      !message->ParseFromArray(bytes.data(), static_cast<int>(bytes.size())))
    return fail("malformed state payload");
  DecodedState out;
  out.entry_ = entry;
  out.message_ = std::move(message);
  out.lease_ = std::move(lease);
  out.truncations_.reserve(limits.size());
  for (const auto &limit : limits) {
    DecodedCaptureLimit copy;
    copy.total_items = limit.total_items;
    copy.captured_items = limit.captured_items;
    copy.path.reserve(limit.path.size());
    for (const auto &step : limit.path)
      copy.path.push_back({step.selector, step.number, step.signed_value,
                           step.bool_value, std::string(step.string_value)});
    out.truncations_.push_back(std::move(copy));
  }
  return out;
}
std::optional<DecodedEvent>
ContextReceiver::decode_event(std::span<const StateAttachmentView> attachments,
                              std::string *error) {
  auto fail = [&](std::string s) -> std::optional<DecodedEvent> {
    if (error)
      *error = std::move(s);
    return std::nullopt;
  };
  std::size_t total = 0;
  for (const auto &a : attachments) {
    if (a.truncations.size() > limits_.truncation_records)
      return fail("truncation record limit exceeded");
    if (a.payload.size() > limits_.event_bytes - total)
      return fail("complete event exceeds byte limit");
    total += a.payload.size();
    for (const auto &limit : a.truncations) {
      if (limit.path.empty() || limit.path.size() > limits_.path_depth ||
          limit.total_items <= limit.captured_items ||
          limit.captured_items > 4096)
        return fail("invalid truncation metadata");
      total += sizeof(DecodedCaptureLimit) +
               limit.path.size() * sizeof(DecodedPathStep);
      for (const auto &step : limit.path) {
        if (step.selector == CapturePathSelector::string_key) {
          if (step.string_value.size() > 64 * 1024)
            return fail("truncation string selector exceeds 64 KiB");
          total += step.string_value.size();
        }
        if (step.selector == CapturePathSelector::list_index &&
            step.number > 4096)
          return fail("truncation list index exceeds cap");
      }
      if (total > limits_.event_bytes)
        return fail("complete event metadata exceeds byte limit");
    }
  }
  DecodedEvent event;
  event.states.reserve(attachments.size());
  for (const auto &a : attachments) {
    auto state = decode(a.schema_id, a.payload, a.truncations, error);
    if (!state)
      return std::nullopt;
    event.states.push_back(std::move(*state));
  }
  return event;
}
void ContextReceiver::close_epoch() noexcept {
  decltype(schemas_) retired;
  {
    std::lock_guard lock(mutex_);
    if (closed_)
      return;
    closed_ = true;
    retired.swap(schemas_);
  }
  retired.clear();
}
std::size_t ContextReceiver::schema_count() const noexcept {
  std::lock_guard lock(mutex_);
  return schemas_.size();
}

std::vector<ReceiverSchemaView> ContextReceiver::admitted_schemas() const {
  std::vector<ReceiverSchemaView> result;
  {
    std::lock_guard lock(mutex_);
    result.reserve(schemas_.size());
    for (const auto &[id, mapping] : schemas_) {
      (void)id;
      result.emplace_back(ReceiverSchemaView(mapping.entry));
    }
  }
  const auto less = [](const ReceiverSchemaView &left,
                       const ReceiverSchemaView &right) {
    const auto &a = left.family();
    const auto &b = right.family();
    if (a.source_lineage_id != b.source_lineage_id)
      return a.source_lineage_id < b.source_lineage_id;
    if (a.cpp_scope_type != b.cpp_scope_type)
      return a.cpp_scope_type < b.cpp_scope_type;
    if (a.cpp_state_type != b.cpp_state_type)
      return a.cpp_state_type < b.cpp_state_type;
    return left.revision_id() < right.revision_id();
  };
  std::sort(result.begin(), result.end(), less);
  result.erase(std::unique(result.begin(), result.end(),
                           [](const ReceiverSchemaView &left,
                              const ReceiverSchemaView &right) {
                             return left.revision_id() == right.revision_id();
                           }),
               result.end());
  return result;
}
} // namespace tracer::context_engine
