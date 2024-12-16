#!/usr/bin/env bash

current_dir=$(dirname $0)
source $current_dir/../script/env.sh

# VSTART_DEST=$CEPH_ROOT
VSTART_DEST=/mnt/pmem2/zjq/ceph


# BSIZE=512 # 64, 512, 1024, 4096, 65536, 1048576, 4194304, 67108864

BTIME=${BTIME:-8}
BSIZE=${BSIZE:-64}
BTHREAD=${BTHREAD:-16}
# BDEPTH=${BDEPTH:-16}

echo "================status================="
$CEPH_BUILD_ROOT/bin/ceph -c $VSTART_DEST/ceph.conf -s

echo "================create================="
$CEPH_BUILD_ROOT/bin/ceph -c $VSTART_DEST/ceph.conf osd pool delete testbench testbench --yes-i-really-really-mean-it
$CEPH_BUILD_ROOT/bin/ceph -c $VSTART_DEST/ceph.conf osd pool create testbench 100 100

echo "================write================="
sudo sync && echo 3 | sudo tee /proc/sys/vm/drop_caches && sudo sync
nrun $CEPH_BUILD_ROOT/bin/rados -c $VSTART_DEST/ceph.conf bench -p testbench $BTIME write -t $BTHREAD -b $BSIZE --no-cleanup

exit 0

echo "================sr================="
sudo sync && echo 3 | sudo tee /proc/sys/vm/drop_caches && sudo sync
nrun $CEPH_BUILD_ROOT/bin/rados -c $VSTART_DEST/ceph.conf bench -p testbench $BTIME seq -t $BTHREAD

echo "================rr================="
sudo sync && echo 3 | sudo tee /proc/sys/vm/drop_caches && sudo sync
nrun $CEPH_BUILD_ROOT/bin/rados -c $VSTART_DEST/ceph.conf bench -p testbench $BTIME rand -t $BTHREAD

