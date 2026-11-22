#pragma once
namespace nested_receivers {
// The real header declares the nested class but does not define it.
struct Partial { int value = 8; struct Nested; };
}
