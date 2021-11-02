#!/usr/bin/python
import string, os, sys, time, datetime, pdb, collections
import benchmarks, htm, caches

gem5root  = os.path.abspath(os.path.dirname(os.path.realpath(__file__)) + '/../..')

gem5path_dirname = 'gem5_path'
gem5path = os.path.join(gem5root, gem5path_dirname)

# System List [HTM system options, cache options]
system_list = []

#####################################################################
##  Target Systems - Configuration
#####################################################################

# Baseline
#system_list.append(["MESI_Three_Level", htm.config_baseline, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM", htm.config_empty, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.config_baseline, caches.cache_baseline])

processor_list = []

#processor_list.append(64)
#processor_list.append(32)
processor_list.append(16)
processor_list.append(8)
processor_list.append(4)
processor_list.append(2)
processor_list.append(1)


detailed_simulation_cpu_model_list = []
#detailed_simulation_cpu_model_list.append('TimingSimpleCPU')
detailed_simulation_cpu_model_list.append('DerivO3CPU')

####################################################################
##  Benchmark Selection
####################################################################

# benchmark name, input name, arguments, bench dir, binary name, binary suffix, total work items

benchmark_groups = []
#benchmark_groups.append('test-progs-caps-small')
benchmark_groups.append('stamp-small')

benchmark_list = benchmarks.getBenchmarks(benchmark_groups)

simulation_list = []


#############################################################
# Simulation infrastructure options 
#############################################################
arch_name = "x86_64" # "{aarch64,x86_64}"
arch = "X86" # "{X86,ARM}
kernel='vmlinux-5.4.49'  # vmlinux.arm64
os_disk_image='ubuntu-18-04.img' # aarch64-ubuntu-trusty-headless.img'
enable_kvm=1

# Root directory where benchmarks are located in the disk image, must
# be kept in sync with IMAGE_DESTINATION_DIR in upload-to-image.sh
# script in benchmarks-tm repository.
checkpoint_boot_num=0  # Which boot ckpt number is used (if more than one available, see make-boot-checkpoints-
# Root directory where "boot" checkpoints are located
checkpoint_boot_root_dir=os.path.join(gem5root, "checkpoints", arch, str(checkpoint_boot_num))
# Root directory where fast-forward "init" checkpoints will be written
checkpoint_tmpdir_prefix=os.path.join(gem5path,"checkpoints")
# The name of the subdirectory (symlink) in the output directory that
# contains (points to) the "init" checkpoint
checkpoint_subdir="ckpt"
# Name of each generated script for running a simulation
run_script_filename="simulate.sh"
# Name of generated config file with simulation configuration
sim_info_filename = 'simulate.info'

# For simulations that have htm_config (HTM_umu protocols), the binary
# suffix is set as an option in the htm_config dict. For the remaining
# cases (including the HTM protocol from gem5), this is the suffix that
# gets appended to the binary.
binary_suffix = '.htm.fallbacklock'  # Use same binaries as HTM_umu configs
# simulate.sh template
template_script_path = os.path.join(gem5path, "scripts/simulate.sh.common")
# Directory where gem5 binaries are copied to for batch jobs
tmp_gem5_binaries_path = os.path.join(gem5path, "tmp-bin")
proc_maps_file = "ckpt/proc_maps"

extra_detailed_args = ""

build_type = 'opt' # 'opt', 'fast','debug'
network_model = 'simple' # 'garnet2.0'
memory_type='DDR3_1600_8x8' # 'DDR3_200cycles'
memory_size='3GB'
run_gdb = 0
exit_at_roi_end = 0
copy_gem5_binary_tmp_dir = 0 # Copy gem5 binary to tmp dir for simulation
run_pdb = 0
debug_flags = "" # "Exec,O3CPUAll,O3HTM,RubyHTM,ProtocolTrace"
debug_time=0
seq_no=1
num_random_seeds=4 # 10 # Number of random seeds to simulate (when '-s' option passed to gen-scripts.py)

preload="" #"/benchmarks/benchmarks-htm/Splash-3/libhooks_chkpoint.so"

simulation_tag="" # In order to tag simulations when applying patches

results_subdir="tests" #load-delay" #splash" #tests" # The subdirectory inside "gem5/results"
                             # for simulation scripts and results

for processors in processor_list :
    for (protocol, htm_config, cache_config) in system_list:
        for benchmark_config in benchmark_list :
            for detailed_simulation_cpu_model in detailed_simulation_cpu_model_list:
                benchmark_suite, benchmark, arg_prefix, processors_opt, arg_string, benchmark_subdir, binary_filename  = benchmark_config
                
                configuration = (processors, benchmark_config,
                                 detailed_simulation_cpu_model,
                                 protocol, htm_config, cache_config)
                simulation_list.append(configuration)

