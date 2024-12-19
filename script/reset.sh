#!/usr/bin/env bash

current_dir=$(dirname $0)
source $current_dir/env.sh

PMEM_DIR=/mnt/pmem2/zjq
# PMEM_DIR=/home/zjq/chunk/bench
VSTART_DEST=$PMEM_DIR/ceph
VSTART_DEST=$VSTART_DEST $current_dir/pstop.sh && rm -rf $PMEM_DIR/ceph/

MAKEFS=1
CONFIG=1  # default configuration
POPULATE=0
CACHE=1

if [ $# -gt 0 ]; then
    MAKEFS=$1
fi
if [ $# -gt 1 ]; then
    CONFIG=$2
fi

if [ $# -gt 2 ]; then
    POPULATE=$3
fi

if [ $# -gt 3 ]; then
    CACHE=$4
fi

if [ $POPULATE -eq 1 ]; then
    POPULATE='--pmem-populate'
else
    POPULATE=''
fi

if [ $CACHE -eq 0 ]; then
    CACHE='--bluestore-no-metacache'
else
    CACHE=''
fi

case $CONFIG in
    1)
        VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 nrun $CEPH_ROOT/script/pstart.sh -n -x -K --makefs $MAKEFS $POPULATE $CACHE --kstore_path $PMEM_DIR/ceph/kstore
        ;;
    2)
        VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 nrun $CEPH_ROOT/script/pstart.sh -n -x -b --makefs $MAKEFS $POPULATE $CACHE
        ;;
    3)
        VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 nrun $CEPH_ROOT/script/pstart.sh -n -x -b --makefs $MAKEFS $POPULATE $CACHE --pmem-rocksdb
        ;;
    4)
        VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 nrun $CEPH_ROOT/script/pstart.sh -n -x -b --makefs $MAKEFS $POPULATE $CACHE --bluestore-pmem $PMEM_DIR/block
        ;;
    5)
        VSTART_DEST=$VSTART_DEST MDS=1 MON=1 OSD=1 RGW=0 MGR=0 nrun $CEPH_ROOT/script/pstart.sh -n -x -b --makefs $MAKEFS $POPULATE $CACHE --bluestore-pmem $PMEM_DIR/block --pmem-rocksdb
        ;;
esac