#include <vector>
#include <algorithm>
#include "memsafe.h"

using namespace memsafe;

namespace ns {

    MEMSAFE_BASELINE(100);

    void * invalidate_test(int arg) {

        std::vector<int> vect{1, 2, 3, 4};
        {

            //            MEMSAFE_PRINT_AST("*");
            MEMSAFE_BASELINE(200);
            auto beg = vect.begin();
            std::vector<int>::const_iterator beg_const = vect.cbegin();

            auto refff = &vect;

            auto str = std::string();
            std::string_view view(str);
            str.clear();
            auto view_iter = view.begin();

            /* MEMSAFE_UNSAFE */
            {
                MEMSAFE_BASELINE(300);
                vect = {};
                vect.shrink_to_fit();
                //                std::swap(beg, beg);
                std::sort(beg, vect.end());
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
        VarShared<int> var_unsafe1(1);
        memsafe::VarShared<int> var_unsafe2(2);
        memsafe::VarShared<int> var_unsafe3(3);
    }



    MEMSAFE_BASELINE(2000);
    memsafe::VarValue<int> var_value(1);
    memsafe::VarValue<int> var_value2(2);
    memsafe::VarShared<int> var_share(1);
    memsafe::VarGuard<int, memsafe::VarSyncMutex> var_guard(1);

    MEMSAFE_BASELINE(3000);
    static memsafe::VarValue<int> var_static(1);
    static auto static_fail1(var_static.take());
    static auto static_fail2 = var_static.take();

    MEMSAFE_BASELINE(4000);

    memsafe::VarShared<int> memory_test(memsafe::VarShared<int> arg, memsafe::VarValue<int> arg_val) {

        MEMSAFE_BASELINE(4100);
        var_static = var_value;
        {
            var_static = var_value;
            {
                var_static = var_value;
            }
        }

        MEMSAFE_BASELINE(4200);
        memsafe::VarShared<int> var_shared1(1);
        memsafe::VarShared<int> var_shared2(1);

        MEMSAFE_BASELINE(4300);
        var_shared1 = var_shared1;
        var_shared1 = var_shared2;
        {
            MEMSAFE_BASELINE(4400);
            memsafe::VarShared<int> var_shared3(3);
            var_shared1 = var_shared1;
            var_shared2 = var_shared1;
            var_shared3 = var_shared1;

            {
                MEMSAFE_BASELINE(4500);
                memsafe::VarShared<int> var_shared4 = var_shared1;

                var_shared1 = var_shared1;
                var_shared2 = var_shared1;
                var_shared3 = var_shared1;

                var_shared4 = var_shared1;
                var_shared4 = var_shared3;

                var_shared4 = var_shared4;
                //                return var_shared4; // Error
            }

            MEMSAFE_BASELINE(4600);
            //            return arg; // Error
            std::swap(var_shared1, var_shared3);
        }

        MEMSAFE_BASELINE(4700);
        int temp = 3;
        temp = 4;
        var_value = 5;
        *var_value += 6;


        MEMSAFE_BASELINE(4800);
        return 777;
    }

    MEMSAFE_BASELINE(8000);

    memsafe::VarShared<int> memory_test_8(memsafe::VarShared<int> arg) {
        MEMSAFE_BASELINE(8900);
        return arg;
    }


    MEMSAFE_BASELINE(9000);

    memsafe::VarShared<int> memory_test_9() {
        MEMSAFE_BASELINE(9900);
        return VarShared<int>(999);
    }
}

