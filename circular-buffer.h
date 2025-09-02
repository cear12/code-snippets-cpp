#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H


#include <boost/circular_buffer.hpp>

#include <memory>
#include <mutex>

#include <QDebug>

namespace circular_buffer
{
    template <class T>
    class CircularBuffer
    {
    public:
        explicit CircularBuffer(size_t size)
            : buf_(std::unique_ptr<T[]>(new T[size])),
            max_size_(size)
        {

        }

        void put(T item)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            buf_[head_] = item;

            if(full_)
            {
                tail_ = (tail_ + 1) % max_size_;
            }

            head_ = (head_ + 1) % max_size_;

            full_ = head_ == tail_;
        }

        T get()
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if(empty())
            {
                return T();
            }

            //Read data and advance the tail (we now have a free space)
            auto val = buf_[tail_];
            full_ = false;
            tail_ = (tail_ + 1) % max_size_;

            return val;
        }

        void reset()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            head_ = tail_;
            full_ = false;
        }

        bool empty() const
        {
            //if head and tail are equal, we are empty
            return (!full_ && (head_ == tail_));
        }

        bool full() const
        {
            //If tail is ahead the head by 1, we are full
            return full_;
        }

        size_t capacity() const
        {
            return max_size_;
        }

        size_t size() const
        {
            size_t size = max_size_;

            if(!full_)
            {
                if(head_ >= tail_)
                {
                    size = head_ - tail_;
                }
                else
                {
                    size = max_size_ + head_ - tail_;
                }
            }

            return size;
        }

    private:
        std::mutex mutex_;
        std::unique_ptr<T[]> buf_;
        size_t head_ = 0;
        size_t tail_ = 0;
        const size_t max_size_;
        bool full_ = 0;
    };

    void testNative()
    {
        CircularBuffer<uint32_t> circle(10);
        qDebug() << "\n === CPP Circular buffer check ===\n";
        qDebug() << "Size: " << circle.size() << "Capacity: " << circle.capacity();

        uint32_t x = 1;
        qDebug() << "Put 1, val: " << x;
        circle.put(x);

        x = circle.get();
        qDebug() << "Popped: " << x;

        qDebug() << "Empty: ", circle.empty();

        qDebug() << "Adding " << (circle.capacity() - 1) << "values";
        for(uint32_t i = 0; i < circle.capacity() - 1; i++)
        {
            circle.put(i);
        }

        circle.reset();

        qDebug() << "Full: " << circle.full();

        qDebug() << "Adding " << circle.capacity() << "values";
        for(uint32_t i = 0; i < circle.capacity(); i++)
        {
            circle.put(i);
        }

        qDebug() << "Full: " << circle.full();

        qDebug() << "Reading back values: ";
        while(!circle.empty())
        {
            qDebug() << circle.get();
        }
        qDebug() << "\n";

        qDebug() << "Adding 15 values\n";
        for(uint32_t i = 0; i < circle.size() + 5; i++)
        {
            circle.put(i);
        }

        qDebug() << "Full: " << circle.full();

        qDebug() << "Reading back values: ";
        while(!circle.empty())
        {
            qDebug() << circle.get();
        }
        qDebug() << "\n";

        qDebug() << "Empty: " << circle.empty();
        qDebug() << "Full: " << circle.full();
    }

    void testBoost()
    {
        // Create a circular buffer with a capacity for 3 integers.
        boost::circular_buffer<int> cb(3);

        // Insert threee elements into the buffer.
        cb.push_back(1);
        cb.push_back(2);
        cb.push_back(3);

        int a = cb[0];  // a == 1
        int b = cb[1];  // b == 2
        int c = cb[2];  // c == 3

        // The buffer is full now, so pushing subsequent
        // elements will overwrite the front-most elements.

        cb.push_back(4);  // Overwrite 1 with 4.
        cb.push_back(5);  // Overwrite 2 with 5.

        // The buffer now contains 3, 4 and 5.
        a = cb[0];  // a == 3
        b = cb[1];  // b == 4
        c = cb[2];  // c == 5

        // Elements can be popped from either the front or the back.
        cb.pop_back();  // 5 is removed.
        cb.pop_front(); // 3 is removed.

        // Leaving only one element with value = 4.
        int d = cb[0];  // d == 4
    }

    void test()
    {
        testNative();
        testBoost();
    }
}


#endif // CIRCULAR_BUFFER_H
