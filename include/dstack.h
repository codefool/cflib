// dstack - disk based stack
//
// Simple LIFO stack data structure implemented with disk
// as storage.
//
#pragma once

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>

namespace libcf {

class DiskStack {
private:
    size_t          _rl;
    std::string     _fn;
    std::fstream    _fp;
    size_t          _rc;
    off_t           _eof;

protected:    
    DiskStack(size_t reclen, std::string *filename = nullptr);
    virtual ~DiskStack();
    long _push(const void* rec);
    long _pop(void* rec);
    const size_t _size() const;
    const std::string& _file_name() const;
    void _update_eof();
};

template <class R>
class dstack : protected DiskStack
{
public:    
    dstack()
    : DiskStack(sizeof(R))
    {}

    dstack(std::string basename)
    : DiskStack(sizeof(R), &basename)
    {}

    void push(const R& rec)
    {
        DiskStack::_push((void*)&rec);
    }

    R pop()
    {
        R ret;
        DiskStack::_pop((void*)&ret);
        return ret;
    }

    const std::string file_name() const
    {
        return DiskStack::_file_name();
    }

    const size_t size() const
    {
        return DiskStack::_size();
    }
};

} // end namespace libcf
