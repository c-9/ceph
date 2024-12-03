#!/usr/bin/env bash

script_dir=$(dirname $0)
source $script_dir/env.sh

VSTART_DEST=/mnt/pmem0/ceph
VSTART_DEST=$VSTART_DEST $script_dir/src/stop.sh && rm -rf /mnt/pmem0/ceph/
# VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_BUILD_ROOT/../src/pstart.sh -n -x -b --bluestore-pmem /dev/pmem0.2
VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_BUILD_ROOT/../src/pstart.sh -n -x -b --bluestore-pmem /dev/pmem0.2 --pmem-rocksdb
