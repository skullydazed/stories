# Toolchain files and explicit compiler choices take precedence over our defaults.
if(NOT CMAKE_C_COMPILER AND "$ENV{CC}" STREQUAL "" AND NOT CMAKE_TOOLCHAIN_FILE AND "$ENV{CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
	find_program(CMAKE_C_COMPILER NAMES clang gcc cc REQUIRED)
endif()
