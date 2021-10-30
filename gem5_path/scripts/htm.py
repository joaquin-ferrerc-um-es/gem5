import collections
############################################################################
##  HTM system Configuration
############################################################################

fallback_lock_file = "ckpt/fallback_lock"

# HTM config options:              config name, abbrev, gem5 option name,               bool, description)
HtmOption = collections.namedtuple('HtmOption', ['name','abbrev','gem5opt','isbool','descr', 'siminfo'])
# HTM config options:                       config name,                         abbrev,    gem5 option name,                 is bool, descr, simInfo
htm_disable_speculation        = HtmOption("htm_disable_speculation",           "NoSpec",   "disable-speculation",               True,  False, True  )
htm_binary_suffix              = HtmOption("htm_binary_suffix",                 "BinSfx",   None,                                False, True,  True  )
htm_lazy_vm                    = HtmOption("htm_lazy_vm",                       "LV",       "lazy-vm",                           True,  True , True  )
htm_eager_cd                   = HtmOption("htm_eager_cd",                      "ED",       "eager-cd",                          True,  True , True  )
htm_conflict_resolution        = HtmOption("htm_conflict_resolution",           "CR",       "conflict-resolution",               False, True , True  )
htm_lazy_arbitration           = HtmOption("htm_lazy_arbitration",              "LArb",     "lazy-arbitration",                  False, True , True  ) 
htm_lazy_validated_conf_res    = HtmOption("htm_lazy_validated_conf_res",       "LzCR",     "lazy-validated-conf-res",           False, True , True  )
htm_allow_read_set_l0_evictions= HtmOption("htm_allow_read_set_l0_evictions",   "RSL0Ev",   "allow-read-set-l0-cache-evictions", True,  True , True  )
htm_allow_read_set_l1_evictions= HtmOption("htm_allow_read_set_l1_evictions",   "RSL1Ev",   "allow-read-set-l1-cache-evictions", True,  True , True  )
htm_precise_read_set_tracking  = HtmOption("htm_precise_read_set_tracking",     "RSPrec",   "precise-read-set-tracking",         True,  True , True  )
htm_nack_l1_local_evictions    = HtmOption("htm_nack_l1_local_evictions",       "NackL1Ev", "nack-l1-local-evictions",           True,  True , True  )
htm_replace_nontrans_preferred = HtmOption("htm_replace_nontrans_preferred",    "NoRWRepl", "replace-nontrans-preferred",        True,  True , True  )
htm_allow_load_delaying        = HtmOption("htm_allow_load_delaying",           "LDelay",   "allow-load-delaying",               True,  True , True  )
htm_reload_if_stale            = HtmOption("htm_reload_if_stale",               "RldStale", "reload-if-stale",                   True,  True , True  )
htm_l0_downgrade_on_l1_gets    = HtmOption("htm_l0_downgrade_on_l1_gets",       "DwnG",     "l0-downgrade-on-l1-gets",           True,  True , True  )
htm_value_checker              = HtmOption("htm_value_checker",                 "ValChk",   "value-checker",                     True,  False, False )
htm_visualizer                 = HtmOption("htm_visualizer",                    "Visual",   "visualizer",                        True,  False, False )
# HTM library options
htm_max_retries                = HtmOption("htm_max_retries",                   "Retr",      None,                               False, False, True  )
htm_heap_prefault              = HtmOption("htm_heap_prefault",                 "Pflt",      None,                               True,  False, True  )

htm_config_options = []
htm_config_options.append(htm_disable_speculation)
htm_config_options.append(htm_binary_suffix)
htm_config_options.append(htm_lazy_vm)
htm_config_options.append(htm_eager_cd)
htm_config_options.append(htm_conflict_resolution)
htm_config_options.append(htm_lazy_arbitration)
htm_config_options.append(htm_lazy_validated_conf_res)
htm_config_options.append(htm_allow_read_set_l0_evictions)
htm_config_options.append(htm_allow_read_set_l1_evictions)
htm_config_options.append(htm_precise_read_set_tracking)
htm_config_options.append(htm_nack_l1_local_evictions)
htm_config_options.append(htm_replace_nontrans_preferred)
htm_config_options.append(htm_allow_load_delaying)
htm_config_options.append(htm_reload_if_stale)
htm_config_options.append(htm_l0_downgrade_on_l1_gets)
htm_config_options.append(htm_value_checker)
htm_config_options.append(htm_visualizer)
htm_config_options.append(htm_max_retries)
htm_config_options.append(htm_heap_prefault)

# To be used by non-UMU (non-HTM or gem5 HTM) system configurations
config_empty = collections.OrderedDict()

# Baseline: All options disabled by except default HTM policies, set
# to eager CD and lazy VM (no logging)
config_baseline = collections.OrderedDict()
config_baseline[htm_disable_speculation]=False
config_baseline[htm_binary_suffix]='.htm.fallbacklock'
config_baseline[htm_lazy_vm]=True
config_baseline[htm_eager_cd]=True
config_baseline[htm_conflict_resolution]='requester_wins'
config_baseline[htm_lazy_arbitration]=None
config_baseline[htm_lazy_validated_conf_res]=None
config_baseline[htm_allow_read_set_l0_evictions]=True
config_baseline[htm_allow_read_set_l1_evictions]=False
config_baseline[htm_precise_read_set_tracking]=True
config_baseline[htm_nack_l1_local_evictions]=True
config_baseline[htm_replace_nontrans_preferred]=True
config_baseline[htm_allow_load_delaying]=False
config_baseline[htm_reload_if_stale]=False
config_baseline[htm_l0_downgrade_on_l1_gets]=True
config_baseline[htm_value_checker]=False
config_baseline[htm_visualizer]=True
config_baseline[htm_max_retries]=6
config_baseline[htm_heap_prefault]=True


## Abbreviations used for HTM options string values, to generate more
## concise HTM config string description
htm_option_str_abbreviations  = {}
htm_option_str_abbreviations['requester_wins'] = "rw"
htm_option_str_abbreviations['magic'] = "mg"
htm_option_str_abbreviations['committer_wins'] = "cw"
htm_option_str_abbreviations['requester_stalls_cda_base'] = "cdab"
htm_option_str_abbreviations['requester_stalls_cda_base_ntx'] = "cdabntx"
htm_option_str_abbreviations['requester_stalls_cda_hybrid'] = "cdah"
