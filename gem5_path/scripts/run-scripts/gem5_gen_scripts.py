#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os

import options
from gem5_run import get_configs, config_list_options, print_config, error, known_options, Vary

def create_direcory(d):
    if not os.path.exists(d): 
        os.makedirs(d)
        print(f"Created ‘{d}’")
    else:
        print(f"‘{d}’ already exists")

def compare_configs(configs):
    ret = {}
    for o in known_options:
        vals = []
        for c in configs:
            if o in c:
                v = o(c)
            else:
                v = "missing"
                
            if not v in vals:
                vals.append(v)
        if len(vals) == 1:
            if vals[0] != "missing":
                ret[o] = vals[0]
        else:
            ret[o] = Vary(*vals)
    return ret
        
    
def check_duplicate_outputs(configs):
    m = {}
    for conf in configs:
        od = options.output_directory(conf)
        m.setdefault(od, [])
        m[od].append(conf)
    for i in m:
        if len(m[i]) != 1:
            print_config(compare_configs(m[i]))
            error(f"Duplicate output_directory: {options.output_directory(m[i][0])}")

def gen_scripts(c):
    # TODO: eval Derived in c
    
    # TODO: copy binary with timestamp

    create_direcory(options.output_directory(c))
    
    with open(os.path.join(options.output_directory(c), options.launchscript_filename(c)), "w") as launchscript_file:
        launchscript_file.write("#!/bin/bash\n### @launchscript@ ###\n\n")

        for o in config_list_options(c):
            launchscript_file.write(o.launchscript_text_value(o(c)))
        
        launchscript_file.write("""
export M5_SIMULATOR=1
if [ "${benchmarks_mount_image}" = "True" ] ; then
    mkdir -p "${benchmarks_image_mountpoint}"
    mount "${benchmarks_image_device}" "${benchmarks_image_mountpoint}"
    cd "${benchmarks_image_mountpoint}"
fi
sync

if [ "${disable_transparent_hugepages:-False}" == "True" ] ; then
    echo never > /sys/kernel/mm/transparent_hugepage/enabled
fi

if [ "${enable_kvm:-False}" == "False" ] ; then
      sleep 0.${RANDOM_SEED} # Generate variability via sleep
fi
cd "${benchmark_subdir}"

"./${benchmark_binary_filename_base}.${arch}${benchmark_binary_suffix}" ${benchmark_num_threads_option}${num_processors} ${benchmark_args_string}
echo 'Launch script done ($?='"$?"'). Exiting simulation... (m5 exit)'

sync
sleep 2
/sbin/m5 exit
""")

    with open(os.path.join(options.output_directory(c), options.siminfo_filename(c)), "w") as siminfo_file:
        siminfo_file.write("[SimulationInfo]\n")
        for o in config_list_options(c):
            siminfo_file.write(o.siminfo_text_value(c))

    runscript_filename = os.path.join(options.output_directory(c), options.runscript_filename(c))
    with open(runscript_filename, "w") as runscript_file:
        with open(options.runscript_template_filename(c) + ".begin", "r") as runscript_template_file:
            runscript_file.write(runscript_template_file.read())
        
        for o in config_list_options(c):
            runscript_file.write(o.runscript_text_value(c))        

        runscript_file.write("GEM5_OPTIONS=(\n")
        for o in config_list_options(c):
            opt = o.gem5_option_text_value(c)
            if opt != "":
                runscript_file.write("    '" + opt + "'\n")
        runscript_file.write(")\n")

        with open(options.runscript_template_filename(c) + ".end", "r") as runscript_template_file:
            runscript_file.write(runscript_template_file.read())
        
    os.chmod(runscript_filename, 0o755)

import subprocess
    
def enqueue(c):
# TODO
# --exclude               
#if submit_mode:
#  config.copy_gem5_binary_tmp_dir = 1 # Copy binary to tmp dir to prevent overwriting it
    assert(options.htm_visualizer(c) == False)
    assert(options.run_gdb(c) == False)
    od = options.output_directory(c)
    stderr = os.path.join(od, "stderr")
    stdout = os.path.join(od, "stdout")
    runscript_filename = os.path.join(od, options.runscript_filename(c))
    cmd = f"sbatch -J {options.config_description(c)} -e {stderr} -o {stdout} {runscript_filename}"
    subprocess.run(cmd, shell=True, check=True)


import argparse

def parse_args(argsp = argparse.ArgumentParser()):
    argsp.add_argument("--enqueue", action="store_true", help="Submit scripts to SLURM")
    argsp.add_argument("--list", action="store_true", help="List configs instead of generating scripts")

    return argsp.parse_args()

args = parse_args()

import config

configs = get_configs()  

if args.list:
    for c in configs:
        print_config(c)
else:
    check_duplicate_outputs(configs)
    for c in get_configs():
        gen_scripts(c)
    if args.enqueue:
        for c in get_configs():
            enqueue(c)

