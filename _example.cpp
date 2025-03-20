#include <vector>
#include <algorithm>
#include "memsafe.h"

using namespace memsafe;

namespace ns {

    MEMSAFE_BASELINE(100);

    void * invalidate_test(int arg) {

        std::vector<int> vect{1, 2, 3, 4};
        {

            MEMSAFE_BASELINE(200);
            auto beg = vect.begin();
            std::vector<int>::const_iterator beg_const = vect.cbegin();

            auto pointer = &vect; // Error

            auto str = std::string();
            std::string_view view(str);
            str.clear();
            auto view_iter = view.begin(); // Error

            {
                MEMSAFE_BASELINE(300);
                vect = {};
                vect.shrink_to_fit();
                std::sort(beg, vect.end()); // Error
                const std::vector<int> vect_const;
            }
        }

        {

            MEMSAFE_BASELINE(400);
            auto b = LazyCaller<decltype(vect), decltype(std::declval<decltype(vect)>().begin())>(vect, &decltype(vect)::begin);
            auto e = LAZYCALL(vect, end);
            {
                MEMSAFE_BASELINE(500);
                vect = {};
                vect.clear();
                std::sort(*b, *e);
            }
        }

        MEMSAFE_BASELINE(900);
        MEMSAFE_UNSAFE{
            return nullptr;
            MEMSAFE_UNSAFE return nullptr;}
    }

    namespace MEMSAFE_UNSAFE {

        MEMSAFE_BASELINE(1000);
        Shared<int> var_unsafe1(1);
        memsafe::Shared<int> var_unsafe2(2);
        memsafe::Shared<int> var_unsafe3(3);
    }



    MEMSAFE_BASELINE(2000);
    memsafe::Value<int> var_value(1);
    memsafe::Value<int> var_value2(2);
    memsafe::Shared<int> var_share(1);
    memsafe::Shared<int, memsafe::SyncTimedMutex> var_guard(1);

    MEMSAFE_BASELINE(3000);
    static memsafe::Value<int> var_static(1);
    static auto static_fail1(var_static.take()); // Error
    static auto static_fail2 = var_static.take(); // Error

    MEMSAFE_BASELINE(4000);

    memsafe::Shared<int> memory_test(memsafe::Shared<int> arg, memsafe::Value<int> arg_val) {

        MEMSAFE_BASELINE(4100);
        var_static = var_value;
        {
            var_static = var_value;
            {
                var_static = var_value;
            }
        }

        MEMSAFE_BASELINE(4200);
        memsafe::Shared<int> var_shared1(1);
        memsafe::Shared<int> var_shared2(1);

        MEMSAFE_BASELINE(4300);
        var_shared1 = var_shared1; // Error
        MEMSAFE_UNSAFE var_shared1 = var_shared2; // Unsafe

        {
            MEMSAFE_BASELINE(4400);
            memsafe::Shared<int> var_shared3(3);
            var_shared1 = var_shared1; // Error
            var_shared2 = var_shared1; // Error
            var_shared3 = var_shared1;

            {
                MEMSAFE_BASELINE(4500);
                memsafe::Shared<int> var_shared4 = var_shared1;

                var_shared1 = var_shared1; // Error
                var_shared2 = var_shared1; // Error
                var_shared3 = var_shared1;

                var_shared4 = var_shared1;
                var_shared4 = var_shared3;

                var_shared4 = var_shared4; // Error

                if (var_shared4) {
                    MEMSAFE_UNSAFE return var_shared4;
                }
                return var_shared4; // Error
            }

            MEMSAFE_BASELINE(4600);
            std::swap(var_shared1, var_shared2);
            std::swap(var_shared1, var_shared3);
            MEMSAFE_UNSAFE std::swap(var_shared1, var_shared3);

            MEMSAFE_BASELINE(4700);
            return arg; // Error
        }

        MEMSAFE_BASELINE(4700);
        int temp = 3;
        temp = 4;
        var_value = 5;
        *var_value += 6;


        MEMSAFE_BASELINE(4800);
        return 777;
    }

    MEMSAFE_BASELINE(7000);

    void shared_example() {
        std::shared_ptr<int> old_shared; // Error
        Shared<int> var = 1;
        Shared<int> copy;
        copy = var; // Error
        std::swap(var, copy);
        {
            Shared<int> inner = var;
            std::swap(inner, copy); // Error
            inner = copy;
            copy = inner; // Error
        }
    }

    MEMSAFE_BASELINE(8000);

    memsafe::Shared<int> memory_test_8(memsafe::Shared<int> arg) {
        MEMSAFE_BASELINE(8900);
        return arg; // Error
    }

    MEMSAFE_BASELINE(9000);

    memsafe::Shared<int> memory_test_9() {
        MEMSAFE_BASELINE(9900);
        return Shared<int>(999);
    }




    MEMSAFE_BASELINE(10000);

    class RecursiveRef {
    public:
        RecursiveRef * ref_pointer; // Error
        std::shared_ptr<RecursiveRef> ref_shared; // Error
        std::weak_ptr<RecursiveRef> ref_weak;

        Auto<int, int&> ref_int; // Error
        Shared<RecursiveRef> recursive; // Error
        Weak<Shared<RecursiveRef>> ref_weak2;
        Class<RecursiveRef> reference;

        MEMSAFE_UNSAFE RecursiveRef * unsafe_pointer; // Unsafe
        MEMSAFE_UNSAFE std::shared_ptr<RecursiveRef> unsafe_shared; // Unsafe
        MEMSAFE_UNSAFE Auto<int, int&> unsafe_ref_int; // Unsafe
        MEMSAFE_UNSAFE Shared<RecursiveRef> unsafe_recursive; // Unsafe
    };

    void bugfix_11() { // https://github.com/rsashka/memsafe/issues/11
        MEMSAFE_BASELINE(900_011_000);
        std::vector vect(100000, 0);
        auto x = (vect.begin());
        vect = {};
        std::sort(x, vect.end()); // Error
    }
}

