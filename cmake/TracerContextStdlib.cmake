# The supported Linux profile packages the exact archive selected by the native
# compiler. It is linked without libc/libstdc++/libgcc DT_NEEDED dependencies;
# every external operation goes through the installed native kernel table.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libstdc++.a
    OUTPUT_VARIABLE _context_stdlib_archive OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  if(NOT IS_ABSOLUTE "${_context_stdlib_archive}" OR NOT EXISTS "${_context_stdlib_archive}")
    message(FATAL_ERROR "The shared-context profile needs the matching static libstdc++ archive")
  endif()
  file(SHA256 ${_context_stdlib_archive} _context_stdlib_archive_sha256)
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libgcc_eh.a
    OUTPUT_VARIABLE _context_unwind_archive OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  if(NOT IS_ABSOLUTE "${_context_unwind_archive}" OR NOT EXISTS "${_context_unwind_archive}")
    message(FATAL_ERROR "The shared-context profile needs its matching static unwind archive")
  endif()
  find_program(_context_objcopy NAMES llvm-objcopy objcopy REQUIRED)
  execute_process(COMMAND ${CMAKE_NM} -g --defined-only ${_context_stdlib_archive}
    OUTPUT_VARIABLE _context_archive_symbols COMMAND_ERROR_IS_FATAL ANY
    ERROR_QUIET)
  set(_context_personality_dir ${CMAKE_CURRENT_BINARY_DIR}/context-personality)
  file(MAKE_DIRECTORY ${_context_personality_dir})
  set(_context_personality ${_context_personality_dir}/private-personality.o)
  add_custom_command(OUTPUT ${_context_personality}
    COMMAND ${CMAKE_AR} x ${_context_stdlib_archive} eh_personality.o
    COMMAND ${_context_objcopy}
      --redefine-sym=__gxx_personality_v0=__clang_tracer_support_personality
      eh_personality.o ${_context_personality}
    WORKING_DIRECTORY ${_context_personality_dir}
    DEPENDS ${_context_stdlib_archive} VERBATIM)
  set(_context_private_unwind ${CMAKE_CURRENT_BINARY_DIR}/context-unwind-private.a)
  add_custom_command(OUTPUT ${_context_private_unwind}
    COMMAND ${_context_objcopy} --prefix-symbols=__clang_tracer_unwind_
      ${_context_unwind_archive} ${_context_private_unwind}.stage
    COMMAND ${_context_objcopy}
      --redefine-sym=__clang_tracer_unwind__GLOBAL_OFFSET_TABLE_=_GLOBAL_OFFSET_TABLE_
      ${_context_private_unwind}.stage ${_context_private_unwind}
    DEPENDS ${_context_unwind_archive} VERBATIM)
  add_custom_target(tracer_context_unwind_archive DEPENDS ${_context_private_unwind} ${_context_personality})
  add_library(tracer_context_stdlib SHARED src/context_stdlib_runtime.cpp)
  add_dependencies(tracer_context_stdlib tracer_context_unwind_archive)
  set_target_properties(tracer_context_stdlib PROPERTIES OUTPUT_NAME clang_tracer_context_stdlib)
  target_compile_features(tracer_context_stdlib PRIVATE cxx_std_20)
  target_compile_options(tracer_context_stdlib PRIVATE -O2 -fPIC -fno-builtin
    -fvisibility-inlines-hidden -ffunction-sections -fdata-sections
    -fasynchronous-unwind-tables -fexceptions)
  target_compile_definitions(tracer_context_stdlib PRIVATE
    TRACER_CONTEXT_ARCHIVE_SHA256="${_context_stdlib_archive_sha256}")
  target_link_libraries(tracer_context_stdlib PRIVATE ${_context_personality} ${_context_stdlib_archive} ${_context_private_unwind})
  target_link_options(tracer_context_stdlib PRIVATE -nostdlib
    "LINKER:-Bsymbolic,-z,now,--no-undefined,--gc-sections,--eh-frame-hdr,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/context_stdlib.map")
  set(_context_stdlib_roots
    _ZNSaIcEC1ERKS_ _ZNSaIcEC1Ev _ZNSaIcEC2ERKS_ _ZNSaIcEC2Ev _ZNSaIcED1Ev _ZNSaIcED2Ev
    _ZNSaIwEC1ERKS_ _ZNSaIwEC1Ev _ZNSaIwEC2ERKS_ _ZNSaIwEC2Ev _ZNSaIwED1Ev _ZNSaIwED2Ev
    _ZTIb _ZTIc _ZTIa _ZTIh _ZTIs _ZTIt _ZTIi _ZTIj _ZTIl _ZTIm _ZTIx _ZTIy
    _ZTIf _ZTId _ZTIe _ZTIw _ZTIDi _ZTIDs _ZTIDu
    _ZSt17__throw_bad_allocv _ZSt20__throw_length_errorPKc
    _ZSt28__throw_bad_array_new_lengthv _ZSt24__throw_out_of_range_fmtPKcz
    _ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base
    _ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base
    _ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base
    _ZSt18_Rb_tree_decrementPKSt18_Rb_tree_node_base
    _ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_
    _ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_
    _ZSt11_Hash_bytesPKvmm _ZSt15_Fnv_hash_bytesPKvmm
    _ZNKSt8__detail20_Prime_rehash_policy14_M_need_rehashEmmm
    _ZNSt8__detail15_List_node_base7_M_hookEPS0_
    _ZNSt8__detail15_List_node_base9_M_unhookEv
    _ZNSt8__detail15_List_node_base11_M_transferEPS0_S1_
    _ZNSt8__detail15_List_node_base10_M_reverseEv
    _ZNSt8__detail15_List_node_base4swapERS0_S1_)
  foreach(_root IN LISTS _context_stdlib_roots)
    # GCC 8's headers never reference newer helper entry points such as
    # __throw_bad_array_new_length. Do not manufacture an undefined export.
    if(_context_archive_symbols MATCHES "[ \t][TWVDRB] ${_root}(\n|$)")
      target_link_options(tracer_context_stdlib PRIVATE "LINKER:-u,${_root}")
    endif()
  endforeach()
  set_property(TARGET tracer_context_stdlib APPEND PROPERTY LINK_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/context_stdlib.map)
  if(BUILD_TESTING)
    add_executable(context-stdlib-init-tests tests/context_stdlib_init_tests.cpp)
    target_link_libraries(context-stdlib-init-tests PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    add_dependencies(context-stdlib-init-tests tracer_context_stdlib)
    add_test(NAME shared_context_stdlib_init
      COMMAND context-stdlib-init-tests $<TARGET_FILE:tracer_context_stdlib>)
    set_tests_properties(shared_context_stdlib_init PROPERTIES TIMEOUT 15)
  endif()
endif()
