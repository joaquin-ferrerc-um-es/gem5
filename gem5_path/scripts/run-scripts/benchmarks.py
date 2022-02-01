#!/usr/bin/python3
# -*- coding: utf-8 -*-

from gem5_run import Benchmark

# Benchmark suites and their directory in the disk image
dir_stamp = "benchmarks-htm/stamp"
dir_splash3 = "benchmarks-htm/Splash-3"


# name, suite, size, nthreads_option, args_string, subdir, binary_filename
Benchmark("simpletest", "test-progs-caps",     "small",      "-t",       "-a1048576",     "test-progs/caps/sumarray",       "bin/x86/sumarray")

#Benchmark("intruder", "stamp",   "small", "-t", " -a10 -l4 -n2048 -s1"  ,         dir_stamp + "/intruder", "intruder")
Benchmark("genome", "stamp",     "small", "-t", "-g256 -s16 -n16384",             dir_stamp + "/genome",   "genome")
Benchmark("intruder", "stamp",   "small", "-t", " -a10 -l4 -n2048 -s1"  ,         dir_stamp + "/intruder", "intruder")
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

Benchmark("barnes", "splash3",  "small", "< inputs/n16384-p", "",             dir_splash3 + "/barnes",   "BARNES")
Benchmark("fmm", "splash3",     "small", "< inputs/input.16384.", "",           dir_splash3 + "/fmm",   "FMM") # WARNING: Input filename has been modified to work: 'for i in {1,2,4,8,16,32,64}; do mv input.${i}.16384 input.16384.${i}; done'
Benchmark("ocean-c", "splash3",    "small", "-p", "-n258",             dir_splash3 + "/ocean-contiguous_partitions",   "OCEAN-CONT")
Benchmark("ocean-nc", "splash3",   "small", "-p", "-n258",         dir_splash3 + "/ocean-non_contiguous_partitions",   "OCEAN-NOCONT")
Benchmark("radiosity", "splash3",  "small", "-p ", " -ae 5000 -bf 0.1 -en 0.05 -room -batch", dir_splash3 + "/radiosity", "RADIOSITY")
Benchmark("raytrace", "splash3",   "small", "-p", "-m64 inputs/car.env",         dir_splash3 + "/raytrace", "RAYTRACE")
Benchmark("volrend", "splash3",    "small", "", "inputs/head 8",                 dir_splash3 + "/volrend",  "VOLREND")
Benchmark("water-ns", "splash3",   "small", "< inputs/n512-p", "",               dir_splash3 + "/water-nsquared",   "WATER-NSQUARED")
Benchmark("water-sp", "splash3",   "small", "< inputs/n512-p", "",               dir_splash3 + "/water-spatial",    "WATER-SPATIAL")
Benchmark("cholesky", "splash3",   "small", "-p", "inputs/tk15.O",               dir_splash3 + "/cholesky", "CHOLESKY")
Benchmark("fft", "splash3",        "small", "-p", "-m16",                        dir_splash3 + "/fft",   "FFT")
Benchmark("radix", "splash3",      "small", "-p", "-n1048576",                   dir_splash3 + "/radix", "RADIX")
Benchmark("lu-c", "splash3",       "small", "-p", "-n512",                       dir_splash3 + "/lu-contiguous_blocks",     "LU-CONT")
Benchmark("lu-nc", "splash3",      "small", "-p", "-n512",                       dir_splash3 + "/lu-non_contiguous_blocks", "LU-NOCONT")
