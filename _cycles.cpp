#include <map>
#include <vector>
#include <algorithm>
#include "memsafe.h"

using namespace memsafe;

namespace ns {
    struct A;

    // cyclic cross-references between classes that are defined in other translation units
    //    struct A {
    //        Shared<Ext> ext;
    //    };
    MEMSAFE_BASELINE(100);

    class Ext {
        Shared<A> a;
    };

}

// cyclic-analyzer
namespace cycles {

    // simple circular self-references
    MEMSAFE_BASELINE(1_000);

    struct CircleSelf {
        CircleSelf * self;
    };

    struct CircleShared {
        Shared<CircleShared> shared;
    };

    struct CircleSelfUnsafe {
        MEMSAFE_UNSAFE CircleSelf * self;
    };

    struct CircleSharedUnsafe {
        MEMSAFE_UNSAFE Shared<CircleShared> shared;
    };



    // cyclic cross-references
    MEMSAFE_BASELINE(2_000);

    class SharedCross2;

    class SharedCross {
        SharedCross2 *cross2;
    };

    class SharedCross2 {
        SharedCross *cross;
    };

    class SharedCrossUnsafe {
        MEMSAFE_UNSAFE SharedCross2 *cross2;
    };

    class SharedCross2Unsafe {
        MEMSAFE_UNSAFE SharedCross *cross;
    };


    // cyclic cross-references between classes that are defined in other translation units
    MEMSAFE_BASELINE(3_000);

    struct ExtExt : public ns::Ext {
    };

    class ExtExtExt : public ExtExt {
    };



    // Reference types in STD templates
    MEMSAFE_BASELINE(4_000);

    struct ArraySharedInt : public std::vector<Shared<int> > {
    };

    struct SharedArrayInt : public Shared<std::vector<int> > {
    };



    // Reference types when extending templates
    MEMSAFE_BASELINE(5_000);

    template <typename T>
    class SharedSingle : public Shared<T, SyncSingleThread> {
    };

    class SharedSingleInt : public SharedSingle<int> {
    };

    class SharedSingleIntField {
        SharedSingleInt shared_int;
    };






    // Non-reference types with weak references
    MEMSAFE_BASELINE(10_000);

    class NotShared1 {
        int interger;
        std::weak_ptr<NotShared1> weak;
    };

    struct NotShared2 : public NotShared1 {
        NotShared1 not1;
        std::weak_ptr<NotShared2> weak1;
        Weak<Shared<int>> weak2;
    };

}

