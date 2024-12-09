#!/usr/bin/env bash

current_dir=$(dirname $0)
source $current_dir/env.sh

VSTART_DEST=/mnt/pmem0/ceph
VSTART_DEST=$VSTART_DEST $current_dir/src/stop.sh && rm -rf /mnt/pmem0/ceph/

