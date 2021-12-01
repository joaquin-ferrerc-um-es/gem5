
declare_task "build-benchmarks" "Build benchmarks in the host system. Options:
        --architecure X: Build only architecture X
        BUG: Due to the way that becnhmarks are built, only one architecture can be built each time.
"

# TODO: Add options to choose what benchmarks should be built.


task_build-benchmarks() {
    local -a archs=("${ENABLED_ARCHITECTURES[@]}")
    options="$(simpler_getopt "architecure:" "$@")"
    eval set -- "$options"
    while [[ $# -gt 0 ]] ; do
        if [[ "--architecure" == "$1" ]] ; then
            shift
            archs=("$1")
        elif [[ "--" == "$1" ]] ; then
            true # ignore
        else 
            error_and_exit "Unknown option '$1'"
        fi
        shift
    done
    for a in "${archs[@]}" ; do
        build_benchmarks "$a"
    done
}

build_benchmarks() {
    local arch="$1"

    if [[ "$arch" == "x86_64" ]] ; then 
        build_benchmarks_sumarray "$arch"
    else
        # TODO
        echo "$(color yellow "Skipping build of test benchmark (sumarray) because it is not yet supported for '$arch'. TODO: fix this")"
    fi
}

build_benchmarks_sumarray() {
    local arch="$1"

    echo "$(color green "Building test benchmark (sumarray) for $arch")"
    
    if [[ "$arch" == "x86_64" ]] ; then
        local makefile="Makefile.x86"
        export X86_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    else
        error_and_exit "Architecture $arch not supported for benchmark sumarray"
    fi
    pushd "$GEM5_ROOT/tests/test-progs/caps/sumarray" > /dev/null
    make -f "$makefile"
    popd > /dev/null
}

