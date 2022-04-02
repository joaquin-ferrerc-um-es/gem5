system_list = []

# 01: base-vs-pf 1thread
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_pf, caches.cache_baseline])

# 02: base+pf (cfg2) vs l0/l1/l2evict 1thread
# dup: system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg2_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg2_l0rsetevict, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg2_l1rsetevict, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg2_l2rsetevict, caches.cache_baseline])

# 03: base+pf+l1rsetevict (cfg3) vs htmrepl
# dup: system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg3_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg3_l0xactreplac, caches.cache_baseline])

processor_list = []

processor_list.append(1)

results_subdir="teaser/all/2" # The subdirectory inside "gem5/results" for
                       # simulation scripts and results
