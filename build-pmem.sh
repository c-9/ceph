#!/usr/bin/env bash
export PYTHON_ROOT=$HOME/anaconda3
export PMEM_ROOT=/usr/local/pmdk-2.1.0
export ROCKSDB_ROOT=/usr/local/pmem-rocksdb-6.11.4
export BOOST_ROOT=/usr/local/Boost-1.75.0

./do_cmake.sh -DWITH_SYSTEM_BOOST=ON -DBoost_NO_BOOST_CMAKE=ON -DWITH_SYSTEM_ROCKSDB=ON -DWITH_SYSTEM_LIBURING=ON -DPython3_EXECUTABLE=$PYTHON_ROOT/bin/python -DROCKSDB_ROOT=$ROCKSDB_ROOT -DBOOST_ROOT=$BOOST_ROOT -DPMEM_ROOT=$PMEM_ROOT
