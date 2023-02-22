#!/usr/bin/python3
# -*- coding: utf-8 -*-

from gem5_run import Benchmark

# Benchmark suites and their directory in the disk image
dir_stamp = "benchmarks-htm/stamp"

# Benchmark(name, suite, size, nthreads_option, args_string, subdir, binary_filename)

Benchmark("simpletest", "test-progs-caps",     "small",      "-t",       "-a1048576",     "test-progs/caps/sumarray",       "bin/x86/sumarray")

Benchmark("genome", "stamp",     "small", "-t", "-g256 -s16 -n16384",             dir_stamp + "/genome",   "genome")
Benchmark("intruder", "stamp",   "small", "-t", " -a10 -l4 -n2048 -s1"  ,         dir_stamp + "/intruder", "intruder")
Benchmark("intruder-nfs","stamp","small", "-t", " -a10 -l4 -n2048 -s1"  ,         dir_stamp + "/intruder-nfs", "intruder")
Benchmark("kmeans-l", "stamp",   "small", "-p", "-m40 -n40 -t0.05 -i inputs/random-n2048-d16-c16.txt", dir_stamp + "/kmeans",  "kmeans")
Benchmark("kmeans-h", "stamp",   "small", "-p", "-m15 -n15 -t0.05 -i inputs/random-n2048-d16-c16.txt", dir_stamp + "/kmeans", "kmeans")
Benchmark("ssca2", "stamp",      "small", "-t", "-s13 -i1.0 -u1.0 -l3 -p3",       dir_stamp + "/ssca2",    "ssca2")
Benchmark("vacation-l", "stamp", "small", "-c", "-n2 -q90 -u98 -r16384 -t4096",   dir_stamp + "/vacation", "vacation")
Benchmark("vacation-h", "stamp", "small", "-c", "-n4 -q60 -u90 -r16384 -t4096",   dir_stamp + "/vacation", "vacation")
Benchmark("yada", "stamp",       "small", "-t", "-a20 -i inputs/633.2",           dir_stamp + "/yada",     "yada")
#Benchmark("bayes", "stamp",      "small", "-t", "-v32 -r1024 -n2 -p20 -i2 -e2", dir_stamp + "/bayes",   "bayes")
#Benchmark("labyrinth", "stamp",  "small", "-t", "-i inputs/random-x32-y32-z3-n96.txt", dir_stamp + "/labyrinth", "labyrinth")

Benchmark("genome", "stamp",     "medium", "-t", "-g512 -s32 -n32768",             dir_stamp + "/genome",   "genome")
Benchmark("intruder", "stamp",   "medium", "-t", " -a10 -l16 -n4096 -s1",          dir_stamp + "/intruder", "intruder")
Benchmark("intruder-nfs","stamp","medium", "-t", " -a10 -l16 -n4096 -s1",          dir_stamp + "/intruder-nfs", "intruder")
Benchmark("kmeans-l", "stamp",   "medium", "-p", "-m40 -n40 -t0.05 -i inputs/random-n16384-d24-c16.txt", dir_stamp + "/kmeans",  "kmeans")
Benchmark("kmeans-h", "stamp",   "medium", "-p", "-m15 -n15 -t0.05 -i inputs/random-n16384-d24-c16.txt", dir_stamp + "/kmeans", "kmeans")
Benchmark("ssca2", "stamp",      "medium", "-t", "-s14 -i1.0 -u1.0 -l9 -p9",       dir_stamp + "/ssca2",    "ssca2")
Benchmark("vacation-l", "stamp", "medium", "-c", "-n2 -q90 -u98 -r1048576 -t4096", dir_stamp + "/vacation", "vacation")
Benchmark("vacation-h", "stamp", "medium", "-c", "-n4 -q60 -u90 -r1048576 -t4096", dir_stamp + "/vacation", "vacation")
# NOTE: yada input 633.2 is the recommended small, but ttimeu10000 has much longer simulation times than the remaining medium inputs benchmarks...
Benchmark("yada", "stamp",       "medium", "-t", "-a20 -i inputs/633.2",   dir_stamp + "/yada",     "yada")
#Benchmark("yada", "stamp",       "medium", "-t", "-a10 -i inputs/ttimeu10000.2",   dir_stamp + "/yada",     "yada")        
#Benchmark("bayes", "stamp",      "medium", "-t", "-v32 -r4096 -n2 -p20 -i2 -e2",   dir_stamp + "/bayes",   "bayes")
#Benchmark("labyrinth", "stamp",  "medium", "-t", "-i inputs/random-x48-y48-z3-n64.txt", dir_stamp + "/labyrinth", "labyrinth")

for (suite, bench) in [
        ("parsec", "blackscholes"),
        ("parsec", "bodytrack"),
        ("parsec", "canneal"),
        ("parsec", "dedup"),
        ("parsec", "facesim"),
        ("parsec", "ferret"),
        ("parsec", "fluidanimate"),
        ("parsec", "freqmine"),
        #("parsec", "netdedup"),   # Does not work or x86 or arm
        #("parsec", "netferret"),  # Does not work for arm, huge variability for x86
        #("parsec", "netstreamcluster"),  # Does not work for arm, huge variability for x86
        ("parsec", "raytrace"),
        ("parsec", "streamcluster"),
        ("parsec", "swaptions"),
        ("parsec", "vips"),
        ("parsec", "x264"),
        ("splash2", "barnes"),
        ("splash2", "cholesky"),
        ("splash2", "fft"),
        ("splash2", "fmm"),
        ("splash2", "lu_cb"),
        ("splash2", "lu_ncb"),
        ("splash2", "ocean_cp"),
        ("splash2", "ocean_ncp"),
        ("splash2", "radiosity"),
        ("splash2", "radix"),
        ("splash2", "raytrace"),
        ("splash2", "volrend"),
        ("splash2", "water_nsquared"),
        ("splash2", "water_spatial"),
        ("splash2x", "barnes"),
        ("splash2x", "cholesky"),
        ("splash2x", "fft"),
        ("splash2x", "fmm"),
        ("splash2x", "lu_cb"),
        ("splash2x", "lu_ncb"),
        ("splash2x", "ocean_cp"),
        ("splash2x", "ocean_ncp"),
        ("splash2x", "radiosity"),
        ("splash2x", "radix"),
        ("splash2x", "raytrace"),
        ("splash2x", "volrend"),
        ("splash2x", "water_nsquared"),
        ("splash2x", "water_spatial"),
]:
    for size in ["test", "simdev", "simsmall", "simmedium", "simlarge"]:
        Benchmark(bench, suite, size, "-n", "-a run -c gcc-hooks -p %s.%s -i %s" % (suite, bench, size), "parsec", "parsecmgmt-env")
