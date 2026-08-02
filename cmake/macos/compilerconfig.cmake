# CMake macOS compiler configuration module

include_guard(GLOBAL)

if(NOT XCODE)
	message(FATAL_ERROR "Building StreamAssistant Camera on macOS requires the Xcode generator.")
endif()

include(compiler_common)

set(CMAKE_COLOR_DIAGNOSTICS TRUE)
