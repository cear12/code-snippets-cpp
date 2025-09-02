#ifndef BOOST_COPY_FILTERED_H
#define BOOST_COPY_FILTERED_H


namespace boost_copy_filtered
{
    void test()
    {
        using boost::copy;
        using boost::adapters::filtered;
        auto ByGender = []{ .... }
        copy( people | filtered( ByGender() ), tostdout )
    }
}


#endif // BOOST_COPY_FILTERED_H
