# GCC 8 x86 ELF patch-site tables contain relocatable addresses but omit SHF_WRITE.
# Keep the entry NOPs and metadata; make only the metadata writable at load time.
function(tracer_gcc8_patchable_fixture target)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
      AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"
      AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
      AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9")
    target_sources(${target} PRIVATE
      "${PROJECT_SOURCE_DIR}/tests/fixtures/gcc8_patchable_entries.c")
    target_link_options(${target} PRIVATE "LINKER:-z,text")
  endif()
endfunction()
