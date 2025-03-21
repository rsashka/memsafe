#include <map>
#include <vector>
#include <algorithm>
#include "memsafe.h"

using namespace memsafe;

namespace cycles {

    MEMSAFE_BASELINE(100);

    struct A { // Ok
    };

    class SharedA : public Shared<A> { // Ok
    };

    struct B : public A { // Ok
    };

    struct C : public B {
        Shared<A> a; // Error
    };

    struct D : public B {
        SharedA a; // Error
    };

    struct E {
        std::vector<E> e; // Error
    };

    struct F : public A {
        std::vector<A> F; // Error
    };

    struct M {
        std::map<C, D> m;
    };

    struct S {
        std::map<SharedA, SharedA> s; // Error
    };




    MEMSAFE_BASELINE(10_000);
    namespace unsafe {

        struct A { // Ok
        };

        class SharedA : public Shared<A> { // Ok
        };

        struct B : public A { // Ok
        };

        struct C : public B {
            MEMSAFE_UNSAFE Shared<A> a; // Unsafe
        };

        struct D : public B {
            MEMSAFE_UNSAFE SharedA a; // Unsafe
        };

        struct E {
            MEMSAFE_UNSAFE std::vector<E> e; // Unsafe
        };

        struct F : public A {
            MEMSAFE_UNSAFE std::vector<A> F; // Unsafe
        };

        struct MEMSAFE_UNSAFE U {
            std::map<C, D> m; // Unsafe
            std::map<C, D> m2; // Unsafe
            std::map<SharedA, SharedA> s; // Unsafe
            std::vector<A> F; // Unsafe
        };


    }
}

