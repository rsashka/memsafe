#ifdef BUILD_UNITTEST

#include <gtest/gtest.h>

#include "memsafe.h"

using namespace memsafe;

TEST(MemSafe, Sizes) {

    EXPECT_EQ(32, sizeof (std::string));
    EXPECT_EQ(32, sizeof (std::wstring));
    EXPECT_EQ(40, sizeof (std::variant<std::string>));
    EXPECT_EQ(40, sizeof (std::variant<std::string, std::wstring>));

    EXPECT_EQ(16, sizeof (std::runtime_error));
    EXPECT_EQ(16, sizeof (runtime_format));

    class TestClassV0 {

        void func() {
        }
    };

    class TestClassV1 {

        virtual void func() {
        }
    };

    class TestClassV2 {

        virtual void func1() {
        }

        virtual void func2() {
        }
    };

    class TestClassV3 {

        TestClassV3() {
        }

        virtual void func1() {
        }

        virtual void func2() {
        }

        virtual TestClassV3 & operator=(TestClassV3 &) = 0;

        virtual ~TestClassV3() {
        }
    };

    class TestClass1 : std::shared_ptr<int> {
    };

    class TestClass2 : std::shared_ptr<int>, std::enable_shared_from_this<TestClass2> {
    };

    class TestClass3 : std::enable_shared_from_this<TestClass3> {
        int value;
    };

    class TestClass4 : std::enable_shared_from_this<TestClass4> {
    };


    EXPECT_EQ(1, sizeof (TestClassV0));
    EXPECT_EQ(8, sizeof (TestClassV1));
    EXPECT_EQ(8, sizeof (TestClassV2));
    EXPECT_EQ(8, sizeof (TestClassV3));

    EXPECT_EQ(16, sizeof (TestClass1));
    EXPECT_EQ(32, sizeof (TestClass2));
    EXPECT_EQ(4, sizeof (int));
    EXPECT_EQ(24, sizeof (TestClass3));
    EXPECT_EQ(16, sizeof (TestClass4));


    EXPECT_EQ(16, sizeof (VarSync<int>));
    EXPECT_EQ(24, sizeof (VarSyncNone<int>));
    EXPECT_EQ(56, sizeof (VarSyncMutex<int>));
    EXPECT_EQ(80, sizeof (VarSyncRecursiveShared<int>));

    EXPECT_EQ(16, sizeof (VarSync<uint8_t>));
    EXPECT_EQ(24, sizeof (VarSyncNone<uint8_t>));
    EXPECT_EQ(56, sizeof (VarSyncMutex<uint8_t>));
    EXPECT_EQ(80, sizeof (VarSyncRecursiveShared<uint8_t>));

    EXPECT_EQ(16, sizeof (VarSync<uint64_t>));
    EXPECT_EQ(24, sizeof (VarSyncNone<uint64_t>));
    EXPECT_EQ(56, sizeof (VarSyncMutex<uint64_t>));
    EXPECT_EQ(80, sizeof (VarSyncRecursiveShared<uint64_t>));

    EXPECT_EQ(99, sizeof (std::array<uint8_t, 99 >));
    EXPECT_EQ(100, sizeof (std::array<uint8_t, 100 >));
    EXPECT_EQ(101, sizeof (std::array<uint8_t, 101 >));

    EXPECT_EQ(112, sizeof (VarSync<std::array<uint8_t, 100 >>));
    EXPECT_EQ(120, sizeof (VarSyncNone<std::array<uint8_t, 100 >>));
    EXPECT_EQ(152, sizeof (VarSyncMutex<std::array <uint8_t, 100 >>));
    EXPECT_EQ(176, sizeof (VarSyncRecursiveShared<std::array<uint8_t, 100 >>));

    EXPECT_EQ(101, sizeof (std::array<uint8_t, 101 >));
    EXPECT_EQ(112, sizeof (VarSync<std::array<uint8_t, 101 >>));
    EXPECT_EQ(120, sizeof (VarSyncNone<std::array<uint8_t, 101 >>));
    EXPECT_EQ(152, sizeof (VarSyncMutex<std::array <uint8_t, 101 >>));
    EXPECT_EQ(176, sizeof (VarSyncRecursiveShared<std::array<uint8_t, 101 >>));
}

TEST(MemSafe, Cast) {

    EXPECT_EQ(8, sizeof (VarInterface<bool>));
    EXPECT_EQ(8, sizeof (VarInterface<int32_t>));
    EXPECT_EQ(8, sizeof (VarInterface<int64_t>));

    EXPECT_EQ(1, sizeof (bool));
    EXPECT_EQ(16, sizeof (VarValue<bool>));
    EXPECT_EQ(4, sizeof (int32_t));
    EXPECT_EQ(16, sizeof (VarValue<int32_t>));
    EXPECT_EQ(8, sizeof (int64_t));
    EXPECT_EQ(16, sizeof (VarValue<int64_t>));

    VarValue<int> value_int(0);
    int & take_value = *value_int;
    VarAuto<int, int&> take_value2 = value_int.take();

    VarShared<int> shared_int(0);
    int & take_shared1 = *shared_int;
    auto var_take_shared = shared_int.take();
    int & take_shared2 = *var_take_shared;


    //    auto guard_int(0, "*");
    VarGuard<int> guard_int(0);
    //    std::cout << "guard_int: " << guard_int.shared_this.use_count() << "\n";
    //    int & take_guard = *guard_int;

    {
        VarWeak<int> weak_shared1(shared_int);
        VarWeak<int> weak_shared2 = weak_shared1;
        auto weak_shared3 = shared_int.weak();

        VarWeak<int, VarShared<int>> weak_shared4 = shared_int.weak();


        EXPECT_EQ(1, guard_int.load().use_count()) << guard_int.load().use_count();
        EXPECT_EQ(1, guard_int.load().use_count()) << guard_int.load().use_count();
        //    ASSERT_NO_THROW(guard_int.weak().load());
        //    VarWeak<int, VarGuard<int>> weak_guard1 = guard_int.weak();
        //
        //    //    std::cout << "weak_guard1: " << guard_int.shared_this.use_count() << "\n";
        //
        auto weak_guard2(guard_int.load());
        //
        //    //    std::cout << "weak_guard2: " << guard_int.shared_this.use_count() << "\n";
        //
        auto weak_guard3 = guard_int.load();

        //    std::cout << "weak_guard3: " << guard_int.shared_this.use_count() << "\n";



        //    auto auto_shared9(weak_shared1.take());
        //    auto auto_guard9(weak_guard1.take());


        //template <typename V, typename T = V, typename W = std::weak_ptr<T>> class VarWeak;
        ASSERT_NO_THROW(weak_shared1.take());

        VarAuto<int, VarShared<int>> auto_shared(weak_shared1.take());

        EXPECT_EQ(1, guard_int.load().use_count());

        //    auto auto_guard1(weak_guard1.take());

        EXPECT_NO_THROW(*guard_int);

        //    VarAuto<int, VarGuard<int, VarGuardData<int>>> auto_guard2(weak_guard1.take());
        //    std::cout << "auto_guard: " << guard_int.shared_this.use_count() << "\n";

        //    int & take_weak_shared = *auto_shared;
        //    int & take_weak_guard = *auto_guard;
    }

    EXPECT_EQ(1, guard_int.load().use_count());

}

TEST(MemSafe, Threads) {

    {
        unsigned long long g_count = 0;
        std::thread t1([&]() {
            for (auto i = 0; i < 1'000'000; ++i)
                ++g_count;
        });
        std::thread t2([&]() {
            for (auto i = 0; i < 1'000'000; ++i)
                ++g_count;
        });
        t1.join();
        t2.join();

        EXPECT_NE(2'000'000, g_count);
    }

    {
        std::atomic<unsigned long long> a_count{ 0};
        std::thread t3([&]() {
            for (auto i = 0; i < 1'000'000; ++i)
                a_count.fetch_add(1);
        });

        std::thread t4([&]() {
            for (auto i = 0; i < 1'000'000; ++i)
                a_count.fetch_add(1);
        });

        t3.join();
        t4.join();

        EXPECT_EQ(2'000'000, a_count);
    }

    {
        memsafe::VarGuard<int> var_guard(0);
        bool catched = false;

        std::thread other([&]() {
            try {
                *var_guard;
            } catch (...) {
                catched = true;
            }

        });
        other.join();

        ASSERT_TRUE(catched);
    }

    {
        memsafe::VarGuard<int, VarSyncMutex> var_mutex(0);
        bool catched = false;

        std::thread other([&]() {
            try {
                *var_mutex;
            } catch (...) {
                catched = true;
            }

        });
        other.join();

        ASSERT_FALSE(catched);
    }

    {
        memsafe::VarGuard<int, VarSyncRecursiveShared> var_recursive(0);
        bool catched = false;

        std::thread other([&]() {
            try {
                *var_recursive;
                *var_recursive;
                *var_recursive;
            } catch (...) {
                catched = true;
            }

        });
        other.join();

        ASSERT_FALSE(catched);
    }

}

TEST(MemSafe, ApplyAttr) {

    memsafe::VarValue<int> var_value(1);
    static memsafe::VarValue<int> var_static(1);

    var_static = var_value;
    {
        var_static = var_value;
        {
            var_static = var_value;
        }
    }

    memsafe::VarShared<int> var_shared1(0);
    memsafe::VarShared<int> var_shared2(1);

    ASSERT_TRUE(var_shared1);
    ASSERT_TRUE(var_shared2);

    var_shared1 = var_shared2;
    {
        var_shared1 = var_shared2;
        {
            var_shared1 = var_shared2;
        }
    }

    memsafe::VarGuard<int> var_none(1);
    ASSERT_TRUE(var_none.load());

    memsafe::VarGuard<int, VarSyncMutex> var_mutex(1);
    ASSERT_TRUE(var_mutex.load());

    memsafe::VarGuard<int, VarSyncRecursiveShared> var_recursive(1);
    ASSERT_TRUE(var_recursive.load());

    //    var_guard1 = var_guard2;
    //    {
    //        var_guard1 = var_guard2;
    //        {
    //            var_guard1 = var_guard2;
    //        }
    //    }
}

#endif
