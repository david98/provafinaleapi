# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.14

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = "/Users/dvolta/Library/Application Support/JetBrains/Toolbox/apps/CLion/ch-0/192.5728.100/CLion.app/Contents/bin/cmake/mac/bin/cmake"

# The command to remove a file.
RM = "/Users/dvolta/Library/Application Support/JetBrains/Toolbox/apps/CLion/ch-0/192.5728.100/CLion.app/Contents/bin/cmake/mac/bin/cmake" -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/dvolta/CLionProjects/provafinaleapi

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/dvolta/CLionProjects/provafinaleapi/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/provafinaleapi.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/provafinaleapi.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/provafinaleapi.dir/flags.make

CMakeFiles/provafinaleapi.dir/main.c.o: CMakeFiles/provafinaleapi.dir/flags.make
CMakeFiles/provafinaleapi.dir/main.c.o: ../main.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/dvolta/CLionProjects/provafinaleapi/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/provafinaleapi.dir/main.c.o"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/provafinaleapi.dir/main.c.o   -c /Users/dvolta/CLionProjects/provafinaleapi/main.c

CMakeFiles/provafinaleapi.dir/main.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/provafinaleapi.dir/main.c.i"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /Users/dvolta/CLionProjects/provafinaleapi/main.c > CMakeFiles/provafinaleapi.dir/main.c.i

CMakeFiles/provafinaleapi.dir/main.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/provafinaleapi.dir/main.c.s"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /Users/dvolta/CLionProjects/provafinaleapi/main.c -o CMakeFiles/provafinaleapi.dir/main.c.s

CMakeFiles/provafinaleapi.dir/xxhash.c.o: CMakeFiles/provafinaleapi.dir/flags.make
CMakeFiles/provafinaleapi.dir/xxhash.c.o: ../xxhash.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/dvolta/CLionProjects/provafinaleapi/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object CMakeFiles/provafinaleapi.dir/xxhash.c.o"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/provafinaleapi.dir/xxhash.c.o   -c /Users/dvolta/CLionProjects/provafinaleapi/xxhash.c

CMakeFiles/provafinaleapi.dir/xxhash.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/provafinaleapi.dir/xxhash.c.i"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /Users/dvolta/CLionProjects/provafinaleapi/xxhash.c > CMakeFiles/provafinaleapi.dir/xxhash.c.i

CMakeFiles/provafinaleapi.dir/xxhash.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/provafinaleapi.dir/xxhash.c.s"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /Users/dvolta/CLionProjects/provafinaleapi/xxhash.c -o CMakeFiles/provafinaleapi.dir/xxhash.c.s

# Object files for target provafinaleapi
provafinaleapi_OBJECTS = \
"CMakeFiles/provafinaleapi.dir/main.c.o" \
"CMakeFiles/provafinaleapi.dir/xxhash.c.o"

# External object files for target provafinaleapi
provafinaleapi_EXTERNAL_OBJECTS =

provafinaleapi: CMakeFiles/provafinaleapi.dir/main.c.o
provafinaleapi: CMakeFiles/provafinaleapi.dir/xxhash.c.o
provafinaleapi: CMakeFiles/provafinaleapi.dir/build.make
provafinaleapi: CMakeFiles/provafinaleapi.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/dvolta/CLionProjects/provafinaleapi/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking C executable provafinaleapi"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/provafinaleapi.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/provafinaleapi.dir/build: provafinaleapi

.PHONY : CMakeFiles/provafinaleapi.dir/build

CMakeFiles/provafinaleapi.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/provafinaleapi.dir/cmake_clean.cmake
.PHONY : CMakeFiles/provafinaleapi.dir/clean

CMakeFiles/provafinaleapi.dir/depend:
	cd /Users/dvolta/CLionProjects/provafinaleapi/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/dvolta/CLionProjects/provafinaleapi /Users/dvolta/CLionProjects/provafinaleapi /Users/dvolta/CLionProjects/provafinaleapi/cmake-build-debug /Users/dvolta/CLionProjects/provafinaleapi/cmake-build-debug /Users/dvolta/CLionProjects/provafinaleapi/cmake-build-debug/CMakeFiles/provafinaleapi.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/provafinaleapi.dir/depend

