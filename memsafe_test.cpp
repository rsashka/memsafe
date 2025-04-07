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
namespace fs = std::filesystem;

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


    EXPECT_EQ(16, sizeof (Sync<int>));
    EXPECT_EQ(24, sizeof (SyncSingleThread<int>));
    EXPECT_EQ(56, sizeof (SyncTimedMutex<int>));
    EXPECT_EQ(72, sizeof (SyncTimedShared<int>));

    EXPECT_EQ(16, sizeof (Sync<uint8_t>));
    EXPECT_EQ(24, sizeof (SyncSingleThread<uint8_t>));
    EXPECT_EQ(56, sizeof (SyncTimedMutex<uint8_t>));
    EXPECT_EQ(72, sizeof (SyncTimedShared<uint8_t>));

    EXPECT_EQ(24, sizeof (Sync<uint64_t>));
    EXPECT_EQ(32, sizeof (SyncSingleThread<uint64_t>));
    EXPECT_EQ(64, sizeof (SyncTimedMutex<uint64_t>));
    EXPECT_EQ(80, sizeof (SyncTimedShared<uint64_t>));

    EXPECT_EQ(99, sizeof (std::array<uint8_t, 99 >));
    EXPECT_EQ(100, sizeof (std::array<uint8_t, 100 >));
    EXPECT_EQ(101, sizeof (std::array<uint8_t, 101 >));

    EXPECT_EQ(112, sizeof (Sync<std::array<uint8_t, 100 >>));
    EXPECT_EQ(120, sizeof (SyncSingleThread<std::array<uint8_t, 100 >>));
    EXPECT_EQ(152, sizeof (SyncTimedMutex<std::array <uint8_t, 100 >>));
    EXPECT_EQ(168, sizeof (SyncTimedShared<std::array<uint8_t, 100 >>));

    EXPECT_EQ(101, sizeof (std::array<uint8_t, 101 >));
    EXPECT_EQ(112, sizeof (Sync<std::array<uint8_t, 101 >>));
    EXPECT_EQ(120, sizeof (SyncSingleThread<std::array<uint8_t, 101 >>));
    EXPECT_EQ(152, sizeof (SyncTimedMutex<std::array <uint8_t, 101 >>));
    EXPECT_EQ(168, sizeof (SyncTimedShared<std::array<uint8_t, 101 >>));


    EXPECT_EQ(16, sizeof (Shared<int>));
    EXPECT_EQ(16, sizeof (Shared<int, SyncSingleThread>));
    EXPECT_EQ(16, sizeof (Shared<int, SyncTimedMutex>));
    EXPECT_EQ(16, sizeof (Shared<int, SyncTimedShared>));

    EXPECT_EQ(16, sizeof (Shared<uint8_t>));
    EXPECT_EQ(16, sizeof (Shared<uint8_t, SyncSingleThread>));
    EXPECT_EQ(16, sizeof (Shared<uint8_t, SyncTimedMutex>));
    EXPECT_EQ(16, sizeof (Shared<uint8_t, SyncTimedShared>));

    EXPECT_EQ(16, sizeof (Shared<uint8_t>));
    EXPECT_EQ(16, sizeof (Shared<uint8_t, SyncSingleThread>));
    EXPECT_EQ(16, sizeof (Shared<uint8_t, SyncTimedMutex>));
    EXPECT_EQ(16, sizeof (Shared<uint8_t, SyncTimedShared>));

    EXPECT_EQ(16, sizeof (Weak<Shared<int>>));
    EXPECT_EQ(16, sizeof (Weak<Shared<int, SyncSingleThread>>));
    EXPECT_EQ(16, sizeof (Weak<Shared<int, SyncTimedMutex>>));
    EXPECT_EQ(16, sizeof (Weak<Shared<int, SyncTimedShared>>));

    EXPECT_EQ(16, sizeof (Locker<int, Shared<int>>));
    EXPECT_EQ(16, sizeof (Locker<int, Shared<int, SyncSingleThread>>));
    EXPECT_EQ(16, sizeof (Locker<int, Shared<int, SyncTimedMutex>>));
    EXPECT_EQ(16, sizeof (Locker<int, Shared<int, SyncTimedShared>>));

    EXPECT_EQ(8, sizeof (Locker<int, int&>));
    EXPECT_EQ(8, sizeof (Locker<uint8_t, uint8_t&>));
    EXPECT_EQ(8, sizeof (Locker<uint16_t, uint16_t&>));
    EXPECT_EQ(8, sizeof (Locker<uint32_t, uint32_t&>));
    EXPECT_EQ(8, sizeof (Locker<uint64_t, uint64_t&>));
}

TEST(MemSafe, Cast) {

    EXPECT_EQ(1, sizeof (bool));
    EXPECT_EQ(1, sizeof (Value<bool>));
    EXPECT_EQ(4, sizeof (int32_t));
    EXPECT_EQ(4, sizeof (Value<int32_t>));
    EXPECT_EQ(8, sizeof (int64_t));
    EXPECT_EQ(8, sizeof (Value<int64_t>));

    Value<int> value_int(0);
    int & take_value = *value_int;
    Locker<int, int&> take_value2 = value_int.lock();

    Shared<int> shared_int(0);

    //    Auto<int, Shared<int>> take_shared = *shared_int;
    Locker<int, Shared<int>::SharedType> take_shared1 = shared_int.lock();
    //    int & take_shared1 = *shared_int;

    ASSERT_EQ(0, *take_shared1);
    ASSERT_EQ(0, *shared_int.lock());
    //    ASSERT_EQ(0, **shared_int);
    *shared_int.lock() = 11;
    //    **shared_int = 11;
    //    ASSERT_EQ(11, **shared_int);
    ASSERT_EQ(11, *shared_int.lock());

    auto var_take_shared = shared_int.lock();
    int & take_shared2 = *var_take_shared;


    Shared<int, SyncSingleThread> sync_int(22);
    Locker<int, Shared<int, SyncSingleThread>::SharedType> take_sync_int = sync_int.lock();
    auto auto_sync_int = sync_int.lock();
    //    auto auto_sync_int = *sync_int;

    ASSERT_EQ(22, *sync_int.lock());
    //    ASSERT_EQ(22, **sync_int);

    *auto_sync_int = 33;
    ASSERT_EQ(33, *auto_sync_int);
    ASSERT_EQ(33, *sync_int.lock());
    //    ASSERT_EQ(33, **sync_int);

    int temp_sync = *sync_int.lock();
    *sync_int.lock() = 44;
    //    **sync_int = 44;

    ASSERT_EQ(33, temp_sync);
    ASSERT_EQ(44, *auto_sync_int);
    //    ASSERT_EQ(44, **sync_int);
    ASSERT_EQ(44, *sync_int.lock());


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
    ASSERT_NO_THROW(*weak_shared1.lock());

    Locker<int, Shared<int>::SharedType> auto_shared(weak_shared1.lock());
    ASSERT_EQ(3, sync_int.use_count());

    //    auto auto_guard1(weak_guard1.take());

    //    ASSERT_NO_THROW(**sync_int);
    ASSERT_NO_THROW(*sync_int.lock());
    {
        auto taken = sync_int.lock();
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
                var_single.lock();
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
                var_mutex.lock();
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
                auto a1 = var_recursive.lock_const();
                auto a2 = var_recursive.lock_const();
                auto a3 = var_recursive.lock_const();
            } catch (...) {
                not_catched = false;
            }

        });
        read.join();

        ASSERT_TRUE(not_catched);

        std::thread write([&]() {
            try {
                auto a1 = var_recursive.lock();
                auto a2 = var_recursive.lock();
                auto a3 = var_recursive.lock();
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

TEST(MemSafe, Separartor) {
    ASSERT_STREQ("", SeparatorRemove("").c_str());
    ASSERT_STREQ("0", SeparatorRemove("0").c_str());
    ASSERT_STREQ("00", SeparatorRemove("0'0").c_str());
    ASSERT_STREQ("0000", SeparatorRemove("0000").c_str());
    ASSERT_STREQ("000000", SeparatorRemove("0_00_000").c_str());
    ASSERT_STREQ("00000000", SeparatorRemove("0'0'0'0'0'0'0'0").c_str());

    EXPECT_STREQ("0", SeparatorInsert(0).c_str());
    EXPECT_STREQ("1", SeparatorInsert(1).c_str());
    EXPECT_STREQ("111", SeparatorInsert(111).c_str());
    EXPECT_STREQ("1'111", SeparatorInsert(1'111).c_str());
    EXPECT_STREQ("11'111", SeparatorInsert(11'111).c_str());
    EXPECT_STREQ("111'111", SeparatorInsert(111'111).c_str());
    EXPECT_STREQ("111_111_111_111", SeparatorInsert(111'111'111'111, '_').c_str());
}

TEST(MemSafe, MemSafeFile) {

    std::string filename = "unittest-circleref.memsafe";

    fs::remove(filename);
    ASSERT_FALSE(fs::exists(filename));

    MemSafeFile::ClassReadType classes;

    {
        MemSafeFile file(filename, "file_empty.cpp");

        file.WriteFile(classes);

        ASSERT_TRUE(fs::exists(filename));
        MemSafeFile::ClassReadType readed;
        ASSERT_NO_THROW(file.ReadFile(readed));
        ASSERT_TRUE(readed.empty());
    }
    {
        MemSafeFile file(filename, "file1.cpp");

        classes["class0"] = {};

        classes["class1"].parents = {
            {
                "ns::class1", "filepos:1"
            },
            {
                "ns::class2", "filepos:2"
            },
        };
        classes["class1"].fields = {
            {
                "ns::field1", "filepos:1"
            },
            {
                "ns::field2", "filepos:2"
            },
        };

        file.WriteFile(classes);

        ASSERT_TRUE(fs::exists(filename));

        {
            MemSafeFile file(filename, "file_read.cpp");

            MemSafeFile::ClassReadType readed;

            ASSERT_NO_THROW(file.ReadFile(readed));

            ASSERT_EQ(2, readed.size());
            ASSERT_EQ(0, readed["class0"].parents.size());
            ASSERT_EQ(0, readed["class0"].fields.size());
            ASSERT_EQ(2, readed["class1"].parents.size());
            ASSERT_EQ(2, readed["class1"].fields.size());

            ASSERT_STREQ("filepos:1", readed["class1"].parents["ns::class1"].c_str());
            ASSERT_STREQ("filepos:2", readed["class1"].parents["ns::class2"].c_str());
            ASSERT_STREQ("filepos:1", readed["class1"].fields["ns::field1"].c_str());
            ASSERT_STREQ("filepos:2", readed["class1"].fields["ns::field2"].c_str());
        }
    }
    {
        MemSafeFile file(filename, "file2.cpp");

        ASSERT_TRUE(fs::exists(filename));


        classes.erase(classes.find("class0"));

        classes["class1"].parents = {
            {
                "ns::class1", "filepos:111"
            },
            {
                "ns::class2", "filepos:222"
            },
        };
        classes["class1"].fields = {
            {
                "ns::field1", "filepos:111"
            },
            {
                "ns::field2", "filepos:222"
            },
        };


        file.WriteFile(classes);

        ASSERT_TRUE(fs::exists(filename));

        MemSafeFile::ClassReadType readed;

        ASSERT_NO_THROW(file.ReadFile(readed));

        ASSERT_EQ(2, readed.size());
        ASSERT_EQ(0, readed["class0"].parents.size());
        ASSERT_EQ(0, readed["class0"].fields.size());
        ASSERT_EQ(2, readed["class1"].parents.size());
        ASSERT_EQ(2, readed["class1"].fields.size());

        ASSERT_STREQ("filepos:1", readed["class1"].parents["ns::class1"].c_str());
        ASSERT_STREQ("filepos:2", readed["class1"].parents["ns::class2"].c_str());
        ASSERT_STREQ("filepos:1", readed["class1"].fields["ns::field1"].c_str());
        ASSERT_STREQ("filepos:2", readed["class1"].fields["ns::field2"].c_str());
    }
    fs::remove(filename);
    filename += ".bak";
    fs::remove(filename);
}

TEST(MemSafe, WeakList) {

    LinkedWeakList<int> int_list;

    ASSERT_TRUE(int_list.empty());
    ASSERT_STREQ("nullptr", int_list.to_string().c_str()) << int_list.to_string();
    ASSERT_EQ(0, int_list.size());
    int_list.push_back(1);
    ASSERT_STREQ("1 -> ", int_list.to_string().c_str()) << int_list.to_string();
    ASSERT_EQ(1, int_list.size());
    int_list.push_back(2);
    ASSERT_STREQ("1 -> 2 -> ", int_list.to_string().c_str()) << int_list.to_string();
    ASSERT_EQ(2, int_list.size());
    int_list.push_back(3);
    ASSERT_STREQ("1 -> 2 -> 3 -> ", int_list.to_string().c_str()) << int_list.to_string();
    ASSERT_EQ(3, int_list.size());
    int_list.push_front(0);
    int_list.push_front(0);
    ASSERT_STREQ("0 -> 0 -> 1 -> 2 -> 3 -> ", int_list.to_string().c_str()) << int_list.to_string();
    ASSERT_EQ(5, int_list.size());
    int_list.pop_front();
    ASSERT_STREQ("0 -> 1 -> 2 -> 3 -> ", int_list.to_string().c_str()) << int_list.to_string();
    ASSERT_EQ(4, int_list.size());
    int_list.pop_back();
    ASSERT_STREQ("0 -> 1 -> 2 -> ", int_list.to_string().c_str()) << int_list.to_string();
    ASSERT_EQ(3, int_list.size());
    ASSERT_FALSE(int_list.empty());
}

TEST(MemSafe, Plugin) {

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
    cmd += " -Xclang -plugin-arg-memsafe -Xclang circleref-disable";
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


    std::vector<std::string> diag({
        "#log #201",
        "#log #201",
        "#log #202",
        "#log #202",
        "#log #202",
        "#err #204",
        "#err #204",
        "#log #207",
        "#log #207",
        "#log #208",
        "#err #209",
        "#log #209",
        "#warn #208",
        "#err #209",
        "#log #301",
        "#log #302",
        "#warn #301",
        "#warn #302",
        "#err #303",
        "#log #303",
        "#log #401",
        "#log #402",
        "#log #901",
        "#log #903",
        "#log #1001",
        "#log #1002",
        "#log #1003",
        "#log #2003",
        "#log #2004",
        "#err #3002",
        "#log #3002",
        "#err #3003",
        "#log #3003",
        "#log #3003",
        "#log #4101",
        "#log #4103",
        "#log #4105",
        "#log #4201",
        "#log #4202",
        "#log #4302",
        "#log #4401",
        "#log #4501",
        "#log #4513",
        "#log #4603",
        "#err #7003",
        "#log #7003",
        "#log #7004",
        "#log #7005",
        "#log #7009",

        //bugfix_11()
        "#log #900011002",
        "#log #900011002",
        "#log #900011003",
        "#warn #900011003",
        "#err #900011004",
        "#log #900011004",

        //bugfix_12()
        "#log #900012002",
        "#log #900012003",
        "#warn #900012003",
        "#err #900012004",
    });

    size_t pos = log_output.find(MEMSAFE_KEYWORD_START_LOG);
    ASSERT_TRUE(pos != std::string::npos);
    std::string log_str = log_output.substr(pos + strlen(MEMSAFE_KEYWORD_START_LOG), log_output.find("\n\n", pos + strlen(MEMSAFE_KEYWORD_START_LOG)) - pos - strlen(MEMSAFE_KEYWORD_START_LOG));

    //    std::cout << "\n" << log_str << "\n\n";

    std::vector< std::string> log;
    SplitString(log_str, '\n', &log);

    ASSERT_TRUE(log.size()) << log_str;

    while (!log.empty() && !diag.empty()) {

        if (log.front().find(diag.front()) != std::string::npos) {
            log.erase(log.begin());
            diag.erase(diag.begin());
        } else {

            ASSERT_TRUE(diag.front().find(" #") != std::string::npos) << diag.front();

            size_t diag_line = atoi(diag.front().data() + diag.front().find(" #") + 2);
            ASSERT_TRUE(diag_line) << diag.front();

            size_t skip = log.front().find(" #");
            ASSERT_TRUE(skip != std::string::npos) << log.front();

            ASSERT_TRUE(log.front().find(" #", skip + 2) != std::string::npos) << log.front();

            size_t log_line = atoi(log.front().data() + log.front().find(" #", skip + 2) + 2);
            ASSERT_TRUE(log_line) << log.front();

            if (log_line > diag_line) {
                ADD_FAILURE() << "In log not found: " << log.front();
                log.erase(log.begin());
            } else if (log_line < diag_line) {
                ADD_FAILURE() << "In diag not found: " << diag.front();
                diag.erase(diag.begin());
            } else {
                ADD_FAILURE() << "Diag expected: \"" << diag.front() << "\" but found \"" << log.front() << "\"";
                log.erase(log.begin());
                diag.erase(diag.begin());
            }
        }
    }

    for (auto &elem : log) {
        ADD_FAILURE() << "Log not found: " << elem;
    }
    for (auto &elem : diag) {
        ADD_FAILURE() << "Diag not found: " << elem;
    }

}

TEST(MemSafe, Cycles) {

    const std::string cmd_base = "clang-20 -std=c++20 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe -Xclang -plugin-arg-memsafe -Xclang log ";


    fs::remove(MemSafeFile::SHARED_SCAN_FILE_DEFAULT);

    const char * file_log = "_cycles.cpp.log";
    fs::remove(file_log);

    std::system(std::format("{} -c _cycles.cpp > _cycles.cpp.log", cmd_base).c_str());


    ASSERT_TRUE(fs::exists(file_log));
    std::ifstream log_file(file_log);

    ASSERT_TRUE(log_file.is_open());

    std::stringstream log_buffer;
    log_buffer << log_file.rdbuf();

    std::string log_output = log_buffer.str();
    log_file.close();


    ASSERT_TRUE(log_output.find("not found in current translation unit.") != std::string::npos);

    ASSERT_FALSE(fs::exists(MemSafeFile::SHARED_SCAN_FILE_DEFAULT));
    fs::remove("_example.cpp.log");

    std::system(std::format("{} -Xclang -plugin-arg-memsafe -Xclang circleref-write -fsyntax-only _example.cpp > _example.cpp.log", cmd_base).c_str());

    ASSERT_TRUE(fs::exists("_example.cpp.log"));
    ASSERT_TRUE(fs::exists(MemSafeFile::SHARED_SCAN_FILE_DEFAULT));


    std::ifstream example_file("_example.cpp.log");
    ASSERT_TRUE(example_file.is_open());

    std::stringstream example_file_buffer;
    example_file_buffer << example_file.rdbuf();

    std::string example_log = example_file_buffer.str();
    example_file.close();

    ASSERT_TRUE(example_log.size() > 100);
    ASSERT_TRUE(example_log.find("The circular reference analyzer requires two passes.") == std::string::npos);

    std::system(std::format("{} -Xclang -plugin-arg-memsafe -Xclang circleref-read -c _cycles.cpp > _cycles.cpp.log", cmd_base).c_str());


    ASSERT_TRUE(fs::exists(file_log));
    std::ifstream log_file2(file_log);

    ASSERT_TRUE(log_file2.is_open());

    std::stringstream log_buffer2;
    log_buffer2 << log_file2.rdbuf();

    std::string log_output2 = log_buffer2.str();
    log_file2.close();



    ASSERT_TRUE(log_output2.size() > 100);
    ASSERT_TRUE(log_output2.find("The circular reference analyzer requires two passes.") == std::string::npos);


    ASSERT_TRUE(!log_output2.empty());

    ASSERT_TRUE(log_output2.find("Enable dump and process logger") != std::string::npos &&
            log_output2.find("unprocessed attribute!") == std::string::npos)
            << log_output2;


    std::vector<std::string> diag({
        "#log #102",
        "#log #103",
        "#log #102",
        "#log #1003",
        "#log #1002",
        "#err #1003",
        "#err #1003",
        "#log #1006",
        "#err #1007",
        "#log #1011",
        "#log #1010",
        "#log #1003",
        "#err #1003",
        "#warn #1011",
        "#log #1014",
        "#err #1007",
        "#log #2009",
        "#log #2002",
        "#log #2005",
        "#err #2005",
        "#log #2005",
        "#log #2004",
        "#log #2009",
        "#err #2009",
        "#err #2005",
        "#log #2009",
        "#log #2008",
        "#log #2005",
        "#err #2005",
        "#err #2009",
        "#log #2013",
        "#log #2012",
        "#log #2009",
        "#log #2012",
        "#warn #2013",
        "#log #2017",
        "#log #2016",
        "#log #2005",
        "#log #2016",
        "#warn #2017",
        "#log #3002",
        "#log #3002",
        "#log #3005",
        "#log #3005",
        "#log #4002",
        "#log #4002",
        "#log #4005",
        "#log #4005",
        "#log #5003",
        "#log #5003",
        "#log #5006",
        "#log #5006",
        "#log #5009",
        "#log #5009",
        "#log #10002",
        "#log #10007",
    });

    size_t pos = log_output2.find(MEMSAFE_KEYWORD_START_LOG);
    ASSERT_TRUE(pos != std::string::npos);
    std::string log_str = log_output2.substr(pos + strlen(MEMSAFE_KEYWORD_START_LOG), log_output2.find("\n\n", pos + strlen(MEMSAFE_KEYWORD_START_LOG)) - pos - strlen(MEMSAFE_KEYWORD_START_LOG));

    //    std::cout << "\n" << log_str << "\n\n";

    std::vector< std::string> log;
    SplitString(log_str, '\n', &log);

    ASSERT_TRUE(log.size()) << log_str;

    while (!log.empty() && !diag.empty()) {

        if (log.front().find(diag.front()) != std::string::npos) {
            log.erase(log.begin());
            diag.erase(diag.begin());
        } else {

            ASSERT_TRUE(diag.front().find(" #") != std::string::npos) << diag.front();

            size_t diag_line = atoi(diag.front().data() + diag.front().find(" #") + 2);
            ASSERT_TRUE(diag_line) << diag.front();

            size_t skip = log.front().find(" #");
            ASSERT_TRUE(skip != std::string::npos) << log.front();

            ASSERT_TRUE(log.front().find(" #", skip + 2) != std::string::npos) << log.front();

            size_t log_line = atoi(log.front().data() + log.front().find(" #", skip + 2) + 2);
            ASSERT_TRUE(log_line) << log.front();

            if (log_line > diag_line) {
                ADD_FAILURE() << "In log not found: " << log.front();
                log.erase(log.begin());
            } else if (log_line < diag_line) {
                ADD_FAILURE() << "In diag not found: " << diag.front();
                diag.erase(diag.begin());
            } else {
                ADD_FAILURE() << "Diag expected: \"" << diag.front() << "\" but found \"" << log.front() << "\"";
                log.erase(log.begin());
                diag.erase(diag.begin());
            }
        }
    }

    for (auto &elem : log) {
        ADD_FAILURE() << "Log not found: " << elem;
    }
    for (auto &elem : diag) {
        ADD_FAILURE() << "Diag not found: " << elem;
    }

    std::string temp_memsafe = "unittest-all.memsafe";

    fs::remove(temp_memsafe);
    ASSERT_TRUE(!fs::exists(temp_memsafe));

    std::system(std::format("{} -Xclang -plugin-arg-memsafe -Xclang circleref-write={} -fsyntax-only _example.cpp > _example.second.log", cmd_base, temp_memsafe).c_str());
    std::system(std::format("{} -Xclang -plugin-arg-memsafe -Xclang circleref-write={} -fsyntax-only _cycles.cpp > _cycles.second.log", cmd_base, temp_memsafe).c_str());

    ASSERT_TRUE(fs::exists(temp_memsafe));

    MemSafeFile file(temp_memsafe, "unit_test.not_file");

    MemSafeFile::ClassReadType readed;

    ASSERT_NO_THROW(file.ReadFile(readed));

    std::string shared = MemSafeFile::to_string(readed);

    ASSERT_STREQ("cycles::ArraySharedInt {std::vector} fields:{}\ncycles::CircleSelf {} fields:{cycles::CircleSelf}\ncycles::CircleSelfUnsafe {} fields:{cycles::CircleSelf}\ncycles::CircleShared {} fields:{cycles::CircleShared}\ncycles::CircleSharedUnsafe {} fields:{cycles::CircleShared}\ncycles::ExtExt {ns::Ext} fields:{}\ncycles::ExtExtExt {cycles::ExtExt} fields:{}\ncycles::SharedArrayInt {std::vector} fields:{}\ncycles::SharedCross {} fields:{cycles::SharedCross2}\ncycles::SharedCross2 {} fields:{cycles::SharedCross}\ncycles::SharedCross2Unsafe {} fields:{cycles::SharedCross}\ncycles::SharedCrossUnsafe {} fields:{cycles::SharedCross2}\ncycles::SharedSingle {} fields:{}\ncycles::SharedSingleInt {} fields:{}\ncycles::SharedSingleIntField {} fields:{cycles::SharedSingleInt}\nns::A {} fields:{ns::Ext}\nns::Ext {} fields:{ns::A}\n", shared.c_str()) << shared;

    fs::remove("_example.second.log");
    fs::remove("_cycles.second.log");
    fs::remove(temp_memsafe);
}

#endif
