#ifndef FUNCTION_SIZE_H
#define FUNCTION_SIZE_H


#include <QDebug>


namespace function_size
{
    static int test_proc()
    {
        return 1;
    }

    static void size_proc()
    {}

    static size_t test_proc_size()
    {
        return (uintptr_t)((uintptr_t)(void*)size_proc - (uintptr_t)(void*)test_proc);
    }

    void test()
    {
        qDebug() << "function size is: " << test_proc_size();
    }
}

#endif // FUNCTION_SIZE_H
