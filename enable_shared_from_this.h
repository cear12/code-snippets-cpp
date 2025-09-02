#ifndef ENABLE_SHARED_FROM_THIS_H
#define ENABLE_SHARED_FROM_THIS_H


#include <QDebug>

#include <memory>

/*
Обычная реализация enable_shared_from_this сводится к хранению слабой ссылки (к примеру, std::weak_ptr) на this.
Конструкторы std::shared_ptr способны обнаружить наличие базового класса enable_shared_from_this и разделить владение
объектом с уже существующими указателями std::shared_ptr, вместо того, чтобы решить, что указатель больше ничем не управляется.

Обратите внимание, что перед вызовом shared_from_this для объекта t, должен существовать std::shared_ptr, который владеет t.
Также нужно отметить, что enable_shared_from_this предоставляет безопасную альтернативу выражениям наподобие std::shared_ptr<T>(this),
которые могут привести к уничтожению this более одного раза несколькими владеющими указателями, которые не обладают информацией о наличии друг друга.
*/
namespace enable_shared_from_this
{
    struct Good: std::enable_shared_from_this< Good >
    {
        std::shared_ptr< Good > create() {
            return shared_from_this();
        }
    };

    struct Bad
    {
        std::shared_ptr< Bad > create()
        {
            return std::shared_ptr< Bad >( this );
        }
    };

    void test()
    {
        // good
        std::shared_ptr< Good > gp1( new Good );
        std::shared_ptr< Good > gp2 = gp1->create();
        qDebug() << "gp2.use_count() = " << gp2.use_count() << '\n';

        // bad
        std::shared_ptr< Bad > bp1( new Bad );
        std::shared_ptr< Bad > bp2 = bp1->create();
        qDebug()  << "bp2.use_count() = " << bp2.use_count() << '\n';

    }
}


#endif // ENABLE_SHARED_FROM_THIS_H
