#ifndef BUILDER_H
#define BUILDER_H


#include <QDebug>

#include <vector>
#include <iostream>


namespace builder
{
    class Foo
    {
      public:
        class Builder;
        Foo(int prop1, bool prop2, bool prop3, std::vector<int> prop4)
          : prop1{prop1}, prop2{prop2}, prop3{prop3}, prop4{prop4}
        { }
        int prop1;
        bool prop2;
        bool prop3;
        std::vector<int> prop4;
    };
    class Foo::Builder
    {
      public:
        Builder& set_prop1(int value)
        {
            qDebug() << "set_prop1";
            prop1 = value;
            return *this;
        };
        Builder& set_prop2(bool value)
        {
            qDebug() << "set_prop2";
            prop2 = value;
            return *this;
        };
        Builder& set_prop3(bool value)
        {
            qDebug() << "set_prop3";
            prop3 = value;
            return *this;
        };
        Builder& set_prop4(std::vector<int> value)
        {
            qDebug() << "set_prop4";
            prop4 = value;
            return *this;
        };
        Foo build() const
        {
          return Foo{prop1, prop2, prop3, prop4};
        }
      private:
        int prop1 = 0;
        bool prop2 = false;
        bool prop3 = false;
        std::vector<int> prop4 = {};
    };

    void test()
    {
        Foo f = Foo::Builder{}.set_prop1(5)
                              .set_prop3(true)
                              .build();

        std::cout << "hello";
    }
}


#endif // BUILDER_H
