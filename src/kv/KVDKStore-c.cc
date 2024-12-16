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

namespace fs = std::filesystem;

using std::cerr;
using std::ostream;
using std::string;
using std::vector;

using ceph::bufferlist;
using ceph::bufferptr;
using ceph::decode;
using ceph::encode;

static void split_key(const std::string &raw_key, std::string *prefix, std::string *key) {
    size_t pos = raw_key.find(KEY_DELIM, 0);
    ceph_assert(pos != std::string::npos);
    *prefix = raw_key.substr(0, pos);
    *key = raw_key.substr(pos + 1, raw_key.length());
}

static string make_key(const std::string &prefix, const std::string &value) {
    string out = prefix;
    out.push_back(KEY_DELIM);
    out.append(value);
    return out;
}

static bufferlist to_bufferlist(std::string &in) {
    bufferlist bl;
    bl.append(bufferptr((char *)in.c_str(), in.length()));
    return bl;
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
            // if (kv.front() == "max_access_threads") {
            //     kvdk_configs->rep.max_access_threads = std::stoi(kv.back());
            // } else if (kv.front() == "pmem_file_size") {
            //     kvdk_configs->rep.pmem_file_size = std::stoi(kv.back());
            // } else if (kv.front() == "pmem_block_size") {
            //     kvdk_configs->rep.pmem_block_size = std::stoi(kv.back());
            // } else if (kv.front() == "hash_bucket_num") {
            //     kvdk_configs->rep.hash_bucket_num = std::stoi(kv.back());
            // } else {
            //     derr << __func__ << " Invalid option: " << kv.front() << dendl;
            // }
        }
    }

    /*  max_access_threads >= pmem_segment_blocks * pmem_block_size * max_access_threads */
    KVDKSetConfigs(kvdk_configs,
                   64,          /* uint64_t max_access_threads 64*/
                   32ull << 30, /* uint64_t pmem_file_size (64ULL << 30)*/
                   0u,          /* unsigned char populate_pmem_space true*/
                   64u,         /* uint32_t pmem_block_size 64*/
                   2ull << 20,  /* uint64_t pmem_segment_blocks 2 * 1024 * 1024*/
                   1ull << 27,  /* uint64_t hash_bucket_num (1 << 27)*/
                   1            /* uint32_t num_buckets_per_slot 1*/
    );
    assert((32ull << 30) >= 64 * 64 * (2ull << 20));
}

int KVDKStore::init(std::string option_str) {
    KVDKStatus s;
    kvdk_options = option_str;

    if (kvdk_configs) {
        KVDKDestroyConfigs(kvdk_configs);
    }
    kvdk_configs = KVDKCreateConfigs();
    _parse_ops(option_str);

    // if (kvdk_engine) {
    //     KVDKCloseEngine(kvdk_engine);
    // }
    // s = KVDKOpen(kvdk_path.c_str(), kvdk_configs, stdout, &kvdk_engine);
    // assert(s == Ok);

    // if (kvdk_s_configs) {
    //     KVDKDestroySortedCollectionConfigs(kvdk_s_configs);
    // }
    // kvdk_s_configs = KVDKCreateSortedCollectionConfigs();

    // s = KVDKSortedDestroy(kvdk_engine, kvdk_clname.c_str(), kvdk_clname.length());
    // assert(s == Ok);
    // s = KVDKSortedCreate(kvdk_engine, kvdk_clname.c_str(), kvdk_clname.length(), kvdk_s_configs);
    // assert(s == Ok);

    return 0;
}

int KVDKStore::set_merge_operator(
    const string &prefix,
    std::shared_ptr<KeyValueDB::MergeOperator> mop) {
    merge_ops.push_back(std::make_pair(prefix, mop));
    return 0;
}

int KVDKStore::do_open(ostream &out, bool create) {
    dout(1) << __func__ << dendl;
    if (create) {
        if (fs::exists(kvdk_path)) {
            KVDKRemovePMemContents(kvdk_path.c_str());
        }
    }
    KVDKStatus s;

    if (!kvdk_configs) {
        kvdk_configs = KVDKCreateConfigs();
    }
    s = KVDKOpen(kvdk_path.c_str(), kvdk_configs, stdout, &kvdk_engine);
    assert(s == Ok);
    kvdk_s_configs = KVDKCreateSortedCollectionConfigs();
    s = KVDKSortedCreate(kvdk_engine, kvdk_clname.c_str(), kvdk_clname.length(), kvdk_s_configs);
    std::cout << "KVDKSortedCreate: " << kvdk_clname << " Status:" << s << std::endl;
    assert(s == (create ? Ok : Existed));

    return 0;
}

int KVDKStore::open(ostream &out, const std::string &cfs) {
    if (!cfs.empty()) {
        ceph_abort_msg("Not implemented");
    }
    return do_open(out, false);
}

int KVDKStore::create_and_open(ostream &out, const std::string &cfs) {
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
    // KVDKStatus s = KVDKSortedDestroy(kvdk_engine, kvdk_clname.c_str(), kvdk_clname.length());
    // assert(s == Ok);
    KVDKDestroySortedCollectionConfigs(kvdk_s_configs);
    kvdk_s_configs = nullptr;
    KVDKDestroyConfigs(kvdk_configs);
    kvdk_configs = nullptr;
    KVDKCloseEngine(kvdk_engine);
    kvdk_engine = nullptr;
}

int KVDKStore::submit_transaction(KeyValueDB::Transaction t) {
    KVDKTransactionImpl *kt = static_cast<KVDKTransactionImpl *>(t.get());
    dtrace << __func__ << " " << kt->get_ops().size() << dendl;
    for (auto &op : kt->get_ops()) {
        if (op.first == KVDKTransactionImpl::WRITE) {
            kvdk_op_t set_op = op.second;
            _setkey(set_op);
        } else if (op.first == KVDKTransactionImpl::MERGE) {
            kvdk_op_t merge_op = op.second;
            _merge(merge_op);
        } else {
            kvdk_op_t rm_op = op.second;
            ceph_assert(op.first == KVDKTransactionImpl::DELETE);
            _rmkey(rm_op);
        }
    }

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
    ops.push_back(make_pair(WRITE, std::make_pair(std::make_pair(prefix, k),
                                                  to_set_bl)));
}

void KVDKStore::KVDKTransactionImpl::rmkey(const std::string &prefix,
                                           const std::string &k) {
    dtrace << __func__ << " " << prefix << " " << k << dendl;
    ops.push_back(make_pair(DELETE,
                            std::make_pair(std::make_pair(prefix, k),
                                           bufferlist())));
}

void KVDKStore::KVDKTransactionImpl::rmkeys_by_prefix(const string &prefix) {
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
    // std::lock_guard<std::mutex> l(kvdk_lock);
    std::string key = make_key(op.first.first, op.first.second);
    bufferlist bl = op.second;

    // KVDKStatus s = KVDKPut(kvdk_engine, key.c_str(), key.length(), (const char *)bl.c_str(), bl.length(), KVDKCreateWriteOptions());
    KVDKStatus s = KVDKSortedPut(kvdk_engine, kvdk_clname.c_str(), kvdk_clname.length(), key.c_str(), key.length(), (const char *)bl.c_str(), bl.length());
    assert(s == Ok);

    return 0;
}

int KVDKStore::_rmkey(kvdk_op_t &op) {
    // std::lock_guard<std::mutex> l(kvdk_lock);
    std::string key = make_key(op.first.first, op.first.second);

    // KVDKStatus s = KVDKDelete(kvdk_engine, key.c_str(), key.length());
    KVDKStatus s = KVDKSortedDelete(kvdk_engine, kvdk_clname.c_str(), kvdk_clname.length(), key.c_str(), key.length());
    assert(s == Ok);
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
    // std::lock_guard<std::mutex> l(kvdk_lock);
    std::string prefix = op.first.first;
    std::string key = make_key(op.first.first, op.first.second);
    bufferlist bl = op.second;
    int64_t bytes_adjusted = bl.length();

    /*
     *  find the operator for this prefix
     */
    std::shared_ptr<MergeOperator> mop = _find_merge_op(prefix);
    ceph_assert(mop);

    /*
     * call the merge operator with value and non value
     */
    KVDKStatus s;
    // KVDKWriteOptions *write_option = KVDKCreateWriteOptions();
    bufferlist bl_old;
    if (_get(op.first.first, op.first.second, &bl_old) == false) {
        /*
         * Merge non existent.
         */
        std::string new_val;
        mop->merge_nonexistent(bl.c_str(), bl.length(), &new_val);
        // s = KVDKPut(kvdk_engine, key.c_str(), key.length(), new_val.c_str(), new_val.length(), write_option);
        s = KVDKSortedPut(kvdk_engine, kvdk_clname.c_str(), kvdk_clname.length(), key.c_str(), key.length(), new_val.c_str(), new_val.length());
    } else {
        /*
         * Merge existing.
         */
        std::string new_val;
        mop->merge(bl_old.c_str(), bl_old.length(), bl.c_str(), bl.length(), &new_val);
        // s = KVDKPut(kvdk_engine, key.c_str(), key.length(), new_val.c_str(), new_val.length(), write_option);
        s = KVDKSortedPut(kvdk_engine, kvdk_clname.c_str(), kvdk_clname.length(), key.c_str(), key.length(), new_val.c_str(), new_val.length());
        bl_old.clear();
    }
    assert(s == Ok);
    return 0;
}

bool KVDKStore::_get(const std::string &prefix, const std::string &k, bufferlist *out) {
    std::string key = make_key(prefix, k);
    char *value;
    size_t value_len;

    // KVDKStatus s = KVDKGet(kvdk_engine, key.c_str(), key.length(), &value_len, &value);
    KVDKStatus s = KVDKSortedGet(kvdk_engine, kvdk_clname.c_str(), kvdk_clname.length(), key.c_str(), key.length(), &value_len, &value);
    if (s != Ok) {
        return false;
    }
    out->append(bufferptr(value, value_len));
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
                   std::map<string, bufferlist> *out) {
    for (const auto &i : keys) {
        bufferlist bl;
        if (_get(prefix, i, &bl))
            out->insert(make_pair(i, bl));
    }
    return 0;
}

bool KVDKStore::KVDKWholeSpaceIteratorImpl::valid() {
    return KVDKSortedIteratorValid(kvdk_iter);
}

std::string KVDKStore::KVDKWholeSpaceIteratorImpl::key() {
    size_t key_len;
    char *key_res;
    KVDKSortedIteratorKey(kvdk_iter, &key_res, &key_len);
    std::string key(key_res, key_len);
    std::string p, k;
    split_key(key, &p, &k);
    return k;
}

std::pair<string, string> KVDKStore::KVDKWholeSpaceIteratorImpl::raw_key() {
    size_t key_len;
    char *key_res;
    KVDKSortedIteratorKey(kvdk_iter, &key_res, &key_len);
    std::string key(key_res, key_len);
    std::string p, k;
    split_key(key, &p, &k);
    return {p, k};
}

bool KVDKStore::KVDKWholeSpaceIteratorImpl::raw_key_is_prefixed(const std::string &prefix) {
    size_t key_len;
    char *key_res;
    KVDKSortedIteratorKey(kvdk_iter, &key_res, &key_len);
    std::string key(key_res, key_len);
    std::string p, k;
    split_key(key, &p, &k);
    return (p == prefix);
}

bufferlist KVDKStore::KVDKWholeSpaceIteratorImpl::value() {
    size_t val_len;
    char *val_res;
    KVDKSortedIteratorValue(kvdk_iter, &val_res, &val_len);
    std::string value(val_res, val_len);
    return to_bufferlist(value);
}

int KVDKStore::KVDKWholeSpaceIteratorImpl::next() {
    KVDKSortedIteratorNext(kvdk_iter);
    if (KVDKSortedIteratorValid(kvdk_iter))
        return 0;
    else
        return -1;
}

int KVDKStore::KVDKWholeSpaceIteratorImpl::prev() {
    KVDKSortedIteratorPrev(kvdk_iter);
    if (KVDKSortedIteratorValid(kvdk_iter))
        return 0;
    else
        return -1;
}

/*
 * First key >= to given key, if key is null then first key in btree.
 */
int KVDKStore::KVDKWholeSpaceIteratorImpl::seek_to_first(const std::string &k) {
    KVDKSortedIteratorSeek(kvdk_iter, k.c_str(), k.length());
    if (KVDKSortedIteratorValid(kvdk_iter))
        return 0;
    else
        return -1;
}

int KVDKStore::KVDKWholeSpaceIteratorImpl::seek_to_last(const std::string &k) {
    KVDKSortedIteratorSeek(kvdk_iter, k.c_str(), k.length());
    if (KVDKSortedIteratorValid(kvdk_iter))
        return 0;
    else
        return -1;
}

int KVDKStore::KVDKWholeSpaceIteratorImpl::seek_to_first() {
    KVDKSortedIteratorSeekToFirst(kvdk_iter);
    if (KVDKSortedIteratorValid(kvdk_iter))
        return 0;
    else
        return -1;
}

int KVDKStore::KVDKWholeSpaceIteratorImpl::seek_to_last() {
    KVDKSortedIteratorSeekToLast(kvdk_iter);
    if (KVDKSortedIteratorValid(kvdk_iter))
        return 0;
    else
        return -1;
}

KVDKStore::KVDKWholeSpaceIteratorImpl::~KVDKWholeSpaceIteratorImpl() {
    KVDKSortedIteratorDestroy(kvdk_engine, kvdk_iter);
}

int KVDKStore::KVDKWholeSpaceIteratorImpl::upper_bound(const std::string &prefix,
                                                       const std::string &after) {
    dtrace << "upper_bound " << prefix.c_str() << after.c_str() << dendl;
    std::string k = make_key(prefix, after);
    KVDKSortedIteratorSeek(kvdk_iter, k.c_str(), k.length());
    if (KVDKSortedIteratorValid(kvdk_iter))
        return 0;
    else
        return -1;
}

int KVDKStore::KVDKWholeSpaceIteratorImpl::lower_bound(const std::string &prefix,
                                                       const std::string &to) {
    dtrace << "lower_bound " << prefix.c_str() << to.c_str() << dendl;
    std::string k = make_key(prefix, to);
    KVDKSortedIteratorSeek(kvdk_iter, k.c_str(), k.length());
    if (KVDKSortedIteratorValid(kvdk_iter))
        return 0;
    else
        return -1;
}
