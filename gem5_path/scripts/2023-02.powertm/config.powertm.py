#!/usr/bin/python3
# -*- coding: utf-8 -*-

from gem5_run import configs_set, configs_update, configs_vary, Vary, get_benchmarks, update
from options import *
import templates

configs_set(templates.base)
configs_update(templates.cache_baseline) 
configs_vary({cpu_model: "TimingSimpleCPU"},
             {cpu_model: "DerivO3CPU"},
)

configs_vary(
#    update(templates.htm_cfg0_locks, { num_cpus: 1 }),
#    update(templates.htm_cfg1_base, { num_cpus: Vary(1, 16) }),
    update(templates.htm_cfg2_l0rsetevict, { num_cpus: Vary(1, 2, 4, 8, 16) }),
)

configs_vary(
    { htm_binary_suffix: '.htm.fallbacklock', htm_conflict_resolution: "requester_wins"},
    { htm_binary_suffix: '.htm.powertm', htm_conflict_resolution: "power_tm"},

)

configs_update({
    build_type: "opt", # Set binary_type (the dafult comes from tasks-gem5 and may include several values using Vary)
    htm_visualizer: True,
    htm_isolation_checker: True,
    htm_precise_read_set_tracking: False,
    #htm_l0_downgrade_on_l1_gets: True, # Check why it causes a slowdown in some cases (intruder-nfs)

    exit_at_roi_end: True,

    disable_transparent_hugepages: True,

    benchmark: Vary(*(get_benchmarks(suite = "stamp", size = "medium",
                                     name = ["vacation-h",
                                             "kmeans-h",
                                             #"intruder-nfs",
                                             "intruder",
                                             "yada",
                                             "ssca2",
                                     ]))),

    random_seed: Vary(*range(10)),
})

import os
output_dir = os.getenv("OUTPUT_DIR")
if output_dir != None and output_dir != "":
    configs_update({ output_directory_base: os.path.realpath(output_dir) })

