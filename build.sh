#!/usr/bin/env bash

set -e

mode="debug"
#mode="optimised"

disabled_warnings="-Wno-unused-parameter -Wno-unused-function"

cppflags="--std=c++17 -Wall -Wextra -Werror ${disabled_warnings} -fno-exceptions -fno-rtti"
cppflags_debug="-O0 -g"
cppflags_optimised="-O3"

src_dir="src"
build_dir="build"

if ! [ -e ${build_dir} ]; then
	mkdir ${build_dir}
fi

if [ "${mode}" = "debug" ]; then
	cppflags="${cppflags_debug} ${cppflags}"
elif [ "${mode}" = "optimised" ]; then
	cppflags="${cppflags_optimised} ${cppflags}"
else
	echo "Unknown mode ${mode}"
	exit -1
fi

time (
	clang ${cppflags} -o ${build_dir}/metatool ${src_dir}/metatool.cpp

	# Build test
	# ${build_dir}/metatool ${src_dir}/test.cpp > ${src_dir}/meta_generated.h
	# clang ${cppflags} -o ${build_dir}/test ${src_dir}/test.cpp
)

# Run the test
# ${build_dir}/test
