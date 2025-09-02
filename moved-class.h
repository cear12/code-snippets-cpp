#ifndef MOVED_CLASS_H
#define MOVED_CLASS_H


#include <QDebug>


namespace moved_class
{
    //
    class T
    {
    public:
        /* нужно реализовать */
        T()
        {
            qDebug() << "default c-r";
        }

        T( const T& )
        {
            qDebug() << "copy c-r";
        }

        T& operator=( T&& t )
        {
            qDebug() << "move op-r";
            return *this;
        }

        /* остальное вывести из этих */
        T( T&& t )
            : T()
        {
            qDebug() << "move с-r";
            *this = static_cast< T&& >( t );
        }

        T& operator=( const T& t )
        {
            qDebug() << "copy op-r";
            return *this = T( t );
        }
    };

    //
    void test()
    {
        auto f = []( T&& t )
        {
            auto result = std::move( t );
        };

        f( T() );
    }
}


#endif // MOVED_CLASS_H
