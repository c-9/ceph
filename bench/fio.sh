#!/usr/bin/env bash

current_dir=$(dirname $0)
source $current_dir/../env.sh

function mytime () {
     /usr/bin/time -f "real %es user %Us sys %Ss CPU %P\n(%Xtext+%Ddata %Mmax)k\t%Iinputs+%Ooutputs\n(%Fmajor+%Rminor)pagefaults\t%Wswaps" "$@"
}

# VSTART_DEST=$CEPH_ROOT
VSTART_DEST=/mnt/pmem0/ceph
# VSTART_DEST=/mnt/pmem1/ceph1
echo "================status================="
$CEPH_BUILD_ROOT/bin/ceph -c $VSTART_DEST/ceph.conf -s

echo "================create================="

echo "================bench================="
sudo sync && echo 3 | sudo tee /proc/sys/vm/drop_caches && sudo sync
mytime $CEPH_BUILD_ROOT/bin/fio $current_dir/blue.fio


