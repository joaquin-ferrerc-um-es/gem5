#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import subprocess
from gem5_run import gem5_root, config_list_options

def config_describe(conf):
    # TODO: allow to order things better
    dirs = [d for d in [c.descr_dir_text_value(conf) for c in config_list_options(conf)] if d != ""]
    abbrevs = [a for a in [c.descr_abbrev_text_value(conf) for c in config_list_options(conf)] if a != ""]
    return os.path.join("/".join(dirs), "_".join(abbrevs))


option_value_abbreviations = {
    "requester_wins": "rw",
    "magic": "mg",
    "token": "tkn",
    "committer_wins": "cw",
    "requester_stalls_cda_base": "cdab",
    "requester_stalls_cda_base_ntx": "cdabntx",
    "requester_stalls_cda_hybrid": "cdah",
    "requester_stalls_cda_hybrid_ntx": "cdahntx",
}

def abbrev_option_value(s):
    if s in option_value_abbreviations:
        return option_value_abbreviations[s]
    else:
        return s

cache_config_from_tasks_gem5 = {}
def config_from_tasks_gem5(s):
    if not s in cache_config_from_tasks_gem5:
        cache_config_from_tasks_gem5[s] = subprocess.check_output([os.path.join(gem5_root, "gem5_path", "scripts", "tasks-gem5", "tasks-gem5"), "query-config", s],
                                                                  stderr = subprocess.DEVNULL,
                                                                  encoding = "UTF-8")
    return cache_config_from_tasks_gem5[s]


cache_git_revisions = {}
def get_git_revision(conf):
    r = gem5_root # TODO: should use conf
    if not r in cache_git_revisions:
        cache_git_revisions[r] = subprocess.check_output(['git', 'describe', '--dirty', '--always', '--tags'],
                                                         cwd = r, 
                                                         stderr = subprocess.DEVNULL,
                                                         encoding = "UTF-8").strip()
    return cache_git_revisions[r]
