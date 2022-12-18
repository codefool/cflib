#include <filesystem>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>

#include "dstack.h"

namespace libcf {

DiskStack::DiskStack(size_t reclen, std::string *filename)
: _rl(reclen)
{
    if (filename == nullptr) {
        // generate a name
        char filename[] = "/tmp/dstackXXXXXX";
        mkstemp(filename);
        _fn = std::string(filename);
    } else {
        _fn = *filename;
    }

    if ( !std::filesystem::exists( _fn ) )
    {
        _fp.open( _fn, std::ios::out );
        if ( !_fp.is_open() )
        {
            std::stringstream ss;
            ss << "Error " << errno << " creating file " << _fn;
            throw std::runtime_error(ss.str());
        }
        _eof = 0;
        _update_eof();
        _fp.close();
    }

    _fp.open( _fn, std::ios::in|std::ios::out|std::ios::binary );
    if ( !_fp.is_open() )
    {
        std::stringstream ss;
        ss << "Error " << errno << " opening file " << _fn;
        throw std::runtime_error(ss.str());
    }

    _fp.seekg(0, std::ios::beg);
    _fp.read((char *)&_eof, sizeof(_eof));
    _rc = _eof / _rl;
}   

DiskStack::~DiskStack()
{
    if ( _fp.is_open() )
    {
        _fp.close();
    }
}

long DiskStack::_push(const void* rec)
{
    _fp.seekg(_eof, std::ios::beg);
    _fp.write((const char *)rec, _rl);
    _rc++;
    _eof += _rl;
    _update_eof();
    return _rc;
}

long DiskStack::_pop(void * rec)
{
    if ( _rc == 0 )
        return -1;

    _rc--;
    _eof -= _rl;
    _fp.seekg(_eof, std::ios::beg);
    _fp.read( (char *)rec, _rl);
    _update_eof();
    return _rc;
}

void DiskStack::_update_eof()
{
    _fp.seekg(0, std::ios::beg);
    _fp.write((char *)&_eof, sizeof(_eof));
}

const std::string& DiskStack::_file_name() const
{
    return _fn;
}

const size_t DiskStack::_size() const
{
    return _rc;
}


} // end namespace libcf
