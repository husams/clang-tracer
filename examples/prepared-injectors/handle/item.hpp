#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ngd {
class HeapObject {
public:
  HeapObject() = delete;
  explicit HeapObject(int base);
  virtual ~HeapObject();
};
}

// HANDLE_REFERENCE, HANDLE_UNIQUE, HANDLE_SHARED and HANDLE_BASE_MEMBER change
// how the private implementation is held. The default is a raw pointer.
// HANDLE_DEEP adds members that need the deeper captures: a C string, a pointer
// to a scalar, a pointer inside a nested record, a ring of nodes, and pointers
// that must stay address-only. HANDLE_BUDGET adds a function whose arguments
// fill the snapshot storage.
namespace HFW {
class HfwItemImpl;
class HfwManager;

#ifdef HANDLE_DEEP
struct HfwNode {
  int id;
  HfwNode *next;
};
struct HfwNested {
  int tag;
  HfwItemImpl *impl;
};
#endif
#ifdef HANDLE_BUDGET
struct HfwBig {
  std::string text;
  HfwItemImpl *impl = nullptr;
};
#endif
#ifdef HANDLE_LIMITS
// Members that the capture limits act on.
struct HfwLink {
  int id;
  HfwLink *next;
};
struct HfwVectorImpl {
  std::vector<int> values;
};
// More elements than the default --vector-elements, and more bytes than the default slice.
struct HfwWideImpl {
  std::vector<int> values;
};
struct HfwStringsImpl {
  std::string a, b, c, d;
};
#endif
#ifdef HANDLE_STALE
// Checked-safe members whose storage the target has left stale.
struct HfwStaleImpl {
  std::vector<int> values;
  std::unique_ptr<HfwItemImpl> owned;
  std::shared_ptr<HfwItemImpl> shared;
  int tag;
};
// Encoding a virtual base loads its offset through the object's vptr.
struct HfwVBase {
  int top = 0;
};
struct HfwVirtualImpl : virtual HfwVBase {
  int value = 0;
};
struct HfwBigVector {
  std::vector<int> values;
  int tag;
};
#endif
#ifdef HANDLE_MAP
// A node-based container cannot be read safely from a pointer of unknown quality.
struct HfwMapImpl {
  std::map<int, int> entries;
  int tag = 0;
};
#endif
#ifdef HANDLE_SCALARS
enum HfwMode { low = 1, high = 2 };
#endif
#ifdef HANDLE_WIDGET
// The implementation is private and defined in a source file.
class Widget {
public:
  Widget();
  ~Widget();
  int id = 0;

private:
  class Impl;
  Impl *impl_;
  std::unique_ptr<Impl> owned_;
};
// Private nested implementation classes with virtual functions: a pointer to
// the base may point at a private nested derived class.
class PolyWidget {
public:
  PolyWidget();
  ~PolyWidget();
  int id = 0;

private:
  class Impl;
  class Special;
  Impl *impl_;
  Impl *plain_;
};
// The same with a reference to the private implementation.
class Gadget {
public:
  Gadget();
  ~Gadget();
  int id = 0;

private:
  class Impl;
  Impl &impl_;
};
#endif

class HfwItemBase : public ngd::HeapObject {
public:
  using ngd::HeapObject::HeapObject;

#ifdef HANDLE_BASE_MEMBER
protected:
  friend class HfwManager;
  HfwItemImpl *_impl = nullptr;
#endif
};

class HfwItem : public HfwItemBase {
public:
  using HfwItemBase::HfwItemBase;
#ifdef HANDLE_REFERENCE
  HfwItem(int base, HfwItemImpl &implementation)
      : HfwItemBase(base), _impl(implementation) {}
#endif

private:
  friend class HfwManager;
#ifdef HANDLE_DEEP
  const char *_note = nullptr;
  int *_counter = nullptr;
  const std::string *_text = nullptr;
  int *_dangling = nullptr;
  HfwNested _nested{};
  HfwNode *_head = nullptr;
  HfwNode *_loop = nullptr; // Points at a node whose next is the node itself.
#endif
#ifdef HANDLE_SHARED
  std::weak_ptr<HfwItemImpl> _weak;
#endif
#ifdef HANDLE_LIMITS
  HfwLink *_chain = nullptr;
  HfwVectorImpl *_vec = nullptr;
  HfwWideImpl *_wide = nullptr;
  HfwStringsImpl *_strings = nullptr;
#endif
#ifdef HANDLE_STALE
  HfwStaleImpl *_stale = nullptr;
  HfwVirtualImpl *_vbase = nullptr;
  HfwBigVector *_big = nullptr;
#endif
#ifdef HANDLE_MAP
  HfwMapImpl *_mapped = nullptr;
#endif
#ifdef HANDLE_SCALARS
  bool *_flag = nullptr;
  signed char *_signed = nullptr;
  unsigned char *_byte = nullptr;
  HfwMode *_mode = nullptr;
  const char *_long = nullptr;
  const char *_first = nullptr;
  const char *_second = nullptr;
  const char *_third = nullptr;
#endif
#if defined(HANDLE_REFERENCE)
  HfwItemImpl &_impl;
#elif defined(HANDLE_UNIQUE)
  std::unique_ptr<HfwItemImpl> _impl;
#elif defined(HANDLE_SHARED)
  std::shared_ptr<HfwItemImpl> _impl;
#elif !defined(HANDLE_BASE_MEMBER)
  HfwItemImpl *_impl = nullptr;
#endif
};

class HfwManager {
public:
  HfwItem &getItem(int id);
#ifndef HANDLE_UNIQUE
  HfwItem makeItem(int id);
#endif
  int inspectItem(const HfwItem &item);
#ifdef HANDLE_STALE
  int load(HfwItem &out);
  int loadStale();
#endif
#ifdef HANDLE_RECURSE
  int recurse(const HfwItem &item, int depth);
#endif
#ifdef HANDLE_WIDGET
  int widgetId(const Widget &widget);
  int gadgetId(const Gadget &gadget);
  int polyId(const PolyWidget &widget);
#endif
#ifdef HANDLE_BUDGET
  int budget(const HfwBig &a, const HfwBig &b, const HfwBig &c, const HfwBig &d);
  HfwBig makeBig(const HfwBig &a, const HfwBig &b, const HfwBig &c, const HfwBig &d);
#endif

#ifdef HANDLE_DEEP
private:
  HfwItemImpl *tracked_ = nullptr;
#endif
};
}
