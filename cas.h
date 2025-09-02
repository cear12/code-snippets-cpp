#ifndef CAS_H
#define CAS_H


#include <QDebug>


/*
    Compare And Swap. Сравнение с обменом.
    Это атомарная инструкция с тремя аргументами: атомарная переменная или адрес в памяти, ожидаемое значение, новое значение.
    Если и только если значение переменной совпадает с ожидаемым, переменная получает новое значение и инструкция завершается успешно.
    CAS либо просто возвращает булево значение (и тогда ее можно называть Compare And Set),
    либо в случае неудачи возвращает еще и текущее значение первого аргумента.
*/
namespace cas
{
    bool func( int* addr, int& expected, int new_value )
    {
        if( *addr != expected )
        {
            expected = *addr;
            return false;
        }
        *addr = new_value;
        return true;
    }

    void test()
    {
        int addr = 23;
        int expected = 23;
        qDebug() << func( &addr, expected, 34 );
        qDebug() << addr;
    }
}


#endif // CAS_H


std::atomic<int> counter(0);

void increment_counter(int num_iterations) {
    for (int i = 0; i < num_iterations; ++i) {
        int old_value = counter.load();
        int new_value;
        do {
            new_value = old_value + 1;
        } while (!counter.compare_exchange_strong(old_value, new_value));
    }
}

