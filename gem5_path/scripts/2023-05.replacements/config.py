#!/usr/bin/python3
# -*- coding: utf-8 -*-

from gem5_run import configs_set, configs_update, configs_vary, Vary, get_benchmarks, update
from options import *
import templates

configs_set(templates.base)
configs_update(templates.cache_baseline) 
configs_vary(#{cpu_model: "TimingSimpleCPU"},
             {cpu_model: "DerivO3CPU"},
)

configs_vary(
#    update(templates.htm_cfg0_locks, { num_cpus: 1 }),
    update(templates.htm_cfg1_base, { num_cpus: Vary(1, 16) }),
)

configs_vary(
    update(templates.htm_cfg2_l1rsetevict, { htm_trans_aware_l0_replacements: False, htm_trans_aware_l1_replacements: False}),
    update(templates.htm_cfg2_l1rsetevict, { htm_trans_aware_l0_replacements: False, htm_trans_aware_l1_replacements: True}),
    update(templates.htm_cfg2_l1rsetevict, { htm_trans_aware_l0_replacements: True,  htm_trans_aware_l1_replacements: True})    
)

configs_vary(
#    { htm_binary_suffix: '.htm.fallbacklock', htm_conflict_resolution: "requester_wins",  htm_precise_read_set_tracking: False},
#    { htm_binary_suffix: '.htm.fallbacklock', htm_conflict_resolution: "requester_loses", htm_precise_read_set_tracking: False},
    { htm_binary_suffix: '.htm.powertm', htm_conflict_resolution: "requester_wins_power",  htm_max_retries: 2},
)

configs_update({
    build_type: "opt", # Set binary_type (the dafult comes from tasks-gem5 and may include several values using Vary)
    htm_visualizer: True,
    htm_backoff: False,
    htm_isolation_checker: True,
    #htm_precise_read_set_tracking: True,
    htm_l0_downgrade_on_l1_gets: True, # Currently required by precise read set tracking

    exit_at_roi_end: True,

    disable_transparent_hugepages: True,

    benchmark: Vary(*(get_benchmarks(suite = "stamp", size = "medium",
                                     name = [#"genome",
                                             #"kmeans-h",
                                             #"kmeans-qs-h",
                                             #"intruder",
                                             #"intruder-qs",
                                             #"intruder-nfs",
                                             #"intruder-nfs-qs",
                                             #"intruder-rmw",
                                             #"ssca2-tx",
                                             #"ssca2",
                                             "vacation-h",
                                             "yada",
                                     ]))),

    random_seed: Vary(*range(10)),
})

import os
output_dir = os.getenv("OUTPUT_DIR")
if output_dir != None and output_dir != "":
    configs_update({ output_directory_base: os.path.realpath(output_dir) })

