// db - simple database for chessgen
//
// This is not a general-purpose tool, but a bare-bones and (hopefully)
// freakishly fast way to collect positions and check for collissions
// without thrashing the disk.
//
// Use the high two-digits of MD5 as hash into 256 buckets.
// Files are binary with fixed-length records.
//
//
#pragma once
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <thread>

#include "md5.h"

namespace libcf {

#define BUCKET_ID_WIDTH 3
const unsigned short BUCKET_LO  = 0;
const unsigned short BUCKET_HI  = 1 << (4 * BUCKET_ID_WIDTH );

typedef unsigned char   uchar;
typedef uchar         * ucharptr;
typedef const ucharptr  ucharptr_c;
typedef std::shared_ptr<uchar[]> BuffPtr;

// dht_comparitor returns true if lhs == rhs
typedef bool (*dht_comparitor)(const void *lhs, const void *rhs, size_t keysize);
// dht_hasher computes a hash of key, and returns the first n chars
typedef std::string (*dht_hasher)(const void *key, size_t keysize, size_t n);

class NAUGHT_TYPE{};

class DiskHashTable {
    struct BucketFile {
        struct file_guard {
            BucketFile& _bf;
            bool        _was_open;
            file_guard(BucketFile& bf) : _bf(bf) {
                _was_open = _bf._fp != nullptr;
                if ( !_was_open )
                    _bf.open();
            }

            ~file_guard() {
                if ( !_was_open )
                    _bf.close();
            }
        };

        static std::map<size_t, BuffPtr> buff_map;

        std::mutex     _mtx;
        std::FILE*     _fp;
        std::string    _fspec;
        size_t         _keylen;
        size_t         _vallen;
        size_t         _reccnt;
        size_t         _reclen;
        dht_comparitor _compfunc;


        BucketFile( std::string fspec,
                    size_t key_len,
                    size_t val_len = 0,
                    dht_comparitor comp_func = default_comparitor);
        ~BucketFile();
        bool open();
        bool close();
        off_t search(ucharptr_c key, ucharptr   val = nullptr);
        bool  append(ucharptr_c key, ucharptr_c val = nullptr);
        bool  update(ucharptr_c key, ucharptr_c val = nullptr);
        bool  read(size_t recno, ucharptr key, ucharptr val);

        size_t seek();
        BuffPtr get_file_buff();

        off_t search_nolock(ucharptr_c key, ucharptr val = nullptr);
        bool  append_nolock(ucharptr_c key, ucharptr_c val = nullptr);
        bool  update_nolock(ucharptr_c key, ucharptr_c val = nullptr);
    };

public:
    typedef std::shared_ptr<BucketFile>          BucketFilePtr;
    typedef std::map<std::string, BucketFilePtr> BucketFilePtrMap;
    typedef BucketFilePtrMap::const_iterator     BucketFilePtrMapCItr;

protected:
    BucketFilePtrMap   fp_map;
    size_t             keylen;
    size_t             vallen;
    size_t             reclen;
    std::string        path;
    std::string        name;
    size_t             reccnt;
    dht_comparitor     compfunc;
    dht_hasher         hashfunc;

public:
    DiskHashTable();

    virtual ~DiskHashTable();

    bool open(
        const std::string  path_name,
        const std::string  base_name,
        size_t             key_len,
        size_t             val_len = 0,
        dht_comparitor     comp_func = default_comparitor,
        dht_hasher         hash_func = default_hasher);

    size_t size() const {return reccnt;}
    bool search(ucharptr_c key, ucharptr val = nullptr);
    bool insert(ucharptr_c key, ucharptr_c val = nullptr);
    bool append(ucharptr_c key, ucharptr_c val = nullptr);
    bool update(ucharptr_c key, ucharptr_c val = nullptr);

    static std::string get_bucket_fspec(
        const std::string path,
        const std::string base,
        const std::string bucket,
        bool *exists = nullptr);

private:
    std::string calc_bucket_id( ucharptr_c key );
    BucketFilePtr get_bucket( const std::string& bucket, bool must_exist = false );
    std::string get_bucket_fspec( const std::string& bucket, bool* exists = nullptr );
protected:
    static bool default_comparitor( const void * lhs, const void * rhs, size_t keylen );
    static std::string default_hasher(const void * key, size_t keylen, size_t hashlen );
};

template <class K, class V = NAUGHT_TYPE>
class dht : public DiskHashTable
{
public:
    typedef std::pair<K,V> KeyVal;

    dht(
        const std::string  path_name,
        const std::string  base_name,
        dht_comparitor     comp_func = default_comparitor,
        dht_hasher         hash_func = default_hasher
    ) {
        size_t vsize = (typeid(V) == typeid(NAUGHT_TYPE)) ? 0 : sizeof(V);
        DiskHashTable::open(path_name, base_name, sizeof(K), vsize, comp_func, hash_func);
    }

    bool search(K& key)
    {
        return DiskHashTable::search((ucharptr_c)&key, nullptr);
    }
    bool search(K& key, V& val)
    {
        return DiskHashTable::search((ucharptr_c)&key, (ucharptr_c)&val);
    }
    bool insert(K& key)
    {
        return DiskHashTable::insert((ucharptr_c)&key, nullptr);
    }
    bool insert(K& key, V& val)
    {
        return DiskHashTable::insert((ucharptr_c)&key, (ucharptr_c)&val);
    }
    bool append(K& key)
    {
        return DiskHashTable::append((ucharptr_c)&key, nullptr);
    }
    bool append(K& key, V& val)
    {
        return DiskHashTable::append((ucharptr_c)&key, (ucharptr_c)&val);
    }
    bool update(K& key)
    {
        return DiskHashTable::update((ucharptr_c)&key, nullptr);
    }
    bool update(K& key, V& val)
    {
        return DiskHashTable::update((ucharptr_c)&key, (ucharptr_c)&val);
    }
};

} // namespace libcf