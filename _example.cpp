#include <vector>
#include <algorithm>
#include "memsafe.h"

using namespace memsafe;

namespace ns {

    MEMSAFE_LINE(100);

    void iter_test(int arg) {
        // memsafe_clang.so
        // -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe -Xclang -plugin-arg-memsafe -Xclang fixit=memsafe

        std::vector<int> vect{1, 2, 3, 4};
        {
            auto beg = vect.begin();
            std::vector<int>::const_iterator beg_const = vect.cbegin();
            //            const auto end_const(vect.rend());
            auto refff = &vect;

            /* MEMSAFE_UNSAFE */
            {
                vect = {};
                vect.clear();
                std::sort(beg, vect.end());
                const std::vector<int> vect_const;
            }
        }
        {

            auto b = LazyCaller<decltype(vect), decltype(std::declval<decltype(vect)>().begin())>(vect, &decltype(vect)::begin);
            auto e = LAZYCALL(vect, end);
            {
                vect = {};
                vect.clear();
                std::sort(*b, *e);
            }
        }
    }

    namespace MEMSAFE_ATTR("unsafe") {

        MEMSAFE_LINE(1000);
        VarShared<int> var_unsafe1(1);
        memsafe::VarShared<int> var_unsafe2(2);
        memsafe::VarShared<int> var_unsafe3(3);

    }

    MEMSAFE_LINE(2000);
    memsafe::VarValue<int> var_value(1);
    memsafe::VarValue<int> var_value2(2);
    memsafe::VarShared<int> var_share(1);
    memsafe::VarGuard<int, memsafe::VarSyncMutex> var_guard(1);

    MEMSAFE_LINE(3000);
    static memsafe::VarValue<int> var_static(1);
    static auto static_fail1(var_static.take());
    static auto static_fail2 = var_static.take();

    MEMSAFE_LINE(4000);

    memsafe::VarShared<int> stub_function(memsafe::VarShared<int> arg, memsafe::VarValue<int> arg_val) {

        // MEMSAFE_LINE(4100);
        var_static = var_value;
        {
            var_static = var_value;
            {
                var_static = var_value;
            }
        }

        // @todo clang-20 https://github.com/llvm/llvm-project/pull/110334 
        // [clang][frontend] Add support for attribute plugins for statement attributes #110334 
        //        if (*arg) {
        //            [[memsafe("unsafe")]] return 0;
        //        }

        //        MEMSAFE_LINE(4200);
        memsafe::VarShared<int> var_shared1(1);
        memsafe::VarShared<int> var_shared2(1);

        //        MEMSAFE_LINE(4300);
        var_shared1 = var_shared1;
        var_shared1 = var_shared2;
        {
            //            MEMSAFE_LINE(4400);
            memsafe::VarShared<int> var_shared3(3);
            var_shared1 = var_shared1;
            var_shared2 = var_shared1;
            var_shared3 = var_shared1;

            {
                //                MEMSAFE_LINE(4500);
                memsafe::VarShared<int> var_shared4 = var_shared1;

                var_shared1 = var_shared1;
                var_shared2 = var_shared1;
                var_shared3 = var_shared1;

                var_shared4 = var_shared1;
                var_shared4 = var_shared3;

                var_shared4 = var_shared4;
                // return var_shared4; // Error
            }
            //            MEMSAFE_LINE(4600);
            // return arg; // Error
            std::swap(var_shared1, var_shared3);
        }
        //
        //        MEMSAFE_LINE(4700);
        int temp = 3;
        temp = 4;
        var_value = 5;
        *var_value += 6;


        //        MEMSAFE_LINE(4800);
        return 777;
    }

    MEMSAFE_LINE(5000);

    memsafe::VarShared<int> stub_function8(memsafe::VarShared<int> arg) {
        //        MEMSAFE_LINE(5900);
        return arg;
    }

    MEMSAFE_LINE(6000);

    memsafe::VarShared<int> stub_function9() {
        //        MEMSAFE_LINE(6900);
        return VarShared<int>(999);
    }
}

