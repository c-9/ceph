/*
 * keyvalue db based on KVDK with reference to MemDB
 * Author: chunk, chunk@telos.top
 */

#ifndef CEPH_KVDB_KVDKSTORE_H
#define CEPH_KVDB_KVDKSTORE_H

#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <libpmemobj++/string_view.hpp>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "KeyValueDB.h"
#include "include/btree_map.h"
#include "include/buffer.h"
#include "include/common_fwd.h"
#include "include/encoding.h"
#include "kvdk/engine.hpp"
#include "osd/osd_types.h"

using StringView = pmem::obj::string_view;
#define KEY_DELIM '\0'

class KVDKStore : public KeyValueDB {
   public:
    typedef std::pair<std::pair<std::string, std::string>, ceph::bufferlist> kvdk_op_t;
    enum BackendType {
        SORTED_COLLECTION = 0,
        HASH_COLLECTION = 1
    };

    KVDKStore(CephContext *c, const std::string &path, void *p)
        : kvdk_cct(c),
          kvdk_path(path),
          kvdk_priv(p),
          backend_type(SORTED_COLLECTION) {
        kvdk_engine = nullptr;
        kvdk_clname = "default_kvdk_collection";
        {
            kvdk_configs.max_access_threads = 64;
            kvdk_configs.pmem_file_size = 32ull << 30;
            kvdk_configs.populate_pmem_space = 0;
            kvdk_configs.pmem_block_size = 64;
            kvdk_configs.pmem_segment_blocks = 2ull << 20;
            kvdk_configs.hash_bucket_num = 1ull << 27;
            kvdk_configs.num_buckets_per_slot = 1;
        }
    }

    ~KVDKStore() override;
    int set_merge_operator(const std::string &prefix, std::shared_ptr<MergeOperator> mop) override;

    std::shared_ptr<MergeOperator> _find_merge_op(const std::string &prefix);

    static int _test_init(const std::string &dir) { return 0; };

    class KVDKTransactionImpl : public KeyValueDB::TransactionImpl {
       public:
        enum op_type { WRITE = 1,
                       MERGE = 2,
                       DELETE = 3 };

       private:
        std::vector<std::pair<op_type, kvdk_op_t>> ops;
        KVDKStore *db;

        bool key_is_prefixed(const std::string &prefix,
                             const std::string &full_key);

       public:
        const std::vector<std::pair<op_type, kvdk_op_t>> &get_ops() { return ops; };

        void set(const std::string &prefix, const std::string &key,
                 const ceph::bufferlist &val) override;
        using KeyValueDB::TransactionImpl::set;
        void rmkey(const std::string &prefix, const std::string &k) override;
        using KeyValueDB::TransactionImpl::rmkey;
        void rmkeys_by_prefix(const std::string &prefix) override;
        void rm_range_keys(const std::string &prefix, const std::string &start,
                           const std::string &end) override;

        void merge(const std::string &prefix, const std::string &key,
                   const ceph::bufferlist &value) override;
        void clear() { ops.clear(); }
        explicit KVDKTransactionImpl(KVDKStore *_db) : db(_db) { ops.clear(); }
        ~KVDKTransactionImpl() override {};
    };

   private:
    int transaction_rollback(KeyValueDB::Transaction t);
    bool _get(const std::string &prefix, const std::string &k, ceph::bufferlist *out);
    bool _get_locked(const std::string &prefix, const std::string &k, ceph::bufferlist *out);
    std::string _get_data_fn();
    void _parse_ops(const std::string &options);
    /*
     * Transaction states.
     */
    int _merge(const std::string &k, ceph::bufferptr &bl);
    int _merge(kvdk_op_t &op);
    int _setkey(kvdk_op_t &op);
    int _rmkey(kvdk_op_t &op);

    // Helper functions
    static void split_key(const std::string &raw_key, std::string *prefix, std::string *key);
    static std::string make_key(const std::string &prefix, const std::string &value);
    static std::string bufferlist_to_string(const bufferlist &bl);
    static bufferlist string_to_bufferlist(const std::string &str);

   public:
    int init(std::string option_str = "") override;
    int do_open(std::ostream &out, bool create);
    int open(std::ostream &out, const std::string &cfs = "") override;
    int create_and_open(std::ostream &out, const std::string &cfs = "") override;
    void close() override;
    using KeyValueDB::create_and_open;

    KeyValueDB::Transaction get_transaction() override {
        return std::shared_ptr<KVDKTransactionImpl>(new KVDKTransactionImpl(this));
    }

    int submit_transaction(Transaction) override;
    int submit_transaction_sync(Transaction) override;

    int get(const std::string &prefix, const std::set<std::string> &key,
            std::map<std::string, ceph::bufferlist> *out) override;

    int get(const std::string &prefix, const std::string &key,
            ceph::bufferlist *out) override;

    using KeyValueDB::get;

    class KVDKWholeSpaceIteratorImpl : public KeyValueDB::WholeSpaceIteratorImpl {
       protected:
        kvdk::Engine *kvdk_engine;
        std::string kvdk_clname;

       public:
        KVDKWholeSpaceIteratorImpl(kvdk::Engine *engine, const std::string &clname)
            : kvdk_engine(engine), kvdk_clname(clname) {}

        virtual ~KVDKWholeSpaceIteratorImpl() override {}
        virtual int status() override { return 0; }
        virtual bool valid() override = 0;
        virtual std::string key() override = 0;
        virtual std::pair<std::string, std::string> raw_key() override = 0;
        virtual bool raw_key_is_prefixed(const std::string &prefix) override = 0;
        virtual bufferlist value() override = 0;
        virtual int next() override = 0;
        virtual int prev() override = 0;
        virtual int seek_to_first() override = 0;
        virtual int seek_to_last() override = 0;
        virtual int seek_to_first(const std::string &k) override = 0;
        virtual int seek_to_last(const std::string &k) override = 0;
        virtual int upper_bound(const std::string &prefix, const std::string &after) override = 0;
        virtual int lower_bound(const std::string &prefix, const std::string &to) override = 0;
    };

    class KVDKSortedIteratorImpl : public KVDKWholeSpaceIteratorImpl {
       private:
        kvdk::SortedIterator *iter;

       public:
        KVDKSortedIteratorImpl(kvdk::Engine *engine, const std::string &clname)
            : KVDKWholeSpaceIteratorImpl(engine, clname) {
            iter = kvdk_engine->SortedIteratorCreate(kvdk_clname);
        }

        bool valid() override { return iter->Valid(); }

        std::string key() override {
            std::string raw_key = iter->Key();
            std::string p, k;
            split_key(raw_key, &p, &k);
            return k;
        }

        std::pair<std::string, std::string> raw_key() override {
            std::string raw_key = iter->Key();
            std::string p, k;
            split_key(raw_key, &p, &k);
            return {p, k};
        }

        bool raw_key_is_prefixed(const std::string &prefix) override {
            std::string raw_key = iter->Key();
            std::string p, k;
            split_key(raw_key, &p, &k);
            return p == prefix;
        }

        bufferlist value() override {
            std::string val = iter->Value();
            return string_to_bufferlist(val);
        }

        int next() override {
            iter->Next();
            return iter->Valid() ? 0 : -1;
        }

        int prev() override {
            iter->Prev();
            return iter->Valid() ? 0 : -1;
        }

        int seek_to_first() override {
            iter->SeekToFirst();
            return iter->Valid() ? 0 : -1;
        }

        int seek_to_last() override {
            iter->SeekToLast();
            return iter->Valid() ? 0 : -1;
        }

        int seek_to_first(const std::string &k) override {
            iter->Seek(k);
            return iter->Valid() ? 0 : -1;
        }

        int seek_to_last(const std::string &k) override {
            iter->Seek(k);
            return iter->Valid() ? 0 : -1;
        }

        int upper_bound(const std::string &prefix, const std::string &after) override {
            std::string key = make_key(prefix, after);
            iter->Seek(key);
            return iter->Valid() ? 0 : -1;
        }

        int lower_bound(const std::string &prefix, const std::string &to) override {
            std::string key = make_key(prefix, to);
            iter->Seek(key);
            return iter->Valid() ? 0 : -1;
        }

        ~KVDKSortedIteratorImpl() override {
            kvdk_engine->SortedIteratorRelease(iter);
        }
    };

    class KVDKHashIteratorImpl : public KVDKWholeSpaceIteratorImpl {
       private:
        kvdk::HashIterator *iter;

       public:
        KVDKHashIteratorImpl(kvdk::Engine *engine, const std::string &clname)
            : KVDKWholeSpaceIteratorImpl(engine, clname) {
            iter = kvdk_engine->HashIteratorCreate(kvdk_clname);
        }

        bool valid() override { return iter->Valid(); }

        std::string key() override {
            std::string raw_key = iter->Key();
            std::string p, k;
            split_key(raw_key, &p, &k);
            return k;
        }

        std::pair<std::string, std::string> raw_key() override {
            std::string raw_key = iter->Key();
            std::string p, k;
            split_key(raw_key, &p, &k);
            return {p, k};
        }

        bool raw_key_is_prefixed(const std::string &prefix) override {
            std::string raw_key = iter->Key();
            std::string p, k;
            split_key(raw_key, &p, &k);
            return p == prefix;
        }

        bufferlist value() override {
            std::string val = iter->Value();
            return string_to_bufferlist(val);
        }

        int next() override {
            iter->Next();
            return iter->Valid() ? 0 : -1;
        }

        int prev() override {
            iter->Prev();
            return iter->Valid() ? 0 : -1;
        }

        int seek_to_first() override {
            iter->SeekToFirst();
            return iter->Valid() ? 0 : -1;
        }

        int seek_to_last() override {
            iter->SeekToLast();
            return iter->Valid() ? 0 : -1;
        }
        /*
        N.B. KVDK hash does not support seek to key, so we use seek to first and last instead.
        */
        int seek_to_first(const std::string &k) override {
            return seek_to_first();
        }

        int seek_to_last(const std::string &k) override {
            return seek_to_last();
        }

        int upper_bound(const std::string &prefix, const std::string &after) override {
            return seek_to_last();
        }

        int lower_bound(const std::string &prefix, const std::string &to) override {
            return seek_to_first();
        }

        ~KVDKHashIteratorImpl() override {
            kvdk_engine->HashIteratorRelease(iter);
        }
    };

    WholeSpaceIterator get_wholespace_iterator(IteratorOpts opts = 0) override {
        if (backend_type == SORTED_COLLECTION) {
            return std::shared_ptr<KeyValueDB::WholeSpaceIteratorImpl>(
                new KVDKSortedIteratorImpl(kvdk_engine, kvdk_clname));
        } else {
            return std::shared_ptr<KeyValueDB::WholeSpaceIteratorImpl>(
                new KVDKHashIteratorImpl(kvdk_engine, kvdk_clname));
        }
    }

    uint64_t get_estimated_size(std::map<std::string, uint64_t> &extra) override {
        return -EOPNOTSUPP;
    };

    int get_statfs(struct store_statfs_t *buf) override {
        return -EOPNOTSUPP;
    }

   protected:
    CephContext *kvdk_cct;
    std::string kvdk_path;
    void *kvdk_priv;
    std::string kvdk_options;
    std::mutex kvdk_lock;  // TODO

    kvdk::Configs kvdk_configs;
    kvdk::Engine *kvdk_engine;
    std::string kvdk_clname;
    BackendType backend_type;

    // Helper methods for different backends
    int _setkey_sorted(kvdk_op_t &op);
    int _setkey_hash(kvdk_op_t &op);
    int _rmkey_sorted(kvdk_op_t &op);
    int _rmkey_hash(kvdk_op_t &op);
    bool _get_sorted(const std::string &prefix, const std::string &k, bufferlist *out);
    bool _get_hash(const std::string &prefix, const std::string &k, bufferlist *out);
};

#endif
