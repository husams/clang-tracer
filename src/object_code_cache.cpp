#include "object_code_cache.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

#include <fcntl.h>
#include <dlfcn.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#if defined(__aarch64__)
#include <asm/hwcap.h>
#include <sys/auxv.h>
#endif

#include "injector.hpp"
#include "debug_resolver.hpp"
#include "linux_syscalls.hpp"
#include "object_code_cache_decode.hpp"
#include "object_watch_plan.hpp"
#include "object_watch.hpp"
#include "proc_identity.hpp"
#include "tracer/protocol.hpp"

namespace tracer::object_cache {
namespace api = clangtracer::v1;

namespace {
struct SharedFile {
  int descriptor = -1;
  SharedFileIdentity identity{};

  explicit SharedFile(const std::string &path) {
    if (path.empty())
      return;
    if (path.front() != '/' || path.find('\0') != std::string::npos)
      throw std::invalid_argument("shared-file capture requires an absolute server-local path");
    descriptor = ::open(path.c_str(), O_PATH | O_CLOEXEC);
    struct stat info{};
    if (descriptor < 0 || ::fstat(descriptor, &info) || !S_ISREG(info.st_mode) || !info.st_ino) {
      if (descriptor >= 0)
        ::close(descriptor);
      throw std::invalid_argument("cannot pin the regular file selected for shared storage");
    }
    identity = {info.st_dev, info.st_ino};
  }
  ~SharedFile() {
    if (descriptor >= 0)
      ::close(descriptor);
  }
  SharedFile(const SharedFile &) = delete;
  SharedFile &operator=(const SharedFile &) = delete;
};
}

std::uintptr_t load_agent(injector::Process &process, const std::string &path) {
  constexpr char export_name[] = "tracer_agent_object_cache_v35";
  const auto size = std::max(path.size() + 1, sizeof(export_name));
  const auto memory = process.remote_allocate(size);
  std::uintptr_t entry = 0;
  try {
    process.remote_write(memory, path.c_str(), path.size() + 1);
    const auto handle = process.remote_call(process.resolve_remote_symbol("dlopen"),
                                            {memory, RTLD_NOW | RTLD_LOCAL}, 2);
    if (!handle)
      throw std::runtime_error("target could not load the object-cache agent");
    process.remote_write(memory, export_name, sizeof(export_name));
    entry = process.remote_call(process.resolve_remote_symbol("dlsym"), {handle, memory}, 2);
    if (!entry)
      throw std::runtime_error("object-cache agent has no compatible entry point");
  } catch (...) {
    if (!process.unsafe())
      process.remote_free(memory, size);
    throw;
  }
  process.remote_free(memory, size);
  return entry;
}

namespace {
using Boundary = object_watch::Boundary;
struct RuntimePrefixBusy {};

void request_reclaim(Channel &channel) noexcept {
  channel.reclaim_request.store(1, std::memory_order_release);
  (void)::syscall(SYS_futex, &channel.reclaim_request, FUTEX_WAKE, 1, nullptr, nullptr, 0);
}

std::vector<Instruction> verify_function(const object_watch::Plan &plan, Address start,
                                         std::size_t size, std::optional<Boundary> boundary) {
  const auto bytes = plan.code(start).substr(0, size);
  if (bytes.size() != size)
    throw std::runtime_error("cannot verify a complete code-cache function");
  return decode_leaf(start, bytes, !boundary || boundary == Boundary::construct,
      [&](const native_entry::Instruction &inst, Instruction &out) {
        if (inst.flow == NativeFlow::ordinary) {
          if (boundary && inst.changes_receiver)
            throw std::runtime_error("code-cache lifetime function modifies its receiver register");
          if (!boundary) {
            const auto store = plan.sites.find(inst.address);
            if (store != plan.sites.end() && store->second.store)
              instrument_store(out, *store->second.store);
          }
        } else if (inst.flow == NativeFlow::returns) {
          if (boundary == Boundary::construct)
            out.hook = Hook::create;
          else if (boundary == Boundary::get)
            out.hook = Hook::borrow;
          else if (boundary == Boundary::add)
            out.hook = Hook::owner_add;
          else if (boundary == Boundary::destroy)
            out.hook = inst.address == start ? Hook::destroy_both : Hook::destroy_end;
        } else if (boundary == Boundary::destroy && inst.target == start) {
          throw std::runtime_error("code-cache destructor branches to its lifetime entry");
        }
        if (boundary == Boundary::destroy && inst.address == start && out.hook == Hook::none)
          out.hook = Hook::destroy_begin;
      });
}

void inline_mutation_anchor(Instruction &instruction, const object_watch::InlineMutation &mutation) {
  instruction.boundary_pc = mutation.begin;
  if (instruction.pc == mutation.begin) {
    instruction.inline_mutation_entry = true;
    instruction.inline_receiver_register = register_index(mutation.receiver_register, true);
    instruction.inline_receiver_offset = mutation.receiver_offset;
  } else {
    instruction.collection_resume = true;
  }
}

std::vector<Address> mutation_successors(const native_entry::Preview &body, Address mutation) {
  std::vector<Address> result;
  for (const auto &inst : body.instructions) {
    if (inst.flow == NativeFlow::branch && !inst.target)
      throw std::runtime_error("collection writer requires verified branch targets");
    if (inst.flow == NativeFlow::call && inst.target == mutation)
      result.push_back(inst.address + inst.size);
  }
  if (result.empty())
    throw std::runtime_error("collection watch has no verified direct call to its selected mutation");
  return result;
}

struct Dispatch {
  Address get_call = 0, call = 0, successor = 0;
};

Dispatch verify_dispatch(const object_watch::Plan &plan, Address getter) {
  const auto decoded = native_entry::scan(plan.caller_start, plan.caller_bytes);
  if (decoded.decode_failed || !decoded.diagnostic.empty())
    throw std::runtime_error("cannot verify the borrow caller's complete instruction graph");
  Dispatch result;
  Address previous_end = 0;
  for (const auto &inst : decoded.instructions) {
    if (inst.address < previous_end)
      throw std::runtime_error("overlapping instructions in the borrow caller");
    previous_end = inst.address + inst.size;
    if (inst.flow == NativeFlow::call && inst.target == getter) {
      if (result.get_call || inst.size != 5 || plan.code(inst.address).front() != '\xe8')
        throw std::runtime_error("borrow caller requires one direct rel32 call to Image::get");
      result.get_call = inst.address;
    }
    if (inst.indirect_near_call) {
      if (result.call || inst.target)
        throw std::runtime_error("borrow caller requires one verified indirect near call");
      result.call = inst.address;
      result.successor = previous_end;
    }
  }
  if (!result.get_call || result.call < result.get_call + 5)
    throw std::runtime_error("borrow caller requires a direct get call followed by an indirect near call");
  Address next = result.get_call + 5;
  for (const auto &inst : decoded.instructions) {
    if (inst.target && *inst.target >= next && *inst.target <= result.call)
      throw std::runtime_error("alternate incoming edge between get and the indirect call");
  }
  for (const auto &inst : decoded.instructions) {
    if (inst.address < next || inst.address >= result.call)
      continue;
    if (inst.address != next || inst.flow != NativeFlow::ordinary ||
        !inst.falls_through || inst.may_store || inst.changes_stack || !inst.cache_copyable)
      throw std::runtime_error("get and indirect call must have straight-line code without intervening stores");
    next += inst.size;
  }
  if (next != result.call)
    throw std::runtime_error("indirect call is not reached directly after the borrowed return");
  return result;
}

std::pair<Address, std::size_t> entry_patch_window(Address entry, std::string_view bytes) {
  const auto decoded = native_entry::scan(entry, bytes);
  auto start = entry;
  if (bytes.starts_with("\xf3\x0f\x1e\xfa"))
    start += 4; // Preserve the landing instruction for an indirect caller.
  std::size_t size = 0;
  for (const auto &inst : decoded.instructions) {
    if (inst.address < start)
      continue;
    if (inst.address != start + size || inst.flow != NativeFlow::ordinary ||
        !inst.cache_entry_copyable || !inst.falls_through)
      break;
    size += inst.size;
    if (size >= 5)
      return {start, size};
  }
  throw std::runtime_error("code-cache entry redirect needs a five-byte nonfaulting prefix");
}

std::vector<Instruction> verify_retirement_prefix(Address pc, std::string_view bytes) {
  const auto decoded = native_entry::scan(pc, bytes);
  std::vector<Instruction> prefix;
  for (const auto &inst : decoded.instructions) {
    Instruction out;
    out.pc = inst.address;
    out.size = inst.size;
    std::memcpy(out.bytes.data(), bytes.data() + inst.address - pc, inst.size);
    prefix.push_back(out);
  }
  if (prefix.empty() || prefix.back().pc + prefix.back().size != pc + bytes.size())
    throw std::runtime_error("cannot verify destructor entry prefix");
  prefix.front().hook = Hook::destroy_begin;
  return prefix;
}

std::vector<Instruction> straight_fragment(const object_watch::Plan &plan,
    const native_entry::Preview &body, Address start, Hook hook, bool terminal = false,
    bool conditional_exit = false, std::optional<Address> tail_exit = {}, bool short_return = false,
    std::size_t minimum = 5) {
  if (body.decode_failed || !body.diagnostic.empty())
    throw std::runtime_error("cannot verify code around object watch fragment");
  const auto entry = std::find_if(body.instructions.begin(), body.instructions.end(),
      [&](const auto &instruction) { return instruction.address == start; });
  if (entry == body.instructions.end())
    throw std::runtime_error("object watch fragment has no decoded instruction entry");
  std::vector<Instruction> result;
  auto next = start;
  bool returned = false;
  for (auto current = entry; current != body.instructions.end(); ++current) {
    const auto &inst = *current;
    const bool exit = conditional_exit && inst.flow == NativeFlow::branch &&
        inst.falls_through && inst.target;
    const bool tail = tail_exit && inst.flow == NativeFlow::branch &&
        !inst.falls_through && inst.target == tail_exit;
    if (inst.address != next || (inst.flow != NativeFlow::ordinary &&
                                 !(terminal && inst.flow == NativeFlow::returns) && !exit && !tail))
      break;
    const auto bytes = plan.code(inst.address).substr(0, inst.size);
    if (inst.flow == NativeFlow::returns) {
      if (bytes != "\xc3")
        break;
      returned = true;
    } else if (!inst.cache_copyable && !inst.cache_rip_relative && !exit && !tail) {
      break;
    }
    if (inst.address != start &&
        std::any_of(body.instructions.begin(), body.instructions.end(), [&](const auto &candidate) {
          return candidate.target && *candidate.target == inst.address;
        }))
      break;
    Instruction &copy = result.emplace_back();
    copy.pc = inst.address;
    copy.size = inst.size;
    copy.rip_relative = inst.cache_rip_relative;
    std::memcpy(copy.bytes.data(), bytes.data(), inst.size);
    if (exit) {
      const auto opcode = copy.bytes[0];
      if (!((inst.size == 2 && opcode >= 0x70 && opcode <= 0x7f) ||
            (inst.size == 6 && opcode == 0x0f && copy.bytes[1] >= 0x80 && copy.bytes[1] <= 0x8f)))
        return {};
      copy.flow = Flow::conditional;
      copy.target = *inst.target;
      copy.condition = copy.bytes[inst.size == 2 ? 0 : 1] & 0xf;
    }
    if (tail) {
      if (!((inst.size == 5 && copy.bytes[0] == 0xe9) ||
            (inst.size == 2 && copy.bytes[0] == 0xeb)))
        return {};
      copy.flow = Flow::jump;
      copy.target = *inst.target;
    }
    copy.collection_resume = inst.address == start && hook == Hook::collection_resume;
    if (inst.address == start && hook == Hook::collection_enter)
      copy.scope_entry = true;
    else if (inst.address == start && hook != Hook::none && !copy.collection_resume)
      copy.hook = hook;
    if (copy.hook == Hook::none) {
      const auto store = plan.sites.find(inst.address);
      if (store != plan.sites.end() && store->second.store)
        instrument_store(copy, *store->second.store);
    }
    next += inst.size;
    if ((exit && !terminal) || tail)
      break;
    if (returned || (!terminal && next - start >= minimum))
      break;
  }
  if ((next - start < minimum && !(short_return && returned)) || (terminal && !returned))
    return {};
  return result;
}

api::ModuleMapping code_mapping(const api::ListModulesResponse &maps,
                                const std::string &path, Address pc, std::size_t size,
                                bool allow_without_build_id) {
  for (const auto &mapping : maps.mappings())
    if (mapping.path() == path && mapping.begin_address() <= pc &&
        pc < mapping.end_address() && size <= mapping.end_address() - pc &&
        mapping.inode() && (allow_without_build_id || !mapping.build_id().empty()))
      return mapping;
  throw std::runtime_error("code-cache patch has no verified live file-backed executable mapping");
}
} // namespace

struct Watch::Impl {
  api::ObjectWatchRequest request;
  std::uint64_t id;
  std::uint32_t pid;
  const std::uint64_t process_start;
  const std::string executable;
  std::function<void(api::TraceEvent)> publish;
  object_watch::Plan plan;
  std::vector<std::vector<Instruction>> functions;
  std::map<Address, std::vector<Address>> incoming;
  std::map<Address, api::ModuleMapping> patch_mappings;
  Dispatch dispatch;
  unsigned admitted_stores = 0;
  bool fragmented_writer = false;
  bool direct_writer_calls = false;
  bool fragmented_lifecycle = false;
  std::map<Address, std::size_t> entry_windows;
  std::set<Address> runtime_calls;
  std::map<Address, Address> runtime_indirect_ends;
  std::map<Address, Address> short_lifetime_relays;
  std::map<Address, Address> resume_targets;
  std::map<Address, object_watch::Source> sources;
  std::string path;
  Channel *channel = nullptr;
  struct stat channel_identity{};
  const SharedFile shared_file;
  const std::size_t event_buffer;
  const std::size_t runtime_limit;
  std::vector<api::ObjectWatchStatus::Redirect> redirects;
  std::atomic<std::uint64_t> head{0}, publish_errors{0}, coverage_gaps{0};
  std::map<std::uint64_t, api::TraceEvent> open_lifetimes;
  std::uint64_t final_observations = 0;
  std::jthread pump;
  std::jthread admissions;
  mutable std::mutex admission_mutex;
  std::string admission_failure;
  std::string admission_diagnostic;
  bool installed = false, draining = false;
  std::string diagnostic;

  Impl(const api::ObjectWatchRequest &spec, std::uint64_t watch_id, std::uint32_t target,
       const std::string &executable, const std::string &event_path,
       std::function<void(api::TraceEvent)> sink)
      : request(spec), id(watch_id), pid(target),
        process_start(pid ? proc::start_time(proc::directory(pid) / "stat") : 0),
        executable(executable), publish(std::move(sink)),
        plan(target, executable, spec), shared_file(spec.shared_file()),
        event_buffer(spec.event_buffer() ? spec.event_buffer() : default_event_buffer),
        runtime_limit(spec.follow_calls() ? (spec.runtime_limit() ? spec.runtime_limit() :
                                             default_runtime_limit) : 0) {
    if (event_buffer < 2 || event_buffer > max_event_buffer)
      throw std::runtime_error("invalid code-cache event buffer size");
    if (runtime_limit > max_runtime_limit || (spec.runtime_limit() && !spec.follow_calls()))
      throw std::invalid_argument("invalid runtime code-cache translation limit");
    admitted_stores = plan.stores;
    if (request.follow_calls() && (request.first_seen() || request.writer_entry() ||
        plan.caller_start || !request.owner_type().empty() ||
        plan.layout.logical_collection()))
      throw std::invalid_argument("runtime call following requires a standalone writer scope");
#if defined(__aarch64__)
    prepare_arm_fragments();
    if (request.follow_calls())
      for (const auto &[start, bytes] : plan.writer_ranges)
        prepare_runtime_calls(native_entry::scan(start, bytes));
#else
    if (plan.layout.logical_collection()) {
      prepare_collection_fragments();
    } else {
      std::vector<std::pair<Address, std::size_t>> writer_entries;
      admitted_stores = 0;
      for (const auto &[start, bytes] : plan.writer_ranges) {
        const auto body = native_entry::scan(start, bytes);
        const bool fragments = !request.first_seen() &&
            (!plan.shared_writer() || request.follow_calls()) &&
            !plan.caller_start && !request.writer_entry() &&
            std::ranges::any_of(body.instructions, [](const auto &inst) {
              return inst.flow == NativeFlow::call;
            });
        fragmented_writer |= fragments;
        if (fragments) {
          admitted_stores += prepare_writer_fragments(body);
        } else {
          auto function = verify_function(plan, start, bytes.size(), {});
          admitted_stores += std::ranges::count_if(function, [](const auto &inst) {
            return writer(inst.hook);
          });
          functions.push_back(std::move(function));
          writer_entries.emplace_back(start, bytes.size());
        }
      }
      plan.unsupported += plan.stores - admitted_stores;
      if (request.first_seen()) {
        if (std::any_of(functions[0].begin(), functions[0].end(), [&](const auto &inst) {
              return inst.flow != Flow::copy && inst.target == plan.writer_start;
            }))
          throw std::runtime_error("first-seen writer branches to its own entry");
        functions[0].front().scope_entry = true;
        for (auto &inst : functions[0])
          if (inst.size == 1 && inst.bytes[0] == 0xc3)
            inst.hook = Hook::collection_end;
      }
      if (!plan.caller_start && (!plan.shared_writer() || request.follow_calls())) {
        fragmented_lifecycle = plan.layout.primary_rtti || plan.virtual_destructor ||
            !plan.delegating_constructor_tails.empty() || !request.owner_type().empty() ||
            std::any_of(plan.boundaries.begin(), plan.boundaries.end(), [&](const auto &entry) {
              const auto [pc, size] = entry;
              const auto &site = plan.sites.at(pc);
              if (site.boundary != Boundary::construct && site.boundary != Boundary::destroy)
                return false;
              const auto code = native_entry::scan(pc, plan.code(pc).substr(0, size));
              return site.uncertain_complete ||
                  std::any_of(code.instructions.begin(), code.instructions.end(), [](const auto &inst) {
                    return inst.flow == NativeFlow::call || inst.changes_receiver;
                  });
            });
      }
      Address getter = 0;
      std::vector<Address> entries;
      std::map<Address, std::vector<Address>> explicit_redirects;
      const auto add_entry = [&](Address pc, std::size_t size) {
        const auto [patch, width] = entry_patch_window(pc, plan.code(pc).substr(0, size));
        entry_windows.emplace(patch, width);
        explicit_redirects.emplace(pc, std::vector<Address>{patch});
        return patch + width;
      };
      for (const auto &[start, size] : writer_entries) {
        if (plan.shared_writer() || plan.caller_start) {
          add_entry(start, size);
          continue;
        }
        if (request.writer_entry() || plan.writer_address_taken(start)) {
          prepare_writer_entry(start, size);
          continue;
        }
        if (plan.writer_exported(start)) {
          // An export alone does not establish an indirect caller.
          try {
            prepare_writer_entry(start, size);
            continue;
          } catch (const std::runtime_error &) {
            // Retain verified direct calls with partial coverage.
          }
        }
        direct_writer_calls = true;
        entries.push_back(start);
      }
      for (const auto &[pc, size] : plan.boundaries) {
        const auto &site = plan.sites.at(pc);
        const auto boundary = site.boundary;
        if (boundary == Boundary::writer_scope)
          continue;
        if (fragmented_lifecycle && plan.delegating_constructor_tails.count(pc))
          continue; // The target uses the same receiver and application return PC.
        if (site.deleting_destructor && !plan.caller_start) {
          prepare_deleting_destructor(pc, size);
          continue;
        }
        const bool fragment_boundary = fragmented_lifecycle ||
            (!plan.caller_start && boundary == Boundary::destroy && plan.virtual_destructor);
        if (fragment_boundary && (boundary == Boundary::construct || boundary == Boundary::destroy)) {
          const bool constructor = boundary == Boundary::construct;
          const auto enter = constructor ? Hook::constructor_enter : Hook::destructor_enter;
          const auto leave = constructor ? Hook::constructor_return : Hook::destructor_return;
          const bool needs_entry = constructor ?
              std::ranges::any_of(plan.delegating_constructor_tails,
                  [&](const auto &tail) { return tail.second == pc; }) : plan.virtual_destructor;
          const auto fragment_count = functions.size();
          try {
            prepare_boundary_fragments(pc, size, enter, leave, site.uncertain_complete);
          } catch (const std::runtime_error &) {
            if (functions.size() != fragment_count)
              throw;
            // Virtual calls and short delegating jumps can bypass or lack a
            // direct call redirect. Patch the entire leaf in those cases.
            const auto fragment_error = std::current_exception();
            std::vector<Instruction> leaf;
            try {
              leaf = decode_leaf(pc, plan.code(pc).substr(0, size), true,
                                 [](const auto &, Instruction &) {});
            } catch (const std::runtime_error &) {
              std::rethrow_exception(fragment_error);
            }
            if (needs_entry) {
              if (plan.code(pc).starts_with("\xf3\x0f\x1e\xfa"))
                leaf.erase(leaf.begin());
              if (leaf.empty() ||
                  std::ranges::any_of(leaf, [](const auto &inst) { return inst.flow != Flow::copy; }))
                throw std::runtime_error(constructor ? "delegating constructor target has no verified entry redirect" :
                    "virtual destructor has no verified entry redirect; indirect destruction would be missed");
            } else if (plan.direct_redirects({pc}).at(pc) != plan.direct_redirects({pc}, true).at(pc)) {
              throw std::runtime_error("native lifetime leaf has an unverified tail caller");
            }
            if (leaf.size() == 1 && !constructor && leaf.front().bytes[0] == 0xc3) {
              leaf.front().hook = Hook::destroy_both;
            } else {
              if (leaf.size() < 2 || std::ranges::any_of(leaf, [&](const auto &inst) {
                    return inst.flow != Flow::copy && inst.target == pc;
                  }))
                throw;
              leaf.front().hook = enter;
              leaf.front().uncertain_construct = site.uncertain_complete;
              for (auto &inst : leaf)
                if (inst.size == 1 && inst.bytes[0] == 0xc3) {
                  inst.hook = leave;
                  inst.boundary_pc = pc;
                }
            }
            leaf.front().boundary_pc = pc;
            if (needs_entry) {
              const auto end = leaf.back().pc + leaf.back().size;
              const auto padding = end - leaf.front().pc < 5 && end == pc + size ?
                  plan.redirect_padding(pc, leaf.front().pc, end) : 0;
              if (end - leaf.front().pc + padding < 5)
                throw std::runtime_error(constructor ? "delegating constructor target has no verified entry redirect" :
                    "virtual destructor has no verified entry redirect; indirect destruction would be missed");
              add_fragment(std::move(leaf), true, padding);
              continue;
            }
            functions.push_back(std::move(leaf));
            entries.push_back(pc);
          }
          continue;
        }
        if (!plan.caller_start && !request.owner_type().empty()) {
          const auto hooks = boundary == Boundary::add ? std::pair{Hook::owner_add_enter, Hook::owner_add_return} :
              boundary == Boundary::get ? std::pair{Hook::owner_get_enter, Hook::owner_get_return} :
              boundary == Boundary::remove ? std::pair{Hook::owner_remove_enter, Hook::owner_remove_return} :
                                             std::pair{Hook::none, Hook::none};
          if (hooks.first == Hook::none)
            throw std::runtime_error("unsupported native ownership boundary");
          try {
            auto leaf = verify_function(plan, pc, size, {});
            if (leaf.size() < 2 ||
                std::any_of(leaf.begin(), leaf.end(), [&](const auto &inst) {
                  return inst.flow != Flow::copy && inst.target == pc;
                }))
              throw std::runtime_error("owner leaf lacks independent entry and return anchors");
            leaf.front().hook = hooks.first;
            leaf.front().boundary_pc = pc;
            for (auto &inst : leaf)
              if (inst.size == 1 && inst.bytes[0] == 0xc3) {
                inst.hook = hooks.second;
                inst.boundary_pc = pc;
              }
            functions.push_back(std::move(leaf));
            entries.push_back(pc);
          } catch (const std::runtime_error &) {
            try {
              prepare_boundary_fragments(pc, size, hooks.first, hooks.second, false);
            } catch (const std::runtime_error &error) {
              throw std::runtime_error("cannot capture " + plan.source(pc).function + ": " + error.what());
            }
          }
          continue;
        }
        if (site.uncertain_complete)
          throw std::runtime_error("code-cache cannot prove the complete constructor at a shared base entry");
        if (boundary != Boundary::construct && boundary != Boundary::destroy &&
            boundary != Boundary::get && boundary != Boundary::add)
          throw std::runtime_error("unsupported code-cache lifetime boundary");
        if (plan.caller_start && boundary == Boundary::destroy) {
          const auto resume = add_entry(pc, size);
          if (resume >= pc + size)
            throw std::runtime_error("destructor entry redirect has no original continuation");
          resume_targets.emplace(pc, resume);
          functions.push_back(verify_retirement_prefix(pc, plan.code(pc).substr(0, resume - pc)));
        } else {
          functions.push_back(verify_function(plan, pc, size, boundary));
          if (plan.caller_start && (boundary == Boundary::construct || boundary == Boundary::add))
            add_entry(pc, size);
          else if (plan.shared_writer() && !plan.caller_start && plan.module_at(pc) == plan.module)
            add_entry(pc, size);
          else if (boundary != Boundary::get)
            entries.push_back(pc);
        }
        if (boundary == Boundary::get) {
          getter = pc;
          dispatch = verify_dispatch(plan, pc);
          functions[0].front().dispatch_entry = true;
          for (auto &inst : functions[0])
            if (inst.size == 1 && inst.bytes[0] == 0xc3)
              inst.hook = Hook::dispatch_end;
        }
      }
      auto callers = plan.direct_redirects(entries);
      incoming.merge(callers);
      incoming.merge(explicit_redirects);
      if (dispatch.get_call)
        incoming.emplace(getter, std::vector<Address>{dispatch.get_call});
      // Lifetime redirects own their call sites and entry windows. Plan
      // runtime routes against the finished static patch layout.
      if (request.follow_calls())
        for (const auto &[start, bytes] : plan.writer_ranges)
          prepare_runtime_calls(native_entry::scan(start, bytes));
    }
#endif
    if (direct_writer_calls)
      ++plan.unsupported; // Other calls into the original writer are outside this scope.
    if (request.follow_calls() && runtime_calls.empty() && !admitted_stores)
      throw std::runtime_error("selected scopes have no verified runtime call redirect");
    for (const auto &function : functions) {
      if (function.empty())
        continue;
      if (incoming.at(function.front().pc).empty())
        throw std::runtime_error("no verified direct executable redirect for " +
                                 plan.source(function.front().pc).function);
      sources.emplace(function.front().pc, plan.source(function.front().pc));
      for (const auto &inst : function)
        if (inst.hook != Hook::none || inst.collection_resume || inst.inline_mutation_entry) {
          sources.emplace(inst.pc, plan.source(inst.pc));
          if (inst.boundary_pc)
            sources.emplace(inst.boundary_pc, plan.source(inst.boundary_pc));
        }
    }
    for (const auto &[entry, sites] : incoming)
      for (const auto pc : sites)
        for (const auto &function : functions)
          if (!entry_windows.count(pc) && std::any_of(function.begin(), function.end(), [pc](const auto &inst) {
                if (inst.pc == pc && inst.flow == Flow::runtime_call)
                  return false;
                return pc >= inst.pc && pc < inst.pc + inst.size;
              }))
            throw std::runtime_error("a cache redirect overlaps a translated function");

    if (pid && (!entry_windows.empty() || !runtime_calls.empty())) {
      const auto maps = client::list_debug_module_mappings(pid);
      for (const auto &[entry, sites] : incoming)
        for (const auto pc : sites)
          patch_mappings.emplace(pc, code_mapping(maps, plan.module_at(pc), pc,
              patch_size(pc), !plan.shared_writer() || plan.module_at(pc) != plan.module));
    }
    if (event_path.empty())
      return;
    path = (std::filesystem::path(event_path).parent_path() / "object-cache-XXXXXX").string();
    const auto fd = ::mkostemp(path.data(), O_CLOEXEC);
    if (fd < 0)
      throw std::runtime_error("cannot create a private code-cache event channel");
    void *memory = MAP_FAILED;
    const auto size = Channel::mapping_size(event_buffer, runtime_limit);
    if (::fstat(fd, &channel_identity) == 0 && ::ftruncate(fd, size) == 0)
      memory = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (memory == MAP_FAILED) {
      ::unlink(path.c_str());
      throw std::runtime_error("cannot map the code-cache event channel");
    }
    channel = new (memory) Channel;
    for (std::size_t index = 0; index < event_buffer; ++index) {
      auto *slot = new (channel->slot(index)) Slot;
      slot->turn.store(index, std::memory_order_relaxed);
    }
    for (std::size_t index = 0; index < runtime_limit; ++index) {
      new (channel->module(event_buffer, index)) RuntimeModule;
      new (channel->scope_status(event_buffer, runtime_limit, index))
          std::atomic<unsigned>(RuntimeAdmission::idle);
    }
  }

  bool overlaps_fragment(Address start, Address end) const {
    return std::any_of(entry_windows.begin(), entry_windows.end(), [&](const auto &window) {
      return window.first < end && start < window.first + window.second;
    });
  }

  void add_fragment(std::vector<Instruction> fragment, bool terminal, std::size_t padding = 0,
                    Address relay = 0) {
    if (fragment.empty())
      throw std::runtime_error("cannot admit a disjoint native object watch fragment");
    const auto start = fragment.front().pc;
    const auto end = fragment.back().pc + fragment.back().size + padding;
    if (overlaps_fragment(start, end))
      throw std::runtime_error("native object watch fragment overlaps another redirect");
    entry_windows.emplace(start, end - start);
    incoming.emplace(start, std::vector<Address>{start});
    if (relay)
      short_lifetime_relays.emplace(start, relay);
    if (!terminal)
      resume_targets.emplace(start, end);
    functions.push_back(std::move(fragment));
  }

#if defined(__x86_64__)
  void prepare_writer_entry(Address start, std::size_t size) {
    const auto bytes = plan.code(start).substr(0, size);
    // verify_function has already verified the entire leaf, including faulting
    // instructions. The jump can replace those instructions because the
    // translated body executes them with their original application PCs.
    const auto &leaf = *std::ranges::find_if(functions, [&](const auto &function) {
      return function.front().pc == start;
    });
    const auto patch = bytes.starts_with("\xf3\x0f\x1e\xfa") ? start + 4 : start;
    auto end = patch;
    for (const auto &inst : leaf) {
      if (inst.pc < patch)
        continue;
      if (inst.pc != end || end - patch >= 5)
        break;
      end += inst.size;
    }
    if (end - patch < 5) {
      // The padding check also verifies that no direct edge enters the window.
      const auto padding = end == start + size ? plan.redirect_padding(start, patch, end) : 0;
      if (!padding)
        throw std::runtime_error("code-cache writer has no verified entry redirect");
      end += padding;
    } else {
      std::vector<Address> interior;
      for (auto pc = patch + 1; pc < end; ++pc)
        interior.push_back(pc);
      if (std::ranges::any_of(plan.direct_redirects(interior), [](const auto &edge) {
            return !edge.second.empty();
          }))
        throw std::runtime_error("an application edge enters the code-cache writer redirect");
    }
    if (std::ranges::any_of(leaf, [&](const auto &inst) {
          return inst.flow != Flow::copy && inst.target > patch && inst.target < end;
        }))
      throw std::runtime_error("an application edge enters the code-cache writer redirect");
    if (overlaps_fragment(patch, end) || incoming.count(start))
      throw std::runtime_error("code-cache writer entry overlaps another redirect");
    entry_windows.emplace(patch, end - patch);
    incoming.emplace(start, std::vector<Address>{patch});
  }
#endif

#if defined(__aarch64__)
  void prepare_arm_fragments() {
    if (plan.caller_start || request.writer_entry())
      throw std::runtime_error("AArch64 code cache requires a verified writer without an entry override");
    fragmented_writer = true;
    const auto instruction = [&](Address pc) {
      Instruction result;
      result.pc = pc;
      result.size = 4;
      const auto bytes = plan.code(pc).substr(0, result.size);
      if (bytes.size() != result.size || (pc & 3))
        throw std::runtime_error("AArch64 redirect is outside a verified instruction boundary");
      std::memcpy(result.bytes.data(), bytes.data(), bytes.size());
      return result;
    };
    for (const auto &[pc, site] : plan.sites) {
      if (!site.store)
        continue;
      if (pid && !site.store->result_register.empty() && !(::getauxval(AT_HWCAP) & HWCAP_ATOMICS))
        throw std::runtime_error("AArch64 LSE writer requires hardware atomic instructions");
      auto inst = instruction(pc);
      instrument_store(inst, *site.store);
      add_fragment({inst}, false);
    }
    if (!plan.inline_mutations.empty()) {
      const auto body = native_entry::scan(plan.writer_start, plan.writer_bytes);
      for (const auto &[entry, mutation] : plan.inline_mutations) {
        const auto add_anchor = [&](Address pc) {
          const auto existing = std::ranges::find_if(functions, [&](const auto &fragment) {
            return fragment.front().pc == pc;
          });
          if (existing != functions.end()) {
            inline_mutation_anchor(existing->front(), mutation);
            return;
          }
          const auto anchor = std::ranges::find_if(body.instructions, [&](const auto &candidate) {
            return candidate.address == pc;
          });
          if (anchor == body.instructions.end() || anchor->flow != NativeFlow::ordinary ||
              !anchor->cache_copyable)
            throw std::runtime_error("inlined collection mutation has no safe AArch64 redirect");
          auto copy = instruction(pc);
          inline_mutation_anchor(copy, mutation);
          add_fragment({copy}, false);
        };
        add_anchor(entry);
        for (const auto pc : mutation.continuations)
          add_anchor(pc);
      }
    }
    for (const auto &[pc, size] : plan.boundaries) {
      const auto &site = plan.sites.at(pc);
      if (plan.delegating_constructor_tails.count(pc))
        continue;
      Hook enter, leave;
      switch (*site.boundary) {
      case Boundary::construct:
        enter = Hook::constructor_enter;
        leave = Hook::constructor_return;
        break;
      case Boundary::destroy:
        enter = Hook::destructor_enter;
        leave = Hook::destructor_return;
        break;
      case Boundary::writer_scope:
        enter = Hook::collection_enter;
        leave = Hook::collection_end;
        break;
      case Boundary::collection_mutation:
        enter = Hook::collection_suspend;
        leave = Hook::none;
        break;
      case Boundary::add:
        enter = Hook::owner_add_enter;
        leave = Hook::owner_add_return;
        break;
      case Boundary::get:
        enter = Hook::owner_get_enter;
        leave = Hook::owner_get_return;
        break;
      case Boundary::remove:
        enter = Hook::owner_remove_enter;
        leave = Hook::owner_remove_return;
        break;
      default:
        throw std::runtime_error("unsupported AArch64 code-cache boundary");
      }
      const auto body = native_entry::scan(pc, plan.code(pc).substr(0, size));
      if (body.decode_failed || !body.diagnostic.empty() || body.instructions.empty())
        throw std::runtime_error("cannot verify AArch64 lifetime instructions");
      const auto external_tail = std::ranges::count_if(body.instructions, [&](const auto &candidate) {
        return candidate.flow == NativeFlow::branch && candidate.target &&
            (*candidate.target < pc || *candidate.target - pc >= size);
      });
      if (external_tail && (!site.deleting_destructor || external_tail != 1))
        throw std::runtime_error("AArch64 lifetime boundary has an unverified outgoing branch");
      auto entry = body.instructions.begin();
      if (entry->text.starts_with("bti"))
        ++entry;
      if (entry != body.instructions.end() &&
          (entry->text == "paciasp" || entry->text == "pacibsp"))
        ++entry;
      // An address materialization left in place preserves the receiver,
      // stack and link register. Observe the same entry at its next instruction.
      while (entry != body.instructions.end() && entry->cache_entry_copyable)
        ++entry;
      if (entry != body.instructions.end() &&
          std::any_of(body.instructions.begin(), body.instructions.end(), [&](const auto &candidate) {
            return candidate.flow == NativeFlow::branch &&
                   (!candidate.target || *candidate.target == entry->address);
          }))
        throw std::runtime_error("AArch64 lifetime entry may be re-entered within its function");
      if (enter == Hook::destructor_enter && entry != body.instructions.end() && entry->text == "ret" &&
          entry->cache_copyable) {
        auto terminal = instruction(entry->address);
        terminal.hook = Hook::destroy_both;
        terminal.boundary_pc = pc;
        add_fragment({terminal}, true);
        continue;
      }
      if (entry == body.instructions.end() || !entry->cache_copyable ||
          entry->flow != NativeFlow::ordinary)
        throw std::runtime_error("AArch64 lifetime entry cannot be safely relocated");
      const auto existing = std::ranges::find_if(functions, [&](const auto &fragment) {
        return fragment.front().pc == entry->address;
      });
      if (enter == Hook::collection_enter && existing != functions.end() && writer(existing->front().hook)) {
        existing->front().scope_entry = true;
      } else {
        auto inst = instruction(entry->address);
        inst.hook = enter;
        inst.boundary_pc = pc;
        inst.uncertain_construct = site.uncertain_complete;
        add_fragment({inst}, false);
      }
      if (enter == Hook::collection_suspend) {
        const auto writer_body = native_entry::scan(plan.writer_start, plan.writer_bytes);
        const auto successors = mutation_successors(writer_body, pc);
        for (const auto next : successors) {
          const auto successor = std::ranges::find_if(writer_body.instructions, [&](const auto &inst) {
            return inst.address == next;
          });
          if (successor == writer_body.instructions.end() || !successor->cache_copyable ||
              successor->flow != NativeFlow::ordinary)
            throw std::runtime_error("collection mutation has no relocatable original successor");
          const auto patch = std::ranges::find_if(functions, [&](const auto &fragment) {
            return fragment.front().pc == next && writer(fragment.front().hook);
          });
          if (patch != functions.end()) {
            patch->front().collection_resume = true;
            patch->front().boundary_pc = pc;
          } else {
            auto continuation = instruction(next);
            continuation.collection_resume = true;
            continuation.boundary_pc = pc;
            add_fragment({continuation}, false);
          }
        }
        continue;
      }
      unsigned returns = 0;
      for (const auto &candidate : body.instructions) {
        if (candidate.flow != NativeFlow::returns)
          continue;
        if ((candidate.text != "ret" && candidate.text != "retaa" && candidate.text != "retab") ||
            !candidate.cache_copyable)
          throw std::runtime_error("AArch64 lifetime return does not use the original link register");
        auto terminal = instruction(candidate.address);
        terminal.hook = leave;
        terminal.boundary_pc = pc;
        add_fragment({terminal}, true);
        ++returns;
      }
      if (external_tail && site.deleting_destructor) {
        const auto &tail = body.instructions.back();
        const auto next = plan.code(tail.address).substr(0, sizeof(std::uint32_t));
        std::uint32_t branch = 0;
        if (next.size() == sizeof(branch))
          std::memcpy(&branch, next.data(), sizeof(branch));
        if (tail.flow != NativeFlow::branch || tail.falls_through || !tail.target ||
            (branch & 0xfc000000U) != 0x14000000U || !plan.is_delete_target(*tail.target))
          throw std::runtime_error("AArch64 deleting destructor has an unverified tail target");
        auto terminal = instruction(tail.address);
        terminal.flow = Flow::jump;
        terminal.target = *tail.target;
        terminal.hook = leave;
        terminal.boundary_pc = pc;
        add_fragment({terminal}, true);
        ++returns;
      }
      if (!returns)
        throw std::runtime_error("AArch64 lifetime boundary has no verified native return");
    }
  }
#endif

#if !defined(__aarch64__)
  void prepare_deleting_destructor(Address pc, std::size_t size) {
    // D0 may inline D1, so D0 needs an entry hook even when no call to D1
    // survives. A nested D1 sees the identity has already been retired.
    const auto fragment_count = functions.size();
    try {
      prepare_boundary_fragments(pc, size, Hook::destructor_enter, Hook::destructor_return, false);
      return;
    } catch (const std::runtime_error &) {
      if (functions.size() != fragment_count)
        throw;
    }
    const auto bytes = plan.code(pc).substr(0, size);
    const auto body = native_entry::scan(pc, bytes);
    const auto &code = body.instructions;
    if (!body.decode_failed && body.diagnostic.empty() && !code.empty()) {
      const auto first_call = std::ranges::find_if(code, [](const auto &inst) {
        return inst.flow != NativeFlow::ordinary || !inst.falls_through;
      });
      // A D0 that directly forwards its receiver to a watched D1 needs
      // neither its own retirement nor a redirect after deallocation.
      if (first_call != code.end() && first_call->flow == NativeFlow::call && first_call->target &&
          plan.boundaries.count(*first_call->target) &&
          plan.sites.at(*first_call->target).boundary == Boundary::destroy &&
          !plan.sites.at(*first_call->target).deleting_destructor &&
          native_entry::forwards_receiver(pc, bytes.substr(0, first_call->address - pc)))
        return;
      const auto &tail = code.back();
      const auto start = bytes.starts_with("\xf3\x0f\x1e\xfa") ? pc + 4 : pc;
      // An inlined D1 followed by a tail jump to operator delete is done
      // destroying the object by the time execution reaches that jump.
      if (tail.flow == NativeFlow::branch && !tail.falls_through && tail.target &&
          plan.is_delete_target(*tail.target) &&
          std::ranges::none_of(code, [&](const auto &inst) {
            return inst.address != tail.address && inst.flow == NativeFlow::branch &&
                (!inst.target || *inst.target < pc || *inst.target - pc >= size);
          })) {
        auto entry = straight_fragment(plan, body, start, Hook::destructor_enter, false, false, tail.target);
        if (entry.empty())
          throw std::runtime_error("deleting destructor entry is too short for a native redirect");
        entry.front().boundary_pc = pc;
        if (entry.back().pc == tail.address) {
          if (entry.size() == 1) {
            entry.front().hook = Hook::destroy_both;
          } else {
            entry.back().hook = Hook::destructor_return;
            entry.back().boundary_pc = pc;
          }
          add_fragment(std::move(entry), true);
          return;
        }
        if (tail.size == direct_call_width && !overlaps_fragment(tail.address, tail.address + tail.size)) {
          auto leave = straight_fragment(plan, body, tail.address, Hook::destructor_return, false, false, tail.target);
          if (!leave.empty()) {
            leave.front().boundary_pc = pc;
            add_fragment(std::move(entry), false);
            add_fragment(std::move(leave), true);
            return;
          }
        }
      }
    }
    throw std::runtime_error("deleting destructor has no verified native entry and teardown boundary");
  }
#endif

  std::size_t prepare_writer_fragments(const native_entry::Preview &body) {
    if (body.decode_failed || !body.diagnostic.empty() ||
        std::any_of(body.instructions.begin(), body.instructions.end(), [](const auto &inst) {
          return inst.flow == NativeFlow::branch && !inst.target;
        }))
      throw std::runtime_error("native writer fragments require verified instruction and branch boundaries");
    std::set<Address> admitted;
    for (const auto &site : body.instructions) {
      const auto planned = plan.sites.find(site.address);
      if (planned == plan.sites.end() || !planned->second.store || admitted.count(site.address))
        continue;
      const auto after = std::upper_bound(body.instructions.begin(), body.instructions.end(),
          site.address, [](Address address, const auto &inst) { return address < inst.address; });
      for (auto current = std::make_reverse_iterator(after); current != body.instructions.rend();
           ++current) {
        if (site.address - current->address > 15 || current->flow != NativeFlow::ordinary)
          break;
        auto fragment = straight_fragment(plan, body, current->address, Hook::none);
        if (fragment.empty() || fragment.back().pc < site.address ||
            overlaps_fragment(fragment.front().pc, fragment.back().pc + fragment.back().size))
          continue;
        for (const auto &inst : fragment)
          if (writer(inst.hook))
            admitted.insert(inst.pc);
        add_fragment(std::move(fragment), false);
        break;
      }
    }
    if (admitted.empty() && !request.follow_calls())
      throw std::runtime_error("no independently patchable native writer store");
    return admitted.size();
  }

  void prepare_runtime_calls(const native_entry::Preview &body) {
#if defined(__aarch64__)
    if (body.decode_failed || !body.diagnostic.empty())
      throw std::runtime_error("cannot verify the AArch64 runtime call scope");
    for (const auto &call : body.instructions) {
      if (call.flow != NativeFlow::call || (call.target &&
          (plan.static_call_scopes.count(*call.target) || plan.boundaries.count(*call.target))))
        continue;
      const auto bytes = plan.code(call.address).substr(0, call.size);
      std::uint32_t word = 0;
      if (bytes.size() == sizeof(word))
        std::memcpy(&word, bytes.data(), sizeof(word));
      Instruction route;
      route.pc = call.address;
      route.size = sizeof(word);
      route.flow = Flow::runtime_call;
      const auto reg = (word >> 5) & 31;
      if (call.target && (word & 0xfc000000U) == 0x94000000U)
        route.target = *call.target;
      else if (!call.target && (word & 0xfffffc1fU) == 0xd63f0000U && reg != 31)
        route.value = reg + 1;
      else {
        ++plan.unsupported;
        continue;
      }
      std::memcpy(route.bytes.data(), &word, sizeof(word));
      add_fragment({route}, true);
      runtime_calls.insert(route.pc);
    }
#else
    std::vector<Address> interior;
    for (auto it = body.instructions.begin(); it != body.instructions.end(); ++it) {
      const auto &call = *it;
      if (call.flow != NativeFlow::call || (call.target &&
          (plan.static_call_scopes.count(*call.target) || plan.boundaries.count(*call.target))))
        continue;
      const auto bytes = plan.code(call.address).substr(0, call.size);
      const auto end = call.address + bytes.size();
      Instruction route;
      route.pc = call.address;
      route.size = bytes.size();
      route.flow = Flow::runtime_call;
      std::memcpy(route.bytes.data(), bytes.data(), bytes.size());
      std::vector<Instruction> prefix;
      if (call.target && bytes.size() == 5 && bytes.front() == '\xe8') {
        route.target = *call.target;
      } else if (call.indirect_near_call && !call.target) {
        if (const auto operand = x86_indirect_call(route)) {
          route.value = operand->reg;
          route.indirect_memory = !operand->reg;
          route.rip_relative = operand->rip_relative;
          auto start = call.address;
          for (auto previous = it; previous != body.instructions.begin();) {
            const auto &inst = *--previous;
            if (inst.flow != NativeFlow::ordinary || !inst.falls_through ||
                (!inst.cache_copyable && !inst.cache_rip_relative) ||
                inst.address + inst.size != start || inst.text == "endbr64")
              break;
            start = inst.address;
            Instruction copy;
            copy.pc = start;
            copy.size = inst.size;
            copy.rip_relative = inst.cache_rip_relative;
            const auto original = plan.code(start).substr(0, inst.size);
            std::memcpy(copy.bytes.data(), original.data(), inst.size);
            prefix.push_back(copy);
            if (end - start >= 10)
              break;
          }
          if (end - start < 10 || std::ranges::any_of(body.instructions, [&](const auto &inst) {
                return inst.target && *inst.target > start && *inst.target < end;
              }))
            prefix.clear();
          std::reverse(prefix.begin(), prefix.end());
        }
      }
      const auto start = prefix.empty() ? call.address : prefix.front().pc;
      const bool overlaps = std::ranges::any_of(incoming, [&](const auto &redirects) {
        return std::ranges::any_of(redirects.second, [&](Address pc) {
          return pc < end && start < pc + patch_size(pc);
        });
      });
      if ((!route.target && prefix.empty()) || overlaps) {
        ++plan.unsupported;
        continue;
      }
      if (!incoming.emplace(start, std::vector<Address>{start}).second)
        throw std::runtime_error("runtime call redirect overlaps another binding");
      runtime_calls.insert(start);
      if (!prefix.empty()) {
        entry_windows.emplace(start, end - start);
        runtime_indirect_ends.emplace(start, end);
        for (const auto &inst : prefix)
          if (inst.pc != start)
            interior.push_back(inst.pc);
        interior.push_back(call.address);
      }
      prefix.push_back(route);
      functions.push_back(std::move(prefix));
    }
    for (const auto &[target, sources] : plan.direct_redirects(interior))
      if (!sources.empty())
        throw std::runtime_error("an application edge enters the interior of a runtime call redirect");
#endif
  }

  void prepare_boundary_fragments(Address pc, std::size_t size, Hook enter, Hook leave,
                                  bool uncertain) {
    const auto bytes = plan.code(pc).substr(0, size);
    const auto body = native_entry::scan(pc, bytes);
    const auto exits_function = [&](const auto &inst) {
      return inst.flow == NativeFlow::branch && inst.target &&
          (*inst.target < pc || *inst.target - pc >= size);
    };
    const bool constructor = enter == Hook::constructor_enter;
    const bool destructor = enter == Hook::destructor_enter;
    if (body.decode_failed || !body.diagnostic.empty() ||
        std::any_of(body.instructions.begin(), body.instructions.end(), [&](const auto &inst) {
          if (inst.flow != NativeFlow::branch)
            return false;
          if (!inst.target)
            return true;
          if (!exits_function(inst))
            return false;
          if (destructor)
            return !plan.is_delete_target(*inst.target);
          return inst.falls_through || !constructor;
        }))
      throw std::runtime_error("cannot verify native lifetime instruction boundaries");
    const auto start = bytes.starts_with("\xf3\x0f\x1e\xfa") ? pc + 4 : pc;
    auto entry = straight_fragment(plan, body, start, enter);
    if (entry.empty())
      throw std::runtime_error("lifetime entry is too short for a native redirect");
    entry.front().boundary_pc = pc;
    entry.front().uncertain_construct = uncertain;
    struct ExitFragment {
      std::vector<Instruction> code;
      std::size_t padding = 0;
      Address relay = 0;
    };
    std::vector<ExitFragment> returns;
    const auto original_entry = entry;
    Address next_relay = start + 5;
    const bool constructor_tail = constructor && std::ranges::any_of(body.instructions, exits_function);
    bool need_successors = constructor_tail;
    const auto overlaps_entry = [&](const auto &fragment) {
      return fragment.front().pc < entry.back().pc + entry.back().size &&
             entry.front().pc < fragment.back().pc + fragment.back().size;
    };
    const auto check_interior = [&](Address begin, Address end) {
      std::vector<Address> interior;
      for (auto byte = begin + 1; byte < end; ++byte)
        interior.push_back(byte);
      // Check the original graph as well. The direct-edge index deliberately
      // ignores a function's own back edge to its entry.
      if (std::ranges::any_of(body.instructions, [&](const auto &candidate) {
            return candidate.target && *candidate.target > begin && *candidate.target < end;
          }))
        return false;
      return std::ranges::all_of(plan.direct_redirects(interior), [](const auto &edge) {
        return edge.second.empty();
      });
    };
    for (const auto &inst : body.instructions) {
      if (need_successors)
        break;
      const bool tail = exits_function(inst);
      if (inst.flow != NativeFlow::returns && !tail)
        continue;
      if (std::ranges::any_of(returns, [&](const auto &exit) {
            return exit.code.back().pc == inst.address;
          }))
        continue;
      if (tail && inst.falls_through) {
        // The taken edge has completed destruction. Keep the original
        // fallthrough (which can still perform destructor work).
        auto branch = straight_fragment(plan, body, inst.address, Hook::none, true, true);
        const bool combined = branch.size() == 2 && branch.back().bytes[0] == 0xc3;
        if (!combined)
          branch = straight_fragment(plan, body, inst.address, Hook::none, false, true);
        if (branch.empty() || overlaps_entry(branch))
          throw std::runtime_error("conditional deallocation tail has no disjoint native redirect");
        const auto end = branch.back().pc + branch.back().size;
        if (overlaps_fragment(inst.address, end) || !check_interior(inst.address, end))
          throw std::runtime_error("conditional deallocation tail has an alternate incoming edge");
        branch.front().hook = leave;
        branch.front().boundary_pc = pc;
        if (combined) {
          branch.back().hook = leave;
          branch.back().boundary_pc = pc;
        }
        returns.push_back({std::move(branch)});
        continue;
      }
      std::vector<Instruction> terminal;
      std::vector<Instruction> wide_entry;
      std::size_t padding = 0;
      Address relay = 0;
      for (const auto &candidate : body.instructions) {
        if (candidate.address > inst.address || inst.address - candidate.address > 15)
          continue;
        auto trial = straight_fragment(plan, body, candidate.address, Hook::none, !tail,
                                       false, tail ? inst.target : std::nullopt, true);
        const auto end = inst.address + inst.size;
        const auto width = end - candidate.address;
        std::size_t extension = 0;
#if defined(__x86_64__)
        if (!trial.empty() && width < 5 && end == pc + size)
          extension = plan.redirect_padding(pc, candidate.address, end);
#endif
        if (trial.empty() || trial.back().pc != inst.address || overlaps_entry(trial) ||
            overlaps_fragment(trial.front().pc, end + extension))
          continue;
        if (width + extension >= 5) {
          terminal = std::move(trial);
          padding = extension;
          relay = 0;
          wide_entry.clear();
          continue;
        }
        if (!destructor || tail || width < 2 || (!terminal.empty() && !relay))
          continue;
        // A short jump can reach a second rel32 inside the already replaced
        // entry. The relocated entry executes the original bytes there.
        const auto distance = static_cast<std::int64_t>(next_relay) -
                              static_cast<std::int64_t>(candidate.address + 2);
        if (distance < INT8_MIN || distance > INT8_MAX)
          continue;
        auto expanded = straight_fragment(plan, body, start, enter, false, false,
                                          std::nullopt, false, next_relay + 5 - start);
        if (expanded.empty())
          continue;
        const auto entry_end = expanded.back().pc + expanded.back().size;
        if (entry_end > candidate.address || overlaps_fragment(start, entry_end) ||
            std::ranges::any_of(returns, [&](const auto &exit) {
              return exit.code.front().pc < entry_end;
            }) || !check_interior(start, entry_end) || !check_interior(candidate.address, end))
          continue;
        expanded.front().boundary_pc = pc;
        expanded.front().uncertain_construct = uncertain;
        wide_entry = std::move(expanded);
        terminal = std::move(trial);
        padding = 0;
        relay = next_relay;
      }
      if (terminal.empty()) {
        need_successors = true;
        break;
      }
      terminal.back().hook = leave;
      terminal.back().boundary_pc = pc;
      returns.push_back({std::move(terminal), padding, relay});
      if (relay) {
        entry = std::move(wide_entry);
        next_relay += 5;
      }
    }
    if (returns.empty() || need_successors) {
      if (destructor && plan.virtual_destructor)
        throw std::runtime_error("virtual destructor requires verified returns for indirect calls");
      entry = original_entry;
      returns.clear();
      // A tail jump inherits its caller's return PC and never executes a
      // local continuation. Reject incoming branches unless the plan has
      // already proven them to be constructor delegates.
      const auto callers = plan.lifetime_return_callers(pc, constructor);
      for (const auto call : callers) {
        const auto next = call + 5;
        const auto [caller, length] = plan.containing_function(call);
        const auto roots = plan.exception_roots(caller, length);
        const auto caller_body = native_entry::scan(caller, plan.code(caller).substr(0, length), roots);
        auto fragment = straight_fragment(plan, caller_body, next, leave, false, true);
        if (fragment.empty() || overlaps_entry(fragment) ||
            overlaps_fragment(fragment.front().pc, fragment.back().pc + fragment.back().size))
          throw std::runtime_error("lifetime return continuation has no disjoint native redirect");
        const auto end = fragment.back().pc + fragment.back().size;
        if (std::ranges::any_of(roots, [&](const auto root) { return root >= next && root < end; }) ||
            !check_interior(next, end))
          throw std::runtime_error("an application edge enters the lifetime return continuation");
        fragment.front().boundary_pc = pc;
        fragment.front().successor_return = true;
        returns.push_back({std::move(fragment)});
      }
    }
    if (returns.empty())
      throw std::runtime_error("lifetime boundary has no verified native return or direct continuation");
    add_fragment(std::move(entry), false);
    for (auto &[fragment, padding, relay] : returns) {
      const bool conditional = fragment.back().flow == Flow::conditional &&
          fragment.back().hook == Hook::destructor_return;
      const auto terminal = !fragment.front().successor_return && !conditional;
      add_fragment(std::move(fragment), terminal, padding, relay);
    }
  }

  void prepare_collection_fragments() {
    using NativeInstruction = native_entry::Instruction;
    const auto writer_body = native_entry::scan(plan.writer_start, plan.writer_bytes);
    const auto add = [&](std::vector<Instruction> fragment, bool terminal) {
      add_fragment(std::move(fragment), terminal);
    };
    const auto past_endbr = [&](Address pc) {
      return plan.code(pc).starts_with("\xf3\x0f\x1e\xfa") ? pc + 4 : pc;
    };
    add(straight_fragment(plan, writer_body, past_endbr(plan.writer_start), Hook::collection_enter), false);
    std::set<Address> covered_returns;
    const auto add_continuation = [&](Address next, Address boundary) {
      auto tail = straight_fragment(plan, writer_body, next, Hook::collection_resume, true);
      if (!tail.empty() && tail.back().pc + tail.back().size - next <= 18 &&
          !overlaps_fragment(next, tail.back().pc + tail.back().size)) {
        tail.front().boundary_pc = boundary;
        tail.back().hook = Hook::collection_end;
        covered_returns.insert(tail.back().pc);
        add(std::move(tail), true);
        return;
      }
      auto continuation = straight_fragment(plan, writer_body, next, Hook::collection_resume, false, true);
      if (continuation.empty())
        throw std::runtime_error("collection mutation has no safe native return continuation");
      continuation.front().boundary_pc = boundary;
      add(std::move(continuation), false);
    };
    for (const auto &[entry, mutation] : plan.inline_mutations) {
      auto enter = straight_fragment(plan, writer_body, entry, Hook::none);
      if (enter.empty() || enter.back().pc + enter.back().size > mutation.entry_end)
        throw std::runtime_error("inlined collection mutation entry has no safe native redirect");
      inline_mutation_anchor(enter.front(), mutation);
      add(std::move(enter), false);
      for (const auto pc : mutation.continuations)
        add_continuation(pc, entry);
    }
    for (const auto &[pc, size] : plan.boundaries) {
      if (plan.sites.at(pc).boundary != Boundary::collection_mutation)
        continue;
      const auto mutation = native_entry::scan(pc, plan.code(pc).substr(0, size));
      auto enter = straight_fragment(plan, mutation, past_endbr(pc), Hook::collection_suspend);
      if (enter.empty())
        throw std::runtime_error("collection mutation entry has no safe native redirect");
      enter.front().boundary_pc = pc;
      add(std::move(enter), false);
      for (const auto next : mutation_successors(writer_body, pc))
        add_continuation(next, pc);
    }
    for (const auto &ret : writer_body.instructions) {
      const bool tail = ret.flow == NativeFlow::branch && !ret.falls_through && ret.target &&
          (*ret.target < plan.writer_start || *ret.target - plan.writer_start >= plan.writer_bytes.size());
      if ((ret.flow != NativeFlow::returns && !tail) || covered_returns.count(ret.address))
        continue;
      std::vector<Instruction> fragment;
      for (const NativeInstruction &candidate : writer_body.instructions) {
        if (candidate.address > ret.address || ret.address + ret.size - candidate.address > 18)
          continue;
        auto trial = straight_fragment(plan, writer_body, candidate.address, Hook::none, !tail,
                                       false, tail ? ret.target : std::nullopt);
        if (!trial.empty() && trial.back().pc == ret.address &&
            !overlaps_fragment(trial.front().pc, ret.address + ret.size))
          fragment = std::move(trial);
      }
      if (fragment.empty())
        throw std::runtime_error("collection writer return has no safe native redirect");
      fragment.back().hook = Hook::collection_end;
      add(std::move(fragment), true);
    }
    for (const auto &[pc, site] : plan.sites) {
      if (!site.store)
        continue;
      if (std::any_of(entry_windows.begin(), entry_windows.end(), [&](const auto &entry) {
            return pc >= entry.first && pc - entry.first < entry.second;
          }))
        continue;
      auto fragment = straight_fragment(plan, writer_body, pc, Hook::none);
      if (fragment.empty() || overlaps_fragment(pc, fragment.back().pc + fragment.back().size)) {
        // A short store immediately before a call cannot host a five-byte
        // jump. Include its verified straight-line predecessor when that
        // window still leaves the call and all incoming edges intact.
        const auto store = std::lower_bound(writer_body.instructions.begin(), writer_body.instructions.end(), pc,
            [](const auto &instruction, auto address) { return instruction.address < address; });
        for (auto prior = std::make_reverse_iterator(store); prior != writer_body.instructions.rend(); ++prior) {
          if (pc - prior->address > 14)
            break;
          const auto width = pc - prior->address + site.bytes.size();
          auto candidate = straight_fragment(plan, writer_body, prior->address, Hook::none,
                                             false, false, {}, false, std::max<std::size_t>(5, width));
          if (!candidate.empty() &&
              !overlaps_fragment(candidate.front().pc, candidate.back().pc + candidate.back().size)) {
            fragment = std::move(candidate);
            break;
          }
        }
      }
      if (fragment.empty() || overlaps_fragment(fragment.front().pc, fragment.back().pc + fragment.back().size))
        continue;
      add(std::move(fragment), false);
    }
    admitted_stores = 0;
    for (const auto &fragment : functions)
      admitted_stores += std::ranges::count_if(fragment, [](const auto &inst) {
        return writer(inst.hook);
      });
    if (!admitted_stores)
      throw std::runtime_error("no independently patchable native collection store");
    plan.unsupported += plan.stores - admitted_stores;
  }

  std::size_t patch_size(Address pc) const {
    const auto window = entry_windows.find(pc);
    return window == entry_windows.end() ? 5 : window->second;
  }

  ~Impl() {
    if (!channel)
      return;
    channel->activity.fetch_and(~enabled, std::memory_order_acq_rel);
    if (!installed && !draining)
      request_reclaim(*channel);
    admissions = {};
    pump = {};
    ::munmap(channel, Channel::mapping_size(event_buffer, runtime_limit));
    ::unlink(path.c_str());
  }

  void deliver(const Observation &sample, std::uint64_t sequence) {
    api::TraceEvent event;
    event.set_protocol_version(kProtocolVersion);
    event.set_session_id(request.session_id());
    event.set_pid(pid);
    event.set_tid(sample.tid);
    event.set_probe_id(id);
    event.set_probe_sequence(sequence);
    event.set_agent_dropped_events(channel->dropped.load(std::memory_order_relaxed));
    event.set_monotonic_time_ns(sample.time);
    event.set_function_name("object.trace");
    event.set_object_address(sample.object);
    auto &item = *event.mutable_object_observation();
    using Observation = api::ObjectObservation;
    if (sample.hook == Hook::create)
      item.set_kind(Observation::CREATED);
    else if (sample.hook == Hook::construction_aborted)
      item.set_kind(Observation::CONSTRUCTION_ABORTED);
    else if (sample.hook == Hook::construction_uncertain)
      item.set_kind(Observation::LIFETIME_UNCERTAIN);
    else if (sample.hook == Hook::destroy_begin)
      item.set_kind(Observation::DESTROY_BEGIN);
    else if (sample.hook == Hook::destroy_end)
      item.set_kind(Observation::TEARDOWN_END);
    else if (sample.hook == Hook::borrow)
      item.set_kind(Observation::BORROW_RETURN);
    else if (sample.hook == Hook::owner_add)
      item.set_kind(Observation::OWNER_ADDED);
    else if (sample.hook == Hook::owner_remove)
      item.set_kind(Observation::OWNER_REMOVED);
    else if (sample.hook == Hook::collection_enter)
      item.set_kind(Observation::FIRST_SEEN);
    else if (sample.hook == Hook::collection_bound)
      item.set_kind(Observation::RANGE_BOUND);
    else if (sample.hook == Hook::collection_suspend)
      item.set_kind(Observation::RANGE_SUSPENDED);
    else if (sample.hook == Hook::collection_end || sample.hook == Hook::lifetime_reused)
      item.set_kind(Observation::OBSERVATION_ENDED);
    else if (sample.hook == Hook::collection_gap || sample.hook == Hook::async_gap ||
             sample.hook == Hook::store_fault || sample.hook == Hook::owner_gap ||
             sample.hook == Hook::dispatch_gap || sample.hook == Hook::shared_mapping_gap ||
             sample.hook == Hook::post_store_signal || sample.hook == Hook::scope_signal ||
             sample.hook == Hook::admission_gap)
      item.set_kind(Observation::COVERAGE_GAP);
    else
      item.set_kind(request.no_values() || (writer(sample.hook) && sample.gap) ?
          Observation::FIELD_WRITTEN : Observation::FIELD_CHANGED);
    item.set_lifetime_id(sample.id);
    item.set_generation(sample.generation);
    item.set_borrow_id(sample.borrow_id);
    item.set_storage_generation(sample.storage_generation);
    item.set_concrete_type(request.concrete_type());
    item.set_external_write_order_unknown(shared_file.identity.inode != 0);
    if (sample.has_shared_file_offset) {
      auto &storage = *item.mutable_shared_storage();
      storage.set_device(shared_file.identity.device);
      storage.set_inode(shared_file.identity.inode);
      storage.set_file_offset(sample.shared_file_offset);
    }
    item.set_owner_address(sample.owner);
    if (sample.owner_index_plus_one)
      item.set_owner_index(sample.owner_index_plus_one - 1);
    if (sample.runtime_module && sample.runtime_module <= runtime_limit) {
      plan.describe_runtime_origin(sample.pc, sample.runtime_module,
          *channel->module(event_buffer, sample.runtime_module - 1), item);
    } else {
      const auto known_source = sources.find(sample.pc);
      const auto source = known_source != sources.end() ? known_source->second : plan.source(sample.pc);
      plan.describe_origin(sample.pc, source, item);
    }
    const bool store = writer(sample.hook) || sample.hook == Hook::store_fault ||
                       sample.hook == Hook::post_store_signal || sample.hook == Hook::scope_signal;
    item.set_location_kind(store ? Observation::STORE : Observation::ENTRY_ANCHOR);
    if (sample.hook == Hook::construction_aborted) {
      item.set_closure_reason(Observation::UNWOUND);
      item.set_diagnostic("constructor unwound without a verified return");
    } else if (sample.hook == Hook::construction_uncertain) {
      item.set_diagnostic(sample.gap ? "complete and base constructors share the same entry" :
                                      "constructor returned with an unverified receiver");
    } else if (sample.hook == Hook::destroy_end) {
      item.set_closure_reason(Observation::DESTRUCTOR_RETURN);
    } else if (sample.hook == Hook::lifetime_reused) {
      item.set_closure_reason(Observation::SCOPE_CHANGED);
      item.set_diagnostic("receiver storage reused without an observed destructor");
    }
    if (sample.hook == Hook::async_gap)
      item.set_diagnostic("asynchronous signal " + std::to_string(sample.gap) +
                          " arrived in a generated helper; application PC unavailable");
    if (sample.hook == Hook::create)
      item.set_field(plan.layout.union_tag_offset ? object_watch::field_path(request, plan.layout) :
                     plan.layout.opaque_union ? "<storage>" : request.field());
    if (store) {
      item.set_destination(sample.destination);
      item.set_field(object_watch::field_path(request, plan.layout));
    }
    if (sample.hook == Hook::store_fault)
      item.set_diagnostic("store did not reach a verified successor; signal " +
                          std::to_string(sample.gap));
    if (sample.hook == Hook::post_store_signal)
      item.set_diagnostic("asynchronous signal " + std::to_string(sample.gap) +
                          " interrupted value capture after the application store");
    if (sample.hook == Hook::scope_signal)
      item.set_diagnostic("asynchronous signal " + std::to_string(sample.gap) +
                          " interrupted a store at a non-repeatable scope boundary");
    if (sample.hook == Hook::shared_mapping_gap) {
      item.set_destination(sample.destination);
      item.set_diagnostic(sample.id ? "shared destination could not be verified after the store" :
                                      "shared destination changed during construction");
    }
    if (sample.hook == Hook::dispatch_gap) {
      item.set_destination(sample.destination);
      item.set_diagnostic(sample.gap == 2 ? "runtime translation limit reached" :
          "callee could not be translated from verified live executable bytes");
    }
    if (sample.hook == Hook::owner_gap)
      item.set_diagnostic("owner membership could not be verified at the original method boundary");
    if (sample.hook == Hook::admission_gap)
      item.set_diagnostic("receiver admission limit reached");
    if (sample.hook == Hook::owner_remove && sample.gap)
      item.set_diagnostic("previous membership absent at transfer boundary");
    if (plan.layout.logical_collection() || plan.layout.union_tag_offset || shared_file.identity.inode)
      item.set_synchronization(request.synchronization());
    if (sample.hook == Hook::collection_bound || sample.hook == Hook::collection_suspend) {
      item.set_destination(sample.destination);
      item.set_field(object_watch::field_path(request, plan.layout));
    }
    if (request.first_seen()) {
      if (sample.hook == Hook::collection_enter)
        item.set_diagnostic("receiver identity is limited to this invocation");
      if (sample.hook == Hook::collection_end)
        item.set_closure_reason(Observation::METHOD_RETURN);
      if (sample.hook == Hook::collection_gap) {
        constexpr const char *messages[] = {"collection coverage unavailable",
            "collection key is unreadable, duplicated or outside the read budget",
            "previous collection writer invocation has no verified return",
            "declared collection synchronization was violated",
            "collection range changed outside a verified mutation return",
            "old field value is unreadable", "receiver admission limit reached",
            "pointee path is null, unreadable or outside the address range",
            "bound field value is unreadable"};
        item.set_diagnostic(messages[std::min<std::size_t>(sample.gap, std::size(messages) - 1)]);
      }
    }
    if (sample.values || (plan.layout.opaque_union && (writer(sample.hook) || sample.hook == Hook::create)))
      object_watch::set_scalar_values(item, plan.layout,
          {sample.values & 1 ? std::optional(sample.old_value) : std::nullopt,
           sample.values & 4 ? std::optional(sample.old_tag) : std::nullopt},
          {sample.values & 2 ? std::optional(sample.new_value) : std::nullopt,
           sample.values & 8 ? std::optional(sample.new_tag) : std::nullopt});
    if (writer(sample.hook) && sample.gap) {
      if (sample.gap == 2)
        item.set_diagnostic("field changed between observed stores; old value unavailable");
      else if (sample.gap == 3)
        item.set_diagnostic(sample.hook == Hook::rmw ? "read-modify-write values could not be verified" :
                                                     "store values could not be verified");
      else
        item.set_diagnostic(request.serialize_wait_us() ? "observed write ordering unavailable" :
                            "overlapping observed writes; old value unavailable");
    }
    if (sample.depth) {
      auto &stack = *event.mutable_stack();
      stack.set_requested_depth(request.stack_depth());
      const bool cfi = sample.stack_status != 0;
      stack.set_status(cfi ? static_cast<api::StackTrace::Status>(sample.stack_status) :
                             api::StackTrace::PARTIAL);
      stack.set_detail(cfi ? "native CFI from the original application instruction" :
                             "frame-pointer walk in the main executable");
      const auto depth = std::min<unsigned>({sample.depth, request.stack_depth(), stack_limit});
      for (unsigned index = 0; index < depth; ++index) {
        const auto pc = sample.frames[index];
#if defined(__aarch64__)
        constexpr unsigned call_size = 4;
#else
        constexpr unsigned call_size = 1;
#endif
        const auto lookup = index && pc >= call_size ? pc - call_size : pc;
        const bool runtime_origin = !index && sample.runtime_module;
        const bool application = runtime_origin || !plan.code(lookup).empty();
        api::StackFrame runtime_frame;
        const bool runtime_caller = !application && sample.runtime_module &&
            plan.describe_runtime_frame(lookup, sample.runtime_module, runtime_frame);
        if (!application && !runtime_caller && !cfi)
          break;
        auto &frame = *stack.add_frames();
        if (runtime_caller)
          frame.Swap(&runtime_frame);
        frame.set_pc(pc);
        frame.set_stack_pointer(sample.stack_pointers[index]);
        frame.set_kind(api::StackFrame::NATIVE);
        frame.set_application_frame(application || runtime_caller);
        if (!application || runtime_caller)
          continue;
        if (runtime_origin) {
          frame.set_module(item.module());
          frame.set_module_offset(item.elf_pc());
          frame.set_file(item.file());
          frame.set_line(item.line());
          frame.set_function(item.function());
          continue;
        }
        const auto source = plan.source(lookup);
        frame.set_module(plan.module_at(lookup));
        frame.set_module_offset(plan.elf_pc(lookup));
        frame.set_file(source.file);
        frame.set_line(source.line);
        frame.set_function(source.function);
      }
      stack.set_captured_depth(stack.frames_size());
      if (!cfi && stack.captured_depth() == request.stack_depth())
        stack.set_status(api::StackTrace::TRUNCATED);
    }
    if (item.kind() == Observation::CREATED || item.kind() == Observation::FIRST_SEEN) {
      api::TraceEvent anchor;
      anchor.set_tid(event.tid());
      anchor.set_object_address(event.object_address());
      auto &identity = *anchor.mutable_object_observation();
      identity.set_generation(item.generation());
      identity.set_storage_generation(item.storage_generation());
      if (item.has_shared_storage())
        *identity.mutable_shared_storage() = item.shared_storage();
      open_lifetimes[item.lifetime_id()] = std::move(anchor);
    } else if (item.kind() == Observation::TEARDOWN_END || item.kind() == Observation::OBSERVATION_ENDED ||
               (plan.caller_start && item.kind() == Observation::DESTROY_BEGIN)) {
      open_lifetimes.erase(item.lifetime_id());
    } else if (auto found = open_lifetimes.find(item.lifetime_id()); found != open_lifetimes.end()) {
      found->second.mutable_object_observation()->set_owner_address(item.owner_address());
      found->second.mutable_object_observation()->set_storage_generation(item.storage_generation());
    }
    publish(std::move(event));
    if (sample.hook == Hook::collection_gap || sample.hook == Hook::async_gap ||
        sample.hook == Hook::dispatch_gap ||
        sample.hook == Hook::store_fault || sample.hook == Hook::owner_gap ||
        sample.hook == Hook::shared_mapping_gap || sample.hook == Hook::post_store_signal ||
        sample.hook == Hook::scope_signal || sample.hook == Hook::lifetime_reused ||
        sample.hook == Hook::admission_gap ||
        (writer(sample.hook) && sample.gap))
      coverage_gaps.fetch_add(1, std::memory_order_relaxed);
  }

  void collect(std::stop_token stop) noexcept {
    std::uint64_t next = 0;
    for (;;) {
      auto &slot = *channel->slot(next % event_buffer);
      if (slot.turn.load(std::memory_order_acquire) != next + 1) {
        if (stop.stop_requested())
          return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      const auto event = slot.event;
      slot.turn.store(next + event_buffer, std::memory_order_release);
      try {
        deliver(event, next + 1);
      } catch (...) {
        publish_errors.fetch_add(1, std::memory_order_relaxed);
      }
      head.store(++next, std::memory_order_release);
    }
  }

  void reply_to_admission(unsigned phase) noexcept {
    channel->admission.phase.store(phase, std::memory_order_release);
    ::syscall(SYS_futex, &channel->admission.phase, FUTEX_WAKE, 1, nullptr, nullptr, 0);
  }

  void admit_runtime_scope() noexcept {
    const std::lock_guard lock(admission_mutex);
    auto &admission = channel->admission;
    unsigned ready = RuntimeAdmission::ready;
    if (!admission.phase.compare_exchange_strong(ready, RuntimeAdmission::committing,
                                                  std::memory_order_acq_rel))
      return;
    if (admission.scope >= runtime_limit) {
      reply_to_admission(RuntimeAdmission::idle);
      return;
    }
    auto &scope_status = *channel->scope_status(event_buffer, runtime_limit, admission.scope);
    const auto reply = [&](unsigned status, unsigned phase = RuntimeAdmission::idle) {
      scope_status.store(status, std::memory_order_release);
      reply_to_admission(phase);
    };
    injector::Process process;
    try {
      const auto start = admission.function;
      const auto width = admission.width;
      if (!start || !width || width > UINT64_MAX - start || !admission.count ||
          admission.count > width || admission.count > SIZE_MAX / sizeof(RuntimePatch) ||
          !admission.module || admission.module > runtime_limit)
        throw std::runtime_error("runtime scope is outside the verified executable");
      const auto &module = *channel->module(event_buffer, admission.module - 1);
      if (module.generation.load(std::memory_order_acquire) != admission.module ||
          module.path.front() != '/' || module.path.back() != '\0')
        throw std::runtime_error("runtime scope has no verified mapped ELF identity");
      const std::string module_path(module.path.data());
      process.attach_for_patch(pid);
      const auto directory = proc::directory(pid);
      const auto verify_identity = [&] {
        if (proc::start_time(directory / "stat") != process_start ||
            std::filesystem::read_symlink(directory / "exe") != executable)
          throw std::runtime_error("target execution changed during runtime scope admission");
      };
      verify_identity();
      std::vector<RuntimePatch> requested(admission.count);
      process.remote_read(admission.patches, requested.data(), requested.size() * sizeof(RuntimePatch));
      std::vector<api::ObjectWatchStatus::Redirect> additions;
      std::map<Address, std::size_t> windows;
      const auto original = plan.runtime_scope_code(module_path, module.bias, start, module.sha256, width,
          [&](std::string_view original) {
            const auto body = native_entry::scan(start, original);
            if (body.decode_failed || !body.diagnostic.empty())
              throw std::runtime_error("runtime scope instructions could not be verified");
            std::vector<Address> interior;
            for (const auto &patch : requested) {
              const auto instruction = std::ranges::lower_bound(body.instructions, patch.pc, {},
                                                               &native_entry::Instruction::address);
              if (instruction == body.instructions.end() || instruction->address != patch.pc ||
                  patch.size == 0 || patch.size > patch.original.size() ||
                  patch.size > width - (patch.pc - start) || !patch.gateway ||
                  patch.gateway >= (Address{1} << 63) ||
                  (patch.call && !patch.dispatch &&
                   (instruction->flow != NativeFlow::call || instruction->size != patch.size)) ||
                  (patch.dispatch && (!patch.call || patch.size < 10 || patch.dispatch >= (Address{1} << 63))) ||
                  !windows.emplace(patch.pc, patch.size).second ||
                  std::ranges::any_of(redirects, [&](const auto &owned) {
                    return owned.pc() < patch.pc + patch.size &&
                           patch.pc < owned.pc() + owned.original().size();
                  }))
                throw std::runtime_error("runtime scope redirect overlaps or exceeds a verified window");
              const auto before = original.substr(patch.pc - start, patch.size);
              if (std::memcmp(before.data(), patch.original.data(), before.size()))
                throw std::runtime_error("runtime scope redirect disagrees with verified ELF bytes");
              auto end = instruction;
              auto next = patch.pc;
              while (end != body.instructions.end() && next < patch.pc + patch.size) {
                if (end->address != next)
                  break;
                if (next != patch.pc)
                  interior.push_back(next);
                next += end->size;
                ++end;
              }
              if (next != patch.pc + patch.size ||
                  (patch.dispatch && (end == instruction || (end - 1)->flow != NativeFlow::call ||
                                      (end - 1)->target || !(end - 1)->indirect_near_call)))
                throw std::runtime_error("runtime scope redirect splits an original instruction");
              auto &redirect = additions.emplace_back();
              redirect.set_pc(patch.pc);
              redirect.set_translated_pc(patch.gateway);
              redirect.set_original(std::string(before));
              std::string replacement;
#if defined(__aarch64__)
              auto branch = direct_branch(patch.pc, patch.gateway);
              if (!branch || patch.dispatch || patch.size != sizeof(*branch))
                throw std::runtime_error("runtime AArch64 gateway is outside the branch range");
              if (patch.call)
                *branch |= 0x80000000U;
              replacement.assign(reinterpret_cast<const char *>(&*branch), sizeof(*branch));
#else
              const auto delta = static_cast<std::int64_t>(patch.gateway) - static_cast<std::int64_t>(patch.pc + 5);
              if (patch.size < 5 || delta < INT32_MIN || delta > INT32_MAX)
                throw std::runtime_error("runtime x86-64 gateway is outside the branch range");
              replacement.assign(patch.size, '\x90');
              replacement.front() = patch.call && !patch.dispatch ? '\xe8' : '\xe9';
              const auto displacement = static_cast<std::int32_t>(delta);
              std::memcpy(replacement.data() + 1, &displacement, sizeof(displacement));
              if (patch.dispatch) {
                const auto call_delta = static_cast<std::int64_t>(patch.dispatch) -
                                        static_cast<std::int64_t>(patch.pc + patch.size);
                if (call_delta < INT32_MIN || call_delta > INT32_MAX)
                  throw std::runtime_error("runtime dispatch is outside the x86-64 branch range");
                const auto offset = patch.size - 5;
                replacement[offset] = '\xe8';
                const auto call_displacement = static_cast<std::int32_t>(call_delta);
                std::memcpy(replacement.data() + offset + 1, &call_displacement, sizeof(call_displacement));
              }
#endif
              redirect.set_installed(std::move(replacement));
            }
            return interior;
          });
      std::vector<injector::CodePatch> batch;
      for (const auto &redirect : additions)
        batch.push_back({redirect.pc(), redirect.original().data(), redirect.installed().data(),
                         redirect.original().size()});
      std::vector<Address> destinations;
      for (const auto &patch : requested) {
        destinations.push_back(patch.gateway);
        if (patch.dispatch)
          destinations.push_back(patch.dispatch);
      }
      std::map<Address, api::ModuleMapping> mappings;
      redirects.reserve(redirects.size() + additions.size());
      const auto committed = process.patch_code_batch(batch, [&] {
        verify_identity();
        auto live = std::string(original.size(), '\0');
        process.remote_read(start, live.data(), live.size());
        if (live != original)
          throw std::runtime_error("live runtime scope disagrees with verified ELF bytes");
        std::ifstream maps(directory / "maps");
        std::vector<std::pair<Address, Address>> gateways;
        bool same_channel = false, same_code = false;
        for (std::string line; std::getline(maps, line);) {
          unsigned long long begin, end, offset, inode;
          unsigned device_major, device_minor;
          char permissions[5]{};
          if (std::sscanf(line.c_str(), "%llx-%llx %4s %llx %x:%x %llu",
                          &begin, &end, permissions, &offset, &device_major, &device_minor, &inode) != 7)
            continue;
          same_channel |= inode == channel_identity.st_ino &&
              device_major == major(channel_identity.st_dev) && device_minor == minor(channel_identity.st_dev);
          same_code |= begin <= start && start < end && width <= end - start &&
              permissions[0] == 'r' && permissions[1] == '-' && permissions[2] == 'x' &&
              inode == admission.inode && device_major == major(admission.device) &&
              device_minor == minor(admission.device) && start - begin <= admission.file_offset &&
              admission.file_offset - (start - begin) == offset;
          if (permissions[0] == 'r' && permissions[1] == '-' &&
              permissions[2] == 'x' && permissions[3] == 'p' && !inode)
            gateways.emplace_back(begin, end);
        }
        if (!same_channel || !same_code)
          throw std::runtime_error("runtime scope mapping changed before the redirects could be committed");
        const auto modules = client::list_debug_module_mappings(pid);
        for (const auto destination : destinations)
          if (std::ranges::none_of(gateways, [&](const auto &range) {
                return range.first <= destination && destination < range.second;
              }))
            throw std::runtime_error("runtime scope gateway is outside private executable memory");
        for (const auto &redirect : additions)
          mappings.emplace(redirect.pc(), code_mapping(modules, module_path, redirect.pc(),
                                                       redirect.original().size(), true));
      }, [&]() noexcept {
        redirects.insert(redirects.end(), std::make_move_iterator(additions.begin()),
                         std::make_move_iterator(additions.end()));
        entry_windows.merge(windows);
        patch_mappings.merge(mappings);
        scope_status.store(RuntimeAdmission::accepted, std::memory_order_release);
        channel->runtime_translations.fetch_add(1, std::memory_order_relaxed);
      });
      process.detach();
      reply(committed ? RuntimeAdmission::accepted : RuntimeAdmission::rejected);
    } catch (const std::exception &error) {
      admission_diagnostic = error.what();
      if (process.unsafe()) {
        admission_failure = error.what();
        channel->activity.fetch_and(~enabled, std::memory_order_acq_rel);
        reply(RuntimeAdmission::rejected, RuntimeAdmission::disabled);
      } else {
        reply(RuntimeAdmission::rejected);
      }
    }
  }

  void verify_live(injector::Process &process) const {
    if (plan.layout.logical_collection() || fragmented_writer || fragmented_lifecycle)
      plan.verify_live(pid);
    if (plan.caller_start) {
      std::string current(plan.caller_bytes.size(), '\0');
      process.remote_read(plan.caller_start, current.data(), current.size());
      if (current != plan.caller_bytes)
        throw std::runtime_error("live borrow caller disagrees with verified ELF");
    }
    for (const auto &function : functions)
      for (const auto &instruction : function) {
        std::array<std::uint8_t, 15> current{};
        process.remote_read(instruction.pc, current.data(), instruction.size);
        if (!std::equal(current.begin(), current.begin() + instruction.size,
                        instruction.bytes.begin()))
          throw std::runtime_error("live code-cache instruction disagrees with verified ELF");
      }
  }

  void verify_mappings() const {
    if (patch_mappings.empty())
      return;
    const auto maps = client::list_debug_module_mappings(pid);
    for (const auto &[pc, prior] : patch_mappings) {
      const auto current = code_mapping(maps, prior.path(), pc, patch_size(pc),
                                        !plan.shared_writer() || prior.path() != plan.module);
      if (current.begin_address() != prior.begin_address() ||
          current.end_address() != prior.end_address() ||
          current.file_offset() != prior.file_offset() ||
          current.device() != prior.device() || current.inode() != prior.inode() ||
          current.build_id() != prior.build_id())
        throw std::runtime_error("live mapping identity changed at a code-cache redirect");
    }
  }

  std::vector<injector::CodePatch> patches(bool restore) const {
    std::vector<injector::CodePatch> result;
    for (const auto &redirect : redirects)
      result.push_back({redirect.pc(), (restore ? redirect.installed() : redirect.original()).data(),
                        (restore ? redirect.original() : redirect.installed()).data(),
                        redirect.original().size()});
    return result;
  }
};

Watch::Watch(const api::ObjectWatchRequest &request, std::uint64_t id, std::uint32_t pid,
             const std::string &executable, const std::string &event_path,
             std::function<void(api::TraceEvent)> publish)
    : impl_(std::make_unique<Impl>(request, id, pid, executable, event_path, std::move(publish))) {}
Watch::~Watch() = default;

void Watch::install(injector::Process &process, std::uintptr_t agent_entry,
                    std::chrono::steady_clock::time_point deadline, bool permanent_startup_module) {
  auto &p = *impl_;
  p.verify_live(process);
  Init init;
  if (p.path.size() >= sizeof(init.path))
    throw std::runtime_error("code-cache event channel path is too long");
  std::memcpy(init.path, p.path.c_str(), p.path.size() + 1);
  init.scalar = p.plan.layout.scalar;
  init.field_width = p.plan.layout.field_width;
  init.tagged_union = p.plan.layout.union_tag_offset.has_value();
  init.union_tag_offset = p.plan.layout.union_tag_offset.value_or(0);
  init.receiver_typeinfo = p.plan.layout.primary_rtti || !p.request.owner_type().empty() ?
      p.plan.typeinfo : 0;
  for (const auto sig : p.request.async_signals())
    init.async_signals |= 1ULL << (sig - 1);
  init.stack_depth = p.request.stack_depth();
  init.max_instances = p.request.max_instances();
  init.writer_tid = p.request.writer_tid();
  init.event_buffer = p.event_buffer;
  init.serialize_wait_us = p.request.serialize_wait_us();
  init.runtime_limit = p.runtime_limit;
  init.shared_file = p.shared_file.identity;
  init.max_events = p.request.max_events();
  init.no_values = p.request.no_values();
  init.stop_on_overflow = p.request.overflow_policy() == api::ObjectWatchRequest::STOP_CAPTURE;
  init.cfi_stack = p.request.cfi_stack();
  init.atomic = p.plan.layout.atomic;
  init.first_seen = p.request.first_seen();
  init.permanent_startup_module = permanent_startup_module;
  init.collection.max_instances = p.request.max_instances() ? p.request.max_instances() : 1024;
  if (p.plan.shared_writer()) {
    if (p.plan.module.size() >= sizeof(init.module))
      throw std::runtime_error("shared writer module path is too long");
    std::memcpy(init.module, p.plan.module.c_str(), p.plan.module.size() + 1);
    init.module_bias = p.plan.bias;
  }
  if (p.plan.layout.logical_collection()) {
    const auto &layout = p.plan.layout;
    init.collection = {layout.vector_object, layout.vector_begin, layout.vector_end,
        layout.vector_capacity, layout.stride, layout.key_offset,
        p.request.element_key(), p.request.max_elements(),
        p.request.max_instances() ? p.request.max_instances() : 1024};
    init.collection.is_map = layout.logical_map;
    init.collection.pointer_hops = layout.pointer_offsets.size();
    std::copy(layout.pointer_offsets.begin(), layout.pointer_offsets.end(),
              init.collection.pointer_offsets.begin());
    init.collection.map = layout.map;
    init.collection.max_string_bytes = p.request.max_string_bytes();
    const auto &key = p.request.map_key();
    if (key.size() > init.collection.map_key.size())
      throw std::runtime_error("code-cache map key exceeds its verified bound");
    std::copy(key.begin(), key.end(), init.collection.map_key.begin());
    init.collection.map_key_size = key.size();
  }
  if (!p.request.owner_type().empty()) {
    const auto &layout = p.plan.layout;
    init.borrow = {p.dispatch.get_call ? p.dispatch.get_call + 5 : 0, p.dispatch.successor, p.plan.typeinfo,
                   layout.vector_begin, layout.vector_end, layout.vector_capacity,
                   p.request.max_elements(), layout.base_offset};
  }
  std::vector<Function> translated(p.functions.size());
  const auto table_size = translated.size() * sizeof(Function);
  const std::vector<Address> static_scopes(p.plan.static_call_scopes.begin(), p.plan.static_call_scopes.end());
  const auto scopes_size = static_scopes.size() * sizeof(Address);
  std::vector<ConstructorReturn> constructor_returns;
  for (const auto &[entry, pc] : p.plan.complete_constructor_returns)
    constructor_returns.push_back({entry, pc});
  const auto returns_size = constructor_returns.size() * sizeof(ConstructorReturn);
  std::size_t size = sizeof(init) + table_size + scopes_size + returns_size;
  for (const auto &function : p.functions) {
    if (function.size() > (SIZE_MAX - size) / sizeof(Instruction))
      throw std::runtime_error("code-cache request size overflows");
    size += function.size() * sizeof(Instruction);
  }
  const auto remote = process.remote_allocate(size);
  try {
    const auto table = remote + sizeof(init);
    auto address = table + table_size;
    if (!static_scopes.empty()) {
      init.static_scopes = reinterpret_cast<const Address *>(address);
      init.static_scope_count = static_scopes.size();
      process.remote_write(address, static_scopes.data(), scopes_size);
      address += scopes_size;
    }
    if (!constructor_returns.empty()) {
      init.complete_constructor_returns = reinterpret_cast<const ConstructorReturn *>(address);
      init.complete_constructor_return_count = constructor_returns.size();
      process.remote_write(address, constructor_returns.data(), returns_size);
      address += returns_size;
    }
    for (std::size_t index = 0; index < p.functions.size(); ++index) {
      const auto &function = p.functions[index];
      const auto &callers = p.incoming.at(function.front().pc);
      auto &descriptor = translated[index];
      descriptor = {reinterpret_cast<const Instruction *>(address), function.size(), callers.front(), 0};
      if (const auto resume = p.resume_targets.find(function.front().pc);
          resume != p.resume_targets.end())
        descriptor.resume = resume->second;
      const auto bytes = function.size() * sizeof(Instruction);
      process.remote_write(address, function.data(), bytes);
      address += bytes;
    }
    init.functions = reinterpret_cast<Function *>(table);
    init.function_count = translated.size();
    process.remote_write(table, translated.data(), table_size);
    process.remote_write(remote, &init, sizeof(init));
    const auto result = process.remote_call(agent_entry, {remote}, 1);
    process.remote_read(remote, &init, sizeof(init));
    if (result)
      throw std::runtime_error(std::string("code-cache initialization failed: ") + init.error);
    process.remote_read(table, translated.data(), table_size);
  } catch (...) {
    if (!process.unsafe())
      process.remote_free(remote, size);
    throw;
  }
  process.remote_free(remote, size);
  ::unlink(p.path.c_str());
  p.path.clear();

  for (unsigned index = 0; index < p.functions.size(); ++index) {
    if (p.functions[index].empty())
      continue;
    const auto translated_pc = translated[index].translated;
    for (auto caller : p.incoming.at(p.functions[index].front().pc)) {
      api::ObjectWatchStatus::Redirect redirect;
      redirect.set_pc(caller);
      redirect.set_translated_pc(translated_pc);
      redirect.set_original(std::string(p.plan.code(caller).substr(0, p.patch_size(caller))));
      auto bytes = redirect.original();
#if defined(__aarch64__)
      auto branch = direct_branch(caller, translated_pc);
      if (!branch || bytes.size() != sizeof(*branch))
        throw std::runtime_error("AArch64 code-cache mapping is out of branch range");
      if (p.runtime_calls.count(caller))
        *branch |= 0x80000000U; // BL preserves the original application link address.
      std::memcpy(bytes.data(), &*branch, sizeof(*branch));
#else
      const auto put_branch = [&](Address pc, Address target) {
        const auto delta = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(pc + 5);
        if (delta < std::numeric_limits<std::int32_t>::min() ||
            delta > std::numeric_limits<std::int32_t>::max())
          throw std::runtime_error("code-cache mapping is out of range of a verified redirect");
        const auto displacement = static_cast<std::int32_t>(delta);
        std::memcpy(bytes.data() + (pc - caller) + 1, &displacement, sizeof(displacement));
      };
      if (const auto relay = p.short_lifetime_relays.find(caller);
          relay != p.short_lifetime_relays.end()) {
        bytes.assign(bytes.size(), '\x90');
        bytes[0] = '\xeb';
        bytes[1] = static_cast<char>(static_cast<std::int64_t>(relay->second) -
                                     static_cast<std::int64_t>(caller + 2));
        redirect.set_installed(std::move(bytes));
        p.redirects.push_back(std::move(redirect));
        continue;
      }
      const bool entry = p.entry_windows.count(caller);
      if (entry) {
        bytes.assign(bytes.size(), '\x90');
        bytes[0] = '\xe9';
      }
      put_branch(caller, translated_pc);
      if (const auto end = p.runtime_indirect_ends.find(caller); end != p.runtime_indirect_ends.end()) {
        const auto call_pc = end->second - 5;
        bytes[call_pc - caller] = '\xe8';
        put_branch(call_pc, translated[index].dispatch);
      }
      for (const auto &[exit, relay] : p.short_lifetime_relays) {
        if (relay <= caller || relay + 5 > caller + bytes.size())
          continue;
        const auto destination = std::ranges::find_if(p.functions, [&](const auto &function) {
          return function.front().pc == exit;
        });
        if (destination == p.functions.end())
          throw std::runtime_error("missing translated lifetime relay destination");
        bytes[relay - caller] = '\xe9';
        put_branch(relay, translated[destination - p.functions.begin()].translated);
      }
#endif
      redirect.set_installed(std::move(bytes));
      p.redirects.push_back(std::move(redirect));
    }
  }

  p.pump = std::jthread([&p](std::stop_token stop) { p.collect(stop); });
  if (p.runtime_limit) {
    p.admissions = std::jthread([&p](std::stop_token stop) {
      while (!stop.stop_requested()) {
        if (p.channel->admission.phase.load(std::memory_order_acquire) == RuntimeAdmission::ready)
          p.admit_runtime_scope();
        else
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
  }
  p.verify_live(process);
  const auto batch = p.patches(false);
  try {
    while (!process.patch_code_batch(batch, [&] {
      p.verify_mappings();
      if (!p.entry_windows.empty() || !p.runtime_calls.empty() || p.fragmented_lifecycle)
        p.verify_live(process);
    }, [&]() noexcept {
      if (p.request.timeout_seconds())
        p.channel->deadline_ns.store(std::chrono::duration_cast<std::chrono::nanoseconds>(
            (std::chrono::steady_clock::now() + std::chrono::seconds(p.request.timeout_seconds()))
                .time_since_epoch()).count(), std::memory_order_release);
      if (p.runtime_limit)
        p.channel->runtime_prefix_gate.store(enabled, std::memory_order_release);
      p.channel->activity.store(enabled, std::memory_order_release);
    })) {
      // The occupied window was left intact and the peers were released.
      if (std::chrono::steady_clock::now() >= deadline)
        throw std::runtime_error("a target thread occupies a code-cache redirect");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  } catch (...) {
    p.channel->activity.fetch_and(~enabled, std::memory_order_acq_rel);
    if (process.unsafe()) {
      p.installed = true;
      p.draining = true;
      p.diagnostic = "code-cache redirect installation is uncertain";
    }
    throw;
  }
  p.installed = true;
  if (p.runtime_limit)
    p.reply_to_admission(RuntimeAdmission::idle);
  p.diagnostic = !p.runtime_calls.empty() ? "runtime call redirects with verified original return PCs" :
      p.fragmented_writer ? "native writer fragments with verified original continuations" :
      p.plan.layout.logical_collection() ?
      "native collection fragments with verified mutation and storage generations" : p.dispatch.call ?
      "in-process leaf translation with verified borrowed indirect dispatch" :
#if defined(__aarch64__)
      "in-process AArch64 leaf translation with verified scalar and atomic redirects";
#else
      p.direct_writer_calls ?
      "in-process x86-64 leaf translation with atomic MOV/XADD/CMPXCHG and verified direct writer call sites only"
      " (use --writer-entry to require entry coverage)" :
      "in-process x86-64 leaf translation with atomic MOV/XADD/CMPXCHG and verified writer entry redirects";
#endif
}

void Watch::stop_runtime_admissions() {
  auto &p = *impl_;
  {
    const std::lock_guard lock(p.admission_mutex);
    if (!p.admission_failure.empty())
      throw std::runtime_error(p.admission_failure);
    p.reply_to_admission(RuntimeAdmission::disabled);
  }
  p.admissions = {};
}

bool Watch::restore(injector::Process &process) {
  auto &p = *impl_;
  if (!p.installed)
    return true;
  p.draining = true;
  try {
    if (!process.patch_code_batch(p.patches(true), [&] {
          p.verify_mappings();
          if (p.channel->runtime_prefix_gate.load(std::memory_order_acquire) & ~enabled)
            throw RuntimePrefixBusy{};
        }, [&]() noexcept {
          p.channel->runtime_prefix_gate.store(0, std::memory_order_release);
          request_reclaim(*p.channel);
        })) {
      p.diagnostic = "a target thread occupies a code-cache redirect; restoration pending";
      return false;
    }
  } catch (const RuntimePrefixBusy &) {
    p.diagnostic = "a thread is entering through a runtime call redirect; restoration pending";
    return false;
  } catch (const std::exception &error) {
    p.channel->activity.fetch_and(~enabled, std::memory_order_acq_rel);
    p.diagnostic = error.what();
    throw;
  }
  p.installed = false;
  p.channel->activity.fetch_and(~enabled, std::memory_order_acq_rel);
  p.diagnostic = "all code-cache redirects restored";
  return true;
}

bool Watch::retire_if_mapping_changed(std::chrono::steady_clock::time_point deadline) {
  auto &p = *impl_;
  std::unique_lock lock(p.admission_mutex);
  if (!p.installed || p.redirects.empty())
    return false;
  std::ifstream maps(proc::directory(p.pid) / "maps");
  if (!maps)
    return false;
  std::vector<std::pair<Address, Address>> live_ranges;
  for (std::string line; std::getline(maps, line);) {
    unsigned long long begin, end, offset, inode;
    unsigned dev_major, dev_minor;
    char permissions[5]{};
    if (std::sscanf(line.c_str(), "%llx-%llx %4s %llx %x:%x %llu",
                    &begin, &end, permissions, &offset, &dev_major, &dev_minor, &inode) != 7)
      return false;
    live_ranges.emplace_back(begin, end);
    if (inode == p.channel_identity.st_ino && dev_major == major(p.channel_identity.st_dev) &&
        dev_minor == minor(p.channel_identity.st_dev))
      return false;
    if (permissions[2] == 'x' && std::ranges::any_of(p.redirects, [&](const auto &redirect) {
          return redirect.translated_pc() >= begin && redirect.translated_pc() < end;
        }))
      return false;
  }
  if (live_ranges.empty() || maps.bad())
    return false;

  // An absent channel alone does not establish that restoration is unnecessary.
  // A live redirect still points at our original cache even if another actor
  // removed the channel mapping independently.
  for (const auto &redirect : p.redirects) {
    std::array<char, 15> bytes{};
    const auto size = redirect.installed().size();
    if (size > bytes.size())
      return false;
    iovec local{bytes.data(), size};
    iovec remote{reinterpret_cast<void *>(redirect.pc()), size};
    const auto amount = linux_api::process_vm_readv(p.pid, &local, 1, &remote, 1, 0);
    if (amount == static_cast<ssize_t>(size)) {
      if (std::string_view(bytes.data(), size) == redirect.installed())
        return false;
    } else if (amount != -1 || errno != EFAULT ||
               std::ranges::any_of(live_ranges, [&](const auto &range) {
                 return redirect.pc() < range.second &&
                        redirect.pc() + size > range.first;
               })) {
      return false;
    }
  }
  p.reply_to_admission(RuntimeAdmission::disabled);
  lock.unlock();
  finish(deadline, api::ObjectObservation::MAPPING_CHANGED);
  return true;
}

void Watch::finish(std::chrono::steady_clock::time_point deadline,
                   api::ObjectObservation::ClosureReason reason) {
  using Observation = api::ObjectObservation;
  const bool process_exited = reason == Observation::TARGET_EXITED;
  const bool mappings_changed = reason == Observation::MAPPING_CHANGED;
  auto &p = *impl_;
  {
    const std::lock_guard lock(p.admission_mutex);
    p.reply_to_admission(RuntimeAdmission::disabled);
  }
  p.admissions = {};
  p.channel->activity.fetch_and(~enabled, std::memory_order_acq_rel);
  if (!process_exited && !mappings_changed &&
      p.channel->capture_limit.load(std::memory_order_acquire) == Channel::overflow)
    reason = Observation::BUFFER_OVERFLOW;
  if (!process_exited && !mappings_changed) {
    while (p.channel->activity.load(std::memory_order_acquire) ||
           p.head.load(std::memory_order_acquire) != p.channel->tail.load(std::memory_order_acquire)) {
      if (std::chrono::steady_clock::now() >= deadline)
        throw std::runtime_error("code-cache observations are still draining");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  p.pump = {};
  const auto emit_final = [&](api::TraceEvent &event, Observation::Kind kind) {
    const auto sequence = p.head.load(std::memory_order_acquire) + p.final_observations;
    if (p.request.max_events() && sequence >= p.request.max_events())
      return false;
    event.set_protocol_version(kProtocolVersion);
    event.set_session_id(p.request.session_id());
    event.set_pid(p.pid);
    event.set_probe_id(p.id);
    event.set_probe_sequence(sequence + 1);
    event.set_agent_dropped_events(p.channel->dropped.load(std::memory_order_relaxed));
    event.set_function_name("object.trace");
    event.set_monotonic_time_ns(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    auto &observation = *event.mutable_object_observation();
    observation.set_kind(kind);
    observation.set_concrete_type(p.request.concrete_type());
    observation.set_source_unavailable(true);
    observation.set_external_write_order_unknown(p.shared_file.identity.inode != 0);
    observation.set_closure_reason(reason);
    p.publish(std::move(event));
    ++p.final_observations;
    return true;
  };
  if (mappings_changed) {
    p.coverage_gaps.fetch_add(1, std::memory_order_relaxed);
    api::TraceEvent event;
    event.mutable_object_observation()->set_diagnostic("the observed execution mappings changed");
    emit_final(event, Observation::COVERAGE_GAP);
  }
  for (auto &[id, event] : p.open_lifetimes) {
    event.mutable_object_observation()->set_lifetime_id(id);
    if (!emit_final(event, Observation::OBSERVATION_ENDED))
      break;
  }
  p.open_lifetimes.clear();
  p.draining = false;
  if (process_exited || mappings_changed) {
    p.installed = false;
    p.diagnostic = mappings_changed ? "execution mapping changed and old redirects are absent" :
                                     "target exited";
  }
}

api::ObjectWatchStatus Watch::status() const {
  const auto &p = *impl_;
  const std::lock_guard lock(p.admission_mutex);
  api::ObjectWatchStatus report;
  report.set_watch_id(p.id);
  report.set_backend(api::ObjectWatchRequest::CODE_CACHE);
  report.set_event_buffer(p.event_buffer);
  report.set_active(p.installed);
  report.set_restoration_pending(p.draining || !p.admission_failure.empty());
  report.set_store_sites(p.admitted_stores);
  report.set_unsupported_sites(p.plan.unsupported);
  report.set_no_values(p.request.no_values());
  report.set_external_write_order_unknown(p.shared_file.identity.inode != 0);
  const auto received = p.head.load(std::memory_order_acquire);
  const auto errors = std::min(received, p.publish_errors.load(std::memory_order_relaxed));
  report.set_observations(received - errors + p.final_observations);
  report.set_executed_stores(p.channel->executed_stores.load(std::memory_order_relaxed));
  report.set_dropped(p.channel->dropped.load(std::memory_order_relaxed) +
                     p.publish_errors.load(std::memory_order_relaxed));
  report.set_coverage_gaps(p.coverage_gaps.load(std::memory_order_relaxed));
  report.set_runtime_translations(p.channel->runtime_translations.load(std::memory_order_acquire));
  report.set_runtime_reclaimed(p.channel->runtime_reclaimed.load(std::memory_order_acquire));
  report.set_static_fragments(p.channel->static_fragments.load(std::memory_order_acquire));
  report.set_static_reclaimed(p.channel->static_reclaimed.load(std::memory_order_acquire));
  report.set_diagnostic(!p.admission_failure.empty() ? p.admission_failure :
      p.installed && !p.admission_diagnostic.empty() ? p.admission_diagnostic : p.diagnostic);
  if (const auto limit = p.channel->capture_limit.load(std::memory_order_relaxed)) {
    report.set_capture_limit(limit == Channel::event_limit ? "max-events" :
                             limit == Channel::overflow ? "buffer-overflow" : "timeout");
  } else if (const auto deadline = p.channel->deadline_ns.load(std::memory_order_relaxed);
             deadline && p.installed && deadline <= static_cast<std::uint64_t>(
                 std::chrono::duration_cast<std::chrono::nanoseconds>(
                     std::chrono::steady_clock::now().time_since_epoch()).count())) {
    report.set_capture_limit("timeout");
  }
  if (p.dispatch.call) {
    report.set_dispatch_pc(p.dispatch.call);
    report.set_dispatch_target(p.plan.writer_start);
    report.set_dispatch_return(p.dispatch.successor);
    report.set_matched_dispatches(p.channel->matched_dispatches.load(std::memory_order_relaxed));
  }
  for (const auto &redirect : p.redirects)
    *report.add_redirects() = redirect;
  return report;
}

api::ObjectWatchStatus Watch::preview() const {
  const auto &p = *impl_;
  if (p.pid)
    p.plan.verify_live(p.pid);
  api::ObjectWatchStatus report;
  report.set_backend(api::ObjectWatchRequest::CODE_CACHE);
  report.set_event_buffer(p.event_buffer);
  report.set_store_sites(p.admitted_stores);
  report.set_unsupported_sites(p.plan.unsupported);
  report.set_no_values(p.request.no_values());
  report.set_external_write_order_unknown(p.shared_file.identity.inode != 0);
  report.set_diagnostic(!p.runtime_calls.empty() ? "verified call redirects; callees resolve at runtime" :
      p.fragmented_writer ? "verified native writer fragments and redirect windows" :
      p.plan.layout.logical_collection() ? "verified native collection fragments and redirect windows" :
      p.direct_writer_calls ? "verified direct writer call sites only (use --writer-entry to require entry coverage)" :
                        "verified leaf translations and candidate redirect windows");
  *report.mutable_plan() = p.plan.describe(p.pid != 0);
  if (p.pid) {
    for (const auto &[entry, sites] : p.incoming)
      for (auto pc : sites) {
        auto &redirect = *report.add_redirects();
        redirect.set_pc(pc);
        redirect.set_original(std::string(p.plan.code(pc).substr(0, p.patch_size(pc))));
      }
    if (p.dispatch.call) {
      report.set_dispatch_pc(p.dispatch.call);
      report.set_dispatch_target(p.plan.writer_start);
      report.set_dispatch_return(p.dispatch.successor);
    }
  }
  return report;
}
} // namespace tracer::object_cache

namespace tracer::object_watch {
clangtracer::v1::ObjectWatchStatus preview(
    const clangtracer::v1::ObjectWatchRequest &request, std::uint32_t pid,
    const std::string &executable) {
  using Request = clangtracer::v1::ObjectWatchRequest;
  if (request.follow_calls() && request.backend() != Request::CODE_CACHE)
    throw std::invalid_argument("runtime call following requires code-cache write capture");
  clangtracer::v1::ObjectWatchStatus result;
  if (request.backend() == Request::CODE_CACHE) {
    result = object_cache::Watch(request, 0, pid, executable, {}, {}).preview();
  } else {
    Plan plan(pid, executable, request);
    if (pid)
      plan.verify_live(pid);
    result.set_store_sites(plan.stores);
    result.set_unsupported_sites(plan.unsupported);
    result.set_no_values(request.no_values());
    result.set_diagnostic("verified ELF store sites and lifecycle anchors for diagnostic capture");
    *result.mutable_plan() = plan.describe(pid != 0);
  }
  result.set_binding_status(request.has_prepared_bindings()
      ? compare_prepared(request.prepared_bindings(), result.plan(), request.no_values())
      : pid ? "live" : "offline");
  return result;
}
} // namespace tracer::object_watch
