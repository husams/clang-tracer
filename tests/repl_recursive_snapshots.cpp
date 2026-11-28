// Automatic capture of records that contain themselves (issue #180): the
// generated codec captures populated trees, lists and shared graphs, and the
// REPL's snapshot decoder reads every node back.
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// clang-format off
#include "../src/object_snapshot.hpp"
#include "fixtures/snapshot_recursive.hpp"
#include "context_codec.hpp"
#include "snapshot_recursive_generated.hpp"
// clang-format on

namespace tracer_repl {
using namespace recursive_fixture;
template <> struct EventRecord<TreeNode> {
  static constexpr const char *type_name = "recursive_fixture::TreeNode";
  static auto fields() {
    return std::make_tuple(event_member("v", &TreeNode::v), event_member("name", &TreeNode::name),
                           event_member("kids", &TreeNode::kids));
  }
};
template <> struct EventRecord<ListNode> {
  static constexpr const char *type_name = "recursive_fixture::ListNode";
  static auto fields() {
    return std::make_tuple(event_member("v", &ListNode::v), event_member("next", &ListNode::next));
  }
};
template <> struct EventRecord<SharedNode> {
  static constexpr const char *type_name = "recursive_fixture::SharedNode";
  static auto fields() {
    return std::make_tuple(
        event_member("v", &SharedNode::v), event_member("other", &SharedNode::other),
        event_member("next", &SharedNode::next), event_member("text", &SharedNode::text));
  }
};
template <> struct EventRecord<Entry> {
  static constexpr const char *type_name = "recursive_fixture::Entry";
  static auto fields() {
    return std::make_tuple(event_member("name", &Entry::name),
                           event_member("folder", &Entry::folder));
  }
};
template <> struct EventRecord<Folder> {
  static constexpr const char *type_name = "recursive_fixture::Folder";
  static auto fields() {
    return std::make_tuple(event_member("id", &Folder::id),
                           event_member("entries", &Folder::entries),
                           event_member("named", &Folder::named));
  }
};
template <> struct EventRecord<Holder> {
  static constexpr const char *type_name = "recursive_fixture::Holder";
  static auto fields() {
    return std::make_tuple(event_member("left", &Holder::left),
                           event_member("right", &Holder::right),
                           event_member("folder", &Holder::folder),
                           event_member("tail", &Holder::tail));
  }
};
} // namespace tracer_repl

namespace {
using namespace recursive_fixture;
using namespace tracer::context_engine;
using tracer_repl::EventValue;
namespace generated = snapshot_recursive_generated;

void require(bool ok, const std::string &message) {
  if (!ok)
    throw std::runtime_error(message);
}

template <class Error, class Call>
std::string rejects(Call call, const char *message) {
  try {
    call();
  } catch (const Error &error) {
    return error.what();
  }
  throw std::runtime_error(message);
}

struct Capture {
  bool ok = false;
  CaptureStatus status = CaptureStatus::ok;
  std::string payload;
  std::string cut, failure;
};

// What the generated capture code does for one root (value_snapshot_codegen.cpp).
template <class T> Capture capture(const T &value, CaptureLimits limits = {}) {
  std::vector<std::byte> storage(limits.bytes);
  BoundedProtoWriter writer(storage, limits);
  EncodeBudget budget;
  budget.max_items = limits.item_cap;
  budget.item_cap = limits.item_cap;
  budget.limits = limits;
  EncodeBudget::PointerGraph pointers(limits.items);
  budget.pointer_graph = &pointers;
  Capture result;
  const bool encoded = encode_root(writer, value, budget);
  const auto bytes = writer.finish();
  result.status = writer.status();
  result.ok = encoded && result.status == CaptureStatus::ok;
  result.payload.assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  result.cut = budget.pointee_cut ? budget.pointee_cut : "";
  result.failure = budget.failure ? budget.failure : "";
  return result;
}

EventValue decode(const generated::Schema &schema, const std::string &payload) {
  google::protobuf::RepeatedPtrField<std::string> roles, types;
  for (std::size_t i = 0; i < schema.role_count; ++i)
    *roles.Add() = schema.roles[i];
  for (std::size_t i = 0; i < schema.type_count; ++i)
    *types.Add() = schema.types[i];
  return tracer_repl::decode_owned_value(
      schema.cpp_type, schema.root,
      std::string(reinterpret_cast<const char *>(schema.descriptor), schema.descriptor_size),
      payload, roles, types, false, true);
}

std::uint64_t address_of(const void *object) { return reinterpret_cast<std::uintptr_t>(object); }

// A captured smart pointer: its address, the copy of its object and why there is none.
struct Link {
  std::uint64_t address;
  bool has_value, suppressed;
  std::string omitted;
  const EventValue *value;
};
Link link(const EventValue &pointer) {
  const auto &value = pointer.field("value");
  return {pointer.field("address").as<std::uint64_t>(), !value.is_null(),
          pointer.field("suppressed").as<bool>(), pointer.field("omitted").as<std::string>(),
          &value};
}

TreeNode tree(int v, std::string name, std::vector<TreeNode> kids = {}) {
  TreeNode node;
  node.v = v;
  node.name = std::move(name);
  node.kids = std::move(kids);
  return node;
}

// A populated three-level tree: every node and value is read back, at every level.
void populated_tree() {
  const auto root = tree(1, "root", {tree(2, "left", {tree(4, "left.a"), tree(5, "left.b")}),
                                     tree(3, "right", {tree(6, "right.a")})});
  const auto captured = capture(root);
  require(captured.ok && captured.cut.empty(), "a three-level tree was not captured");
  const auto value = decode(generated::TreeNode, captured.payload);
  require(value.cpp_type() == "recursive_fixture::TreeNode", "the tree root lost its type");
  std::function<void(const TreeNode &, const EventValue &)> same;
  std::size_t nodes = 0;
  same = [&](const TreeNode &expected, const EventValue &actual) {
    ++nodes;
    require(actual.field("v").as<int>() == expected.v, "tree node value differs");
    require(actual.field("name").as<std::string>() == expected.name, "tree node name differs");
    const auto &kids = actual.field("kids");
    require(kids.container_role() == "vector", "a nested kids container lost its role");
    require(kids.size() == expected.kids.size(), "tree node has the wrong number of kids");
    for (std::size_t i = 0; i < expected.kids.size(); ++i) {
      require(kids.at(i).cpp_type() == "recursive_fixture::TreeNode", "a tree kid lost its type");
      same(expected.kids[i], kids.at(i));
    }
  };
  same(root, value);
  require(nodes == 6, "not every tree node was visited");
  const auto typed = value.as<TreeNode>();
  require(typed.kids.size() == 2 && typed.kids[0].kids[1].name == "left.b" &&
              typed.kids[1].kids[0].v == 6,
          "the typed tree differs from the captured one");
  // The control: a node without kids.
  const auto leaf = decode(generated::TreeNode, capture(tree(9, "leaf")).payload);
  require(leaf.field("v").as<int>() == 9 && leaf.field("kids").size() == 0,
          "a tree node without kids was not captured");
}

std::unique_ptr<ListNode> chain(int count, std::vector<const ListNode *> *nodes = nullptr) {
  std::unique_ptr<ListNode> head;
  for (int v = count; v >= 1; --v) {
    auto node = std::make_unique<ListNode>();
    node->v = v;
    node->next = std::move(head);
    head = std::move(node);
  }
  if (nodes)
    for (const auto *node = head.get(); node; node = node->next.get())
      nodes->push_back(node);
  return head;
}

// A five-node list through std::unique_ptr, and the null control.
void unique_list() {
  std::vector<const ListNode *> nodes;
  const auto head = chain(5, &nodes);
  const auto captured = capture(*head);
  require(captured.ok && captured.cut.empty(), "a five-node list was not captured");
  const auto value = decode(generated::ListNode, captured.payload);
  const EventValue *node = &value;
  for (int v = 1; v <= 5; ++v) {
    require(node->field("v").as<int>() == v, "list node value differs");
    const auto next = link(node->field("next"));
    require(next.omitted.empty() && !next.suppressed, "an in-budget list node was marked omitted");
    if (v == 5) {
      require(next.address == 0 && !next.has_value, "the last list node does not end the list");
      break;
    }
    require(next.address == address_of(nodes[static_cast<std::size_t>(v)]) && next.has_value,
            "a list node was not captured behind its pointer");
    node = next.value;
  }
  auto typed = value.as<ListNode>();
  int sum = 0, count = 0;
  for (const auto *item = &typed; item; item = item->next.get(), ++count)
    sum += item->v;
  require(count == 5 && sum == 15, "the typed list differs from the captured one");
  ListNode single;
  single.v = 7;
  const auto alone = decode(generated::ListNode, capture(single).payload);
  require(alone.field("v").as<int>() == 7 && !link(alone.field("next")).has_value,
          "a list node without a successor was not captured");
}

// Two records that contain each other, a map of the record itself, a private
// member and a base class that holds the derived record.
void mutual_recursion() {
  Folder root;
  root.id = 1;
  root.entries.resize(2);
  root.entries[0].name = "docs";
  root.entries[0].folder = std::make_unique<Folder>();
  root.entries[0].folder->id = 2;
  root.entries[0].folder->entries.resize(1);
  root.entries[0].folder->entries[0].name = "deep";
  root.entries[0].folder->entries[0].folder = std::make_unique<Folder>();
  root.entries[0].folder->entries[0].folder->id = 3;
  root.entries[1].name = "empty";
  root.named["alias"].id = 4;
  root.named["alias"].named["inner"].id = 5;
  const auto captured = capture(root);
  require(captured.ok && captured.cut.empty(), "mutually recursive records were not captured");
  const auto value = decode(generated::Folder, captured.payload);
  require(value.field("id").as<int>() == 1 && value.field("entries").size() == 2,
          "the outer folder differs");
  const auto &docs = value.field("entries").at(0);
  require(docs.cpp_type() == "recursive_fixture::Entry" &&
              docs.field("name").as<std::string>() == "docs",
          "the first entry differs");
  const auto inner = link(docs.field("folder"));
  require(inner.has_value && inner.value->field("id").as<int>() == 2 &&
              inner.address == address_of(root.entries[0].folder.get()),
          "the folder of an entry was not captured");
  const auto &deep = inner.value->field("entries").at(0);
  require(deep.field("name").as<std::string>() == "deep" &&
              link(deep.field("folder")).value->field("id").as<int>() == 3 &&
              link(deep.field("folder")).value->field("entries").size() == 0,
          "the third level of mutually recursive records differs");
  require(!link(value.field("entries").at(1).field("folder")).has_value,
          "an entry without a folder gained one");
  const auto &named = value.field("named");
  require(named.container_role() == "map" && named.size() == 1 &&
              named.at(0).field("key").as<std::string>() == "alias" &&
              named.at(0).field("value").field("id").as<int>() == 4,
          "a map of the record itself differs");
  const auto &nested = named.at(0).field("value").field("named");
  require(nested.container_role() == "map" && nested.size() == 1 &&
              nested.at(0).field("key").as<std::string>() == "inner" &&
              nested.at(0).field("value").field("id").as<int>() == 5 &&
              nested.at(0).field("value").field("named").size() == 0,
          "a nested map of the record itself differs");

  Hidden hidden(1);
  hidden.kids().emplace_back(2);
  hidden.kids().emplace_back(3);
  hidden.kids()[1].kids().emplace_back(4);
  const auto secret = capture(hidden);
  require(secret.ok, "a record with private recursive members was not captured");
  const auto seen = decode(generated::Hidden, secret.payload);
  require(seen.field("v_").as<int>() == 1 && seen.field("kids_").size() == 2 &&
              seen.field("kids_").at(0).field("v_").as<int>() == 2 &&
              seen.field("kids_").at(1).field("kids_").at(0).field("v_").as<int>() == 4,
          "private recursive members differ");
}

// The record inside containers of containers.
void nested_containers() {
  Grid root;
  root.id = 1;
  root.cells.resize(2);
  root.cells[0].resize(2);
  root.cells[0][0].id = 2;
  root.cells[0][1].id = 3;
  root.cells[0][1].cells.resize(1);
  root.cells[0][1].cells[0].resize(1);
  root.cells[0][1].cells[0][0].id = 4;
  root.index[7].resize(1);
  root.index[7][0].id = 5;
  root.index[7][0].index[8].resize(2);
  root.index[7][0].index[8][1].id = 6;
  const auto captured = capture(root);
  require(captured.ok && captured.cut.empty(), "a record in nested containers was not captured");
  const auto value = decode(generated::Grid, captured.payload);
  const auto &cells = value.field("cells");
  require(cells.container_role() == "vector" && cells.size() == 2 &&
              cells.at(0).container_role() == "vector" && cells.at(0).size() == 2 &&
              cells.at(1).size() == 0 && cells.at(0).at(0).field("id").as<int>() == 2,
          "a container of containers of the record differs");
  const auto &inner = cells.at(0).at(1).field("cells");
  require(inner.container_role() == "vector" && inner.size() == 1 &&
              inner.at(0).container_role() == "vector" &&
              inner.at(0).at(0).field("id").as<int>() == 4 &&
              inner.at(0).at(0).field("cells").size() == 0,
          "the second level of nested containers differs");
  const auto &index = value.field("index");
  require(index.container_role() == "map" && index.size() == 1 &&
              index.at(0).field("key").as<int>() == 7 &&
              index.at(0).field("value").at(0).field("id").as<int>() == 5,
          "a map of containers of the record differs");
  const auto &deeper = index.at(0).field("value").at(0).field("index");
  require(deeper.container_role() == "map" && deeper.at(0).field("key").as<int>() == 8 &&
              deeper.at(0).field("value").size() == 2 &&
              deeper.at(0).field("value").at(1).field("id").as<int>() == 6,
          "the second level of a map of containers differs");
}

std::shared_ptr<SharedNode> shared(int v) {
  auto node = std::make_shared<SharedNode>();
  node->v = v;
  return node;
}

// A cycle is cut where an object is met again while it is being captured; an
// object reached twice without a cycle is captured once and referred to.
void cycles_and_aliases() {
  // Two nodes that own each other, entered through the first.
  auto first = shared(1), second = shared(2);
  first->next = second;
  second->next = first;
  auto captured = capture(*first);
  require(captured.ok && captured.cut.empty(), "a two-node cycle did not end the capture cleanly");
  auto value = decode(generated::SharedNode, captured.payload);
  const auto to_second = link(value.field("next"));
  require(to_second.has_value && to_second.address == address_of(second.get()) &&
              to_second.value->field("v").as<int>() == 2,
          "the second node of a cycle was not captured");
  const auto back = link(to_second.value->field("next"));
  require(!back.has_value && back.suppressed && back.omitted == "cycle" &&
              back.address == address_of(first.get()),
          "the pointer that closes a cycle is not marked as one");
  const auto message = rejects<std::runtime_error>(
      [&] { (void)value.as<SharedNode>(); }, "a typed value was rebuilt from an ownership cycle");
  require(message.find("cycle") != std::string::npos, "the cycle error does not name the cycle");

  // The same cycle below a record that is not part of it.
  Holder holder;
  holder.left = first;
  holder.tail = 9;
  captured = capture(holder);
  require(captured.ok, "a cycle below another record was not captured");
  value = decode(generated::Holder, captured.payload);
  const auto left = link(value.field("left"));
  const auto next = link(left.value->field("next"));
  const auto closing = link(next.value->field("next"));
  require(left.has_value && next.has_value && !closing.has_value && closing.omitted == "cycle" &&
              closing.address == left.address && value.field("tail").as<int>() == 9,
          "a cycle below another record is not cut at the repeated object");
  second->next.reset(); // Break the test-owned cycle.

  // A node that owns itself.
  auto self = shared(5);
  self->next = self;
  value = decode(generated::SharedNode, capture(*self).payload);
  require(link(value.field("next")).omitted == "cycle" &&
              link(value.field("next")).address == address_of(self.get()),
          "a node that owns itself is not cut at its own pointer");
  self->next.reset();

  // Two edges share a child; nothing owns itself.
  auto child = shared(7);
  child->text = "shared";
  Holder dag;
  dag.left = shared(1);
  dag.right = shared(2);
  dag.left->next = child;
  dag.right->next = child;
  dag.right->other = dag.left;
  captured = capture(dag);
  require(captured.ok && captured.cut.empty(), "a graph with a shared child was not captured");
  value = decode(generated::Holder, captured.payload);
  const auto first_edge = link(link(value.field("left")).value->field("next"));
  const auto right = link(value.field("right"));
  const auto second_edge = link(right.value->field("next"));
  const auto to_left = link(right.value->field("other"));
  require(first_edge.has_value && first_edge.omitted.empty() &&
              first_edge.value->field("text").as<std::string>() == "shared",
          "the first edge to a shared child does not carry it");
  require(!second_edge.has_value && second_edge.suppressed && second_edge.omitted == "alias" &&
              second_edge.address == first_edge.address,
          "the second edge to a shared child is not marked as an alias");
  require(!to_left.has_value && to_left.omitted == "alias" &&
              to_left.address == address_of(dag.left.get()),
          "a pointer to an object captured earlier is not marked as an alias");
  // The typed value shares the rebuilt objects again.
  const auto typed = value.as<Holder>();
  require(typed.left && typed.right && typed.left->next && typed.left->next->text == "shared" &&
              typed.right->next.get() == typed.left->next.get() &&
              typed.right->other.get() == typed.left.get() && typed.left->next.use_count() == 2,
          "the typed value does not share the object two pointers own");
}

// A chain longer than the depth budget: the nodes before the cut are kept, the
// pointer at the cut names the object and the limit, and the capture says so.
void depth_cut() {
  std::vector<const ListNode *> nodes;
  const auto head = chain(40, &nodes);
  const auto captured = capture(*head);
  require(captured.ok, "a chain that crosses the depth budget lost the whole capture: " +
                           captured.failure);
  require(captured.cut == "depth", "the capture does not report the depth cut");
  const auto value = decode(generated::ListNode, captured.payload);
  const EventValue *node = &value;
  std::size_t kept = 0;
  for (;; ++kept) {
    require(node->field("v").as<int>() == static_cast<int>(kept + 1),
            "a node before the depth cut differs");
    const auto next = link(node->field("next"));
    if (!next.has_value) {
      require(kept + 1 < nodes.size(), "the whole chain fit the depth budget");
      require(next.omitted == "depth" && next.suppressed &&
                  next.address == address_of(nodes[kept + 1]),
              "the pointer at the depth cut does not name the next node and the limit");
      break;
    }
    require(next.omitted.empty() && next.address == address_of(nodes[kept + 1]),
            "a pointer before the depth cut differs");
    node = next.value;
  }
  require(kept + 1 >= 8, "the depth budget kept fewer nodes than it has room for");
  const auto message = rejects<std::runtime_error>(
      [&] { (void)value.as<ListNode>(); }, "a typed list was rebuilt from a cut chain");
  require(message.find("depth") != std::string::npos,
          "the error of a cut chain does not name the limit");

  // A wider budget keeps more of the same chain, and a chain within it is whole.
  CaptureLimits wide;
  wide.depth = max_capture_depth;
  const auto more = capture(*head, wide);
  require(more.ok && more.cut == "depth", "a wider depth budget did not cut the chain");
  const auto wider = decode(generated::ListNode, more.payload);
  std::size_t wide_kept = 1;
  for (const EventValue *item = &wider; link(item->field("next")).has_value;
       item = link(item->field("next")).value)
    ++wide_kept;
  require(wide_kept > kept + 1, "a wider depth budget did not keep more nodes");
  const auto whole = capture(*chain(static_cast<int>(wide_kept)), wide);
  require(whole.ok && whole.cut.empty(), "a chain within the depth budget was cut");

  // A record nested by value has no pointer to cut at: the budget fails the capture.
  auto deep = tree(0, "leaf");
  for (int level = 1; level <= 40; ++level)
    deep = tree(level, "level", {std::move(deep)});
  const auto failed = capture(deep);
  require(!failed.ok && failed.cut.empty() && failed.failure == "snapshot exceeds depth budget",
          "a by-value tree deeper than the depth budget was reported as captured");
}

// The byte, element and string budgets cut at the pointer whose object did not
// fit; the rest of the capture is kept.
void other_budget_cuts() {
  // Bytes: each node carries 200 bytes of text.
  std::shared_ptr<SharedNode> head;
  std::vector<const SharedNode *> nodes;
  for (int v = 8; v >= 1; --v) {
    auto node = shared(v);
    node->text.assign(200, static_cast<char>('a' + v));
    node->next = std::move(head);
    head = std::move(node);
  }
  for (const auto *node = head.get(); node; node = node->next.get())
    nodes.push_back(node);
  CaptureLimits small;
  small.bytes = 1024;
  auto captured = capture(*head, small);
  require(captured.ok && captured.cut == "bytes", "a chain that crosses the byte budget was lost");
  auto value = decode(generated::SharedNode, captured.payload);
  const EventValue *node = &value;
  std::size_t kept = 0;
  for (;; ++kept) {
    require(node->field("text").as<std::string>() == nodes[kept]->text,
            "a node before the byte cut differs");
    const auto next = link(node->field("next"));
    if (!next.has_value) {
      require(next.omitted == "bytes" && next.address == address_of(nodes[kept + 1]),
              "the pointer at the byte cut does not name the next node and the limit");
      break;
    }
    node = next.value;
  }
  require(kept >= 1 && kept + 1 < nodes.size(), "the byte budget cut at the wrong node");

  // Elements: a folder with more entries than one container may hold.
  Holder holder;
  holder.left = shared(1);
  holder.folder = std::make_unique<Folder>();
  holder.folder->entries.resize(20);
  holder.tail = 3;
  CaptureLimits few;
  few.item_cap = 10;
  captured = capture(holder, few);
  require(captured.ok && captured.cut == "elements",
          "an object that crosses the element budget was not left out alone");
  value = decode(generated::Holder, captured.payload);
  const auto folder = link(value.field("folder"));
  require(!folder.has_value && folder.omitted == "elements" &&
              folder.address == address_of(holder.folder.get()) &&
              link(value.field("left")).has_value && value.field("tail").as<int>() == 3,
          "the element cut is not at the pointer of the folder");
  const auto message = rejects<std::runtime_error>(
      [&] { (void)value.as<Holder>(); }, "a typed value was rebuilt without its folder");
  require(message.find("elements") != std::string::npos,
          "the error of an element cut does not name the limit");
  // The same folder as the root has no pointer to cut at.
  require(!capture(*holder.folder, few).ok, "a root over the element budget was captured");

  // String: the object that holds the long string is left out, and an object
  // it had reached first is captured again where the next pointer finds it.
  auto reached = shared(30);
  reached->text = "reached twice";
  Holder twice;
  twice.left = shared(10);
  twice.left->other = reached;
  twice.left->text.assign(300, 'x');
  twice.right = reached;
  CaptureLimits text;
  text.string_bytes = 100;
  captured = capture(twice, text);
  require(captured.ok && captured.cut == "string",
          "an object with a string over the budget was not left out alone");
  value = decode(generated::Holder, captured.payload);
  const auto left = link(value.field("left")), right = link(value.field("right"));
  require(!left.has_value && left.omitted == "string" &&
              left.address == address_of(twice.left.get()),
          "the string cut is not at the pointer of the object");
  require(right.has_value && right.omitted.empty() &&
              right.value->field("text").as<std::string>() == "reached twice",
          "an object first reached inside a part that was left out is not captured afterwards");
}

// Many owned objects: the table of captured objects outgrows its inline storage.
void many_pointees() {
  Folder root;
  root.entries.resize(300);
  for (std::size_t i = 0; i < root.entries.size(); ++i) {
    root.entries[i].name = "entry " + std::to_string(i);
    root.entries[i].folder = std::make_unique<Folder>();
    root.entries[i].folder->id = static_cast<int>(i);
  }
  const auto captured = capture(root);
  require(captured.ok && captured.cut.empty(), "a record with many owned objects was not captured");
  const auto value = decode(generated::Folder, captured.payload);
  require(value.field("entries").size() == 300, "not every entry was captured");
  for (std::size_t i = 0; i < 300; ++i) {
    const auto folder = link(value.field("entries").at(i).field("folder"));
    require(folder.has_value && folder.omitted.empty() &&
                folder.value->field("id").as<int>() == static_cast<int>(i),
            "an owned object among many differs");
  }

  EncodeBudget::PointerGraph graph(1000);
  using Admission = EncodeBudget::PointerGraph::Admission;
  for (std::uintptr_t address = 16; address <= 16 * 1000; address += 16)
    require(graph.note(address) == Admission::added, "a new address was not admitted");
  require(graph.size() == 1000 && graph.note(16 * 1001) == Admission::full,
          "the table admitted more addresses than its limit");
  require(graph.note(16 * 500) == Admission::seen && !graph.admit(16 * 500),
          "an admitted address was not recognised");
  graph.rewind(400);
  require(graph.size() == 400 && graph.note(16 * 400) == Admission::seen &&
              graph.note(16 * 401) == Admission::added && graph.note(16 * 1000) == Admission::added,
          "rewinding the table did not forget exactly the newest addresses");
  for (std::uintptr_t address = 16; address <= 16 * 400; address += 16)
    require(graph.note(address) == Admission::seen, "rewinding the table lost an older address");
}

} // namespace

int main() {
  try {
    populated_tree();
    unique_list();
    mutual_recursion();
    nested_containers();
    cycles_and_aliases();
    depth_cut();
    other_budget_cuts();
    many_pointees();
    std::cout << "recursive snapshots: ok\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "recursive snapshots: " << error.what() << '\n';
    return 1;
  }
}
