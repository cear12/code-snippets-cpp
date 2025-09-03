/*
    variadic templates with a base case / with termination function
*/
void print()
{
    // termination function
}

template <typename E, typename... Types>
void print(const E Arg, const Types&... args)
{
    cout << Arg << endl;
    print(args...);
}

/*
    variadic templates without a base case / wihtout termination function
*/
template<typename... Args> void print(Args&&... args)
{
    char dummy[sizeof...(Args)] =
    {
        ((std::cout << args <<'\n'), 0)...
    };
    (void) dummy; // cause compilator warning
}