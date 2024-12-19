#!/usr/bin/env bash
current_dir=$(dirname $0)
source $current_dir/env.sh

rsync -vhra $CEPH_ROOT/. chunk@chunk-legion:/home/chunk/workspace/git/git-own/ceph/ --include='**.gitignore' --exclude='/.git' --filter=':- .gitignore' --delete-after
