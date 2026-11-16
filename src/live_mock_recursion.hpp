#pragma once

// Stored in the native agent so mock images never define their own JIT TLS.
extern "C" void* clangtracer_live_mock_top_v1(void* value, bool set) noexcept;
