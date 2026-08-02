# CMake macOS build dependencies module

include_guard(GLOBAL)

include(buildspec_common)

function(_check_dependencies_macos)
	set(arch universal)
	set(platform macos)
	set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")
	set(prebuilt_filename "macos-deps-VERSION-ARCH_REVISION.tar.xz")
	set(prebuilt_destination "obs-deps-VERSION-ARCH")
	set(obs-studio_filename "VERSION.tar.gz")
	set(obs-studio_destination "obs-studio-VERSION")
	set(dependencies_list prebuilt obs-studio)

	_check_dependencies()

	list(APPEND CMAKE_FRAMEWORK_PATH "${dependencies_dir}/Frameworks")
	set(CMAKE_FRAMEWORK_PATH ${CMAKE_FRAMEWORK_PATH} PARENT_SCOPE)
endfunction()

_check_dependencies_macos()
