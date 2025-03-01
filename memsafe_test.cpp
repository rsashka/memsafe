#ifdef BUILD_UNITTEST

#include <gtest/gtest.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>

#include "memsafe.h"
#include "memsafe_plugin.h"

using namespace memsafe;
using namespace std::chrono_literals;

TEST(MemSafe, Sizes) {

    EXPECT_EQ(32, sizeof (std::string));
    EXPECT_EQ(32, sizeof (std::wstring));
    EXPECT_EQ(40, sizeof (std::variant<std::string>));
    EXPECT_EQ(40, sizeof (std::variant<std::string, std::wstring>));

    EXPECT_EQ(16, sizeof (std::runtime_error));
    EXPECT_EQ(16, sizeof (memsafe_error));

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

    {
        auto taken = guard_int.take();
    }

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

        std::chrono::duration<double, std::milli> elapsed;

        std::thread other([&]() {
            try {

                const auto start = std::chrono::high_resolution_clock::now();
                std::this_thread::sleep_for(100ms);
                elapsed = std::chrono::high_resolution_clock::now() - start;
                *var_mutex;
            } catch (...) {
                catched = true;
            }

        });
        ASSERT_TRUE(other.joinable());
        other.join();

        ASSERT_TRUE(elapsed <= 200.0ms);

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

TEST(MemSafe, Depend) {
    {
        std::vector<int> vect(100000, 0);
        auto b = vect.begin();
        auto e = vect.end();

        EXPECT_EQ(8, sizeof (b));
        EXPECT_EQ(8, sizeof (e));

        vect = {};
        //        vect.shrink_to_fit();
        std::sort(b, e);

    }
    {
        std::vector<int> vect(100000, 0);

        auto b = LazyCaller<decltype(vect), decltype(std::declval<decltype(vect)>().begin())>(vect, &decltype(vect)::begin);
        auto e = LAZYCALL(vect, end);
        auto c = LAZYCALL(vect, clear);
        auto s = LAZYCALL(vect, size);

        EXPECT_EQ(32, sizeof (b));
        EXPECT_EQ(32, sizeof (e));

        ASSERT_EQ(100000, vect.size());
        ASSERT_EQ(100000, *s);

        *c;

        ASSERT_EQ(0, vect.size());
        ASSERT_EQ(0, *s);

        // std::vector<int> data(1, 1);
        // auto reserve = LAZYCALL(vect, reserve, 10UL);
        // *reserve;
        //
        // auto db = LAZYCALL(data, begin);
        // auto de = LAZYCALL(data, end);
        //
        // ASSERT_EQ(1, data[0]);
        // // @todo variadic tempalte as variadic template argument
        // auto call = LAZYCALL(vect, assign, db, de);

        // data.clear();
        // data.shrink_to_fit();
        // *call;

        // ASSERT_EQ(1, vect.size());
        // ASSERT_EQ(1, *s);
        //
        // ASSERT_EQ(1, data[0]);
        // ASSERT_EQ(1, vect[0]);

        vect.shrink_to_fit();
        std::sort(*b, *e);
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
     * When compiling a file with an analyzer plugin, the debug output is written to a file.
     * This file is read in the test and debug messages of the plugin 
     * are searched for in it (both successful and unsuccessful checks).
     * 
     * If all check messages are found, the test is considered successful.
     */

    std::string cmd = "clang-20";
    cmd += " -std=c++20 -ferror-limit=500 ";
    cmd += " -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe ";
    cmd += " -Xclang -plugin-arg-memsafe -Xclang log ";
    cmd += " -c _example.cpp > _example.cpp.log";

    const char * file_log = "_example.cpp.log";
    fs::remove(file_log);

    int err = std::system(cmd.c_str());

    ASSERT_TRUE(fs::exists(file_log));
    std::ifstream log_file(file_log);

    ASSERT_TRUE(log_file.is_open());

    std::stringstream log_buffer;
    log_buffer << log_file.rdbuf();

    std::string log_output = log_buffer.str();
    log_file.close();


    ASSERT_TRUE(!log_output.empty());

    ASSERT_TRUE(log_output.find("Enable dump and process logger") != std::string::npos &&
            log_output.find("unprocessed attribute!") == std::string::npos)
            << log_output;


    std::multiset<std::string> diag({
        "#log #201",
        "#log #202",
        "#log #202",
        "#err #204",
        "#log #207",
        "#log #208",
        "#err #209",
        "#log #301",
        "#log #301",
        "#log #302",
        "#err #303",
        "#log #303",
        "#log #401",
        "#log #402",
        "#log #501",
        "#log #901",
        "#log #902",
        "#log #903",
        "#log #903",
        "#log #1001",
        "#log #1002",
        "#log #1003",
        "#log #2003",
        "#err #3002",
        "#err #3003",
        "#log #4201",
        "#log #4202",
        "#err #4301",
        "#log #4302",
        "#warn #4302",
        "#log #4401",
        "#err #4402",
        "#err #4403",
        "#err #4404",
        "#log #4501",
        "#err #4503",
        "#err #4504",
        "#err #4505",
        "#err #4507",
        "#err #4508",
        "#err #4510",
        "#log #4513",
        "#warn #4513",
        "#err #4515",
        "#log #4601",
        "#err #4602",
        "#log #4603",
        "#warn #4603",
        "#err #4701",
        "#log #4703",
        "#log #4801",
        "#err #8901",
        "#log #9901",
    });

    size_t line;
    std::multimap<size_t, const char *> list;
    for (auto &elem : diag) {
        line = atoi(&elem.substr(elem.find(" #") + 2)[0]);
        ASSERT_TRUE(line) << elem;
        list.emplace(line, elem.data());
    }

    size_t pos = log_output.find(MEMSAFE_KEYWORD_START_LOG);
    ASSERT_TRUE(pos != std::string::npos);
    std::string log_str = log_output.substr(pos + strlen(MEMSAFE_KEYWORD_START_LOG), log_output.find("\n\n", pos + strlen(MEMSAFE_KEYWORD_START_LOG)) - pos - strlen(MEMSAFE_KEYWORD_START_LOG));

    //    std::cout << "\n" << log_str << "\n\n";

    std::vector< std::string> log;
    SplitString(log_str, '\n', &log);

    ASSERT_TRUE(log.size()) << log_str;

    pos = 0;
    while (pos < log.size()) {

        size_t type_pos = log[pos].find("#");
        ASSERT_TRUE(type_pos != std::string::npos) << pos << " " << log[pos];

        size_t hash_pos = log[pos].find(" #", type_pos + 1);
        ASSERT_TRUE(hash_pos != std::string::npos) << pos << " " << log[pos];

        line = atoi(log[pos].data() + hash_pos + 2);
        ASSERT_TRUE(line) << pos << " " << log[pos];

        size_t msg_pos = log[pos].find(" ", hash_pos + 1);
        ASSERT_TRUE(msg_pos != std::string::npos) << pos << " " << log[pos];

        auto found = list.find(line);
        if (found == list.end()) {
            ADD_FAILURE() << "Not found: " << log[pos] << " " << line << " pos: " << pos;
        } else if (log[pos].find(found->second) != type_pos) {
            ADD_FAILURE() << "At mark line: #" << line << " expected: \"" << found->second << "\" but found \"" << log[pos] << "\"";
            list.erase(found);
        } else {
            list.erase(found);
        }
        pos++;
    }

    for (auto &elem : list) {
        ADD_FAILURE() << "Not found: " << elem.second;
    }

}

#endif
