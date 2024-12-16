#!/usr/bin/env bash

current_dir=$(dirname $0)
source $current_dir/env.sh

VSTART_DEST=/mnt/pmem2/zjq/ceph
VSTART_DEST=$VSTART_DEST $current_dir/pstop.sh && rm -rf /mnt/pmem2/zjq/ceph/
MAKEFS=true
if [ $# -gt 0 ]; then
    echo "reset.sh makefs: $1"
    MAKEFS=$1
fi
#  --bluestore-no-metacache
# VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_ROOT/script/pstart.sh -n -x -b --makefs $MAKEFS
# VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_ROOT/script/pstart.sh -n -x -b --makefs $MAKEFS --pmem-rocksdb
VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_ROOT/script/pstart.sh -n -x -K --makefs $MAKEFS --kstore_path /mnt/pmem2/zjq/ceph/kstore

# VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_ROOT/script/pstart.sh -n -x -b --makefs $MAKEFS --bluestore-pmem /mnt/pmem2/zjq/block
# VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_ROOT/script/pstart.sh -n -x -b --makefs $MAKEFS --bluestore-pmem /mnt/pmem2/zjq/block --pmem-rocksdb
# VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 $CEPH_ROOT/script/pstart.sh -n -x -K --makefs $MAKEFS --bluestore-pmem /mnt/pmem2/zjq/block --kstore_path /mnt/pmem2/zjq/ceph/kstore 