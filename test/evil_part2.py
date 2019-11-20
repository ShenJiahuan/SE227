#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import random
from multiprocessing import Pool, cpu_count

def evil_process(evil_clients, evil_file, evil_id):
    evil_client = random.choice(evil_clients)
    with open("{0}/{1}".format(evil_client, evil_file), "r+b", buffering=0) as f:
        f.seek(evil_id)
        f.write(chr(evil_id % 26 + ord("A")).encode("ascii"))

if len(sys.argv) != 3:
    print("usage: ./evil_part2.py yfs1 yfs2")
    exit(-1)

evil_clients = sys.argv[1:]
evil_file = "evil2"

count = 1024
open("{0}/{1}".format(evil_clients[0], evil_file), "w").close()
with open("{0}/{1}".format(evil_clients[0], evil_file), "r+b", buffering=0) as f:
    for i in range(count):
        f.write(b"?")
p = Pool(cpu_count() * 8)
for i in range(count):
    p.apply_async(evil_process, args=(evil_clients, evil_file, i,))
p.close()
p.join()

with open("{0}/{1}".format(evil_clients[0], evil_file), "r+b", buffering=0) as f:
    data = f.read().decode("ascii")
    assert(len(data) == count)
    for i in range(len(data)):
        assert(data[i] == chr(i % 26 + ord("A")))