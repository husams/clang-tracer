#pragma once

#include "tracer/module_index.hpp"

namespace tracer::module_index {
using namespace ::tracer_module;
std::string format_json(const ModuleType &);
std::string format_json(const ModuleSymbol &);
std::string format_json(const ModulePage<ModuleType> &);
std::string format_json(const ModulePage<ModuleSymbol> &);
} // namespace tracer::module_index
