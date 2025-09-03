int main (int argc, char* argv[])
{
    // 1
    for (int i=1; i < argc; ++i)
    {
            if (strcmp(argv[i], "-v")==0)
            {
                    verbose = true;
            }
            if (strcmp(argv[i], "-t") ==0)
            {
                    test = true;
            }
    }

    for (int i=1; i < argc; ++i)
    {
            if (argv[i][0] != '-')
                    processFile(argv[i]);
    }

    // 2
    std::vector<std::string_view> args{ argv, argv + argc };

    for (auto arg : args) 
    {
        std::cout << arg << '\n';
    }    
}