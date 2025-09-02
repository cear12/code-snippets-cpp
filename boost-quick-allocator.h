#ifndef BOOST_QUICK_ALLOCATOR_H
#define BOOST_QUICK_ALLOCATOR_H


#define BOOST_SP_USE_QUICK_ALLOCATOR

#include <QDebug>

#include <boost/shared_ptr.hpp>
#include <ctime>

namespace boost_quick_allocator
{
    void test()
    {
        boost::shared_ptr<int> p;
        std::time_t then = std::time(nullptr);
        for (int i = 0; i < 10000000; ++i)
          p.reset(new int{i});
        std::time_t now = std::time(nullptr);
        qDebug() << now - then << '\n';
    }
}

#endif // BOOST_QUICK_ALLOCATOR_H
