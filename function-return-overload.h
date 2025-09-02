#ifndef FUNCTION_RETURN_OVERLOAD_H
#define FUNCTION_RETURN_OVERLOAD_H


#include <QDebug>

#include <iostream>


/*
    Argument-Dependent Lookup. Поиск, зависящий от аргументов.
    Он же поиск Кёнига — в честь Andrew Koenig.
    Это набор правил для разрешения неквалифицированных имен функций (т. е. имен без оператора ::),
    дополнительный к обычному разрешению имен. Упрощенно: имя функции ищется в пространствах имен, относящихся к ее аргументам
    (это пространство, содержащее тип аргумента, сам тип, если это класс, все его предки и т.п.).
*/
namespace function_return_overload
{
    struct Foo
    {
        int a = 10;
        int b = 20;
    };

    struct Func
    {
        Func(){}
        inline operator std::string() const
        {
            return "10";
        }
        inline operator unsigned() const
        {
            return 2;
        }

        inline operator Foo() const
        {
            return { 100, 200 };
        }
    };

    void test()
    {
        std::cout << ( std::string )Func() << std::endl;
        std::cout << ( unsigned )Func() << std::endl;

        auto val = Foo( Func() ).a;

        std::cout << val << std::endl;
    }
}


#endif // FUNCTION_RETURN_OVERLOAD_H
