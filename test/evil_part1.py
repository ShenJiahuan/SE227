#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys

if len(sys.argv) != 2:
    print("usage: ./evil_part1.py yfs1")
    exit(-1)

evil_client = sys.argv[1]
evil_file = "evil1"

with open("{0}/{1}".format(evil_client, evil_file), "w") as f:
        f.seek(1)
        f.write("1")
    
with open("{0}/{1}".format(evil_client, evil_file), "r") as f:
    data = f.read()
    assert(len(data) == 2)
    assert(data[0] == "\0")
    assert(data[1] == "1")