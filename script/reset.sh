#!/usr/bin/env bash

current_dir=$(dirname $0)
source $current_dir/env.sh

VSTART_DEST=/mnt/pmem2/zjq/ceph
VSTART_DEST=$VSTART_DEST $current_dir/pstop.sh && rm -rf /mnt/pmem2/zjq/ceph/

MAKEFS=1
CONFIG=1  # default configuration

if [ $# -gt 0 ]; then
    MAKEFS=$1
fi
if [ $# -gt 1 ]; then
    CONFIG=$2
fi

case $CONFIG in
    1)
        VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 nrun $CEPH_ROOT/script/pstart.sh -n -x -b --makefs $MAKEFS
        ;;
    2)
        VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 nrun $CEPH_ROOT/script/pstart.sh -n -x -b --makefs $MAKEFS --pmem-rocksdb
        ;;
    3)
        VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 nrun $CEPH_ROOT/script/pstart.sh -n -x -b --makefs $MAKEFS --bluestore-pmem /mnt/pmem2/zjq/block
        ;;
    4)
        VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 nrun $CEPH_ROOT/script/pstart.sh -n -x -b --makefs $MAKEFS --bluestore-pmem /mnt/pmem2/zjq/block --pmem-rocksdb
        ;;
    5)
        VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 nrun $CEPH_ROOT/script/pstart.sh -n -x -K --makefs $MAKEFS --kstore_path /mnt/pmem2/zjq/ceph/kstore
        ;;
esac