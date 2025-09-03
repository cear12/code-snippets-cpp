struct IFoo // abstract interface
{
    virtual int getValue() = 0;
};

int main()
{
    // 1
    auto createVoldemortType = [] // use lambda auto return type
    {
        struct Voldemort // localy defined type
        {
            int getValue() { return 21; }
        };
        return Voldemort{}; // return unnameable type
    };

    // 2
    auto fooFactory = []
    {
        struct VoldeFoo: IFoo  // local Voldemort type derived from IFoo
        {
            int getValue() override { return 42; }
        };
        return VoldeFoo{};
    };

    auto foo = fooFactory();    

  auto unnameable = createVoldemortType();  // must use auto!
  decltype(unnameable) unnameable2;         // but, can be used with decltype
  return unnameable.getValue() +            // can use unnameable API
         unnameable2.getValue();            // returns 42
}

/*
struct IFoo // abstract interface
{
    virtual int getValue() = 0;
};