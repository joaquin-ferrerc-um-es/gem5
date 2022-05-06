#!/usr/bin/python3
# -*- coding: utf-8 -*-

from gem5_run import configs_set, configs_update, configs_vary, Vary, get_benchmarks, update
from options import *
import templates

configs_set(templates.base)
configs_update(templates.cache_baseline) 
configs_vary(
    update(templates.htm_cfg0_locks, { num_cpus: 1 }),
    update(templates.htm_cfg1_base, { num_cpus: Vary(1, 16) }),
    update(templates.htm_cfg1_pf, { num_cpus: Vary(1, 16) }),
    update(templates.htm_cfg2_l0rsetevict, { num_cpus: Vary(1, 16) }),
    update(templates.htm_cfg2_l1rsetevict, { num_cpus: Vary(1, 16) }),
    update(templates.htm_cfg2_l2rsetevict, { num_cpus: Vary(1, 16) }),
    update(templates.htm_cfg3_l0xactreplac, { num_cpus: 1 }),
    update(templates.htm_cfg4_base, { num_cpus: 16 }),
    update(templates.htm_cfg4_cdab64, { num_cpus: 16 }),
    update(templates.htm_cfg4_cdah64, { num_cpus: 16 }),
    update(templates.htm_cfg5_precrset_rldstale, { num_cpus: 16 }),
    update(templates.htm_cfg6_lazycd, { num_cpus: 16 }),
    update(templates.htm_cfg7_magic, { num_cpus: 16 }),
    update(templates.htm_cfg8_ee, { num_cpus: 16 }),
)

configs_update({
    build_type: "opt", # Set binary_type (the dafult comes from tasks-gem5 and may include several values using Vary)
    htm_visualizer: False,

    benchmark: Vary(*(get_benchmarks(suite = "stamp", size = "small",
                                     name = ["vacation-h",
                                             "kmeans-h",
                                             "intruder",
                                             "yada",
                                             "ssca2"]))),

    random_seed: Vary(*range(10)),
})

