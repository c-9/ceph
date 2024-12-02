#!/usr/bin/env bash

script_dir=$(dirname $0)
source $script_dir/../env.sh

# VSTART_DEST=$CEPH_ROOT
VSTART_DEST=/mnt/pmem0/ceph
# VSTART_DEST=/mnt/pmem1/ceph1
BTIME=30
BSIZE=4MB
BTHREAD=16
BDEPTH=16
echo "================status================="
$CEPH_BUILD_ROOT/bin/ceph -c $VSTART_DEST/ceph.conf -s

echo "================create================="
$CEPH_BUILD_ROOT/bin/ceph -c $VSTART_DEST/ceph.conf osd pool delete testbench testbench --yes-i-really-really-mean-it
$CEPH_BUILD_ROOT/bin/ceph -c $VSTART_DEST/ceph.conf osd pool create testbench 100 100

echo "================write================="
sudo sync && echo 3 | sudo tee /proc/sys/vm/drop_caches && sudo sync
$CEPH_BUILD_ROOT/bin/rados -c $VSTART_DEST/ceph.conf bench -p testbench $BTIME write -t $BTHREAD -b $BSIZE --no-cleanup

echo "================sr================="
sudo sync && echo 3 | sudo tee /proc/sys/vm/drop_caches && sudo sync
$CEPH_BUILD_ROOT/bin/rados -c $VSTART_DEST/ceph.conf bench -p testbench $BTIME seq -t $BTHREAD

echo "================rr================="
sudo sync && echo 3 | sudo tee /proc/sys/vm/drop_caches && sudo sync
$CEPH_BUILD_ROOT/bin/rados -c $VSTART_DEST/ceph.conf bench -p testbench $BTIME rand -t $BTHREAD

