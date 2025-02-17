// DiskQueue
//
// A disk-based queue that can grow without bound
//
// This works like a FAT. There are two files: .idx and .dat, which
// contain the block allocation linked-list and the data file itself.
//
// The idx contains:
// - Starting recno provided (if any) when the queue was created.
// - Last recno assigned
// - The number of blocks currently being managed
// - Index of the first allocated block (BLOCK_HEAD)
// - Index of the first free block (BLOCK_FREE)
//
// The Index for each block contains:
// - disposition (either FREE or ALLOC)
// - fpos_t of the start of the block
// - fpos_t of the first unused rec in the block
//
//
//
//

#pragma once

#include <iostream>
#include <filesystem>
#include <fstream>
#include <list>
#include <mutex>

namespace libcf {

typedef uint64_t        dq_block_id_t;
typedef uint64_t        dq_rec_no_t;
typedef unsigned char * dq_data_t;

const dq_block_id_t BLOCK_NIL = -1;

extern const dq_rec_no_t MAX_BLOCK_SIZE;   // 256 MiB
extern const char * dq_naught;

#pragma pack(1)

struct IndexRec
{
    std::fpos_t _start;
    std::fpos_t _free;
    dq_block_id_t _next;
};

struct QueueRecPos
{
    dq_block_id_t _block_id;
    dq_rec_no_t   _rec_no;

    bool operator==(const QueueRecPos& o) const
    {
        return _block_id == o._block_id && _rec_no == o._rec_no;
    }
};

struct QueueHeader
{
    dq_rec_no_t   _block_size;
    dq_rec_no_t   _rec_len;
    dq_rec_no_t   _max_block_size;
    dq_rec_no_t   _recs_per_block;
    dq_rec_no_t   _rec_cnt;
    dq_block_id_t _block_cnt;
    dq_block_id_t _alloc_cnt;
    dq_block_id_t _free_cnt;
    QueueRecPos   _push;
    QueueRecPos   _pop;
};

#pragma pack()

class QueueFile
{
protected:
    std::string  _fspec;
    std::fstream _fp;
    std::mutex   _mtx;
public:
    QueueFile();
    virtual ~QueueFile();
    bool open();
    bool open(std::string fspec);
    std::istream& seekg( std::streamoff pos, std::ios_base::seekdir dir );
    std::istream& read( char *s, std::streamsize n );
    std::ostream& write( const char *s, std::streamsize n);
    void close();
    bool is_open() { return _fp.is_open(); }
    std::mutex& mtx();
};

typedef std::list<dq_block_id_t> BlockList;

class DiskQueue
{
private:
    std::string _path;
    std::string _name;
    QueueHeader _header;
    QueueFile   _idx;
    QueueFile   _dat;
    BlockList   _alloc;
    BlockList   _free;

public:
    DiskQueue(std::string path, std::string name, dq_rec_no_t reclen, dq_rec_no_t maxblocksize = MAX_BLOCK_SIZE);
    virtual ~DiskQueue();
    bool empty();
    void push(const dq_data_t data);
    bool pop(dq_data_t data);
    dq_rec_no_t size();
private:
    void write_index();
    void read_index();
};

template<class T>
class dq : public DiskQueue {
public:
    dq( std::string path, std::string name, dq_rec_no_t maxblocksize = MAX_BLOCK_SIZE )
    : DiskQueue(path, name, sizeof(T), maxblocksize)
    {}

    void push(T& data) {
        DiskQueue::push((dq_data_t)&data);
    };

    bool pop(T& data) {
        return DiskQueue::pop((dq_data_t)&data);
    }    
};

} // namespace libcf