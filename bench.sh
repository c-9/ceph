#!/usr/bin/env bash

export PYTHON_ROOT=$HOME/anaconda3
export PMEM_ROOT=/usr/local/pmdk-2.1.0
export BOOST_ROOT=/usr/local/Boost-1.75.0
export ROCKSDB_ROOT=/usr/local/rocksdb-6.11.4
export CEPH_ROOT=$HOME/chunk/git/ceph-17.2.7
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/local/lib:/usr/lib
export LD_LIBRARY_PATH=$CEPH_ROOT/build/lib:$PMEM_ROOT/lib:$BOOST_ROOT/lib:$ROCKSDB_ROOT/lib:$LD_LIBRARY_PATH

VSTART_DEST=$CEPH_ROOT/build
#VSTART_DEST=/mnt/pmem1/build
echo "================status================="
$CEPH_ROOT/build/bin/ceph -c $VSTART_DEST/ceph.conf -s

echo "================create================="
$CEPH_ROOT/build/bin/ceph -c $VSTART_DEST/ceph.conf osd pool create testbench 100 100

echo "================write================="
echo 3 | sudo tee /proc/sys/vm/drop_caches && sudo sync
$CEPH_ROOT/build/bin/rados -c $VSTART_DEST/ceph.conf bench -p testbench 10 write --no-cleanup

echo "================sr================="
echo 3 | sudo tee /proc/sys/vm/drop_caches && sudo sync
$CEPH_ROOT/build/bin/rados -c $VSTART_DEST/ceph.conf bench -p testbench 10 seq

echo "================rr================="
echo 3 | sudo tee /proc/sys/vm/drop_caches && sudo sync
$CEPH_ROOT/build/bin/rados -c $VSTART_DEST/ceph.conf bench -p testbench 10 rand

