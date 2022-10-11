
declare_task "build-benchmarks-virtual" "Build benchmarks in a virtual machine, directly in the benchmarks image. Options:
        --architecture X: Build only architecture X
"

# TODO: Add options to choose what benchmarks should be built.
BENCHMARKS_STAMP_SELECTED=(
    "bayes"
    "genome"
    "intruder"
    "kmeans"
    "labyrinth"
    "ssca2"
    "vacation"
    "yada"
)

task_build-benchmarks-virtual() {
    local -a archs=("${ENABLED_ARCHITECTURES[@]}")
    options="$(simpler_getopt "architecture:" "$@")"
    eval set -- "$options"
    while [[ $# -gt 0 ]] ; do
        if [[ "--architecture" = "$1" ]] ; then
            shift
            archs=("$1")
        elif [[ "--" = "$1" ]] ; then
            true # ignore
        else 
            error_and_exit "Unknown option '$1'"
        fi
        shift
    done
    for a in "${archs[@]}" ; do
        build_benchmarks_virtual "$a"
    done
}

build_benchmarks_virtual() {
    local arch="$1"

    if [[ "$arch" = "none" ]] ; then 
        build_benchmarks_virtual_sumarray "$arch"
    else
        # TODO
        echo "$(color yellow "Skipping build of test benchmark (sumarray) in a virtual mechine because it is not yet supported for '$arch'. TODO: fix this")"
    fi

    # TODO
    #if [ "$BENCHMARKS_STAMP_ENABLED" = "yes" ] ; then
    #    build_benchmarks_virtual_stamp "$arch"
    #fi
    echo "$(color yellow "Skipping build of STAMP in a virtual machine because it is not yet supported for '$arch'. TODO: fix this")"

    if [ "$BENCHMARKS_PARSEC_ENABLED" = "yes" ] ; then
        if [[ "$arch" = "aarch64" ]] ; then 
            build_benchmarks_virtual_parsec "$arch"
        else
            echo "$(color yellow "Skipping build of PARSEC in a virtual machine because it is not yet supported for '$arch'. TODO: fix this")"
        fi
    fi
}

build_benchmarks_virtual_sumarray() {
    local arch="$1"

    echo "$(color green "Building test benchmark (sumarray) for $arch")"

    error_and_exit "TODO"
    
    if [[ "$arch" = "x86_64" ]] ; then
        local makefile="Makefile.x86"
        export X86_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    else
        error_and_exit "Architecture $arch not supported for benchmark sumarray"
    fi
    pushd "$GEM5_ROOT/tests/test-progs/caps/sumarray" > /dev/null
    make -f "$makefile"
    popd > /dev/null
}

build_benchmarks_virtual_stamp() {
    local arch="$1"

    echo "$(color green "Building stamp benchmarks for $arch in a virtual machine")"
    
    #if [[ "$arch" = "x86_64" ]] ; then
        # TODO
    #elif [[ "$arch" = "aarch64" ]] ; then
	# TODO
    #else
        error_and_exit "Architecture $arch not supported for building stamp in a virtual machine"
    #fi
}

check_stamp_gem5_directory_links() {
    if [[ ! -d "$(absolute_path "$BENCHMARKS_HTM_STAMP")" || ! -L "${GEM5_ROOT}/${BENCHMARKS_HTM_STAMP}" ]] ; then
        error_and_exit "Stamp directory symlink '$(absolute_path "$BENCHMARKS_HTM_STAMP")' not found. Clone the repository in a directory out of ${GEM5_ROOT} and create a symbolic link to it in '$(dirname "$(absolute_path "$BENCHMARKS_HTM_STAMP")")'."
    fi

    if [[ ! -d "$(absolute_path "$BENCHMARKS_HTM_STAMP")/gem5" ]] ; then
        ln -s "$GEM5_ROOT" "$(absolute_path "$BENCHMARKS_HTM_STAMP")/gem5"
    fi
}

build_benchmarks_virtual_parsec_update_source() {
    local arch="$1"
    local image_name="$(absolute_path "$(get_benchmarks_disk_image "$arch")")"
    echo "$(color green "Updating sources for parsec benchmarks for $arch")"
    update_benchmarks_image_ensure_image_exists "$image_name"
    local parsec_dir="$(absolute_path "$BENCHMARKS_PARSEC_DIR")"
    local -a update_parsec_cmds=(
        --command "mkdir -p /mnt/sdb1/parsec/"
        --rsync-exclude-from="${parsec_dir}/.gitignore"
        --src "${parsec_dir}/" --rsync-to "/mnt/sdb1/parsec/"
    )
    "$VDS" --img "$image_name" \
           --command "[ -d /mnt/sdb1 ] || { echo \"Could not mount image '$image_name'\" ; exit 1 ; }" \
           \
           "${update_parsec_cmds[@]}"
}

VBS="${SCRIPT_DIR}/../virtual-build-server"
[[ -x "$VBS" ]] || error_and_exit "virtual-build-server script not found ($VBS)"

build_benchmarks_virtual_parsec() {
    local arch="$1"

    echo "$(color green "Building parsec benchmarks for $arch in a virtual machine")"
    
    check_parsec_gem5_directory_links
    
    local image_name="$(absolute_path "$(get_benchmarks_disk_image "$arch")")"

    if [[ "$arch" = "aarch64" ]] ; then
        build_benchmarks_virtual_parsec_update_source "$arch"
        "$VBS" --type arm-ubuntu \
               --img "$image_name" \
               --command "[ -d /mnt/vdb1 ] || { echo \"Could not mount image '$image_name'\" ; exit 1 ; }" \
               \
               --command "ln -s /mnt/vdb1/ /benchmarks" \
               --command "cd /benchmarks/parsec ; ./parsecmgmt-env -a build -c gcc-hooks -p aarch64_compatible" 
    else
        echo "$(color red "Building PARSEC in a virtual machine not implemented for $arch")"
    fi
}

check_parsec_gem5_directory_links() {
    if [[ ! -d "$(absolute_path "$BENCHMARKS_PARSEC_DIR")" || ! -L "${GEM5_ROOT}/${BENCHMARKS_PARSEC_DIR}" ]] ; then
        error_and_exit "Parsec directory symlink '$(absolute_path "$BENCHMARKS_PARSEC_DIR")' not found. Clone the repository in a directory out of ${GEM5_ROOT} and create a symbolic link to it in '$(dirname "$(absolute_path "$BENCHMARKS_PARSEC_DIR")")'."
    fi
}

