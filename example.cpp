#include "tyre/tyre.h"
#include <cassert>
#include <iostream>

struct duck_visitors
{
    static constexpr tyre::visitor_list visitors
    {
        tyre::visitor<struct look, bool(std::any const&)>([](auto const& x) { return x.look(); }),
        tyre::visitor<struct walk, void(std::any&, int)>([](auto& x, int n) { x.walk(n); }),
        tyre::visitor<struct quack, void(std::any&)>([](auto& x) { x.quack(); }),
    };
};
using any_duck = tyre::any<duck_visitors>;

void test(any_duck duck)
{
    bool const ok = tyre::visit<look>(duck);
    tyre::visit<walk>(duck, 42);
    tyre::visit<quack>(duck);

    assert(ok);
}

int main()
{
    struct my_duck
    {
        bool look() const { std::cout << "o_O\n"; return true; }
        void walk(int n) { std::cout << "Walked " << n << " steps.\n"; }
        void quack() { std::cout << "Quack!\n"; }
    };

    test(my_duck());
}
