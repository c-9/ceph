#!/usr/bin/env bash
rsync -vhra . chunk@chunk-legion:/home/chunk/workspace/git/git-own/ceph/ --include='**.gitignore' --exclude='/.git' --filter=':- .gitignore' --delete-after
