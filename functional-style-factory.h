#ifndef FUNCTIONAL_STYLE_FACTORY_H
#define FUNCTIONAL_STYLE_FACTORY_H


#include <QDebug>

#include <functional>
#include <memory>


namespace functional_style_factory
{
    //
    struct IProduct
    {
        virtual void foo() = 0;
    };

    //
    struct ConcreteProduct : public IProduct
    {
        void foo()
        {
            qDebug() << "ConcreteProduct created";
        }
    };

    using Factory = std::function< std::unique_ptr< IProduct >() >;

    void Client( const Factory& makeProduct )
    {
        auto product = makeProduct();
        product->foo();
    }

    void test()
    {
        Client( []{ return std::make_unique< ConcreteProduct >(); } );
    }
}

#endif // FUNCTIONAL_STYLE_FACTORY_H
