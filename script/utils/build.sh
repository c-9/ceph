#!/usr/bin/env sh
g++ -O3 -mavx512f pmem_populate.cc -o pp -pthread -lpmem
