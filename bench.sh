#!/usr/bin/env bash

script_dir=$(dirname $0)
source $script_dir/env.sh

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

