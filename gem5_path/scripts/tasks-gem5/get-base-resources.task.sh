
declare_task "get-base-resources" "Get base resources (disk images, kernels…). Not completely implemented yet. Options:
        --architecure X: Get only architecture X
        --overwrite: Overwrite existing files (TODO).
"

task_get-base-resources() {
    local -a archs=("${ENABLED_ARCHITECTURES[@]}")
    local overwrite=no
    options="$(simpler_getopt "architecure:,overwrite" "$@")"
    eval set -- "$options"
    while [[ $# -gt 0 ]] ; do
        if [[ "--architecure" == "$1" ]] ; then
            shift
            archs=("$1")
        elif [[ "--overwrite" == "$1" ]] ; then
            overwrite=yes
        elif [[ "--" == "$1" ]] ; then
            true # ignore
        else 
            error_and_exit "Unknown option '$1'"
        fi
        shift
    done
    for arch in "${archs[@]}" ; do
        get_base_resources "$arch" "$overwrite"
    done
}

get_base_resources() {
    local arch="$1"
    local overwrite="$2"
    echo "Getting resources for $arch."

    local kernel="$(get_kernel "$arch")"
    local disk_image="$(get_base_image "$arch")"
    local bootloader="$(get_bootloader "$arch")"

    # TODO
    echo "$(color red "TODO: Implement me. Current implementation is just a placeholder, and is likely incorrect for your purposes.")"
    echo "$(color red "Some files will be copied from /home/users/caps/…. You should read the script and verify that they are correct before proceeding")"
    echo "$(color red "Press Enter to continue, or Control-C to cancel.")"
    read key
    
    mkdir -p "$(dirname "$GEM5_ROOT/$kernel")"
    mkdir -p "$(dirname "$GEM5_ROOT/$disk_image")"
    
    # from echo
    if [[ "$arch" == "x86_64" && "${ARCH_BASE_IMAGE_FILENAME[$arch]}" == "ubuntu-18-04.img" && "${ARCH_KERNEL[$arch]}" == "vmlinux-5.4.49" ]] ; then
        rm -f "$GEM5_ROOT/$kernel"
        ln -sf /home/users/caps/gem5_full_system/x86_64/binaries/vmlinux-5.4.49 "$GEM5_ROOT/$kernel"
        rm -f  "$GEM5_ROOT/$disk_image"
        ln -sf /home/users/caps/gem5_full_system/x86_64/disks/ubuntu-18-04.img "$GEM5_ROOT/$disk_image"
    elif [[ "$arch" == "aarch64" && "${ARCH_BASE_IMAGE_FILENAME[$arch]}" == "ubuntu-18.04-arm64-docker.img" && "${ARCH_KERNEL[$arch]}" == "vmlinux.arm64" ]] ; then
        rm -f  "$GEM5_ROOT/$kernel"
        ln -sf /home/users/caps/gem5_full_system/aarch64/binaries/vmlinux.arm64 "$GEM5_ROOT/$kernel"
        rm -f  "$GEM5_ROOT/$disk_image"
        ln -sf /home/users/caps/gem5_full_system/aarch64/disks/ubuntu-18.04-arm64-docker.img "$GEM5_ROOT/$disk_image"
        rm -f  "$GEM5_ROOT/$bootloader"
        ln -sf /home/users/caps/gem5_full_system/aarch64/binaries/boot_v2.arm64 "$GEM5_ROOT/$bootloader"
    else
        ...
    fi

    ## from public sites
    #if [[ "$arch" == "x86_64" && "${ARCH_KERNEL[$arch]}" == "vmlinux-5.4.49" && "${ARCH_BASE_IMAGE_FILENAME[$arch]}" == "ubuntu-18.04-base-boottest.img" ]] ; then
    #    # http://resources.gem5.org/resources/linux-kernel
    #     # https://www.gem5.org/project/2020/03/09/boot-tests.html
    #    curl http://dist.gem5.org/dist/current/images/x86/ubuntu-18-04/base.img.gz | gunzip > "$(absolute_path "$disk_image")"
    #    curl http://dist.gem5.org/dist/v20-1/kernels/x86/static/vmlinux-5.4.49 -o  "$(absolute_path "$kernel")"
    # elif [[ "$arch" == "aarch64" ]] ; then
    #    ...
    #else
    #     ...
    #fi        
} 

