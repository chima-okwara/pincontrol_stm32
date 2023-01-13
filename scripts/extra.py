#!/usr/bin/env python3

# PlatformIO pre-action script for STM32 builds:
#   Generate C++ header from STM's corresponding µC-specific SVD file (XML).

Import('env')
from os import path
import subprocess, sys

# dig up the SVD file name
svdf = env.BoardConfig().get('debug.svd_path')
chip = path.splitext(svdf)[0]

gen = './svdgen.py'
out = '../jee-svd.h'

# decide whether to (re-)generate the output file
go = True
if path.exists(out) and path.getmtime(out) > path.getmtime(gen):
    with open(out) as f:
        go = chip not in f.readline() # rewrite if 1st line doesn't match

if go:
    print(f'svdGen: {chip} to {out}', file=sys.stderr)
    subprocess.run(f'python3 {gen} {chip} >{out}', shell=1, check=1)
