#!/usr/bin/env bash

export PYTHON_ROOT=$HOME/anaconda3
# export PYTHON_ROOT=$HOME/anaconda3/envs/py39
export PMEM_ROOT=/usr/local/pmdk-2.1.0
export BOOST_ROOT=/usr/local/Boost-1.75.0
export ROCKSDB_ROOT=/usr/local/rocksdb-6.11.4
export CEPH_ROOT=$HOME/chunk/git/ceph-17.2.7
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/local/lib:/usr/lib
export LD_LIBRARY_PATH=$CEPH_ROOT/build/lib:$PMEM_ROOT/lib:$BOOST_ROOT/lib:$ROCKSDB_ROOT/lib:$LD_LIBRARY_PATH


