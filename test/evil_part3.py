#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys

if len(sys.argv) != 2:
    print("usage: ./evil_part3.py yfs1")
    exit(-1)

evil_client = sys.argv[1]
evil_file_prefix = "evil3"

for i in range(1050):
    if i % 100 == 0:
        print("creating {0} of {1} files...".format(i, 1050))
    try:
        f = open("{0}/{1}-{2}".format(evil_client, evil_file_prefix, i), "w")
    except:
        assert(False)
    try: 
        f.close()
    except:
        assert(False)