#!/usr/bin/python
import string, datetime, os, sys, time, config, pdb, getopt, socket, tempfile, subprocess, math, shutil, popen2, collections
import caches, benchmarks
#from config import *

g_default_search_path = os.environ["PATH"]

def set_default_search_path(path):
  global g_default_search_path
  assert(path != "")
  g_default_search_path = path
  return

def run_command(command_string, input_string="", max_lines=0, verbose=0, echo=1, throw_exception=1):
    assert(g_default_search_path != "")
    os.environ["PATH"] = g_default_search_path
    if echo:
        print "running:", command_string
    obj = popen2.Popen4(command_string)
    output = ""

    obj.tochild.write(input_string)
    obj.tochild.close()
    line = obj.fromchild.readline()
    while (line):
        if verbose == 1:
            print line,
        output += line
        line = obj.fromchild.readline()
    exit_status = obj.wait()

    if(max_lines != 0):
        lines = output.split("\n");
        output = string.join(lines[-max_lines:], "\n")

    if throw_exception and exit_status != 0:
        raise RegressionError(command_string, output)
    return output

# returns a list of lines in the file that matches the pattern
def grep(filename, pattern):
    result = [];
    file = open(filename,'r')
    for line in file.readlines():
        if re.match(pattern, line):
            result.append(string.strip(line))
    return result

def usage():
  print "Usage: "

def parseOptions():
  queue=False
  local=False
  seeds=False
  config_file="config.py"
  try:
    opts, args = getopt.getopt(sys.argv[1:], "hlqsc:", ["help", "local", "queue", "seeds", "config"])
  except getopt.GetoptError as err:
    # print help information and exit:
    print str(err)  # will print something like "option -a not recognized"
    usage()
    sys.exit(2)
  for o, a in opts:
    if o in ("-h", "--help"):
      usage()
      sys.exit()
    elif o in ("-q", "--queue"):
      queue=True
    elif o in ("-l", "--local"):
      local=True
    elif o in ("-s", "--seeds"):
      seeds=True
    elif o in ("-c", "--config"):
      config_file=a
    else:
      assert False, "unhandled option"
  return queue, local, seeds, config_file

def remove_first_if_equals(s, c):
    if (len(s) > 0 and s[0] == c):
        return s[1:]
    else: 
        return(s)

###############################################################################
###                   Microbenchmarks
###############################################################################


######## Use #########
# Simply run this script to generate scripts (to manually run
# benchmarks) In order to submit jobs to the cluster, use '-q'. To
# generate scripts to create init checkpoints, use '-i'

submit_mode, local_mode, seeds_mode, config_file = parseOptions()

config_file = os.path.splitext(config_file)[0]

print "Loading Config: " + config_file

config = __import__(config_file)

print "Generating simulation scripts..."

seed_list = [0]
nodelist = ""
repository_revision = str(subprocess.check_output(['git', 'describe', '--dirty', '--always', '--tags'])).split()[0]
#repository_revision = str(subprocess.check_output(['git', 'log', '-1', '--oneline'])).split()[0]

if submit_mode:
  print "Submitting jobs..."
  config.run_gdb = 0  # No gdb when submitting
  config.copy_gem5_binary_tmp_dir = 1 # Copy binary to tmp dir to prevent overwriting it


if seeds_mode:
  print "[%d random seed(s)]" % config.num_random_seeds
  seed_list.extend(range(1,config.num_random_seeds));

results_prefix="%s/%s_%d" % (config.results_subdir,
                                datetime.date.today(),
                                config.seq_no)

cvsroot_results = os.path.join(config.gem5root, "results", results_prefix)

# Dictionary of paths to gem5 binary, per protocol
gem5_binary_exec_path = {}

# Copy gem5 binaries to tmp dirs, set path to gem5 binary for each protocol
for protocol, cache_config in config.system_list:
  # Locate gem5 executable
  gem5_exec_path = "%s/build/%s_%s/gem5.%s" % (config.gem5root, config.arch,
                                               protocol,
                                               config.build_type)
  if config.copy_gem5_binary_tmp_dir:
    # NOTE: Copying the binary is not enough to ensure that batch
    # simulations are not affected by changes to the source tree,
    # since Python config scripts in gem5-root/config are still
    # executed when starting a batch job.

    # Create a temporary directory and copy gem5 binary
    # Different binaries for each arch/protocol combination
    tmpdir_prefix = ("%s/%s/%s" % (config.tmp_gem5_binaries_path, config.arch, protocol))
    if not os.path.exists(tmpdir_prefix):
      print "Creating %s" % tmpdir_prefix
      os.makedirs(tmpdir_prefix)
    # Different tmpdir for each revision
    tmpdir_path = ("%s/gem5-rev%s." % (tmpdir_prefix, repository_revision))
    tmpdir = tempfile.mkdtemp(prefix=tmpdir_path)
    exec_path = "%s/gem5.%s" % (tmpdir, config.build_type)
    ret = subprocess.call(["cp", gem5_exec_path, tmpdir])
    if ret != 0:
      print "Failed to copy gem5.%s binary to tmp dir %s" % (config.build_type, tmpdir)
      sys.exit()
  else:
    exec_path = gem5_exec_path

  # Add tmpdir to protocols
  gem5_binary_exec_path[protocol] = exec_path

scripts = {}
for random_seed in seed_list:
  for (processors, benchmark_config, cpu_model,
       protocol, cache_config) in config.simulation_list:
    benchmark_suite, benchmark, arg_prefix, processors_opt, arg_string, benchmark_subdir, binary_filename = benchmark_config

    if protocol not in gem5_binary_exec_path:
      print "Could not find gem5 executable path for protocol %s, build type %s" % (protocol, config.build_type)
      sys.exit()

    gem5_executable_filepath = "%s" % (gem5_binary_exec_path[protocol])

    if config.results_subdir[0] == '/':
      # absolute results dir
      print "results subdir must be a relative path. Edit config.py and try again"
      sys.exit()
    if (local_mode):
      # Simulator installed in local filesystem (/scratch) , thus files
      # not accessible to any other host in SLURM queue
      hostname = socket.gethostname().split(".")[0]
      nodelist = "--nodelist=" + hostname

    benchmark_name = benchmark
    binary_suffix = config.binary_suffix

    cache_config_description="Unknown"
    cache_options_str = ''
    # Create a copy of config to change cache_l2_caches option only
    # for the current number of processors
    cache_conf = collections.OrderedDict(cache_config)
    # Sliced L2 cache: automatically set to number of processors, must
    # be left unset (set to 0) in config.py
    assert (cache_conf[caches.cache_l2_caches] == 0)
    cache_conf[caches.cache_l2_caches] = processors
    for prototype in caches.cache_config_options:
      option = cache_conf[prototype]
      if prototype.gem5opt != None:
         if prototype.gem5opt == "name":
           cache_config_description=option
         elif prototype.shared:
           cache_options_str += ' --'+prototype.gem5opt+'='+str(option / processors)
         elif prototype.gem5opt.startswith("l0") and "Two_Level" in protocol:
           pass
         else:
           cache_options_str += ' --'+prototype.gem5opt+'='+str(option)

    results_bench_config = "%s/%s/%s/%s/%dp/%s-%s/%s" %  \
                  (cvsroot_results, cpu_model,
                   protocol, cache_config_description,
                   processors, benchmark_suite,
                   arg_prefix, benchmark_name)

    # Checkpoint reuse disabled by default
    reuse_ckpt_path = None
    results_dir_base = results_bench_config

    if not os.path.exists(results_dir_base):
      os.makedirs(results_dir_base)

    results_dir = os.path.join(results_dir_base, str(random_seed))
    if not os.path.exists(results_dir):
      os.makedirs(results_dir)

    ########### Boot-script generation #########
    bootscript_filename = "bootscript_%s_%s_%s_%02dp.rcS" % \
                      (binary_suffix, benchmark_name, arg_prefix, processors)
    bootscript_path = "%s/%s" % (results_dir, bootscript_filename)
    bootscript_file = open("%s" % (bootscript_path), "w")

    bootscript_file.write("#!/bin/bash\n")
    bootscript_file.write("### @launchscript@ ###\n\n")
    bootscript_file.write("PROCESSORS=%d\n" % processors)
    bootscript_file.write("BENCHMARK_DIR=%s\n" % benchmark_subdir)
    bootscript_file.write("BINARY_SUFFIX=%s\n" % binary_suffix)

    bootscript_file.write("BINARY_FILENAME=%s\n" % binary_filename)
    bootscript_file.write("BENCHMARK_ARG_STRING='%s'\n" % arg_string)
    bootscript_file.write("RANDOM_SEED='%d'\n" % random_seed)
    bootscript_file.write("\n")
    bootscript_file.write("sync\n") # For tty to show "Welcome to Ubuntu.."
    bootscript_file.write("mkdir %s\n" % benchmarks.benchmark_disk_image_mountpoint)
    if config.arch_name == 'x86_64':
      filesystem_prefix = 'hd'
    elif config.arch_name == 'aarch64':
      filesystem_prefix = 'sd'
    else:
      print "Unknown architecture name %s" % config.arch_name
      sys.exit(2)

    bootscript_file.write("mount /dev/%sb1  %s\n" %
                          (filesystem_prefix,
                           benchmarks.benchmark_disk_image_mountpoint))

    # Must set M5_SIMULATOR=1 in order to enable m5 ops. Otherwise,
    # benchmarks typically suppress m5 ops by mmap'ing m5_mem to a
    # zero-filled region of memory instead of /dev/mem.
    bootscript_file.write("export M5_SIMULATOR=1\n")

    benchmark_suite_root_dir = os.path.join(benchmarks.benchmark_disk_image_mountpoint,
                                            benchmarks.benchmark_suites[benchmark_suite])
    # Variability is only needed if we are not using KVM..
    if not config.enable_kvm:
      bootscript_file.write("sleep 0.${RANDOM_SEED} # Generate variability via random seed \n")
    bootscript_file.write("cd %s/%s\n" % ( benchmark_suite_root_dir, benchmark_subdir))
    bootscript_file.write("export LD_PRELOAD=%s\n" % (config.preload));
    bootscript_file.write("./${BINARY_FILENAME}${BINARY_SUFFIX} %s${PROCESSORS} ${BENCHMARK_ARG_STRING}\n" % (processors_opt))
    # In case binary not found, give some time to tty to print error message
    bootscript_file.write("echo 'Launch script done. Exiting simulation...(m5 exit)'\n")
    bootscript_file.write("sync; sleep 2\n")
    bootscript_file.write("/sbin/m5 exit\n")
    bootscript_file.close()

    ########### Simulation info  #########
    siminfo_filename = config.sim_info_filename;
    siminfo_path = "%s/%s" % (results_dir, siminfo_filename)
    siminfo_file = open("%s" % (siminfo_path), "w")

    siminfo_file.write("[SimulationInfo]\n")
    siminfo_file.write("num_cpus=%d\n" % processors)
    siminfo_file.write("protocol=%s\n" % protocol)
    siminfo_file.write("cpu_model=%s\n" % cpu_model)
    siminfo_file.write("benchmark_name=%s\n" % benchmark_name)
    siminfo_file.write("benchmark_size=%s\n" % arg_prefix)
    siminfo_file.write("random_seed=%d\n" % random_seed)
    siminfo_file.write("git_revision=%s\n" % repository_revision)
    siminfo_file.close()

    ########### Simulation script generation #########

    script_filename = config.run_script_filename
    script_path = "%s/%s" % (results_dir, script_filename)

    script_file = open("%s" % (script_path), "w")

    script_file.write("#!/bin/bash\n\n")
    script_file.write('set -o nounset\n')
    script_file.write('set -o pipefail\n')
    script_file.write('set -o errexit\n')
    script_file.write('trap \'echo "$SCRIPT_COMMAND: error $? at line $LINENO"\' ERR\n\n')
    script_file.write('SCRIPT_DIR="$(readlink -fm "$(dirname "$0")")"\n')
    script_file.write('SCRIPT_COMMAND="$(basename "$0")"\n')

    script_file.write("GEM5_ROOT=%s\n" % config.gem5root)
    script_file.write("HOST=`hostname`\n")
    script_file.write("\n")

    # Debug configuration
    script_file.write("DEBUG_START_TICK=\n")
    script_file.write("DEBUG_FLAGS=%s\n" % config.debug_flags)
    script_file.write("BUILD_TYPE=%s\n" % config.build_type)
    script_file.write("RUN_GDB=%d\n" % config.run_gdb)
    script_file.write("RUN_PDB=%d\n" % config.run_pdb)
    script_file.write("EXIT_AT_ROI_END=%d\n" % config.exit_at_roi_end)
    script_file.write("ENABLE_KVM=%d\n" % config.enable_kvm)

    script_file.write("EXTRA_DETAILED_ARGS=\" %s \"\n" % config.extra_detailed_args)

    script_file.write("\n### System configuration ### \n")
    script_file.write("KERNEL_FILENAME=%s\n" % config.kernel)
    script_file.write("DISK_IMAGE_FILENAME=%s\n" % config.os_disk_image)
    script_file.write("ARCH_NAME=%s\n" % config.arch_name)
    script_file.write("PROCESSORS=%d\n" % processors)
    script_file.write("BENCHMARK=%s\n" % benchmark)
    script_file.write("BENCHMARK_NAME=%s\n" % benchmark_name)
    script_file.write("BINARY_FILENAME=%s\n" % binary_filename)
    script_file.write("BINARY_SUFFIX=%s\n" % binary_suffix)
    script_file.write("BENCHMARK_ARG_PREFIX=%s\n" % arg_prefix)
    script_file.write("BENCHMARK_ARG_STRING='%s'\n" % arg_string)
    script_file.write("WORKLOAD=%s_%s\n" % (benchmark, arg_prefix)) # Need "_" instead of "-" to find bootscript
    script_file.write("ARCH=%s\n" % config.arch)
    script_file.write("PROTOCOL=%s\n" % protocol)
    script_file.write("RESULTS_DIR=%s\n" % results_dir)
    script_file.write("RANDOM_SEED=%d\n" % random_seed)
    script_file.write("NETWORK_MODEL=%s\n" % config.network_model)
    script_file.write("MEMORY_TYPE=%s\n" % config.memory_type)
    script_file.write("MEMORY_SIZE=%s\n" % config.memory_size)
    script_file.write('CACHE_OPTIONS_STRING="%s"\n' % cache_options_str)

    script_file.write("SIM_INFO_FILENAME=%s\n" % os.path.join(results_dir,config.sim_info_filename))
    script_file.write("REPOSITORY_REVISION_ID=%s\n" % repository_revision)

    bootscript_filename = "bootscript_%s_%s_%s_%02dp.rcS" % \
                          (binary_suffix, benchmark_name, arg_prefix,
                           processors)
    script_file.write("BOOT_SCRIPT=%s\n" % bootscript_path)

    script_file.write("SIMULATION_TAG=%s\n" % config.simulation_tag)

    script_file.write("\n### Checkpoint configuration ### \n")
    script_file.write("KEEP_CHECKPOINT=1\n")
    script_file.write("CHECKPOINT_INIT_SUBDIR=%s\n" % config.checkpoint_subdir)
    script_file.write("CHECKPOINT_BOOT_ROOT_DIR=%s\n" %  config.checkpoint_boot_root_dir)
    script_file.write("CHECKPOINT_TMPDIR_PREFIX=%s\n" % config.checkpoint_tmpdir_prefix)

    # File containing /proc/<pid>/maps of simulated process
    script_file.write("PROC_MAPS_FILE=%s\n" % os.path.join(results_dir,config.proc_maps_file))

    script_file.write("\n### CPU configuration ### \n")
    script_file.write("DETAILED_SIMULATION_CPU_MODEL=%s\n" % cpu_model)

    script_file.write("\n### Cache configuration ### \n")

    # gem5 binary
    if config.copy_gem5_binary_tmp_dir:
      script_file.write("\n### Location of 'gem5' executable in tmp dir  ### \n")
      script_file.write("GEM5_EXEC_PATH=%s\n" % gem5_executable_filepath)
    else:
      script_file.write("\n### Location of 'gem5' executable   ### \n")
      script_file.write('GEM5_EXEC_PATH="${GEM5_ROOT}/build/${ARCH}_${PROTOCOL}/gem5.${BUILD_TYPE}"\n')

    script_file.write("\n### Submit mode (SLURM)  ### \n")
    script_file.write("SUBMIT_MODE=%d\n" % submit_mode)

    script_file.write("\n### %s template ### \n" %
                      os.path.basename(config.template_script_path))

    script_file.close()
    # Now append template script
    run_command("cat %s >> %s " % (config.template_script_path, script_path), echo = 0 )
    run_command("chmod +x %s" % (script_path), echo = 0)

    if (script_path in scripts):
        print "Duplicated script path %s" % script_path
        print "Conflicting configuration:", scripts[script_path]
        sys.exit(-1)
    else:
      scripts[script_path] =  (processors, benchmark_config, cpu_model,
                               protocol, cache_config)

    if (submit_mode == 1):
      ## ######################################################
      ## Generate scripts and submit jobs to the cluster
      ## ######################################################
      assert(config.debug_time == 0)
      print "Job %s_%d submitted" % (script_filename, random_seed)

      job_name = results_dir

      run_command("sbatch -J %s %s -e %s/stderr -o %s/stdout  %s " % \
                  (job_name, nodelist, results_dir, results_dir, script_path),
                  echo = 1)
    else:
      ## ######################################################
      ## Generate scripts (manual execution)
      ## ######################################################

      print "  Making script %s" % (script_path)


