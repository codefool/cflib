#include <dq.h>

namespace libcf {

const dq_rec_no_t MAX_BLOCK_SIZE = 1024*1024*256;   // 256 MiB
const char * dq_naught = "\0";

QueueFile::QueueFile()
: _fp(nullptr)
{}

QueueFile::~QueueFile()
{
    close();
}

// return true if file existed
bool QueueFile::open()
{
    std::filesystem::path path(_fspec);
    bool exists = std::filesystem::exists(path);
    if ( !exists )
    {
        _fp.open( _fspec, std::ios::out );
        if ( !_fp.is_open() )
        {
            std::stringstream ss;
            ss << "Error " << errno << " creating file " << _fspec;
            throw std::runtime_error(ss.str());
        }
        _fp.close();
    }

    _fp.open( _fspec, std::ios::in|std::ios::out|std::ios::binary );
    if ( !_fp.is_open() )
    {
        std::stringstream ss;
        ss << "Error " << errno << " opening file " << _fspec;
        throw std::runtime_error(ss.str());
    }

    return exists;
}

bool QueueFile::open(std::string fspec)
{
    _fspec = fspec;
    return open();
}

std::istream& QueueFile::seekg( std::streamoff pos, std::ios_base::seekdir dir )
{
    return _fp.seekg( pos, dir );
}

std::istream& QueueFile::read( char *s, std::streamsize n )
{
    return _fp.read( s, n );
}

std::ostream& QueueFile::write( const char *s, std::streamsize n)
{
    return _fp.write( s, n );
}

void QueueFile::close()
{
    if (_fp.is_open())
        _fp.close();
}

std::mutex& QueueFile::mtx()
{
    return _mtx;
}

DiskQueue::DiskQueue(
    std::string path, 
    std::string name, 
    dq_rec_no_t reclen,
    dq_rec_no_t maxblocksize
) : _path(path), _name(name)
{
    // queue is in its own folder
    // path/name/name.idx and name.dat
    std::filesystem::path fpath(path);
    std::stringstream ss;
    ss << path << '/' << name;
    std::filesystem::create_directories(ss.str());
    ss << '/' << name;
    if ( _idx.open(ss.str() + ".idx") )
    {
        read_index();
    }
    else
    {
        _header._block_cnt      = 0;
        _header._rec_len        = reclen;
        _header._max_block_size = maxblocksize;
        _header._recs_per_block = maxblocksize / reclen;
        _header._rec_cnt        = 0;
        _header._block_size     = _header._recs_per_block * reclen;
        _header._alloc_cnt      = 0;
        _header._free_cnt       = 0;
        _header._push._block_id = BLOCK_NIL;
        _header._push._rec_no   = _header._recs_per_block;
        _header._pop ._block_id = BLOCK_NIL;
        _header._pop ._rec_no   = _header._recs_per_block;

        write_index();
    }

    _dat.open(ss.str() + ".dat");
}

DiskQueue::~DiskQueue()
{
    write_index();
    _dat.close();
}

void DiskQueue::push(const dq_data_t data)
{
    std::lock_guard<std::mutex> lock(_dat.mtx());
    if ( _header._push._rec_no == _header._recs_per_block )
    {
        // current block is full - get a fresh block
        if ( _free.empty() )
        {
            // create a new block at end of file
            off_t pos = _header._block_cnt * _header._block_size;
            _header._push._block_id = _header._block_cnt;
            _header._push._rec_no   = 0;
            _alloc.push_back(_header._block_cnt);
            _header._block_cnt++;
            _dat.seekg(pos, std::ios::beg);
            _dat.write(dq_naught, _header._block_size);
        }
        else
        {
            // pull the head off of the free chain
            dq_block_id_t block = _free.front();
            _free.pop_front();
            _alloc.push_back(block);
            _header._push._block_id = block;
            _header._push._rec_no   = 0;
        }
        write_index();
    }
    off_t pos = (_header._push._block_id * _header._block_size) +
                (_header._push._rec_no   * _header._rec_len);
    _dat.seekg( pos, std::ios::beg );
    _dat.write( reinterpret_cast<const char *>(data), _header._rec_len );
    if ( _header._pop._block_id == BLOCK_NIL )
        _header._pop = _header._push;
    _header._push._rec_no++;
    _header._rec_cnt++;
}

bool DiskQueue::pop(dq_data_t data)
{
    // pop record off the top of the queue
    // if last record in the block, move block to end of
    // free chain
    std::lock_guard<std::mutex> lock(_dat.mtx());
    if ( empty() )
        return false;
    if ( _header._pop._rec_no == _header._recs_per_block )
    {
        // put this block on the free chain,
        // setup next block (if any)
        if ( _header._pop._block_id != BLOCK_NIL )
        {
            _alloc.pop_front();
            _free.push_back(_header._pop._block_id);
        }
        if ( _alloc.empty() )
        {
            _header._pop._block_id = BLOCK_NIL;
            _header._pop._rec_no   = _header._recs_per_block;
        }
        else
        {
            _header._pop._block_id = _alloc.front();
            _header._pop._rec_no   = 0;
        }
        write_index();
    }

    off_t pos = (_header._pop._block_id * _header._block_size) +
                (_header._pop._rec_no   * _header._rec_len);
    _dat.seekg( pos, std::ios::beg );
    _dat.read( reinterpret_cast<char *>(data), _header._rec_len );
    _header._pop._rec_no++;
    _header._rec_cnt--;

    // TODO: if empty at this point, we should release all allocated disk space
    // if ( empty() ) {
    //  ... reset everything to first block and free the others ...
    // }

    return true;
}

void DiskQueue::write_index()
{
    std::lock_guard<std::mutex> lock(_idx.mtx());
    if ( !_idx.is_open() )
        _idx.open();
    _header._alloc_cnt = _alloc.size();
    _header._free_cnt  = _free .size();
    _idx.seekg( 0, std::ios::beg );
    _idx.write( reinterpret_cast<const char *>(&_header), sizeof(QueueHeader) );
    // write the alloc chain
    for ( auto id : _alloc )
        _idx.write( reinterpret_cast<const char *>(&id), sizeof(id) );
    for ( auto id : _free )
        _idx.write( reinterpret_cast<const char *>(&id), sizeof(id) );
    _idx.close();
}

void DiskQueue::read_index()
{
    std::lock_guard<std::mutex> lock(_idx.mtx());
    if ( !_idx.is_open() )
        _idx.open();
    _idx.seekg( 0, std::ios::beg );
    _idx.read( reinterpret_cast<char *>(&_header), sizeof(QueueHeader) );        
    // write the alloc chain
    for ( int idx(0); idx < _header._alloc_cnt; ++idx )
    {
        BlockList::value_type id;
        _idx.read( reinterpret_cast<char *>(&id), sizeof(id) );
        _alloc.push_back(id);
    }
    for ( int idx(0); idx < _header._free_cnt; ++idx )
    {
        BlockList::value_type id;
        _idx.read( reinterpret_cast<char *>(&id), sizeof(id) );
        _free.push_back( id );
    }
    _idx.close();
}

} // namespace libcf