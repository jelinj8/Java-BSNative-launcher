cmake_minimum_required(VERSION 3.16)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Oldest macOS supported")
project(java_native_launcher C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -O2)

if(NOT DEFINED ENV{JAVA_HOME})
	message(FATAL_ERROR "JAVA_HOME must point to a JDK, for its JNI headers")
endif()
file(TO_CMAKE_PATH "$ENV{JAVA_HOME}" JDK_HOME)

set(COMMON src/common/launcher.c)

if(WIN32)
	enable_language(RC)
	add_executable(launcher WIN32 src/windows/main.c src/windows/platform.c src/windows/launcher.rc src/common/launch.c ${COMMON})
	target_include_directories(launcher PRIVATE "${JDK_HOME}/include" "${JDK_HOME}/include/win32")
	target_compile_definitions(launcher PRIVATE UNICODE _UNICODE)
	# no dependency on MinGW's runtime DLLs, so one copied file is all an app needs
	target_link_options(launcher PRIVATE -municode -static)

	add_executable(unit_tests test/unit_tests.c src/windows/platform.c ${COMMON})
	target_compile_definitions(unit_tests PRIVATE UNICODE _UNICODE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_executable(launcher src/posix/main.c src/posix/platform.c src/common/launch.c ${COMMON})
	target_include_directories(launcher PRIVATE "${JDK_HOME}/include" "${JDK_HOME}/include/linux")
	target_link_libraries(launcher PRIVATE dl pthread)

	add_executable(unit_tests test/unit_tests.c src/posix/platform.c ${COMMON})
	target_link_libraries(unit_tests PRIVATE dl pthread)
elseif(APPLE)
	add_executable(launcher src/posix/main.c src/posix/platform.c src/common/launch.c ${COMMON})
	target_include_directories(launcher PRIVATE "${JDK_HOME}/include" "${JDK_HOME}/include/darwin")
	target_link_libraries(launcher PRIVATE "-framework CoreFoundation")

	add_executable(unit_tests test/unit_tests.c src/posix/platform.c ${COMMON})
	target_link_libraries(unit_tests PRIVATE "-framework CoreFoundation")
else()
	message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)
