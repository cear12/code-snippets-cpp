sizeof(begin(std::declval<T>())) != sizeof(char);
    instead of
sizeof(begin( T{} )) != sizeof(char);


template <bool condition, typename Type>
struct EnableIf;

template <typename Type>
struct EnableIf<true, Type>
{
    using type = Type;
};

emplate <typename T, typename U>
struct IsSame
{
    static constexpr bool value = false;
};
 
template <typename T>
struct IsSame<T, T>
{
    static constexpr bool value = true;
};

template <typename T>
EnableIf<true, void>::type
printContainer(T container)
{
    ...
}

template <typename T>
typename EnableIf<!IsSame<T, int>::value, void>::type
printContainer(T container)
{
    std::cout << "Values:{ ";
    for(auto value : container)
        std::cout << value << " ";
    std::cout << "}\n";
}

template <typename T, typename = typename T::iterator> 
void printContainer(T container)
{
    std::cout << "Values:{ ";
    for(auto value : container)
        std::cout << value << " ";
    std::cout << "}\n";
}

/*
1. static_assert( std::is_container_type::, )
   static_assert(std::is_scalar<typename Container::value_type>::value, "Тип элемента должен быть скалярным");
2. template <typename Container, typename = typename Container::iterator>
2'.template <typename Container, typename = std::enable_if_t<std::is_scalar<typename Container::value_type>::value>>
3. template <typename T>
decltype(begin(std::declval<T>()), end(std::declval<T>()), void())
print(T container)
{
    std::cout << "Values:{ ";
    for(auto value : container)
        std::cout << value << " ";
    std::cout << "}\n";
}
template <typename Container, typename = std::enable_if_t<is_container<Container>::value>>
4. constexpr bool isScalarType = std::is_scalar_v<typename Container::value_type>;

    if constexpr (isScalarType) { // or
    if constexpr (std::is_scalar_v<typename Container::value_type>) {
        ..
    }
*/
