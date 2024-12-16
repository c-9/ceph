/*
 * keyvalue db based on KVDK with reference to MemDB
 * Author: chunk, chunk@telos.top
 */

#include "KVDKStore.h"

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "KeyValueDB.h"
#include "common/debug.h"
#include "common/errno.h"
#include "common/perf_counters.h"
#include "include/buffer.h"
#include "include/buffer_raw.h"
#include "include/ceph_assert.h"
#include "include/compat.h"
#include "include/str_list.h"
#include "include/str_map.h"

#define dout_context g_ceph_context
#define dout_subsys ceph_subsys_kvdkstore
#undef dout_prefix
#define dout_prefix *_dout << "kvdk: "
#define dtrace dout(10)
#define dwarn dout(0)
#define dinfo dout(0)

using StringView = pmem::obj::string_view;
namespace fs = std::filesystem;

using ceph::bufferlist;
using ceph::bufferptr;
using ceph::decode;
using ceph::encode;

std::string KVDKStore::bufferlist_to_string(const bufferlist &bl) {
    std::string str;
    str.resize(bl.length());                   // Resize string to hold the data
    bl.begin().copy(bl.length(), str.data());  // Use iterator's copy method
    return str;
}

// Helper function to convert string to bufferlist
bufferlist KVDKStore::string_to_bufferlist(const std::string &str) {
    bufferlist bl;
    bl.append(str.data(), str.length());
    return bl;
}

void KVDKStore::split_key(const std::string &raw_key, std::string *prefix, std::string *key) {
    size_t pos = raw_key.find(KEY_DELIM, 0);
    ceph_assert(pos != std::string::npos);
    *prefix = raw_key.substr(0, pos);
    *key = raw_key.substr(pos + 1, raw_key.length());
}

std::string KVDKStore::make_key(const std::string &prefix, const std::string &value) {
    std::string out = prefix;
    out.push_back(KEY_DELIM);
    out.append(value);
    return out;
}

std::string KVDKStore::_get_data_fn() {
    return kvdk_path;
}

void KVDKStore::_parse_ops(const std::string &options) {
    if (!options.empty()) {
        std::list<std::string> ops;
        get_str_list(options, ",", ops);
        for (auto &op : ops) {
            std::list<std::string> kv;
            get_str_list(op, "=", kv);

            if (kv.size() != 2) {
                derr << __func__ << " Invalid option: " << op << dendl;
                continue;
            }
            if (kv.front() == "max_access_threads") {
                kvdk_configs.max_access_threads = std::stoi(kv.back());
            } else if (kv.front() == "pmem_file_size") {
                kvdk_configs.pmem_file_size = std::stoi(kv.back());
            } else if (kv.front() == "populate_pmem_space") {
                kvdk_configs.populate_pmem_space = std::stoi(kv.back());
            } else if (kv.front() == "pmem_block_size") {
                kvdk_configs.pmem_block_size = std::stoi(kv.back());
            } else if (kv.front() == "pmem_segment_blocks") {
                kvdk_configs.pmem_segment_blocks = std::stoi(kv.back());
            } else if (kv.front() == "hash_bucket_num") {
                kvdk_configs.hash_bucket_num = std::stoi(kv.back());
            } else if (kv.front() == "num_buckets_per_slot") {
                kvdk_configs.num_buckets_per_slot = std::stoi(kv.back());
            } else {
                derr << __func__ << " Invalid option: " << kv.front() << dendl;
            }
        }
    }
    assert(kvdk_configs.pmem_file_size >= kvdk_configs.pmem_block_size * kvdk_configs.pmem_segment_blocks * kvdk_configs.max_access_threads);
}

int KVDKStore::init(std::string option_str) {
    kvdk_options = option_str;
    {
        kvdk_configs.max_access_threads = 64;
        kvdk_configs.pmem_file_size = 32ull << 30;
        kvdk_configs.populate_pmem_space = 0;
        kvdk_configs.pmem_block_size = 64;
        kvdk_configs.pmem_segment_blocks = 2ull << 20;
        kvdk_configs.hash_bucket_num = 1ull << 27;
        kvdk_configs.num_buckets_per_slot = 1;
    }
    _parse_ops(option_str);
    return 0;
}

int KVDKStore::set_merge_operator(
    const std::string &prefix,
    std::shared_ptr<KeyValueDB::MergeOperator> mop) {
    merge_ops.push_back(std::make_pair(prefix, mop));
    return 0;
}

int KVDKStore::do_open(std::ostream &out, bool create) {
    dout(1) << __func__ << dendl;
    if (create) {
        if (fs::exists(kvdk_path)) {
            // KVDKRemovePMemContents(kvdk_path.c_str());
        }
    }
    kvdk::Status s;
    // pmem::obj::string_view path_view(kvdk_path);
    StringView path_view(kvdk_path);
    s = kvdk::Engine::Open(path_view, &kvdk_engine, kvdk_configs, stdout);

    assert(s == kvdk::Status::Ok);
    s = kvdk_engine->SortedCreate(kvdk_clname);
    std::cout << "KVDKSortedCreate: " << kvdk_clname << " Status:" << s << std::endl;
    assert(s == (create ? kvdk::Status::Ok : kvdk::Status::Existed));

    return 0;
}

int KVDKStore::open(std::ostream &out, const std::string &cfs) {
    if (!cfs.empty()) {
        ceph_abort_msg("Not implemented");
    }
    return do_open(out, false);
}

int KVDKStore::create_and_open(std::ostream &out, const std::string &cfs) {
    if (!cfs.empty()) {
        ceph_abort_msg("Not implemented");
    }
    return do_open(out, true);
}

KVDKStore::~KVDKStore() {
    close();
    dout(10) << __func__ << " Destroying KVDKStore instance: " << dendl;
}

void KVDKStore::close() {
    delete kvdk_engine;
    kvdk_engine = nullptr;
}

int KVDKStore::submit_transaction(KeyValueDB::Transaction t) {
    KVDKTransactionImpl *kt = static_cast<KVDKTransactionImpl *>(t.get());

    auto batch = kvdk_engine->WriteBatchCreate();

    for (auto &op : kt->get_ops()) {
        std::string key = make_key(op.second.first.first, op.second.first.second);
        std::string value = bufferlist_to_string(op.second.second);
        if (op.first == KVDKTransactionImpl::WRITE) {
            batch->SortedPut(kvdk_clname, key, value);
        } else if (op.first == KVDKTransactionImpl::DELETE) {
            batch->SortedDelete(kvdk_clname, key);
        } else if (op.first == KVDKTransactionImpl::MERGE) {
            // Handle merge operation if needed
            kvdk_op_t merge_op = op.second;
            _merge(merge_op);
        }
    }

    kvdk::Status s = kvdk_engine->BatchWrite(batch);
    assert(s == kvdk::Status::Ok);

    return 0;
}

int KVDKStore::submit_transaction_sync(KeyValueDB::Transaction tsync) {
    dtrace << __func__ << " " << dendl;
    submit_transaction(tsync);
    return 0;
}

int KVDKStore::transaction_rollback(KeyValueDB::Transaction t) {
    KVDKTransactionImpl *kt = static_cast<KVDKTransactionImpl *>(t.get());
    kt->clear();
    return 0;
}

void KVDKStore::KVDKTransactionImpl::set(
    const std::string &prefix, const std::string &k, const bufferlist &to_set_bl) {
    dtrace << __func__ << " " << prefix << " " << k << dendl;
    ops.push_back(make_pair(WRITE, std::make_pair(std::make_pair(prefix, k), to_set_bl)));
}

void KVDKStore::KVDKTransactionImpl::rmkey(const std::string &prefix,
                                           const std::string &k) {
    dtrace << __func__ << " " << prefix << " " << k << dendl;
    ops.push_back(make_pair(DELETE, std::make_pair(std::make_pair(prefix, k), bufferlist())));
}

void KVDKStore::KVDKTransactionImpl::rmkeys_by_prefix(const std::string &prefix) {
    KeyValueDB::Iterator it = db->get_iterator(prefix);
    for (it->seek_to_first(); it->valid(); it->next()) {
        rmkey(prefix, it->key());
    }
}

void KVDKStore::KVDKTransactionImpl::rm_range_keys(const std::string &prefix, const std::string &start, const std::string &end) {
    KeyValueDB::Iterator it = db->get_iterator(prefix);
    it->lower_bound(start);
    while (it->valid()) {
        if (it->key() >= end) {
            break;
        }
        rmkey(prefix, it->key());
        it->next();
    }
}

void KVDKStore::KVDKTransactionImpl::merge(
    const std::string &prefix, const std::string &key, const bufferlist &value) {
    dtrace << __func__ << " " << prefix << " " << key << dendl;
    ops.push_back(make_pair(MERGE, make_pair(std::make_pair(prefix, key), value)));
    return;
}

int KVDKStore::_setkey(kvdk_op_t &op) {
    std::string key = make_key(op.first.first, op.first.second);
    std::string value = bufferlist_to_string(op.second);

    kvdk::Status s = kvdk_engine->SortedPut(kvdk_clname, key, value);
    assert(s == kvdk::Status::Ok);

    return 0;
}

int KVDKStore::_rmkey(kvdk_op_t &op) {
    std::string key = make_key(op.first.first, op.first.second);

    kvdk::Status s = kvdk_engine->SortedDelete(kvdk_clname, key);
    assert(s == kvdk::Status::Ok);
    return 0;
}

std::shared_ptr<KeyValueDB::MergeOperator> KVDKStore::_find_merge_op(const std::string &prefix) {
    for (const auto &i : merge_ops) {
        if (i.first == prefix) {
            return i.second;
        }
    }
    dtrace << __func__ << " No merge op for " << prefix << dendl;
    return NULL;
}

int KVDKStore::_merge(kvdk_op_t &op) {
    std::string prefix = op.first.first;
    std::string key = make_key(op.first.first, op.first.second);
    bufferlist bl = op.second;

    // Find the merge operator for this prefix
    std::shared_ptr<MergeOperator> mop = _find_merge_op(prefix);
    assert(mop);

    // Get existing value if any
    std::string existing_value;
    kvdk::Status s = kvdk_engine->SortedGet(kvdk_clname, key, &existing_value);

    std::string new_value;
    if (s == kvdk::Status::NotFound) {
        // Merge nonexistent
        mop->merge_nonexistent(bl.c_str(), bl.length(), &new_value);
    } else {
        // Merge existing
        assert(s == kvdk::Status::Ok);
        mop->merge(existing_value.c_str(), existing_value.length(),
                   bl.c_str(), bl.length(), &new_value);
    }

    // Write the merged result
    s = kvdk_engine->SortedPut(kvdk_clname, key, new_value);
    assert(s == kvdk::Status::Ok);

    return 0;
}

bool KVDKStore::_get(const std::string &prefix, const std::string &k, bufferlist *out) {
    std::string key = make_key(prefix, k);
    std::string value;

    kvdk::Status s = kvdk_engine->SortedGet(kvdk_clname, key, &value);
    if (s != kvdk::Status::Ok) {
        return false;
    }

    out->clear();
    out->append(value);
    return true;
}

bool KVDKStore::_get_locked(const std::string &prefix, const std::string &k, bufferlist *out) {
    std::lock_guard<std::mutex> l(kvdk_lock);
    return _get(prefix, k, out);
}

int KVDKStore::get(const std::string &prefix, const std::string &key,
                   bufferlist *out) {
    int ret;
    if (_get(prefix, key, out)) {
        ret = 0;
    } else {
        ret = -ENOENT;
    }
    return ret;
}

int KVDKStore::get(const std::string &prefix, const std::set<std::string> &keys,
                   std::map<std::string, bufferlist> *out) {
    for (const auto &i : keys) {
        bufferlist bl;
        if (_get(prefix, i, &bl))
            out->insert(make_pair(i, bl));
    }
    return 0;
}
