#ifndef AS_CAST_H
#define AS_CAST_H


#include <QDebug>

/*
    Альтернатива dynamic_cast

    "ручное RTTI" (Manual RTTI)
    "псевдо-RTTI"
*/
namespace _cast
{
    //
    struct Item
    {
        virtual struct ItemA* ItemA() { return nullptr; }
        virtual struct ItemB* ItemB() { return nullptr; }
        virtual struct ItemC* ItemC() { return nullptr; }
    };

    struct ItemA : public Item
    {
        ItemA* ItemA() override { return this; }
    };

    //
    void test()
    {
        if( Item* item = new ItemA )
        {
            if( item->asItemA() )
            {
                qDebug() << "real ItemA";
            }
            else
            {
                qDebug() << "not real ItemA";
            }
        }
    }
}


#endif // DYNAMIC_CAST_H
