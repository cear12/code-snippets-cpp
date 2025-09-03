// break NRVO (tested on MSVC 10.0)
std::string test()
{
    if (false) {
        return std::string("never get here");
    }

    std::string s = "Hi!";
    return s;
}