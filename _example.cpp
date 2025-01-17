#include "memsafe.h"


using namespace memsafe;

namespace ns {

    namespace [[memsafe("unsafe")]] {

        VarShared<int> var_unsafe1(1);
        memsafe::VarShared<int> var_unsafe2(2);
        memsafe::VarShared<int> var_unsafe3(3);

    }

    memsafe::VarValue<int> var_value(1);
    memsafe::VarValue<int> var_value2(2);

    memsafe::VarShared<int> var_share(1);
    memsafe::VarGuard<int, memsafe::VarSyncMutex> var_guard(1);

    static memsafe::VarValue<int> var_static(1);

    memsafe::VarShared<int> stub_function(memsafe::VarShared<int> arg, memsafe::VarValue<int> arg_val) {

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

        memsafe::VarShared<int> var_shared1(1);
        memsafe::VarShared<int> var_shared2(1);

        var_shared1 = var_shared1; // error ?????????????????????
        var_shared1 = var_shared2; // error
        {
            memsafe::VarShared<int> var_shared3(3);

            var_shared1 = var_shared1; // error
            var_shared2 = var_shared1; // error
            var_shared3 = var_shared1; // OK

            {
                memsafe::VarShared<int> var_shared4 = var_shared1;

                var_shared1 = var_shared1; // error
                var_shared2 = var_shared1; // error
                var_shared3 = var_shared1; // OK

                var_shared4 = var_shared1; // OK
                var_shared4 = var_shared3; // OK

                var_shared4 = var_shared4; // error ??????????
                // return var_shared4; // Error
            }
            // return arg; // Error
            std::swap(var_shared1, var_shared3);
        }

        int temp = 3;
        temp = 4;
        var_value = 5;
        *var_value += 6;


        return VarShared<int>(777);
    }

    memsafe::VarShared<int> stub_function8() {
        return VarShared<int>(888);
    }

    memsafe::VarShared<int> stub_function9() {
        return VarShared<int>(999);
    }
    //    decltype(auto) stub_auto() {
    //        auto result = var_static.take();
    //        return result;
    //    }

}

