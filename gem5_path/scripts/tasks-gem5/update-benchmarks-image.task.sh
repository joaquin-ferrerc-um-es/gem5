
declare_task "update-benchmarks-image" "Build benchmarks and update (or create) the benchmarks disk image. Options:
        --architecure X: Build only architecture X
"

# TODO: Add options to choose what benchmarks should be built.

task_update-benchmarks-image() {
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
        update_benchmarks_image "$a"
    done
}

VDS="${SCRIPT_DIR}/../virtual-disk-server"
[[ -x "$VDS" ]] || error_and_exit "virtual-disk-server script not found ($VDS)"

update_benchmarks_image() {
    local arch="$1"

    # TODO: make this optional
    # First ensure that the benchmarks are built
    build_benchmarks "$arch"

    local image_name="$(absolute_path "$(get_benchmarks_disk_image "$arch")")"

    if [[ ! -f "$image_name" ]] ; then
        echo "Disk image not found, creating it. ($image_name)"
        truncate -s 2G "$image_name"
        "$VDS" --img "$image_name" --command 'echo "- - - -" | sfdisk /dev/sdb && mke2fs -j -m0 -L "benchmarks" /dev/sdb1'
    fi
    "$VDS" --img "$image_name" \
           --command "[ -d /mnt/sdb1 ] || { echo \"Could not mount image '$image_name'\" ; exit 1 ; }" \
           --command "mkdir -p /mnt/sdb1/benchmarks-htm/" \
           --src "$GEM5_ROOT/tests/test-progs/" --rsync-to "/mnt/sdb1/test-progs/" \
           --src "$GEM5_ROOT/util/m5/build/x86/out/m5" --copy-to "/mnt/sdb1/benchmarks-htm/" # TODO: the m5 binary should be arch dependent
}

