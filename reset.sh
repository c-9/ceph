#!/usr/bin/env bash

current_dir=$(dirname $0)
source $current_dir/env.sh

VSTART_DEST=/mnt/pmem0/ceph
VSTART_DEST=$VSTART_DEST $current_dir/src/stop.sh && rm -rf /mnt/pmem0/ceph/

# VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_BUILD_ROOT/../src/pstart.sh -n -x -b
# VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_BUILD_ROOT/../src/pstart.sh -n -x -b --pmem-rocksdb
VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_BUILD_ROOT/../src/pstart.sh -n -x -K --kstore_path /mnt/pmem0/ceph/kstore

# VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_BUILD_ROOT/../src/pstart.sh -n -x -b --bluestore-pmem /mnt/pmem0/block
# VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_BUILD_ROOT/../src/pstart.sh -n -x -b --bluestore-pmem /mnt/pmem0/block --pmem-rocksdb
# VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_BUILD_ROOT/../src/pstart.sh -n -x -K --bluestore-pmem /mnt/pmem0/block --kstore_path /mnt/pmem0/ceph/kstore 