#!/usr/bin/env bash

current_dir=$(dirname $0)
source $current_dir/env.sh

VSTART_DEST=/mnt/pmem2/ceph
VSTART_DEST=$VSTART_DEST $CEPH_ROOT/script/pstop.sh && rm -rf /mnt/pmem2/ceph/

