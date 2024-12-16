#!/usr/bin/env bash
current_dir=$(dirname $0)
source $current_dir/env.sh

mode=RelWithDebInfo
# mode=Debug
./do_cmake.sh -DCMAKE_INSTALL_PREFIX=/usr/local/ceph \
    -DCMAKE_FIND_DEBUG_MODE=OFF \
    -DCMAKE_BUILD_TYPE=$mode \
    -DWITH_MGR=OFF \
    -DWITH_MGR_DASHBOARD_FRONTEND=OFF \
    -DWITH_SYSTEM_BOOST=ON \
    -DBoost_NO_BOOST_CMAKE=ON \
    -DWITH_SYSTEM_ZSTD=ON \
    -DWITH_SYSTEM_ROCKSDB=ON \
    -DWITH_SYSTEM_PMDK=ON \
    -DWITH_BLUESTORE_PMEM=ON \
    -DWITH_PMEM_ROCKSDB=ON \
    -DWITH_RBD_RWL=ON \
    -DWITH_SYSTEM_KVDK=ON \
    -DWITH_SYSTEM_LIBURING=OFF \
    -DWITH_FIO=ON \
    -DALLOCATOR=tcmalloc \
    -DPython3_ROOT_DIR=$PYTHON_ROOT \
    -DPython3_EXECUTABLE=$PYTHON_ROOT/bin/python \
    -DROCKSDB_ROOT=$ROCKSDB_ROOT \
    -DKVDK_ROOT=$KVDK_ROOT \
    -DBOOST_ROOT=$BOOST_ROOT \
    -DPMEM_ROOT=$PMEM_ROOT \
    -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer"
