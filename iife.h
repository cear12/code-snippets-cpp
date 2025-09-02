#ifndef IIFE_H
#define IIFE_H

https://habr.com/ru/articles/470265/#IIFE
https://www.cppstories.com/2016/11/iife-for-complex-initialization/


#include <QDebug>

/*
    Immediately-Invoked Function Expression
    Немедленно вызываемое функциональное выражение.

    Зачем это надо?
    Ну например, как в приведенном коде для того, чтобы инициализировать константу результатом нетривиального
    вычисления и не засорить при этом область видимости лишними переменными и функциями.
*/
namespace iife
{
    void test()
    {
        static constexpr int a = 12;
        static constexpr int b = 24;
        const auto foo = []
        {
            if( a > b )
            {
                return 36*( a - b );
            }
            return ( b > 345 ) ? ( a << 3 + b*35 ) : 77;
        }();

        qDebug() << foo;
    };
};


#endif // IIFE_H
