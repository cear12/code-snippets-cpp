#ifndef NON_MEMBER_FRIEND_INTERFACE_H
#define NON_MEMBER_FRIEND_INTERFACE_H


#include <QDebug>


namespace non_member_friend_interface
{
    namespace ns
    {
        class foo
        {
        public:
            void member()
            {
                qDebug() << "it's member";
            }
        private:
            // Private data
        };

        void non_member(foo obj)
        {
            obj.member();
        }
    }

    void test()
    {
        ns::foo obj;
        non_member(obj); /* без уазания ns, т.к. ищется в том же ns, что и аргумент */
    }
}

#endif // NON_MEMBER_FRIEND_INTERFACE_H
