#pragma once


#include <QDebug>
#include <QThread>

#include <future>
#include <iostream>
#include <thread>

namespace sync_wrapper
{
    //
    template< typename T >
    class AsyncWrapper
    {
    public:
        AsyncWrapper( std::function< T() > func )
        {
            qDebug() << "start ync";
            result_ = std::async( std::move( func ) );
        }
        ~AsyncWrapper()
        {
            result_.get();
            qDebug() << "finish ync";
        }

        std::future< T > result_;
    };

    //
    void callerFunc()
    {
        qDebug() << "start caller func";

        std::function< void() > f = []() -> void
        {
            for( int i = 0; i < 10; ++i )
            {
                qDebug() << i;
                QThread::sleep( 1);
            }
        };

        AsyncWrapper< void > yncWrapper( std::move( f ) );
        qDebug() << "finish caller func";
    }
}
