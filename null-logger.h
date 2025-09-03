Logger& GetNullLogger() noexcept
{
    static NullLogger null_logger{};
    return null_logger;
}

std::shared_ptr< Looger > makeNullLogger()
{
    return std::shared_ptr< Logger >( std::shared_ptr< void >{}, &GetNullLogger() );
}