#include <iostream>
#include <string>

#include "dstack.h"

struct Record {
    char name[128];
};

libcf::dstack<Record> stack(std::string("/tmp/bogus"));

int main()
{
    std::cout << stack.file_name() << ' ' << stack.size() << std::endl;
    Record r;

    for ( int i = 1; i < 20; ++i )
    {
        std::sprintf(r.name, "%030d", i);
        stack.push(r);
    }

    for ( int i = 1; i < 15; ++i )
    {
        r = stack.pop();
    }

    for ( int i = 1; i < 20; ++i )
    {
        std::sprintf(r.name, "%030d", i + 100);
        stack.push(r);
    }

    while (stack.size())
    {
        r = stack.pop();
        std::cout << r.name << std::endl;
    }



    return 0;
}