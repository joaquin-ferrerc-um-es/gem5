#!/usr/bin/python3
# -*- coding: utf-8 -*-

import templates
from gem5_run import configs_set, configs_merge, configs_vary, Vary, get_benchmarks
from options import *

configs_set(templates.base)

configs_merge(templates.cache_baseline)

configs_vary(
    templates.htm_cfg1_base,
    templates.htm_cfg1_precise,
    templates.htm_cfg1_reqstalls,
    templates.htm_cfg1_rset_l0_evict,
)

configs_vary(*[ { num_processors: x } for x in [8, 4] ])

configs_merge({
    build_type: "opt",
#    num_processors: Vary(8, 4),
})

configs_merge({
    benchmark: Vary(*get_benchmarks(suite = "stamp", size = "small")),
#    num_processors: Vary(2,4),
})

configs_merge({ random_seed: Vary(*range(3)) })

