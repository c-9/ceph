#!/usr/bin/env bash

script_file=$0
if [ "${script_file}" = "-bash" ] ; 
    then  script_file=${BASH_ARGV[0]} 
fi
script_dir=$(cd `dirname ${script_file}` && pwd)

export PYTHON_ROOT=$HOME/anaconda3
# export PYTHON_ROOT=$HOME/anaconda3/envs/py39
export PMEM_ROOT=/usr/local/pmdk-2.1.0
export BOOST_ROOT=/usr/local/Boost-1.75.0
export ROCKSDB_ROOT=/usr/local/rocksdb-6.11.4
# export ROCKSDB_ROOT=/usr/local/pmem-rocksdb-6.11.4
export KVDK_ROOT=/usr/local/kvdk
export CEPH_ROOT=$script_dir
# export CEPH_ROOT=$HOME/chunk/git/ceph-17.2.7
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/local/lib:/usr/lib
export LD_LIBRARY_PATH=$CEPH_ROOT/build/lib:$PMEM_ROOT/lib:$BOOST_ROOT/lib:$ROCKSDB_ROOT/lib:$KVDK_ROOT/lib:$LD_LIBRARY_PATH


