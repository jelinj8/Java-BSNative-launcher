cmake_minimum_required(VERSION 3.16)
project(java_native_launcher C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -O2)

if(NOT DEFINED ENV{JAVA_HOME})
	message(FATAL_ERROR "JAVA_HOME must point to a JDK, for its JNI headers")
endif()
file(TO_CMAKE_PATH "$ENV{JAVA_HOME}" JDK_HOME)

set(COMMON src/common/launcher.c)

if(WIN32)
	add_executable(launcher WIN32 src/windows/main.c src/windows/platform.c ${COMMON})
	target_include_directories(launcher PRIVATE "${JDK_HOME}/include" "${JDK_HOME}/include/win32")
	target_compile_definitions(launcher PRIVATE UNICODE _UNICODE)
	# no dependency on MinGW's runtime DLLs, so one copied file is all an app needs
	target_link_options(launcher PRIVATE -municode -static)

	add_executable(unit_tests test/unit_tests.c src/windows/platform.c ${COMMON})
	target_compile_definitions(unit_tests PRIVATE UNICODE _UNICODE)
else()
	message(FATAL_ERROR "Only Windows is implemented so far")
endif()

enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)
