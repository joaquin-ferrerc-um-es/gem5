#!/usr/bin/python3
# -*- coding: utf-8 -*-

from gem5_run import configs_set, configs_update, configs_vary, Vary, get_benchmarks, update
from options import *
import templates

configs_set(templates.base)
configs_update(templates.cache_baseline) 
configs_vary(
    update(templates.htm_cfg0_locks, { num_processors: 1 }),
    update(templates.htm_cfg1_base, { num_processors: Vary(1, 16) }),
    update(templates.htm_cfg1_pf, { num_processors: Vary(1, 16) }),
    update(templates.htm_cfg2_l0rsetevict, { num_processors: Vary(1, 16) }),
    update(templates.htm_cfg2_l1rsetevict, { num_processors: Vary(1, 16) }),
    update(templates.htm_cfg2_l2rsetevict, { num_processors: Vary(1, 16) }),
    update(templates.htm_cfg3_l0xactreplac, { num_processors: 1 }),
    update(templates.htm_cfg4_base, { num_processors: 16 }),
    update(templates.htm_cfg4_cdab64, { num_processors: 16 }),
    update(templates.htm_cfg4_cdah64, { num_processors: 16 }),
    update(templates.htm_cfg5_precrset_rldstale, { num_processors: 16 }),
    update(templates.htm_cfg6_lazycd, { num_processors: 16 }),
    update(templates.htm_cfg7_magic, { num_processors: 16 }),
    update(templates.htm_cfg8_ee, { num_processors: 16 }),
)

configs_update({
    build_type: "opt", # Set binary_type (the dafult comes from tasks-gem5 and may include several values using Vary)
    htm_visualizer: False,

    disable_transparent_hugepages: True,

    benchmark: Vary(*(get_benchmarks(suite = "stamp", size = "small",
                                     name = ["vacation-h",
                                             "kmeans-h",
                                             "intruder",
                                             "yada",
                                             "ssca2"]))),

    random_seed: Vary(*range(10)),
})

import os
output_dir = os.getenv("OUTPUT_DIR")
if output_dir != None and output_dir != "":
    configs_update({ output_directory_base: output_dir })

