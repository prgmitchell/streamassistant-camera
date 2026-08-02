# CMake macOS helper functions module

include_guard(GLOBAL)

include(helpers_common)

function(set_target_properties_plugin target)
	set(options "")
	set(oneValueArgs "")
	set(multiValueArgs PROPERTIES)
	cmake_parse_arguments(PARSE_ARGV 0 _STPO "${options}" "${oneValueArgs}" "${multiValueArgs}")

	while(_STPO_PROPERTIES)
		list(POP_FRONT _STPO_PROPERTIES key value)
		set_property(TARGET ${target} PROPERTY ${key} "${value}")
	endwhile()

	set_target_properties(
		${target}
		PROPERTIES
			BUNDLE TRUE
			BUNDLE_EXTENSION plugin
			XCODE_ATTRIBUTE_PRODUCT_NAME ${target}
			XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${MACOS_BUNDLEID}
			XCODE_ATTRIBUTE_MARKETING_VERSION ${PLUGIN_VERSION}
			XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION ${PLUGIN_BUILD_NUMBER}
			XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE YES
			XCODE_ATTRIBUTE_INFOPLIST_FILE ""
			XCODE_ATTRIBUTE_INFOPLIST_KEY_CFBundleDisplayName ${target}
			XCODE_ATTRIBUTE_INSTALL_PATH "$(USER_LIBRARY_DIR)/Application Support/obs-studio/plugins"
			XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
			XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES
			XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
			XCODE_ATTRIBUTE_CODE_SIGN_STYLE Manual
	)

	if(TARGET plugin-support)
		target_link_libraries(${target} PRIVATE plugin-support)
	endif()

	target_install_resources(${target})
	install(TARGETS ${target} LIBRARY DESTINATION .)

	configure_file(cmake/macos/resources/distribution.in "${CMAKE_CURRENT_BINARY_DIR}/distribution" @ONLY)
	configure_file(
		cmake/macos/resources/create-package.cmake.in
		"${CMAKE_CURRENT_BINARY_DIR}/create-package.cmake"
		@ONLY
	)
	install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/create-package.cmake")
endfunction()

function(target_install_resources target)
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
		file(GLOB_RECURSE data_files "${CMAKE_CURRENT_SOURCE_DIR}/data/*")
		foreach(data_file IN LISTS data_files)
			cmake_path(
				RELATIVE_PATH
				data_file
				BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
				OUTPUT_VARIABLE relative_path
			)
			cmake_path(GET relative_path PARENT_PATH relative_path)
			target_sources(${target} PRIVATE "${data_file}")
			set_property(SOURCE "${data_file}" PROPERTY MACOSX_PACKAGE_LOCATION "Resources/${relative_path}")
		endforeach()
	endif()
endfunction()
