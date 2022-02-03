#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from gem5_run import gem5_root, get_configs, config_list_options, print_config, error, known_options, mix_configs, Vary
import options

import os
import subprocess   
import argparse
import importlib.util

def create_directory(d):
    if not os.path.exists(d): 
        os.makedirs(d)
        print(f"Created ‘{d}’")
    else:
        print(f"‘{d}’ already exists")

def check_duplicate_outputs(configs):
    m = {}
    for conf in configs:
        od = options.output_directory(conf)
        m.setdefault(od, [])
        m[od].append(conf)
    for i in m:
        if len(m[i]) != 1:
            print_config(mix_configs(m[i]))
            error(f"Duplicate output_directory: {options.output_directory(m[i][0])}")

def gen_scripts(c):
    # TODO: copy binary with timestamp

    create_directory(options.output_directory(c))
    
    with open(options.launchscript_template_filename(c), "r") as launchscript_template_file:
        with open(os.path.join(options.output_directory(c), options.launchscript_filename(c)), "w") as launchscript_file:
            template_text = launchscript_template_file.read()
            variables_text = "".join([o.launchscript_text_value(c) for o in config_list_options(c)])
            launchscript_file.write(template_text.replace("{{{variables}}}", variables_text))
       
    with open(os.path.join(options.output_directory(c), options.siminfo_filename(c)), "w") as siminfo_file:
        siminfo_file.write("[SimulationInfo]\n")
        for o in config_list_options(c):
            siminfo_file.write(o.siminfo_text_value(c))

    with open(options.runscript_template_filename(c), "r") as runscript_template_file:
        with open(os.path.join(options.output_directory(c), options.runscript_filename(c)), "w") as runscript_file:
            template_text = runscript_template_file.read()
            variables_text = "".join([o.runscript_text_value(c) for o in config_list_options(c)]) + \
                "GEM5_OPTIONS=(\n" + \
                "\n".join([f"    '{otv}'" for otv in [o.gem5_option_text_value(c) for o in config_list_options(c)] if otv != ""]) + \
                ")\n"
            runscript_file.write(template_text.replace("{{{variables}}}", variables_text))
        
    os.chmod(runscript_filename, 0o755)

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

def parse_args(argsp = argparse.ArgumentParser()):
    argsp.add_argument("--enqueue", action="store_true", help="Submit scripts to SLURM")
    argsp.add_argument("--list", action="store_true", help="List configs instead of generating scripts")
    argsp.add_argument("--list-mixed", action="store_true", help="List all configs mixed in one using Vary values, instead of generating scripts")
    argsp.add_argument("--config-file", type=str, default=os.path.join(gem5_root, "gem5_path/scripts/run-scripts/config.py"), help="Config file")
    return argsp.parse_args()

def load_config_file(config_file):
    if config_file[-3:] != ".py":
        error(f"Invalid config file name '{config_file}'. Must end in '.py'")
    if not os.path.exists(args.config_file):
        error(f"Config file '{args.config_file}' not found.\nYou may want to create it using '{os.path.join(gem5_root, 'gem5_path/scripts/run-scripts/config.py.example')}' as a starting point. ")
    spec = importlib.util.spec_from_file_location("config", config_file)
    if spec == None:
        error(f"Invalid config file '{config_file}'.")
    config_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(config_module)

    
args = parse_args()

load_config_file(args.config_file)

configs = get_configs()  

if args.list:
    for c in configs:
        print_config(c)
    print(f"{len(configs)} configurations.")

if args.list_mixed:
    print_config(mix_configs(configs))
    print(f"{len(configs)} configurations.")

if not (args.list or args.list_mixed):
    check_duplicate_outputs(configs)
    for c in get_configs():
        gen_scripts(c)
    if args.enqueue:
        for c in get_configs():
            enqueue(c)

