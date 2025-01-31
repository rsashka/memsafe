#ifdef BUILD_UNITTEST

#include <gtest/gtest.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

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

    EXPECT_EQ(1, sizeof (bool));
    EXPECT_EQ(1, sizeof (VarValue<bool>));
    EXPECT_EQ(4, sizeof (int32_t));
    EXPECT_EQ(4, sizeof (VarValue<int32_t>));
    EXPECT_EQ(8, sizeof (int64_t));
    EXPECT_EQ(8, sizeof (VarValue<int64_t>));

    VarValue<int> value_int(0);
    int & take_value = *value_int;
    VarAuto<int, int&> take_value2 = value_int.take();

    VarShared<int> shared_int(0);
    int & take_shared1 = *shared_int;

    ASSERT_EQ(0, *shared_int);
    *shared_int = 11;
    ASSERT_EQ(11, *shared_int);

    auto var_take_shared = shared_int.take();
    int & take_shared2 = *var_take_shared;


    //    auto guard_int(0, "*");
    VarGuard<int> guard_int(22);
    int temp_guard = *guard_int;
    guard_int.set(33);

    ASSERT_EQ(22, temp_guard);
    ASSERT_EQ(33, *guard_int);

    //    std::cout << "guard_int: " << guard_int.shared_this.use_count() << "\n";
    //    int & take_guard = *guard_int;


    VarWeak<int> weak_shared1(shared_int);
    VarWeak<int> weak_shared2 = weak_shared1;
    auto weak_shared3 = shared_int.weak();

    VarWeak<int, VarShared<int>> weak_shared4 = shared_int.weak();


    ASSERT_EQ(1, guard_int.use_count()) << guard_int.use_count();
    ASSERT_EQ(1, guard_int.use_count()) << guard_int.use_count();
    ASSERT_NO_THROW(guard_int.weak());
    VarWeak<int, VarGuard<int>> weak_guard1 = guard_int.weak();
    //
    //    //    std::cout << "weak_guard1: " << guard_int.shared_this.use_count() << "\n";
    //
    auto weak_guard2(guard_int.weak());
    //
    //    //    std::cout << "weak_guard2: " << guard_int.shared_this.use_count() << "\n";
    //
    auto weak_guard3 = guard_int.weak();

    //    std::cout << "weak_guard3: " << guard_int.shared_this.use_count() << "\n";



    //    auto auto_shared9(weak_shared1.take());
    //    auto auto_guard9(weak_guard1.take());


    //template <typename V, typename T = V, typename W = std::weak_ptr<T>> class VarWeak;
    ASSERT_NO_THROW(weak_shared1.take());

    VarAuto<int, VarShared<int>> auto_shared(weak_shared1.take());

    ASSERT_EQ(1, guard_int.use_count());

    //    auto auto_guard1(weak_guard1.take());

    ASSERT_NO_THROW(*guard_int);

    //    VarAuto<int, VarGuard<int, VarGuardData<int>>> auto_guard2(weak_guard1.take());
    //    std::cout << "auto_guard: " << guard_int.shared_this.use_count() << "\n";

    //    int & take_weak_shared = *auto_shared;
    //    int & take_weak_guard = *auto_guard;



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
    ASSERT_TRUE(var_none);

    memsafe::VarGuard<int, VarSyncMutex> var_mutex(1);
    ASSERT_TRUE(var_mutex);

    memsafe::VarGuard<int, VarSyncRecursiveShared> var_recursive(1);
    ASSERT_TRUE(var_recursive);

}

TEST(MemSafe, Plugin) {

    namespace fs = std::filesystem;

    
    // Example of running a plugin to compile a file
    
    /* The plugin operation is checked as follows.
     * A specific file with examples of template usage from the metsafe/ file is compiled
     * The example file contains correct C++ code, 
     * but the variables in it are used both correctly and with violations of the rules.
     * 
     * When compiling a file with an analyzer plugin, debug output is created and written to a debug file.
     * This file is read in the test and debug messages of the plugin 
     * are searched for in it (both successful and unsuccessful checks).
     * 
     * If all check messages are found, the test is considered successful.
     */        
    
    std::string cmd = "clang-19";
    cmd += " -std=c++20 -ferror-limit=500 ";
    cmd += " -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe ";
    cmd += " -Xclang -plugin-arg-memsafe -Xclang fixit=memsafe ";
    cmd += " _example.cpp ";

    const char * file_out = "_example.cpp.memsafe";
    fs::remove(file_out);
    int err = std::system(cmd.c_str());

    ASSERT_TRUE(fs::exists(file_out));

    std::ifstream file(file_out);

    ASSERT_TRUE(file.is_open());

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string output = buffer.str();
    file.close();

    ASSERT_TRUE(!output.empty());

    std::set<std::string> diag({
        //MEMSAFE_LINE(1000);        
        "unsafe, 1001",
        "unsafe, 1002",
        "unsafe, 1003",
        "shared, 1001, 1",
        "shared, 1002, 1",
        "shared, 1003, 1",

        //MEMSAFE_LINE(2000);
        "value, 2001, 1",
        "value, 2002, 1",
        "shared, 2003, 1",
        "shared, 2004, 1",

        //MEMSAFE_LINE(3000);
        "value, 3001, 1",
        "error, 3002",
        "auto, 3002, 1",
        "error, 3003",
        "auto, 3003, 1",

        //MEMSAFE_LINE(4000);
        "shared, 4002, 1",
        "value, 4002, 1",


        //MEMSAFE_LINE(4100);
        "value, 4005, 1",
        "approved, 4005",
        "value, 4005, 1",
        "value, 4007, 1",
        "approved, 4007",
        "value, 4007, 1",
        "value, 4009, 1",
        "approved, 4009",
        "value, 4009, 1",



        //MEMSAFE_LINE(4200);
        "shared, 4020, 2",
        "shared, 4021, 2",

        //MEMSAFE_LINE(4300) !!!!!!!!!!!!!!!
        "shared, 4024, 2",
        "error, 4024",
        "shared, 4024, 2",

        "shared, 4025, 2",
        "error, 4025",
        "shared, 4025, 2",


        //MEMSAFE_LINE(4400) !!!!!!!!!!!!!!!
        "shared, 4028, 3",

        "shared, 4029, 2",
        "error, 4029",
        "shared, 4029, 2",

        "shared, 4030, 2",
        "error, 4030",
        "shared, 4030, 2",
        "shared, 4031, 3",
        "error, 4031",
        "shared, 4031, 2",


        // MEMSAFE_LINE(4500) !!!!!!!!!!!!!!! 
        "shared, 4035, 4",
        "shared, 4037, 2",
        "error, 4037",
        "shared, 4037, 2",
        "shared, 4038, 2",
        "error, 4038",
        "shared, 4038, 2",
        "shared, 4039, 3",
        "error, 4039",
        "shared, 4039, 2",

        "shared, 4041, 4",
        "error, 4041",
        "shared, 4041, 2",
        "shared, 4042, 4",
        "error, 4042",
        "shared, 4042, 3",
        "shared, 4044, 4",
        "error, 4044",
        "shared, 4044, 4",

        // MEMSAFE_LINE(4600) !!!!!!!!!!!!!!!
        "error, 4049",
        "shared, 4049, 2",
        "shared, 4049, 3",
        // MEMSAFE_LINE(4700) !!!!!!!!!!!!!!!
        "value, 4055, 1",
        "approved, 4055",

        //MEMSAFE_LINE(5000)
        "error, 5004",
                
        //  MEMSAFE_LINE(6000)
        "approved, 6004",

    });


    for (auto &str : diag) {
        EXPECT_TRUE(output.find(str) != std::string::npos) << str;
    }

}
#endif
