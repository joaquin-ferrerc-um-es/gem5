#!/usr/bin/python3
# -*- coding: utf-8 -*-

import templates
from gem5_run import configs_set, configs_merge, configs_vary, Vary, get_benchmarks
from options import *

configs_set(templates.base)

configs_merge(templates.cache_baseline)

configs_vary(
    templates.htm_cfg1_base,
    templates.htm_cfg1_l0rsetevict, 
    templates.htm_cfg1_l1rsetevict,
    templates.htm_cfg1_l1rsetevict_pf,
    templates.htm_cfg1_l1rsetevict_pf_dwng,
    templates.htm_cfg1_l1rsetevict_pf_dwng_lazycd_magic,
    templates.htm_cfg1_l1rsetevict_pf_dwng_lazycd_magic_cw,
    templates.htm_cfg1_l1rsetevict_pf_dwng_lazycd_token,
    templates.htm_cfg1_l1rsetevict_pf_dwng_precise,
    templates.htm_cfg1_l1rsetevict_pf_dwng_precise_reqstalls,
)

configs_vary(*[ { num_processors: x } for x in [8, 4] ])

configs_merge({
    build_type: "opt",
#    num_processors: Vary(8, 4),
})

configs_merge({
    benchmark: Vary(*(get_benchmarks(suite = "stamp", size = "small")
                      + get_benchmarks(suite = "stamp", size = "medium"))),
})

configs_merge({ random_seed: Vary(*range(3)) })

