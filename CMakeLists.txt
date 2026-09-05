cmake_minimum_required(VERSION 3.28...4.3)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version} LANGUAGES C)

if(APPLE)
	enable_language(OBJC)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	enable_language(CXX)
elseif(NOT WIN32)
	message(FATAL_ERROR "StreamAssistant Camera supports Windows, macOS, and Linux.")
endif()

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE c_std_17)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/camera-config.c
    src/camera-config.h
    src/platform-services.h
    src/plugin-main.c
    src/signaling-metadata.c
    src/signaling-metadata.h
)

if(WIN32)
	target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/platform-services-windows.c)
	target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE bcrypt shell32 user32 winhttp)
elseif(APPLE)
	find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
	find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
	find_library(SECURITY_FRAMEWORK Security REQUIRED)
	target_sources(
		${CMAKE_PROJECT_NAME}
		PRIVATE src/platform-services-macos.m src/signaling-metadata-macos.m
	)
	target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>")
	target_link_libraries(
		${CMAKE_PROJECT_NAME}
		PRIVATE ${APPKIT_FRAMEWORK} ${FOUNDATION_FRAMEWORK} ${SECURITY_FRAMEWORK}
	)
elseif(OS_LINUX)
	find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets WebSockets)
	target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/platform-services-linux.cpp src/signaling-metadata-linux.cpp)
	target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::WebSockets)
	target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -Wl,--no-undefined)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

include(CTest)
if(BUILD_TESTING)
	add_executable(streamassistant-camera-config-test tests/camera-config-test.c src/camera-config.c)
	target_compile_features(streamassistant-camera-config-test PRIVATE c_std_17)
	target_include_directories(streamassistant-camera-config-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
	if(WIN32)
		target_sources(streamassistant-camera-config-test PRIVATE src/platform-services-windows.c)
		target_link_libraries(streamassistant-camera-config-test PRIVATE bcrypt shell32 user32)
	elseif(APPLE)
		target_sources(streamassistant-camera-config-test PRIVATE src/platform-services-macos.m)
		target_compile_options(streamassistant-camera-config-test PRIVATE "$<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>")
		target_link_libraries(streamassistant-camera-config-test PRIVATE ${APPKIT_FRAMEWORK} ${SECURITY_FRAMEWORK})
	elseif(OS_LINUX)
		target_sources(streamassistant-camera-config-test PRIVATE src/platform-services-linux.cpp)
		target_link_libraries(streamassistant-camera-config-test PRIVATE Qt6::Widgets)
	endif()
	add_test(NAME camera-config COMMAND streamassistant-camera-config-test)

	add_executable(streamassistant-camera-signaling-metadata-test tests/signaling-metadata-test.c src/signaling-metadata.c)
	target_compile_features(streamassistant-camera-signaling-metadata-test PRIVATE c_std_17)
	target_include_directories(streamassistant-camera-signaling-metadata-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
	target_compile_definitions(streamassistant-camera-signaling-metadata-test PRIVATE STREAMASSISTANT_METADATA_PARSE_ONLY)
	add_test(NAME signaling-metadata COMMAND streamassistant-camera-signaling-metadata-test)

	if(OS_LINUX)
		add_executable(streamassistant-camera-linux-test tests/linux-platform-test.cpp
			src/platform-services-linux.cpp src/signaling-metadata-linux.cpp src/signaling-metadata.c src/camera-config.c)
		target_include_directories(streamassistant-camera-linux-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
		target_compile_definitions(streamassistant-camera-linux-test PRIVATE STREAMASSISTANT_METADATA_TEST_ENDPOINT)
		target_link_libraries(streamassistant-camera-linux-test PRIVATE Qt6::Widgets Qt6::WebSockets)
		add_test(NAME linux-platform COMMAND streamassistant-camera-linux-test)
		set_tests_properties(linux-platform PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)
	endif()
endif()
