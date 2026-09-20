add_executable(PsramBufferTest
  PsramBufferTest.cpp
  ${REPO_ROOT}/lib/hal/HalMemory.cpp
)

target_include_directories(PsramBufferTest PRIVATE
  stubs
  ${REPO_ROOT}/lib/hal
  ${REPO_ROOT}/src
)

target_link_libraries(PsramBufferTest PRIVATE crosspoint_test_common)
if(MSVC)
  target_compile_options(PsramBufferTest PRIVATE /UNDEBUG)
else()
  target_compile_options(PsramBufferTest PRIVATE -UNDEBUG)
endif()

add_test(NAME PsramBufferTest COMMAND PsramBufferTest)
