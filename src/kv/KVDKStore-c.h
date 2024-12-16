/*
 * keyvalue db based on KVDK with reference to MemDB
 * Author: chunk, chunk@telos.top
 */

#ifndef CEPH_KVDB_KVDKSTORE_H
#define CEPH_KVDB_KVDKSTORE_H

#include <boost/scoped_ptr.hpp>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>

#include "KeyValueDB.h"
#include "include/btree_map.h"
#include "include/buffer.h"
#include "include/common_fwd.h"
#include "include/encoding.h"
#include "kvdk/engine.h"
#include "osd/osd_types.h"

#define KEY_DELIM '\0'

class KVDKStore : public KeyValueDB {
   public:
    typedef std::pair<std::pair<std::string, std::string>, ceph::bufferlist> kvdk_op_t;
    KVDKStore(CephContext *c, const std::string &path, void *p)
        : kvdk_cct(c),
          kvdk_path(path),
          kvdk_priv(p) {
        kvdk_configs = nullptr;
        kvdk_engine = nullptr;
        kvdk_clname = "default_kvdk_sortedcollection";
        kvdk_s_configs = nullptr;
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
        KVDKEngine *kvdk_engine;
        std::string kvdk_clname;
        KVDKSortedIterator *kvdk_iter;

       public:
        KVDKWholeSpaceIteratorImpl(KVDKEngine *kvdk_engine, std::string kvdk_clname) : kvdk_engine(kvdk_engine), kvdk_clname(kvdk_clname) {
            kvdk_iter = KVDKSortedIteratorCreate(kvdk_engine, kvdk_clname.c_str(), kvdk_clname.length(), NULL, NULL);
        }

        int seek_to_first(const std::string &k) override;
        int seek_to_last(const std::string &k) override;

        int seek_to_first() override;
        int seek_to_last() override;

        int upper_bound(const std::string &prefix,
                        const std::string &after) override;
        int lower_bound(const std::string &prefix, const std::string &to) override;
        bool valid() override;

        int next() override;
        int prev() override;
        int status() override { return 0; };

        std::string key() override;
        std::pair<std::string, std::string> raw_key() override;
        bool raw_key_is_prefixed(const std::string &prefix) override;
        ceph::bufferlist value() override;
        ~KVDKWholeSpaceIteratorImpl() override;
    };

    uint64_t get_estimated_size(std::map<std::string, uint64_t> &extra) override {
        return -EOPNOTSUPP;
    };

    int get_statfs(struct store_statfs_t *buf) override {
        return -EOPNOTSUPP;
    }

    WholeSpaceIterator get_wholespace_iterator(IteratorOpts opts = 0) override {
        return std::shared_ptr<KeyValueDB::WholeSpaceIteratorImpl>(
            new KVDKWholeSpaceIteratorImpl(kvdk_engine, kvdk_clname));
    }

   protected:
    CephContext *kvdk_cct;
    std::string kvdk_path;
    void *kvdk_priv;
    std::string kvdk_options;
    std::mutex kvdk_lock;  // TODO
    KVDKConfigs *kvdk_configs;
    KVDKEngine *kvdk_engine;
    std::string kvdk_clname;
    KVDKSortedCollectionConfigs *kvdk_s_configs;
};

#endif
