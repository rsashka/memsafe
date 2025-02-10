#pragma once
#ifndef INCLUDED_MEMSAFE_H_
#define INCLUDED_MEMSAFE_H_

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <iostream>
#include <stdint.h>
#include <stdexcept>
#include <memory>

#include <mutex>
#include <shared_mutex>
#include <thread>

#include <format>

#endif

#ifdef BUILD_UNITTEST
// Removing all restrictions on access to protected and private fields for testing cases
#define SCOPE(scope)  public
#else
#define SCOPE(scope)  scope
#endif   

/**
 * @def SCOPE( scope )
 * 
 * Removing all restrictions on access to protected and private fields for testing cases
 * 
 */


#if defined __has_attribute
#if __has_attribute(memsafe)
#define MEMSAFE_ATTR(...) [[memsafe(__VA_ARGS__)]]
#define MEMSAFE_LINE(number) namespace MEMSAFE_ATTR("line", #number) {}
#endif
#endif

// Disable memory safety plugin attributes 
#ifndef MEMSAFE_ATTR
#define MEMSAFE_ATTR(...)
#define MEMSAFE_LINE(number)
#define MEMSAFE_DISABLE
#endif

#define MEMSAFE_ERR(name) MEMSAFE_ATTR("error", name)
#define MEMSAFE_WARN(name) MEMSAFE_ATTR("warning", name)

#define MEMSAFE_PROFILE(file) MEMSAFE_ATTR("profile", file)
#define MEMSAFE_STATUS(status) MEMSAFE_ATTR("status", status)
#define MEMSAFE_UNSAFE MEMSAFE_ATTR("status", "unsafe")

#define MEMSAFE_SHARED(name) MEMSAFE_ATTR("shared", name)
#define MEMSAFE_AUTO(name) MEMSAFE_ATTR("auto", name)
#define MEMSAFE_INVALIDATE(name) MEMSAFE_ATTR("invalidate", name)


/**
 * @def MEMSAFE_ATTR(...)
 * 
 * Inserts [[memsafe( ... )]] attributes into the С++ code 
 * if they are supported by the compiler 
 * and the library semantic analysis plugin is connected
 * 
 */

/**
 * @def MEMSAFE_LINE(number)
 * 
 * Set the current base line number of [[memsafe( "line", "num" )]] 
 * markers used when debugging the plugin.
 * If you don't use this macro, the line numbers at the debug marker positions 
 * will match the line numbers in the source file
 * 
 * @todo clang-20  error: 'memsafe' attribute annot be applied to a statement
 * To apply custom attributes to expressions, you need to update clang, 
 * as the current release version (19) 
 * does not have this functionality (handleStmtAttribute)
 * 
 */


/**
 * @def MEMSAFE_DISABLE
 * 
 * Defining a macro to disable the code analyzer plugin 
 * (this header file is compiled as normal C++ code without additional analysis by the compiler plugin)
 */

/**
 * @def MEMSAFE_PROFILE("file_name_profile")
 * 
 * Select safety profile *(load from file not implemented)*.
 * Empty mane - reset to default profile.
 */

/**
 * @def MEMSAFE_ERR("full:class::name")
 * 
 * The fully qualified name of the class that will generate the error message 
 * when used (or when using a class derived from the specified one).
 */

/**
 * @def MEMSAFE_WARN("full:class::name")
 * 
 * The fully qualified name of the class that will generate the warning message 
 * when used (or when using a class derived from the specified one).
 */

/**
 * @def MEMSAFE_SHARED("full:class::name")
 * 
 * The fully qualified name of a class that contains a strong pointer
 * to shared data whose ownership is to be controlled at the syntax level.
 */

/**
 * @def MEMSAFE_AUTO("full:class::name")
 * 
 * The fully qualified name of a class that contains a strong pointer 
 * to an automatic (temporary) variable whose ownership must be controlled at the syntax level.
 */

/**
 * @def MEMSAFE_INVALIDATE("unsafe data type")
 * 
 * An unsafe data type that can be corrupted if data in the underlying object is modified.
 */


namespace memsafe { // Begin define memory safety classes

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    class memsafe_error : public std::runtime_error {
    public:
        template <class... Args>
        memsafe_error(std::format_string<Args...> fmt, Args&&... args) : std::runtime_error{std::format(fmt, args...)}
        {
        }
    };

#endif

    // Pre-definition of templates 

    // Wrapper around the standard std::shared_ptr and std::unique_ptr template operators to throw an exception when accessing nullptr
    template <typename V> class shared_ptr;
    template <typename V> class unique_ptr;

    // Base virtual classes of multithreaded synchronization primitives with a uniform interface
    template <typename V> class VarSync;
    // A class without a synchronization primitive, but with the ability to work only in one application thread
    template <typename V> class VarSyncNone;
    // Class with mutex for simple multithreaded synchronization
    template <typename V> class VarSyncMutex;
    // Recursive shared_mutex for multi-thread synchronization for exclusive read/write lock or shared read-only lock
    template <typename V> class VarSyncRecursiveShared;

    // Template class for a shared variable
    template <typename V> class VarShared;
    // Template class for guard variable
    template <typename V, template <typename> typename S = VarSyncNone > class VarGuard;
    // Template class for variable with weak reference
    template <typename V, typename T> class VarWeak;

    /*
     * Basic abstract interface template for multithreaded synchronization for data access
     */

    template <typename V>
    class VarSync {
    public:

        typedef std::chrono::milliseconds TimeoutType;
        static constexpr TimeoutType SyncTimeoutDeedlock = std::chrono::milliseconds(5000);

    public://protected:
        V data;

        inline void check_timeout(const VarSync<V>::TimeoutType &timeout) {
            if (&timeout != &VarSync<V>::SyncTimeoutDeedlock) {
                throw memsafe_error("Timeout is not applicable for this object type!");
            }
        }

    public:

        VarSync(V v) : data(v) {
        }

        virtual void unlock() = 0;
        virtual bool try_lock(const TimeoutType &timeout = SyncTimeoutDeedlock) = 0;

        virtual void unlock_shared() = 0;
        virtual bool try_lock_shared(const TimeoutType &timeout = SyncTimeoutDeedlock) = 0;
    };

    /**
     * Temporary variable - owner of object reference (dereferences weak pointers 
     * and increments ownership count of strong references).
     * 
     * Always contains only valid data (a valid reference and a captured synchronization object), 
     * otherwise an exception will be thrown when attempting to create the object.
     * 
     * A helper template class wrapper for managing the lifetime 
     * of captured variables and releasing their ownership in the destructor.   
     * Similar to class and performs same functions as https://en.cppreference.com/w/cpp/thread/lock_guard
     * 
     * V - Variable value type
     * T - The type of the specific captured variable
     */
    template <typename V, typename T, template <typename> typename S = VarSync >
    class VarAuto {
    protected:
        T taken; // memsafe::shared_ptr<V> or memsafe::shared_ptr<VarGuard<V>>
    public:

        VarAuto(T arg, const VarSync<V>::TimeoutType &timeout = VarSync<V>::SyncTimeoutDeedlock);

        virtual ~VarAuto();

        inline V& operator*();
        inline const V& operator*() const;
    };

    /**
     * Base template class - interface for accessing the value of a variable using @ref VarAuto
     */
    template <typename V, typename T>
    class VarInterface {
    public:

        VarAuto<V, T> take(const VarSync<V>::TimeoutType &timeout = VarSync<V>::SyncTimeoutDeedlock);
        const VarAuto<V, T> take_const(const VarSync<V>::TimeoutType &timeout = VarSync<V>::SyncTimeoutDeedlock) const;
    };

    /**
     * Variable by value without references.   
     * A simple wrapper over data to unify access to it using VarAuto
     */
    template <typename V>
    class VarValue : public VarInterface<V, V&> {
    protected:
        V value;
    public:

        VarValue(V val) : value(val) {
        }

        inline V& operator*() {
            return value;
        };

        inline const V& operator*() const {
            return value;
        };

        inline VarAuto<V, V&> take() {
            return VarAuto<V, V&> (value);
        }

        inline VarAuto<V, V&> take() const {
            return take_const();
        }

        inline const VarAuto<V, V&> take_const() const {
            return VarAuto<V, V&> (value);
        }
    };

    /**
     * Reference variable (shared pointer) without multithreaded access control
     */

    template <typename V>
    class VarShared : public VarInterface<V, VarShared < V >>, public memsafe::shared_ptr<V>
    {
        public:

        typedef std::weak_ptr<V> WeakType;

        VarShared(VarShared<V> &val) : memsafe::shared_ptr<V>(val) {
        }

        VarShared(V val) : memsafe::shared_ptr<V>(std::make_shared<V>(val)) {
        }

        inline VarAuto<V, VarShared < V >> take(const VarSync<V>::TimeoutType &timeout = VarSync<V>::SyncTimeoutDeedlock) {
            return VarAuto<V, VarShared < V >> (*this, timeout);
        }

        inline const VarAuto<V, VarShared < V >> take_const(const VarSync<V>::TimeoutType &timeout = VarSync<V>::SyncTimeoutDeedlock) const {
            return VarAuto<V, VarShared < V >> (*this, timeout);
        }

        inline V& operator*() {
            if (V * temp = this->template shared_ptr<V>::get()) {
                return *temp;
            }
            throw memsafe_error("null pointer exception");
        }

        inline const V& operator*() const {
            if (const V * temp = this->template shared_ptr<V>::get()) {
                return *temp;
            }
            throw memsafe_error("null pointer exception");
        }

        VarWeak<V, VarShared < V >> weak();

        virtual ~VarShared() {
        }

    };


    /**
     * Reference variable (strong pointer) with control over multi-threaded access to data.   
     * By default, it operates on a single application thread without creating a cross-thread access synchronization object
     * using the @ref VarSyncNone template class with control std::thread::id control 
     * to ensure that data is accessed only on a single application thread.
     */

    template <typename V, template <typename> typename S>
    class VarGuard : public VarInterface<V, VarGuard < V, S >>, public memsafe::shared_ptr<S<V>>, public std::enable_shared_from_this<VarGuard<V, S>>
    {
        public:

        /* https://en.cppreference.com/w/cpp/memory/shared_ptr 
         * All member functions (including copy constructor and copy assignment) can be called by multiple threads 
         * on different instances of shared_ptr  without additional synchronization even 
         * if these instances are copies and share ownership of the same object. 
         * 
         * If multiple threads of execution access the same shared_ptr without synchronization 
         * and any of those accesses uses a non-const member function of shared_ptr then a data race will occur; 
         * the shared_ptr overloads of atomic functions can be used to prevent the data race. 
         * 
         * @todo replace std::shared_ptr<V> to std::atomic<std::shared_ptr<V>>
         */

        typedef std::weak_ptr<S < V>> WeakType;

        public:

        VarGuard(V val) : memsafe::shared_ptr<S < V >> (std::make_shared<S < V >> (val)) {
        }

        /**
         * Read inline value as triviall copyable only
         */
        template < typename = std::enable_if<std::is_trivially_copyable_v < V >> >
                inline V operator*() const {
            //            auto lock = take();
            //            return this->memsafe::shared_ptr<S < V >>::operator*().data;
            auto guard_lock = take_const();
            //            this->get()->lock();
            V result = this->memsafe::shared_ptr<S < V>>::operator*().data;
            //            this->get()->unlock();
            return result;
        }

        inline void set(const V && value) {
            //            auto lock = take();
            //            this->memsafe::shared_ptr<S < V >>::operator*().data = value;

            auto guard_lock = take();
            //            this->get()->lock();
            this->memsafe::shared_ptr<S < V>>::operator*().data = value;
            //            this->get()->unlock();//////
        }

        VarAuto<V, VarGuard < V, S >, S> take(const VarSync<V>::TimeoutType &timeout = VarSync<V>::SyncTimeoutDeedlock) {
            return VarAuto<V, VarGuard <V, S >, S> (*this, timeout);
        }

        inline VarAuto<V, VarGuard < V, S >, S> take(const VarSync<V>::TimeoutType &timeout = VarSync<V>::SyncTimeoutDeedlock) const {
            return take_const(timeout);
        }

        VarAuto<V, VarGuard < V, S >, S> take_const(const VarSync<V>::TimeoutType &timeout = VarSync<V>::SyncTimeoutDeedlock) const {
            return VarAuto<V, VarGuard <V, S >, S> (*this, timeout);
        }

        VarWeak<V, VarGuard<V, S >> weak();

        virtual ~VarGuard() {
        }
    };

    /**
     * A wrapper class template for storing weak pointers to shared and guard variables
     * 
     */

    template <typename V, typename T = VarShared<V>>
    class VarWeak : public VarInterface<V, T>, public T::WeakType {
    public:

        VarWeak(T & ptr) : T::WeakType(ptr) {
        }

        VarWeak(const VarWeak & old) : T::WeakType(old) {
        }


        auto take(const VarSync<V>::TimeoutType &timeout = VarSync<V>::SyncTimeoutDeedlock) -> typename std::conditional<std::is_base_of_v<VarShared<V>, T>,
        VarAuto<V, VarShared < V >>,
        VarAuto<V, VarGuard< V >>>::type
        {
            if constexpr (std::is_base_of_v<VarShared<V>, T>) {
                return VarAuto<V, VarShared < V >> (*this->lock(), timeout);
            } else {
                return VarAuto<V, VarGuard < V >> (*this->lock(), timeout);
            }
        }

        auto take_const(const VarSync<V>::TimeoutType &timeout = VarSync<V>::SyncTimeoutDeedlock) const -> typename std::conditional<std::is_base_of_v<VarShared<V>, T>,
        VarAuto<V, VarShared < V >>,
        VarAuto<V, VarGuard< V >>>::type
        {
            if constexpr (std::is_base_of_v<VarShared<V>, T>) {
                return VarAuto<V, VarShared < V >> (*this->lock(), timeout);
            } else {
                return VarAuto<V, VarGuard < V >> (*this->lock(), timeout);
            }
        }

        virtual ~VarWeak() {
        }
    };

    /*
     * 
     * 
     */

    template <typename V, typename T, template <typename> typename S>
    inline V & VarAuto<V, T, S>::operator*() { /// Это значение (адрес)
        if constexpr (std::is_base_of_v<VarShared<V>, T>) {
            return *taken;
        } else if constexpr (std::is_base_of_v< VarGuard<V, S>, T>) {
            return taken.guard->data;
        } else {
            return taken;
        }
    };

    template <typename V, typename T, template <typename> typename S>
    inline const V & VarAuto<V, T, S>::operator*() const { /// Это значение (адрес)
        if constexpr (std::is_base_of_v<VarGuard<V, S>, T>) {
            return taken.guard->data;
        } else if constexpr (std::is_base_of_v<VarShared<V>, T>) {
            return *taken;
        } else {
            return taken;
        }
    };

    template <typename V, typename T, template <typename> typename S >
    VarAuto<V, T, S>::VarAuto(T arg, const VarSync<V>::TimeoutType &timeout) : taken(arg) {
        if constexpr (std::is_base_of_v<VarGuard<V, S>, T>) {
            // Only guard variables need to be locked.
            if (!taken->try_lock(timeout)) {
                throw memsafe_error("try_lock timeout");
            }
        }
    };

    template <typename V, typename T, template <typename> typename S >
    VarAuto<V, T, S>::~VarAuto() {
        if constexpr (std::is_base_of_v<VarGuard<V, S>, T>) {
            // Only guard variables need to be unlocked.
            taken->unlock();
        }
    };

    /*
     * 
     * 
     */

    template <typename V, template <typename> typename S >
    inline VarWeak<V, VarGuard<V, S >> VarGuard<V, S>::weak() {
        return VarWeak<V, VarGuard<V, S >> (*this);
    }

    template <typename V>
    inline VarWeak<V, VarShared < V >> VarShared<V>::weak() {
        return VarWeak<V, VarShared < V >> (*this);
    }

    /**
     * A class without a synchronization primitive and with the ability to work only in one application thread.   
     * Used to control access to data from only one application thread without creating a synchronization object between threads.
     */
    template <typename V>
    class VarSyncNone : public VarSync<V> {
        SCOPE(protected) :
        const std::thread::id m_thread_id;

        inline void check_thread() {
            if (m_thread_id != std::this_thread::get_id()) {
                throw memsafe_error("Using a single thread variable in another thread!");
            }
        }

    public:

        VarSyncNone(V data) : VarSync<V>(data), m_thread_id(std::this_thread::get_id()) {
        }

        inline bool try_lock(const VarSync<V>::TimeoutType &timeout) override final {
            VarSync<V>::check_timeout(timeout);
            check_thread();
            return true;
        }

        inline void unlock() override final {
            check_thread();
        }

        inline bool try_lock_shared(const VarSync<V>::TimeoutType &timeout) override final {
            VarSync<V>::check_timeout(timeout);
            check_thread();
            return true;
        }

        inline void unlock_shared() override final {
            check_thread();
        }
    };

    /**
     * Class with mutex for simple multithreaded synchronization
     */

    template <typename V>
    class VarSyncMutex : public VarSync<V>, protected std::mutex {
    public:

        VarSyncMutex(V v) : VarSync<V>(v) {
        }

        inline bool try_lock(const VarSync<V>::TimeoutType &timeout) override final {
            VarSync<V>::check_timeout(timeout);
            std::mutex::lock();
            return true;
        }

        inline void unlock() override final {
            std::mutex::unlock();
        }

        inline bool try_lock_shared(const VarSync<V>::TimeoutType &timeout) override final {
            VarSync<V>::check_timeout(timeout);
            std::mutex::lock();
            return true;
        }

        inline void unlock_shared() override final {
            std::mutex::unlock();
        }
    };

    /**
     * Recursive shared_mutex for multi-thread synchronization for exclusive read/write lock or shared read-only lock
     */

    template <typename V>
    class VarSyncRecursiveShared : public VarSync<V>, protected std::shared_timed_mutex {
        SCOPE(protected) :
        const std::chrono::milliseconds m_timeout;
    public:

        VarSyncRecursiveShared(V data, std::chrono::milliseconds deadlock = VarSyncRecursiveShared::SyncTimeoutDeedlock)
        : VarSync<V>(data), m_timeout(deadlock) {
        }

        inline bool try_lock(const VarSync<V>::TimeoutType &timeout) override final {
            return std::shared_timed_mutex::try_lock_for(&timeout == &VarSync<V>::SyncTimeoutDeedlock ? m_timeout : timeout);
        }

        inline void unlock() override final {
            std::shared_timed_mutex::unlock();
        }

        inline bool try_lock_shared(const VarSync<V>::TimeoutType &timeout) override final {
            return std::shared_timed_mutex::try_lock_shared_for(&timeout == &VarSync<V>::SyncTimeoutDeedlock ? m_timeout : timeout);
        }

        inline void unlock_shared() override final {
            std::shared_timed_mutex::unlock_shared();
        }
    };

    /**
     * Wrapper over the standard std::shared_ptr template to throw exceptions when accessing nullptr
     * 
     * https://en.cppreference.com/w/cpp/memory/shared_ptr/operator%2A
     * Dereferences the stored pointer. The behavior is undefined if the stored pointer is null. 
     */

    template <typename V>
    class shared_ptr : public std::shared_ptr<V> {
    public:

        inline V* operator->() {
            if (V * temp = std::shared_ptr<V>::get()) {
                return temp;
            }
            throw memsafe_error("null pointer exception");
        }

        inline const V* operator->() const {
            if (V * temp = std::shared_ptr<V>::get()) {
                return temp;
            }
            throw memsafe_error("null pointer exception");
        }

        inline V& operator*() {
            if (V * temp = std::shared_ptr<V>::get()) {
                return *temp;
            }
            throw memsafe_error("null pointer exception");
        }

        inline const V& operator*() const {
            if (V * temp = std::shared_ptr<V>::get()) {
                return *temp;
            }
            throw memsafe_error("null pointer exception");
        }
    };

    /**
     * Wrapper over the standard std::unique_ptr template to throw exceptions when accessing nullptr
     * operator* and operator-> provide access to the object owned by *this.
     * 
     * https://en.cppreference.com/w/cpp/memory/unique_ptr/operator%2A
     * The behavior is undefined if get() == nullptr.
     * These member functions are only provided for unique_ptr for the single objects i.e. the primary template. 
     */

    template <typename V>
    class unique_ptr : public std::unique_ptr<V> {
    public:

        inline V* operator->() {
            if (V * temp = std::unique_ptr<V>::get()) {
                return temp;
            }
            throw memsafe_error("null pointer exception");
        }

        inline const V* operator->() const {
            if (V * temp = std::unique_ptr<V>::get()) {
                return temp;
            }
            throw memsafe_error("null pointer exception");
        }

        inline V& operator*() {
            if (V * temp = std::unique_ptr<V>::get()) {
                return *temp;
            }
            throw memsafe_error("null pointer exception");
        }

        inline const V& operator*() const {
            if (V * temp = std::unique_ptr<V>::get()) {
                return *temp;
            }
            throw memsafe_error("null pointer exception");
        }
    };

    /*
     * 
     * 
     * 
     */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    class LazyCallerInterface {
    public:
        auto operator*();
        auto operator*() const;
    };

    //std::tuple_element_t<N, std::tuple<InputPortTypes...>>

    //    template <typename T, typename R, typename ... Args> class LazyCaller;

    template<int...> struct index_tuple {
    };

    template<int I, typename IndexTuple, typename... Types>
    struct make_indexes_impl;

    template<int I, int... Indexes, typename T, typename ... Types>
    struct make_indexes_impl<I, index_tuple<Indexes...>, T, Types...> {
        typedef typename make_indexes_impl<I + 1, index_tuple<Indexes..., I>, Types...>::type type;
    };

    template<int I, int... Indexes>
    struct make_indexes_impl<I, index_tuple<Indexes...> > {
        typedef index_tuple<Indexes...> type;
    };

    template<typename ... Types>
    struct make_indexes : make_indexes_impl<0, index_tuple<>, Types...> {
    };

    template<typename T, typename R, typename ... Args, int... Indexes >
    R call_helper(T &obj, R(T::*method)(Args...), index_tuple< Indexes... >, std::tuple<Args...>&& tup) {
        return (obj.*method)(std::forward<Args>(std::get<Indexes>(tup))...);
    }

    template<typename T, typename R, typename ... Args>
    R call(T &obj, R(T::*method)(Args...), const std::tuple<Args...>& tup) {
        return call_helper(obj, method, typename make_indexes<Args...>::type(), std::tuple<Args...>(tup));
    }

    template<typename T, typename R, typename ... Args>
    R call(T &obj, R(T::*method)(Args...), std::tuple<Args...>&& tup) {
        return call_helper(obj, method, typename make_indexes<Args...>::type(), std::forward<std::tuple<Args...>>(tup));
    }


#ifdef _MSC_VER // Microsoft compilers
#define EXPAND(x) x
#define __NARGS(_1, _2, _3, _4, _5, _6, _7, _8, _9, VAL, ...) VAL
#define NARGS_1(...) EXPAND(__NARGS(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#define AUGMENTER(...) unused, __VA_ARGS__
#define NARGS(...) NARGS_1(AUGMENTER(__VA_ARGS__))
#else // Others
#define NARGS(...) __NARGS(0, ## __VA_ARGS__, 9,8,7,6,5,4,3,2,1,0)
#define __NARGS(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,N,...) N
#endif

    static_assert(NARGS() == 0);
    static_assert(NARGS(A) == 1);
    static_assert(NARGS(A, B) == 2);
    static_assert(NARGS(A, B, C) == 3);
    static_assert(NARGS(A, B, C, D) == 4);
    static_assert(NARGS(A, B, C, D, E) == 5);
    static_assert(NARGS(A, B, C, D, E, F) == 6);
    static_assert(NARGS(A, B, C, D, E, F, G) == 7);
    static_assert(NARGS(A, B, C, D, E, F, G, X) == 8);
    static_assert(NARGS(A, B, C, D, E, F, G, X, Y) == 9);


#define DECLTYPE_EXPAND_0()
#define DECLTYPE_EXPAND_1(arg1) decltype(arg1)
#define DECLTYPE_EXPAND_2(arg1, arg2)  decltype(arg1), decltype(arg2)
#define DECLTYPE_EXPAND_3(arg1, arg2, arg3) decltype(arg1), decltype(arg2), decltype(arg3)
#define DECLTYPE_EXPAND_4(arg1, arg2, arg3, arg4) decltype(arg1), decltype(arg2), decltype(arg3), decltype(arg4)
#define DECLTYPE_EXPAND_5(arg1, arg2, arg3, arg4, arg5) decltype(arg1), decltype(arg2), decltype(arg3), decltype(arg4), decltype(arg5)
#define DECLTYPE_EXPAND_6(arg1, arg2, arg3, arg4, arg5, arg6) decltype(arg1), decltype(arg2), decltype(arg3), decltype(arg4), decltype(arg5), decltype(arg6)
#define DECLTYPE_EXPAND_7(arg1, arg2, arg3, arg4, arg5, arg6, arg7) decltype(arg1), decltype(arg2), decltype(arg3), decltype(arg4), decltype(arg5), decltype(arg6), decltype(arg7)
#define DECLTYPE_EXPAND_8(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) decltype(arg1), decltype(arg2), decltype(arg3), decltype(arg4), decltype(arg5), decltype(arg6), decltype(arg7), decltype(arg8)
#define DECLTYPE_EXPAND_9(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) decltype(arg1), decltype(arg2), decltype(arg3), decltype(arg4), decltype(arg5), decltype(arg6), decltype(arg7), decltype(arg8), decltype(arg9)

#define DECLTYPE_EXPAND_HELPER(count, ...) DECLTYPE_EXPAND_ ## count (__VA_ARGS__)
#define DECLTYPE_EXPAND(count, ...) DECLTYPE_EXPAND_HELPER(count, __VA_ARGS__)
#define DECLTYPE(...) DECLTYPE_EXPAND(NARGS(__VA_ARGS__) __VA_OPT__(,) __VA_ARGS__)

#endif

    /**
     * @def LAZYCALL(variable, method, ...)
     * 
     * Macro for creating a deferred call to a class method. 
     * Used to safely work with data types listed using the macro.
     * 
     * Method arguments cannot be of types from the list @ref MEMSAFE_INVALIDATE
     * 
     * Someday, using static reflection, the same thing can be done without macros.
     * 
     */

#define LAZYCALL(variable, method, ...) LazyCaller<decltype(variable), decltype(std::declval<decltype(variable)>().method(__VA_ARGS__)) __VA_OPT__(,) DECLTYPE(__VA_ARGS__) >(variable, &decltype(variable):: method __VA_OPT__(,) __VA_ARGS__ )

    /**
     * Lazy (deferred) invocation of class methods to safely work 
     * with iterators and other types of addresses 
     * that may become invalid after the underlying object's data has changed.
     * 
     * @ref LAZYCALL()
     */
    template <typename T, typename R, typename ... Args>
    class LazyCaller : public LazyCallerInterface {
        T &object;

        union {
            R(T::*method)(Args...);
            R(T::*method_const)(Args...) const;
        };

        std::tuple<Args...> args;

    public:

        LazyCaller(T &var, R(T::*call)(Args...), Args&&... args) : object(var), method(call), args(std::make_tuple(std::forward<Args>(args)...)) {
        }

        LazyCaller(T &var, R(T::*call)(Args...) const, Args&&... args) : object(var), method_const(call), args(std::make_tuple(std::forward<Args>(args)...)) {
        }

        inline R operator*() {
            return call<T, R, Args...>(object, method, args);
        }

        inline R operator*() const {
            return call<T, R, Args...>(object, method_const, args);
        }
    };


    /*
     * 
     * 
     */

    MEMSAFE_PROFILE(""); // Reset to default profile

    MEMSAFE_ERR("std::auto_ptr");
    MEMSAFE_ERR("std::shared_ptr");
    MEMSAFE_WARN("std::auto_ptr");
    MEMSAFE_WARN("std::shared_ptr");

    MEMSAFE_SHARED("memsafe::VarShared");
    MEMSAFE_AUTO("memsafe::VarAuto");

    MEMSAFE_INVALIDATE("__gnu_cxx::__normal_iterator");
    MEMSAFE_INVALIDATE("std::reverse_iterator");

    /*
     * For a basic_string_view str, pointers, iterators, and references to elements of str are invalidated 
     * when an operation invalidates a pointer in the range [str.data(), str.data() + str.size()).
     */
    MEMSAFE_INVALIDATE("std::basic_string_view");


    // End define memory safety classes
#if !defined(MEMSAFE_DISABLE)    
    MEMSAFE_STATUS("enable"); // Enable memory safety plugin
#endif

} // namespace memsafe


#endif // INCLUDED_MEMSAFE_H_
