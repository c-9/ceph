#!/usr/bin/env bash

script_dir=$(dirname $0)
source $script_dir/env.sh

VSTART_DEST=/mnt/pmem0/ceph
VSTART_DEST=$VSTART_DEST $script_dir/src/stop.sh && rm -rf /mnt/pmem0/ceph/

