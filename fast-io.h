/*
    ios_base - for all threads
    ios      - for this thread
*/

static const auto fastIO = []()
{
    std::ios_base::sync_with_stdio( 0 );
    std::cin.tie( 0 );

    return 0;
}();