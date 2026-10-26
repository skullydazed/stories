# Git checkouts follow the submodule revision recorded by Stories. Complete source archives already contain the files and do not require Git or network access.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
	find_package(Git REQUIRED)
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" submodule update --init -- inja
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		RESULT_VARIABLE stories_inja_result
	)
	if(NOT stories_inja_result STREQUAL "0")
		message(FATAL_ERROR "Cannot initialize the Inja submodule (git exited with ${stories_inja_result})")
	endif()
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/inja"
		OUTPUT_VARIABLE stories_inja_revision
		OUTPUT_STRIP_TRAILING_WHITESPACE
		COMMAND_ERROR_IS_FATAL ANY
	)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/INJA_REVISION")
	file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/INJA_REVISION" stories_inja_revision LIMIT_COUNT 1)
else()
	set(stories_inja_revision "not recorded in this source tree")
endif()

foreach(stories_inja_file include/inja/inja.hpp third_party/include/nlohmann/json.hpp LICENSE third_party/include/nlohmann/LICENSE.MIT)
	if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/inja/${stories_inja_file}")
		message(FATAL_ERROR "Missing inja/${stories_inja_file}. Use a Git checkout or a complete Stories source archive.")
	endif()
endforeach()

# Header-only integration avoids running upstream's source-generation and installation steps in our build.
add_library(stories-inja INTERFACE)
target_include_directories(stories-inja SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/inja/include" "${CMAKE_CURRENT_SOURCE_DIR}/inja/third_party/include")
target_compile_features(stories-inja INTERFACE cxx_std_17)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/inja-upstream.txt" "Source: https://github.com/pantor/inja\nCommit: ${stories_inja_revision}\nIncludes upstream nlohmann/json headers. Both libraries are unmodified and MIT licensed.\n")
