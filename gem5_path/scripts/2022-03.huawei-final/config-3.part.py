system_list = []

# 01: base-vs-pf 1thread
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_pf, caches.cache_baseline])

# 04: base+pf+l1rsetevict+l0xactreplac (cfg4) vs conf-res policy
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg4_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg4_cdab64, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg4_cdah64, caches.cache_baseline])

# 05: base+pf+l1rsetevict+l0xactreplac+cdah (cfg5) vs precise read set
# dup: system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg5_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg5_precrset_rldstale, caches.cache_baseline])

# 06: base+pf+l1rsetevict+l0xactreplac+cdah+precise+reload (cfg6) vs lazycd+token
# dup: system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg6_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg6_lazycd, caches.cache_baseline])

# 07: base+pf+l1rsetevict+l0xactreplac+lazycd+token arb (cfg7) vs magic arb
# dup: system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg7_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg7_magic, caches.cache_baseline])

# 08: baseline vs el vs ll vs ee
# dup: system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg8_base, caches.cache_baseline])
# dup: system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg8_el, caches.cache_baseline])
# dup: system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg8_ll, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg8_ee, caches.cache_baseline])

processor_list = []

processor_list.append(16)

results_subdir="teaser/all/3" # The subdirectory inside "gem5/results" for
                       # simulation scripts and results
