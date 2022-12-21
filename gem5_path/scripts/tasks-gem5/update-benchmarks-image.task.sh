
declare_task "update-benchmarks-image" "Build benchmarks and update (or create) the benchmarks disk image. Options:
        --architecture X: Build only architecture X
"

# TODO: Add options to choose what benchmarks should be built.

task_update-benchmarks-image() {
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
        update_benchmarks_image "$a"
    done
}

VDS="${SCRIPT_DIR}/../virtual-disk-server"
[[ -x "$VDS" ]] || error_and_exit "virtual-disk-server script not found ($VDS)"

clean_benchmarks_stamp_all() {
    if [ "$BENCHMARKS_STAMP_ENABLED" = "yes" ] ; then
        check_stamp_gem5_directory_links
        "$(absolute_path "$BENCHMARKS_HTM_STAMP/make.all")" clean
    fi
}

update_benchmarks_image_ensure_image_exists() {
    local image_name="$1"
    if [[ ! -f "$image_name" ]] ; then
        echo "$(color green "Disk image not found, creating it. ($image_name)")"
        truncate -s "$BENCHMARKS_DISK_IMAGE_SIZE" "$image_name"
        "$VDS" --img "$image_name" --command 'echo "- - - -" | sfdisk /dev/sdb && mke2fs -j -m0 -L "benchmarks" /dev/sdb1'
    fi
}

update_benchmarks_image() {
    local arch="$1"

    # TODO: make this optional
    # First ensure that the benchmarks are built
    clean_benchmarks_stamp_all # clean stamp benchmark before rebuilding to include only the binaries for the desired arch
    build_benchmarks "$arch"

    local image_name="$(absolute_path "$(get_benchmarks_disk_image "$arch")")"

    update_benchmarks_image_ensure_image_exists "$image_name"

    local -a update_stamp_cmds=()
    if [[ "$BENCHMARKS_STAMP_ENABLED" = "yes" ]] ; then
        update_stamp_cmds=(
            --command "mkdir -p /mnt/img1p1/benchmarks-htm/"
            --src "$GEM5_ROOT/tests/test-progs/" --rsync-to "/mnt/img1p1/test-progs/" 
            --src "$GEM5_ROOT/gem5_path/benchmarks/benchmarks-htm/stamp/" --rsync-to "/mnt/img1p1/benchmarks-htm/stamp/" 
            --command "/mnt/img1p1/benchmarks-htm/stamp/prepare-inputs" 
            --src "$GEM5_ROOT/gem5_path/benchmarks/benchmarks-htm/libs/" --rsync-to "/mnt/img1p1/benchmarks-htm/libs/" 
        )
    fi
    local -a update_parsec_cmds=()
    if [[ "$BENCHMARKS_PARSEC_ENABLED" = "yes" ]] ; then
        if [[ "$arch" = "x86_64" ]] ; then
            local parsec_dir="$(absolute_path "$BENCHMARKS_PARSEC_DIR")"
            update_parsec_cmds=(
                --command "mkdir -p /mnt/img1p1/parsec/"
                --src "${parsec_dir}/" --rsync-to "/mnt/img1p1/parsec/"
            )
        else
            echo "$(color green "PARSEC benchmarks will not be uploaded because they are built directly in the image for $arch.")"
        fi
    fi
    
    "$VDS" --img "$image_name" \
           \
           --src "$GEM5_ROOT/util/m5/build/$(get_m5_arch_name "$arch")/out/m5" --copy-to "/mnt/img1p1/benchmarks-htm/" \
           \
           "${update_stamp_cmds[@]}" \
           "${update_parsec_cmds[@]}"
}

