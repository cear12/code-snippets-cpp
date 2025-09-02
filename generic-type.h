#ifndef GENERIC_TYPE_H
#define GENERIC_TYPE_H


#include <QDebug>


namespace generic_type
{
    enum Type { TypeA, TypeB };

    template< int > struct GenericType final {};

    static GenericType< TypeA > GenericTypeA;
    static GenericType< TypeB > GenericTypeB;

    void print( GenericType< TypeA > )
    {
        qDebug() << "TypeA";
    }

    void print( GenericType< TypeB > )
    {
        qDebug() << "TypeB";
    }

    void test()
    {
        // style a
        print( GenericType< TypeA >{} );
        print( GenericType< TypeB >{} );

        // style b
        print( GenericTypeA );
        print( GenericTypeB );
    }
}


#endif // GENERIC_TYPE_H
