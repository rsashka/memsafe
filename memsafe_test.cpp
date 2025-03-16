#ifdef BUILD_UNITTEST

#include <gtest/gtest.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>

#include "memsafe.h"
#include "memsafe_plugin.h"
#include "memsafe_old.h"

using namespace memsafe;
using namespace std::chrono_literals;

//TEST(MemSafe, Sizes) {
//
//    EXPECT_EQ(32, sizeof (std::string));
//    EXPECT_EQ(32, sizeof (std::wstring));
//    EXPECT_EQ(40, sizeof (std::variant<std::string>));
//    EXPECT_EQ(40, sizeof (std::variant<std::string, std::wstring>));
//
//    EXPECT_EQ(16, sizeof (std::runtime_error));
//    EXPECT_EQ(16, sizeof (memsafe_error));
//
//    class TestClassV0 {
//
//        void func() {
//        }
//    };
//
//    class TestClassV1 {
//
//        virtual void func() {
//        }
//    };
//
//    class TestClassV2 {
//
//        virtual void func1() {
//        }
//
//        virtual void func2() {
//        }
//    };
//
//    class TestClassV3 {
//
//        TestClassV3() {
//        }
//
//        virtual void func1() {
//        }
//
//        virtual void func2() {
//        }
//
//        virtual TestClassV3 & operator=(TestClassV3 &) = 0;
//
//        virtual ~TestClassV3() {
//        }
//    };
//
//    class TestClass1 : std::shared_ptr<int> {
//    };
//
//    class TestClass2 : std::shared_ptr<int>, std::enable_shared_from_this<TestClass2> {
//    };
//
//    class TestClass3 : std::enable_shared_from_this<TestClass3> {
//        int value;
//    };
//
//    class TestClass4 : std::enable_shared_from_this<TestClass4> {
//    };
//
//
//    EXPECT_EQ(1, sizeof (TestClassV0));
//    EXPECT_EQ(8, sizeof (TestClassV1));
//    EXPECT_EQ(8, sizeof (TestClassV2));
//    EXPECT_EQ(8, sizeof (TestClassV3));
//
//    EXPECT_EQ(16, sizeof (TestClass1));
//    EXPECT_EQ(32, sizeof (TestClass2));
//    EXPECT_EQ(4, sizeof (int));
//    EXPECT_EQ(24, sizeof (TestClass3));
//    EXPECT_EQ(16, sizeof (TestClass4));
//
//
//    EXPECT_EQ(16, sizeof (VarSync<int>));
//    EXPECT_EQ(24, sizeof (VarSyncNone<int>));
//    EXPECT_EQ(56, sizeof (VarSyncMutex<int>));
//    EXPECT_EQ(80, sizeof (VarSyncRecursiveShared<int>));
//
//    EXPECT_EQ(16, sizeof (VarSync<uint8_t>));
//    EXPECT_EQ(24, sizeof (VarSyncNone<uint8_t>));
//    EXPECT_EQ(56, sizeof (VarSyncMutex<uint8_t>));
//    EXPECT_EQ(80, sizeof (VarSyncRecursiveShared<uint8_t>));
//
//    EXPECT_EQ(16, sizeof (VarSync<uint64_t>));
//    EXPECT_EQ(24, sizeof (VarSyncNone<uint64_t>));
//    EXPECT_EQ(56, sizeof (VarSyncMutex<uint64_t>));
//    EXPECT_EQ(80, sizeof (VarSyncRecursiveShared<uint64_t>));
//
//    EXPECT_EQ(99, sizeof (std::array<uint8_t, 99 >));
//    EXPECT_EQ(100, sizeof (std::array<uint8_t, 100 >));
//    EXPECT_EQ(101, sizeof (std::array<uint8_t, 101 >));
//
//    EXPECT_EQ(112, sizeof (VarSync<std::array<uint8_t, 100 >>));
//    EXPECT_EQ(120, sizeof (VarSyncNone<std::array<uint8_t, 100 >>));
//    EXPECT_EQ(152, sizeof (VarSyncMutex<std::array <uint8_t, 100 >>));
//    EXPECT_EQ(176, sizeof (VarSyncRecursiveShared<std::array<uint8_t, 100 >>));
//
//    EXPECT_EQ(101, sizeof (std::array<uint8_t, 101 >));
//    EXPECT_EQ(112, sizeof (VarSync<std::array<uint8_t, 101 >>));
//    EXPECT_EQ(120, sizeof (VarSyncNone<std::array<uint8_t, 101 >>));
//    EXPECT_EQ(152, sizeof (VarSyncMutex<std::array <uint8_t, 101 >>));
//    EXPECT_EQ(176, sizeof (VarSyncRecursiveShared<std::array<uint8_t, 101 >>));
//}

TEST(MemSafe, Cast) {

    EXPECT_EQ(1, sizeof (bool));
    EXPECT_EQ(1, sizeof (Value<bool>));
    EXPECT_EQ(4, sizeof (int32_t));
    EXPECT_EQ(4, sizeof (Value<int32_t>));
    EXPECT_EQ(8, sizeof (int64_t));
    EXPECT_EQ(8, sizeof (Value<int64_t>));

    Value<int> value_int(0);
    int & take_value = *value_int;
    Auto<int, int&> take_value2 = value_int.take();

    Shared<int> shared_int(0);

    //    Auto<int, Shared<int>> take_shared = *shared_int;
    Auto<int, Shared<int>::SharedType> take_shared1 = shared_int.take();
    //    int & take_shared1 = *shared_int;

    ASSERT_EQ(0, *take_shared1);
    ASSERT_EQ(0, *shared_int.take());
    //    ASSERT_EQ(0, **shared_int);
    *shared_int.take() = 11;
    //    **shared_int = 11;
    //    ASSERT_EQ(11, **shared_int);
    ASSERT_EQ(11, *shared_int.take());

    auto var_take_shared = shared_int.take();
    int & take_shared2 = *var_take_shared;


    Shared<int, SyncSingleThread> sync_int(22);
    Auto<int, Shared<int, SyncSingleThread>::SharedType> take_sync_int = sync_int.take();
    auto auto_sync_int = sync_int.take();
    //    auto auto_sync_int = *sync_int;

    ASSERT_EQ(22, *sync_int.take());
    //    ASSERT_EQ(22, **sync_int);

    *auto_sync_int = 33;
    ASSERT_EQ(33, *auto_sync_int);
    ASSERT_EQ(33, *sync_int.take());
    //    ASSERT_EQ(33, **sync_int);

    int temp_sync = *sync_int.take();
    *sync_int.take() = 44;
    //    **sync_int = 44;

    ASSERT_EQ(33, temp_sync);
    ASSERT_EQ(44, *auto_sync_int);
    //    ASSERT_EQ(44, **sync_int);
    ASSERT_EQ(44, *sync_int.take());


    auto weak_shared(shared_int.weak());
    Weak<Shared<int>> weak_shared1(shared_int.weak());
    Weak<Shared<int>> weak_shared2 = weak_shared1;
    auto weak_shared3 = shared_int.weak();

    Weak<Shared<int>> weak_shared4 = shared_int.weak();


    ASSERT_EQ(3, sync_int.use_count()) << sync_int.use_count();
    ASSERT_NO_THROW(sync_int.weak());
    Weak<Shared<int, SyncSingleThread>> weak_sync1 = sync_int.weak();
    //
    //    //    std::cout << "weak_guard1: " << guard_int.shared_this.use_count() << "\n";
    //
    auto weak_sync2(sync_int.weak());
    //
    //    //    std::cout << "weak_guard2: " << guard_int.shared_this.use_count() << "\n";
    //
    auto weak_sync3 = sync_int.weak();

    //    std::cout << "weak_guard3: " << guard_int.shared_this.use_count() << "\n";


    //template <typename V, typename T = V, typename W = std::weak_ptr<T>> class VarWeak;
    //    ASSERT_NO_THROW(**weak_shared1);
    ASSERT_NO_THROW(*weak_shared1.take());

    Auto<int, Shared<int>::SharedType> auto_shared(weak_shared1.take());
    ASSERT_EQ(3, sync_int.use_count());

    //    auto auto_guard1(weak_guard1.take());

    //    ASSERT_NO_THROW(**sync_int);
    ASSERT_NO_THROW(*sync_int.take());
    {
        auto taken = sync_int.take();
    }

    //    VarAuto<int, VarGuard<int, VarGuardData<int>>> auto_guard2(weak_guard1.take());
    //    std::cout << "auto_guard: " << guard_int.shared_this.use_count() << "\n";

    //    int & take_weak_shared = *auto_shared;
    //    int & take_weak_guard = *auto_guard;

}

#pragma clang attribute push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

class TestClass1 {
public:

    Class<TestClass1> field;
    Class<TestClass1> field_2;

    Shared<TestClass1> m_shared;
    Shared<TestClass1, SyncSingleThread> m_single;
    Shared<TestClass1, SyncTimedMutex> m_mutex;
    Shared<TestClass1, SyncTimedShared> m_recursive;

    TestClass1() : field(*this, this->field), field_2(*this, this->field_2) {
    }
};

#pragma clang attribute pop

TEST(MemSafe, Class) {

    TestClass1 cls;


    // Ignore warning only for unit test from field Shared<TestClass1>
#pragma clang attribute push
#pragma clang diagnostic ignored "-Winvalid-offsetof"

    ASSERT_EQ(0, offsetof(TestClass1, field));
    ASSERT_NE(0, offsetof(TestClass1, field_2));

    ASSERT_EQ((size_t) cls.field.m_instance, (size_t) & cls);
    ASSERT_EQ((size_t) cls.field.m_offset, offsetof(TestClass1, field)) << offsetof(TestClass1, field);

    ASSERT_EQ((size_t) cls.field_2.m_instance, (size_t) & cls);
    ASSERT_EQ(cls.field_2.m_offset, offsetof(TestClass1, field_2)) << offsetof(TestClass1, field_2);

#pragma clang attribute pop


    ASSERT_FALSE(cls.field.m_field.get());
    cls.field = new TestClass1();
    ASSERT_TRUE(cls.field.m_field.get());

    ASSERT_FALSE((*cls.field).field.m_field.get());
    ASSERT_ANY_THROW(
            (*cls.field).field = cls.field; // circular reference
            );
    ASSERT_TRUE(cls.field.m_field.get());

    ASSERT_FALSE((*cls.field).field.m_field.get());
    ASSERT_ANY_THROW(
            (*cls.field).field = cls.field.m_field.get(); // circular reference
            );
    ASSERT_TRUE(cls.field.m_field.get());

    ASSERT_FALSE(cls.field_2.m_field.get());
    ASSERT_ANY_THROW(
            cls.field_2 = cls.field; // Copy of another field
            );
    ASSERT_FALSE(cls.field_2.m_field.get());

};

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
        memsafe::Shared<int, SyncSingleThread> var_single(0);
        bool catched = false;

        std::thread other([&]() {
            try {
                var_single.take();
            } catch (...) {
                catched = true;
            }

        });
        other.join();

        ASSERT_TRUE(catched);
    }

    {
        memsafe::Shared<int, SyncTimedMutex> var_mutex(0);
        bool catched = false;

        std::chrono::duration<double, std::milli> elapsed;

        std::thread other([&]() {
            try {

                const auto start = std::chrono::high_resolution_clock::now();
                std::this_thread::sleep_for(100ms);
                elapsed = std::chrono::high_resolution_clock::now() - start;
                var_mutex.take();
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
        memsafe::Shared<int, SyncTimedShared> var_recursive(0);
        bool not_catched = true;

        std::thread read([&]() {
            try {
                auto a1 = var_recursive.take_const();
                auto a2 = var_recursive.take_const();
                auto a3 = var_recursive.take_const();
            } catch (...) {
                not_catched = false;
            }

        });
        read.join();

        ASSERT_TRUE(not_catched);

        std::thread write([&]() {
            try {
                auto a1 = var_recursive.take();
                auto a2 = var_recursive.take();
                auto a3 = var_recursive.take();
            } catch (...) {
                not_catched = false;
            }

        });
        write.join();

        ASSERT_FALSE(not_catched);

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

    memsafe::Value<int> var_value(1);
    static memsafe::Value<int> var_static(1);

    var_static = var_value;
    {
        var_static = var_value;
        {
            var_static = var_value;
        }
    }

    memsafe::Shared<int> var_shared1(0);
    memsafe::Shared<int> var_shared2(1);

    ASSERT_TRUE(var_shared1);
    ASSERT_TRUE(var_shared2);

    var_shared1 = var_shared2;
    {
        var_shared1 = var_shared2;
        {
            var_shared1 = var_shared2;
        }
    }

    memsafe::Shared<int, SyncSingleThread> var_none(1);
    ASSERT_TRUE(var_none);

    memsafe::Shared<int, SyncTimedMutex> var_mutex(1);
    ASSERT_TRUE(var_mutex);

    memsafe::Shared<int, SyncTimedShared> var_shared(1);
    ASSERT_TRUE(var_shared);

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
        "#warn #208",
        "#err #209",
        "#warn #301",
        "#log #301",
        "#log #301",
        "#warn #302",
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
        "#log #2004",
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
