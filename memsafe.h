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
#define MEMSAFE_LINE(number) namespace [[memsafe("line",number)]] {}
#endif
#endif

// Disable memory safety plugin attributes 
#ifndef MEMSAFE_ATTR
#define MEMSAFE_ATTR(...)
#define MEMSAFE_LINE(number)
#define MEMSAFE_DISABLE
#endif

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
 * Set the current line number of [[memsafe( "line", num )]] markers used when debugging the plugin.
 * If you don't use this macro, the line numbers in the debug markers will match 
 * the line numbers in the source file, e.g. [[memsafe( "line", __LINE__ )]]
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

namespace MEMSAFE_ATTR("define") memsafe { // Begin define memory safety classes

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    class runtime_format : public std::runtime_error {
    public:
        template <class... Args>
        runtime_format(std::format_string<Args...> fmt, Args&&... args) : std::runtime_error{std::format(fmt, args...)}
        {
        }
    };

#endif

    // Pre-definition of templates 

    // Base classes of multithreaded synchronization primitives with a uniform interface for future use
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

    /**
     * A helper template class wrapper for managing the lifetime 
     * of captured variables and releasing their ownership in the destructor.   
     * Similar and performs the same functions as https://en.cppreference.com/w/cpp/thread/lock_guard
     * 
     * V - Variable value type
     * T - The type of the specific captured variable
     */
    template <typename V, typename T>
    class MEMSAFE_ATTR("auto") VarAuto {
    protected:
        T taken; // shared_ptr<V> or shared_ptr<VarGuard<V>>
    public:

        VarAuto(T arg) : taken(arg) {
        }

        inline V& operator*();
        inline const V& operator*() const;

        virtual ~VarAuto();
    };

    /**
     * Base template class - interface for accessing the value of a variable using @ref VarAuto
     */
    template <typename V, typename T>
    class VarInterface {
    public:
        VarAuto<V, T> take();
    };

    /**
     * Variable by value without references.   
     * A simple wrapper over data to unify access to it using VarAuto
     */
    template <typename V>
    class MEMSAFE_ATTR("value") VarValue : public VarInterface<V, V&> {
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
    };

    /**
     * Reference variable (shared pointer) without multithreaded access control
     */

    template <typename V>
    class MEMSAFE_ATTR("shared") VarShared : public VarInterface<V, VarShared < V >>, public std::shared_ptr<V>
    {
        public:

        typedef V ValueType;
        typedef std::shared_ptr<ValueType> SharedType;
        typedef std::weak_ptr<ValueType> WeakType;

        VarShared(VarShared<V> &val) : SharedType(val) {
        }

        VarShared(V val) : SharedType(std::make_shared<ValueType>(val)) {
        }

        VarAuto<V, VarShared < V >> take() {
            return VarAuto<V, VarShared < V >> (*this);
        }

        inline V& operator*() {
            return this->template shared_ptr<V>::operator*();
        }

        inline const V& operator*() const {
            return this->template shared_ptr<V>::operator*();
        }

        VarWeak<V, VarShared < V >> weak();

        virtual ~VarShared() {
        }
    };


    /**
     * Reference variable (strong pointer) with control over multi-threaded access to data.   
     * By default, it operates on a single application thread without creating a cross-thread access synchronization object
     * using the @ref VarSyncNone template class with control 
     * flow identity control to ensure that data is accessed only on a single application thread.
     */

    template <typename V, template <typename> typename S>
    class MEMSAFE_ATTR("shared") VarGuard : public VarInterface<V, VarGuard < V, S >>, public std::shared_ptr<S<V>>, public std::enable_shared_from_this<VarGuard<V, S>>
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

        typedef std::shared_ptr<S < V>> SharedType;
        typedef std::weak_ptr<S < V>> WeakType;

        public:

        VarGuard(V val) : std::shared_ptr<S < V >> (std::make_shared<S < V >> (val)) {
        }

        /**
         * Read inline value as triviall copyable only
         */
        template < typename = std::enable_if<std::is_trivially_copyable_v < V >> >
                inline V operator*() const {
            this->get()->lock();
            V result = this->template shared_ptr<S < V>>::operator*().data;
            this->get()->unlock();
            return result;
        }

        inline void set(const V && value) {
            this->get()->lock();
            this->template shared_ptr<S < V>>::operator*().data = value;
            this->get()->unlock();
        }

        //        const V& operator*() const {
        //            //            static_assert(std::is_trivially_copyable<V>::value, "To be able to directly retrieve a value, the variable type must be trivially copyable.")
        //            //            guard->sync.lock();
        //            //            V result = guard->data;
        //            //            guard->sync.unlock();
        //            //            return result;
        //            //            return this->template shared_ptr<V>::operator*();
        //            this->get()->lock();
        //            this->get()->unlock();
        //            return this->template shared_ptr<S < V>>::operator*().data;
        //            //            return guard->data;
        //        }




        //        template <typename = std::enable_if<std::is_trivially_copyable<V>::value>>
        //        const V & operator*() const {
        //            guard.sync.lock();
        //            V result = *guard.data;
        //            guard.sync.unlock();
        //            return std::move(result);
        //        }




        //        VarGuard(V val, RefType type) : data(std::make_shared<V>(val)), guard(nullptr) { //std::make_shared<G>(val)
        //        }
        //
        //        VarGuard(V val, std::string_view mode) : VarGuard(val, RefTypeFromString(mode)) {
        //        }

        VarAuto<V, VarGuard < V, S >> take() {
            //            if constexpr (std::is_base_of_v<VarInterface<V, T>, T>) {
            //                return this->get()->VarShared<V>::take();
            //            }
            return VarAuto<V, VarGuard <V, S >> (*this);
        }

        //        virtual VarAuto<V, T> take() override final { //
        //            // Захатывать владение, а при удалении VarAuto освобождать
        //            return VarAuto<V, VarShared<V>>(this->VarShared<V>::take());
        //        }


        VarWeak<V, VarGuard<V, S >> weak();

        virtual ~VarGuard() {
            //            std::cout << shared_this.use_count() << "!!!!!\n";
            //            std::cout.flush();
            //            assert(shared_this.use_count() == 2);
            //            shared_this.reset();
        }
    };

    /**
     * A wrapper class template for storing weak pointers to shared and guard variables
     * 
     */

    template <typename V, typename T = VarShared<V>>
    class MEMSAFE_ATTR("weak") VarWeak : public VarInterface<V, T>, public T::WeakType {
    public:

        //        T::WeakType weak;

        // Слабый указатель на????
        // Для VarShared на данные
        // Для VarGuard на сам VarGuard ?????

        VarWeak(T & ptr) : T::WeakType(ptr) {
        }

        //        explicit VarWeak(const VarShared<V> & ptr) : T::WeakType(ptr) {
        //        }
        //
        //        template <typename D, template <typename> typename S >
        //        VarWeak(const VarGuard<D, S> & ptr) : weak(ptr.guard) {
        //        }
        //        VarWeak() : VarWeak(nullptr) {
        //        }

        VarWeak(const VarWeak & old) : T::WeakType(old) {
        }

        //        VarWeak<typename V, typename T>() = default;
        //        VarWeak& operator=(VarWeak&& other) = default;
        //        VarWeak(VarWeak&& other) = default;

        //        VarWeak(const VarWeak<V, std::weak_ptr<V>> & obj) : weak(obj.ptr) {
        //        }
        //        inline T& operator*() override final {
        //            return *(this->lock());
        //            //            //            if constexpr (std::is_same<VarShared<T>, T> (T)) {
        //            //            //                return *static_cast<VarShared < T >> (this);
        //            //            //            } else if constexpr (std::is_same<VarShared < T >> (T)) {
        //            //            //                return *static_cast<VarGuard < T >> (this);
        //            //            //            }
        //        }


        //        memsafe::VarWeak<int, memsafe::VarGuard<int>>::take

        //        VarAuto<V, VarShared<V>> take() {// override final
        //            if constexpr (std::is_same_v<std::weak_ptr<V>, W>) {
        //                return VarAuto<V, VarShared < V >> (VarShared<V>(weak.lock()));
        //                //                return VarAuto<V, VarShared<V>>(weak.lock());
        //            } else {
        //                //                static_assert(std::is_same_v<std::weak_ptr<VarGuard <V>>, W>);
        //                //                return weak.lock()->take();
        //            }
        //        }

        //        VarAuto<V, typename T::SharedType> take() {
        //        }

        auto take() -> typename std::conditional<std::is_base_of_v<VarShared<V>, T>,
        VarAuto<V, VarShared < V >>,
        VarAuto<V, VarGuard< V >>>::type
        {
            if constexpr (std::is_base_of_v<VarShared<V>, T>) {
                assert(this->lock());
                return VarAuto<V, VarShared < V >> (VarShared<V>(*this->lock()));
            } else {
                assert(this->lock());
                return VarAuto<V, VarGuard < V >> (*this->lock());
            }
        }



        //        inline W & operator*() {//override final
        //            return *take();
        //        }

        virtual ~VarWeak() {
        }
    };

    /*
     * 
     * 
     */

    template <typename V, typename T>
    inline V & VarAuto<V, T>::operator*() { /// Это значение (адрес)
        if constexpr (std::is_base_of_v<VarShared<V>, T>) {
            return *taken;
        } else if constexpr (std::is_base_of_v< VarGuard<V>, T>) {
            return taken.guard->data;
            //            //            return *taken;
            //            //        } else if constexpr (std::is_base_of_v<VarGuard<V>, T>) {
            //            return *taken;
            //        } else if constexpr (std::is_base_of_v<VarShared<V>, T>) {
            //            //            return *taken;
            //            //        } else if constexpr (std::is_base_of_v<VarGuard<V>, T>) {
            //            return *taken;
        } else {
            return taken;
        }
    };

    template <typename V, typename T>
    inline const V & VarAuto<V, T>::operator*() const { /// Это значение (адрес)
        if constexpr (std::is_base_of_v<VarGuard<V>, T>) {
            return taken.guard->data;
        } else if constexpr (std::is_base_of_v<VarShared<V>, T>) {
            return *taken;
            //            //            return *taken;
            //            //        } else if constexpr (std::is_base_of_v<VarGuard<V>, T>) {
            //            return *taken;
            //        } else if constexpr (std::is_base_of_v<VarShared<V>, T>) {
            //            //            return *taken;
            //            //        } else if constexpr (std::is_base_of_v<VarGuard<V>, T>) {
            //            return *taken;
        } else {
            return taken;
        }
    };


    //    template <typename V, typename T>
    //    inline T & VarAuto<V, T>::take();
    //    { /// Это захваченный класс
    //        if constexpr (std::is_base_of_v<VarInterface<V>, T>) {
    //            return taken.take();
    //        }
    //        return VarAuto<V, T>(taken);
    //    };

    template <typename T, typename V >
    VarAuto<T, V>::~VarAuto() {
        if constexpr (std::is_base_of_v<VarGuard<V>, T>) {
            // блокировку нужно снимать только для защищенных переменных
            taken.unlock();
        }
    };

    /*
     * 
     * 
     */

    template <typename V, template <typename> typename S >
    inline VarWeak<V, VarGuard<V, S>> VarGuard<V, S>::weak() {
        return VarWeak<V, VarGuard<V, S >> (*this);
    }

    template <typename V>
    inline VarWeak<V, VarShared<V>> VarShared<V>::weak() {
        return VarWeak<V, VarShared < V >> (*this);
    }

    /*
     * Base classes of multithreaded synchronization primitives with a uniform interface for future use
     */

    template <typename V>
    class VarSync {
    public:

        typedef std::chrono::milliseconds TimeoutType;
        static constexpr TimeoutType SyncTimeoutDeedlock = std::chrono::milliseconds(5000);

    public://protected:
        V data;

        inline void check_timeout(const VarSync<V>::TimeoutType &timeout) {
            if (&timeout == &VarSync<V>::SyncTimeoutDeedlock) {
                throw runtime_format("Timeout is not applicable for this object type!");
            }
        }

    public:

        VarSync(V v) : data(v) {
        }

        virtual void lock() = 0;
        virtual void unlock() = 0;
        virtual bool try_lock(const TimeoutType &timeout = SyncTimeoutDeedlock) = 0;

        virtual void lock_shared() = 0;
        virtual void unlock_shared() = 0;
        virtual bool try_lock_shared(const TimeoutType &timeout = SyncTimeoutDeedlock) = 0;
    };

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
                throw runtime_format("Using a single thread variable in another thread!");
            }
        }

    public:

        VarSyncNone(V data) : VarSync<V>(data), m_thread_id(std::this_thread::get_id()) {
        }

        void lock() override final {
            check_thread();
        }

        void unlock() override final {
            check_thread();
        }

        bool try_lock(const VarSync<V>::TimeoutType &timeout) override final {
            VarSync<V>::check_timeout(timeout);
            check_thread();
            return true;
        }

        void lock_shared() override final {
            check_thread();
        }

        void unlock_shared() override final {
            check_thread();
        }

        bool try_lock_shared(const VarSync<V>::TimeoutType &timeout) override final {
            VarSync<V>::check_timeout(timeout);
            check_thread();
            return true;
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

        inline void lock() override final {
            std::mutex::lock();
        }

        inline void unlock() override final {
            std::mutex::unlock();
        }

        inline bool try_lock(const VarSync<V>::TimeoutType &timeout) override final {
            VarSync<V>::check_timeout(timeout);
            std::mutex::lock();
            return true;
        }

        inline void lock_shared() override final {
            std::mutex::lock();
        }

        inline void unlock_shared() override final {
            std::mutex::unlock();
        }

        inline bool try_lock_shared(const VarSync<V>::TimeoutType &timeout) override final {
            VarSync<V>::check_timeout(timeout);
            std::mutex::lock();
            return true;
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

        inline void lock() override final {
            if (!std::shared_timed_mutex::try_lock_for(m_timeout)) {
                throw runtime_format("VarSyncRecursiveShared lock() deadlock!");
            }
        }

        inline void unlock() override final {
            std::shared_timed_mutex::unlock();
        }

        inline bool try_lock(const VarSync<V>::TimeoutType &timeout) override final {
            return std::shared_timed_mutex::try_lock_for(&timeout == &VarSync<V>::SyncTimeoutDeedlock ? m_timeout : timeout);
        }

        inline void lock_shared() override final {
            if (!std::shared_timed_mutex::try_lock_shared_for(m_timeout)) {
                throw runtime_format("VarSyncRecursiveShared lock_shared() deadlock!");
            };
        }

        inline void unlock_shared() override final {
            std::shared_timed_mutex::unlock_shared();
        }

        inline bool try_lock_shared(const VarSync<V>::TimeoutType &timeout) override final {
            return std::shared_timed_mutex::try_lock_shared_for(&timeout == &VarSync<V>::SyncTimeoutDeedlock ? m_timeout : timeout);
        }
    };


    // End define memory safety classes
#if !defined(MEMSAFE_DISABLE)    
    namespace MEMSAFE_ATTR("enable") {
    } // Enable memory safety plugin
#endif

} // namespace memsafe


#endif // INCLUDED_MEMSAFE_H_
