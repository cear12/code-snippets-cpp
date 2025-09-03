template<class... Ts>
std::string to_string(Ts&&... ts)
{
    std::ostringstream oss;
    (oss << ... << std::forward<Ts>(ts));
    return oss.str();
}