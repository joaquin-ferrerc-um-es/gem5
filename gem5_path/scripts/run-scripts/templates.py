#!/usr/bin/python3
# -*- coding: utf-8 -*-
from gem5_run import merge, Derived, Vary, get_benchmarks, get_default_output_subdirectory
from gem5_config_utils import config_describe, config_from_tasks_gem5, get_git_revision
from options import *
from options import gem5_root as gem5_root_option
from gem5_run import gem5_root
import os

# basic config options
base = {
    arch: Vary(*config_from_tasks_gem5("${ENABLED_ARCHITECTURES[@]}").split(" ")),
    protocol: Vary(*config_from_tasks_gem5("${ENABLED_PROTOCOLS[@]}").split(" ")),
    cpu_model: "DerivO3CPU", # or "TimingSimpleCPU"
    num_processors: Vary(*[int(i) for i in config_from_tasks_gem5("${ENABLED_NUM_CPUS[@]}").split(" ")]),
    random_seed: 0,

    output_directory_root: Derived(lambda c: os.path.join(gem5_root_option(c), "results")),
    output_directory_sub: Derived(lambda c: get_default_output_subdirectory(os.path.join(gem5_root_option(c), "results"))),
    config_description: Derived(config_describe),
    output_directory: Derived(lambda c: os.path.join(output_directory_root(c), output_directory_sub(c), config_description(c))),

    gem5_exec_path: Derived(lambda c: os.path.join(gem5_root, config_from_tasks_gem5(f"$(get_gem5_binary {arch(c)} {protocol(c)} {build_type(c)})"))),
    m5_path: Derived(lambda c: os.path.join(gem5_root, "gem5_path", arch(c))),
    m5_arch: Derived(lambda c: {"x86_64": "X86", "aarch64": "ARM", "riscv": "riscv"}[arch(c)] ),
    
    # Benchmark derived options # TODO
    # benchmark: vacation-l small
    benchmark: Vary(*get_benchmarks()),
    benchmark_name: Derived(lambda c: benchmark(c).name),
    benchmark_size: Derived(lambda c: benchmark(c).size),
    benchmark_subdir: Derived(lambda c: benchmark(c).subdir),
    benchmark_binary_filename_base: Derived(lambda c: benchmark(c).binary_filename_base),
    benchmark_binary_suffix: ".htm.fallbacklock", # TODO: htm_binary_suffix if present
    benchmark_num_threads_option: Derived(lambda c: benchmark(c).nthreads_option),
    benchmark_args_string: Derived(lambda c: benchmark(c).args_string),
    benchmark_ld_preload: "",

    # Benchmark disk image options
    benchmarks_mount_image: True,
    benchmarks_disk_image: Derived(lambda c: os.path.join(gem5_root, config_from_tasks_gem5(f"$(get_benchmarks_disk_image {arch(c)})"))),
    benchmarks_image_device: Derived(lambda c: {"x86_64": "/dev/hdb1",
                                                "aarch64": "/dev/sdb1",
                                                "riscv": "TODO"}[arch(c)] ),
    benchmarks_image_mountpoint: Derived(lambda c: {"x86_64": "/benchmarks",
                                                    "aarch64": "/data",
                                                    "riscv": "TODO"}[arch(c)] ),
    
    # other
    gem5_root_option: gem5_root,
    kernel_binary: os.path.join(gem5_root, "gem5_path/x86_64/binaries/vmlinux-5.4.49"), # TODO
    root_device: "/dev/hda1",
    os_disk_image: os.path.join(gem5_root, "gem5_path/x86_64/disks/ubuntu-18-04.img"), # TODO
    git_revision: Derived(get_git_revision),
    terminal_filename: Derived(lambda c: {"x86_64": "system.pc.com_1.device",
                                          "aarch64": "system.terminal",
                                          "riscv": "TODO"}[arch(c)] ),
    arch_specific_opts: Derived(lambda c: {"x86_64": "",
                                           "aarch64": " --machine-type=VExpress_GEM5_V2 --enable-tme",
                                           "riscv": "TODO"}[arch(c)] ),
    checkpoint_boot_root_dir: os.path.join(gem5_root, "gem5_path", "x86_64", "checkpoints", "booted"), # TODO
    
    network_model: "simple", # or 'garnet2.0'
    memory_type: "DDR3_1600_8x8", # or 'DDR3_200cycles'
    memory_size: Derived(lambda c: config_from_tasks_gem5(f"${{ARCH_MEMORY[{arch(c)}]}}")),
    
    # debug options
    enable_kvm: Derived(lambda c: {"x86_64": True,
                                   "aarch64": False,
                                   "riscv": False}[arch(c)] ),
    debug_start_tick: -1, # disabled
    debug_flags: "",
    build_type: Vary(*config_from_tasks_gem5("${ENABLED_BUILD_TYPES[@]}").split(" ")),
    run_gdb: False,
    exit_at_roi_end: True,
    extra_detailed_args: "",
    proc_maps_file: "ckpt/proc_maps",
    disable_transparent_hugepages: False,

    # unnecesary options:
    launchscript_filename: "launch_script.rcS",
    siminfo_filename: "simulate.info",
    runscript_filename: "simulate",
    runscript_template_filename: os.path.join(gem5_root, "gem5_path/scripts/run-scripts/simulate.common"), # TODO
    checkpoint_init_subdir: "ckpt",
}

# Cache config templates
cache_baseline = {
    cache_name: "DefaultCache",
    cache_l0i_size: 32 * 1024,
    cache_l0d_size: 32 * 1024,
    cache_l0i_assoc: 8,
    cache_l0d_assoc: 8,
    cache_l1i_size: 256 * 1024,
    cache_l1d_size: 256 * 1024,
    cache_l1i_assoc: 8,
    cache_l1d_assoc: 8,
    cache_l2_num_caches: Derived(lambda c: num_processors(c)),
    cache_l2_size_per_cache: Derived(lambda c: 32 * 1024 * 1024 // num_processors(c)),
    cache_l2_assoc: 16,
}

cache_test = merge(cache_baseline, {
    cache_name: "TestCache",
    cache_l0i_size: 8 * 1024,
    cache_l0d_size: 8 * 1024,
    cache_l1i_size: 32 * 1024,
    cache_l1d_size: 32 * 1024,
    cache_l2_size_per_cache: Derived(lambda c: 256 * 1024 // num_processors(c)),})

cache_small = merge(cache_baseline, {
    cache_name: "SmallCache",
    cache_l0i_size: 4 * 1024,
    cache_l0d_size: 4 * 1024,
    cache_l1i_size: 32 * 1024,
    cache_l1d_size: 32 * 1024,
})

cache_baseline_2level = merge(cache_baseline, {
    cache_name: "DefaultTwoLevelCache",
    cache_l0i_size: 0,
    cache_l0d_size: 0,
    cache_l1i_size: 32 * 1024,
    cache_l1d_size: 32 * 1024,
    cache_l2_size_per_cache: Derived(lambda c: 8 * 1024 * 1024 // num_processors(c)),
})

# HTM templates
htm_cfg1_base = {
    # TODO ? protocol: "MESI_Three_Level_HTM_umu", # These options don't work with e.g., MESI_Three_Level_HTM_umu
    htm_disable_speculation: False,
    htm_binary_suffix: '.htm.fallbacklock',
    htm_lazy_vm: True,
    htm_eager_cd: True,
    htm_conflict_resolution: 'requester_wins',
    htm_lazy_arbitration: None,
    htm_lazy_validated_conf_res: None,
    htm_allow_read_set_l0_cache_evictions: False,
    htm_allow_write_set_l0_cache_evictions: False,
    htm_allow_read_set_l1_cache_evictions: False,
    htm_allow_write_set_l1_cache_evictions: False,
    htm_allow_read_set_l2_cache_evictions: False,
    htm_allow_write_set_l2_cache_evictions: False,
    htm_precise_read_set_tracking: False,
    htm_trans_aware_l0_replacements: False,
    htm_trans_aware_l1_replacements: False,
    htm_allow_load_delaying: False,
    htm_reload_if_stale: False,
    htm_l0_downgrade_on_l1_gets: False,
    htm_value_checker: False,
    htm_isolation_checker: False,
    htm_visualizer: False, # True,
    htm_max_retries: 6,
    htm_max_backoff: 8,
    htm_heap_prefault: 0,
    htm_fallback_lock_filename: "ckpt/fallback_lock",
}



htm_cfg1_l0rsetevict = merge(htm_cfg1_base, {
    htm_allow_read_set_l0_cache_evictions: True,
})

htm_cfg1_l1rsetevict = merge(htm_cfg1_l0rsetevict, {
    htm_allow_read_set_l1_cache_evictions: True,
})

htm_cfg1_l1rsetevict_pf = merge(htm_cfg1_l1rsetevict, {
    htm_heap_prefault: True,
})

htm_cfg1_l1rsetevict_pf_dwng = merge(htm_cfg1_l1rsetevict_pf, {
    htm_l0_downgrade_on_l1_gets: True,
})

htm_cfg1_l1rsetevict_pf_dwng_lazycd_magic = merge(htm_cfg1_l1rsetevict_pf, {
    htm_eager_cd: False,
    htm_lazy_arbitration: 'magic',
    htm_lazy_validated_conf_res: 'requester_wins',
})

htm_cfg1_l1rsetevict_pf_dwng_lazycd_magic_cw = merge(htm_cfg1_l1rsetevict_pf_dwng_lazycd_magic, {
    htm_lazy_validated_conf_res: 'committer_wins',
})

htm_cfg1_l1rsetevict_pf_dwng_lazycd_token = merge(htm_cfg1_l1rsetevict_pf, {
    htm_eager_cd: False,
    htm_lazy_arbitration: 'token',
    htm_lazy_validated_conf_res: 'requester_wins',
})

htm_cfg1_l1rsetevict_pf_dwng_precise  = merge(htm_cfg1_l1rsetevict_pf_dwng, {
    htm_precise_read_set_tracking: True,
})

htm_cfg1_l1rsetevict_pf_dwng_precise_reqstalls  = merge(htm_cfg1_l1rsetevict_pf_dwng_precise, {
    htm_conflict_resolution: 'requester_stalls_cda_base',
})

htm_cfg1_l1rsetevict_pf_dwng_precise_reqstalls_reload = merge(htm_cfg1_l1rsetevict_pf_dwng_precise_reqstalls, {
    htm_reload_if_stale: True,
})

htm_cfg1_l1rsetevict_pf_dwng_precise_reqstalls_retry64 = merge(htm_cfg1_l1rsetevict_pf_dwng_precise_reqstalls, {
    htm_max_retries: 64,
})

htm_cfg1_l2rwsetevict_pf_dwng_precise_reqstalls_eagervm = merge(htm_cfg1_l1rsetevict_pf_dwng_precise_reqstalls, {
    htm_lazy_vm: False,
    htm_allow_read_set_l2_cache_evictions: True,
    htm_allow_write_set_l0_cache_evictions: True,
    htm_allow_write_set_l1_cache_evictions: True,
    htm_allow_write_set_l2_cache_evictions: True,
    htm_isolation_checker: True,
})

htm_cfg1_l2rwsetevict_pf_dwng_precise_reqstalls_eagervm_reload = merge(htm_cfg1_l2rwsetevict_pf_dwng_precise_reqstalls_eagervm, {
    htm_reload_if_stale: True,
})

