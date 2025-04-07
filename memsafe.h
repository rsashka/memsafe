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
#include <set>

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


#define MEMSAFE_KEYWORD_ATTRIBUTE memsafe
#define MEMSAFE_KEYWORD_PROFILE "profile"
#define MEMSAFE_KEYWORD_STATUS "status"
#define MEMSAFE_KEYWORD_UNSAFE "unsafe"
#define MEMSAFE_KEYWORD_PRINT_AST "print-ast"
#define MEMSAFE_KEYWORD_PRINT_DUMP "print-dump"
#define MEMSAFE_KEYWORD_BASELINE "baseline"

#define MEMSAFE_KEYWORD_START_LOG "#memsafe-log\n"
#define MEMSAFE_KEYWORD_START_DUMP "#memsafe-dump\n"

#define MEMSAFE_KEYWORD_ENABLE "enable"
#define MEMSAFE_KEYWORD_DISABLE "disable"
#define MEMSAFE_KEYWORD_PUSH "push"
#define MEMSAFE_KEYWORD_POP "pop"

// maximum diagnostic level of plugin message when error is detected in source code
#define MEMSAFE_KEYWORD_LEVEL "level"
#define MEMSAFE_KEYWORD_ERROR "error"
#define MEMSAFE_KEYWORD_WARNING "warning"
#define MEMSAFE_KEYWORD_NOTE "note"
#define MEMSAFE_KEYWORD_REMARK "remark"
#define MEMSAFE_KEYWORD_IGNORED "ignored"

#define MEMSAFE_KEYWORD_AUTO_TYPE "auto-type"
#define MEMSAFE_KEYWORD_SHARED_TYPE "shared-type"
#define MEMSAFE_KEYWORD_INVALIDATE_FUNC  "invalidate-func"


#if defined __has_attribute
#if __has_attribute( MEMSAFE_KEYWORD_ATTRIBUTE )
#define MEMSAFE_ATTR(...) [[ MEMSAFE_KEYWORD_ATTRIBUTE (__VA_ARGS__)]]
#define MEMSAFE_BASELINE(number) MEMSAFE_ATTR( MEMSAFE_KEYWORD_BASELINE, #number) void memsafe_stub();
#endif
#endif

// Disable memory safety plugin attributes 
#ifndef MEMSAFE_ATTR
#define MEMSAFE_ATTR(...)
#define MEMSAFE_BASELINE(number)
#define MEMSAFE_DISABLE
#endif

#ifndef TO_STR
#define TO_STR2(ARG) #ARG
#define TO_STR(ARG) TO_STR2(ARG)
#endif

#define MEMSAFE_PROFILE(file) MEMSAFE_ATTR(MEMSAFE_KEYWORD_PROFILE, file)
#define MEMSAFE_STATUS(status) MEMSAFE_ATTR(MEMSAFE_KEYWORD_STATUS, status)
#define MEMSAFE_DIAG_LEVEL(level) MEMSAFE_ATTR(MEMSAFE_KEYWORD_LEVEL, level)
#define MEMSAFE_UNSAFE MEMSAFE_ATTR(MEMSAFE_KEYWORD_UNSAFE, TO_STR(__LINE__))

#define MEMSAFE_ERROR_TYPE(name) MEMSAFE_ATTR(MEMSAFE_KEYWORD_ERROR "-type", name)
#define MEMSAFE_WARNING_TYPE(name) MEMSAFE_ATTR(MEMSAFE_KEYWORD_WARNING "-type", name)
#define MEMSAFE_AUTO_TYPE(name) MEMSAFE_ATTR(MEMSAFE_KEYWORD_AUTO_TYPE, name)
#define MEMSAFE_SHARED_TYPE(name) MEMSAFE_ATTR(MEMSAFE_KEYWORD_SHARED_TYPE, name)
#define MEMSAFE_INVALIDATE_FUNC(name) MEMSAFE_ATTR(MEMSAFE_KEYWORD_INVALIDATE_FUNC, name)

#define MEMSAFE_PRINT_AST(filter) MEMSAFE_ATTR(MEMSAFE_KEYWORD_PRINT_AST, filter) void memsafe_stub();
#define MEMSAFE_PRINT_DUMP(filter) MEMSAFE_ATTR(MEMSAFE_KEYWORD_PRINT_DUMP, filter) void memsafe_stub();

/**
 * @def MEMSAFE_ATTR(...)
 * 
 * Inserts [[memsafe( ... )]] attributes into the С++ code 
 * if they are supported by the compiler 
 * and the library semantic analysis plugin is connected
 * 
 */

/**
 * @def MEMSAFE_BASELINE(number)
 * 
 * Set the current base line number of [[memsafe( "baseline", "num" )]] 
 * markers used when debugging the plugin.
 * 
 * If you don't use this macro, the line numbers at the debug marker positions 
 * will match the line numbers in the source file.
 * 
 * Set base line number  is implemented as a set of custom attributes in the 
 * stub function `void memsafe_stub()` forward declaration, as this can be done anywhere in C++ code.
 */

/**
 * @def MEMSAFE_PRINT_AST(...)
 * 
 * Enables or disables the output of expressions from the source file as AST nodes.
 * Expression filter is planned (not implemented yet).
 * 
 * Control over AST dump output is implemented as a set of custom attributes 
 * in the stub function `void memsafe_stub()` forward declaration, as this can be done anywhere in C++ code.
 */

/**
 * @def MEMSAFE_PRINT_DUMP(...)
 * 
 * Outputs the current state of the plugin and its main internal variables, 
 * as well as a stack of code blocks (hierarchies of variable lifetimes) 
 * for debugging and analysis.
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
 * @def MEMSAFE_ERROR_TYPE("full:class::name")
 * 
 * The fully qualified name of the class that will generate the error message 
 * when used (or when using a class derived from the specified one).
 */

/**
 * @def MEMSAFE_WARNING_TYPE("full:class::name")
 * 
 * The fully qualified name of the class that will generate the warning message 
 * when used (or when using a class derived from the specified one).
 */

/**
 * @def MEMSAFE_SHARED_TYPE("full:class::name")
 * 
 * The fully qualified name of a class that contains a strong pointer
 * to shared data whose ownership is to be controlled at the syntax level.
 */

/**
 * @def MEMSAFE_AUTO_TYPE("full:class::name")
 * 
 * The fully qualified name of a class that contains a strong pointer 
 * to an automatic (temporary) variable whose ownership must be controlled at the syntax level.
 */

/**
 * @def MEMSAFE_INVALIDATE_FUNC("unsafe function name")
 * The name of a function that, when passed as an argument to a base object, 
 * always causes previously obtained references and iterators to be invalidated,
 * regardless of the default settings @ref MEMSAFE_NONCONST_ARG
 * such as std::swap, std::move  etc.
 */


namespace memsafe { // Begin define memory safety classes

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    class memsafe_error : public std::runtime_error {
    public:

        memsafe_error(std::string_view msg) : std::runtime_error(msg.begin()) {
        }

    };

#endif

    // Pre-definition of template class for variable with weak reference
    template <typename T> class Weak;

    /*
     * Base class for shared data with the ability to multi-thread synchronize access
     */

    typedef std::chrono::milliseconds SyncTimeoutType;
    static constexpr SyncTimeoutType SyncTimeoutDeedlock = std::chrono::milliseconds(5000);

    template <typename V>
    class Sync {
    public:

        V data;

        Sync(V v) : data(v), m_const_lock(false) {
        }

        [[nodiscard]]
        bool TryLock(bool const_lock, const SyncTimeoutType &timeout = SyncTimeoutDeedlock) {
            m_const_lock = const_lock;
            return m_const_lock ? try_lock_const(timeout) : try_lock(timeout);
        }

        void UnLock() {
            if (m_const_lock) {
                unlock_const();
            } else {
                unlock();
            }
        }

    protected:
        bool m_const_lock;

        static void timeout_set_error(const SyncTimeoutType &timeout) {
            if (timeout != SyncTimeoutDeedlock) {
                throw memsafe_error("Timeout is not applicable for this object type!");
            }
        }

        [[nodiscard]]
        virtual bool try_lock(const SyncTimeoutType &timeout = SyncTimeoutDeedlock) {
            timeout_set_error(timeout);
            return true;
        }

        [[nodiscard]]
        virtual bool try_lock_const(const SyncTimeoutType &timeout = SyncTimeoutDeedlock) {
            timeout_set_error(timeout);
            return true;
        }

        virtual void unlock() {
        }

        virtual void unlock_const() {
        }

    };

    /**
     * Temporary (auto) variable - owner of object reference (dereferences weak pointers 
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

    template <typename V, typename T>
    class Locker {
    public:

        Locker(T val) : value(val) {
        }

        inline V& operator*() {
            if constexpr (std::is_reference_v<T>) {
                return value;
            } else {
                static_assert(std::is_convertible_v<T, std::shared_ptr<Sync < V>>>);
                return value->data;
            }
        }

        inline const V& operator*() const {
            if constexpr (std::is_reference_v<T>) {
                return value;
            } else {
                static_assert(std::is_convertible_v<T, std::shared_ptr<Sync < V>>>);
                return value->data;
            }
        }

        inline ~Locker() {
            if constexpr (std::is_convertible_v<T, std::shared_ptr<Sync < V>>>) {
                value->UnLock();
            } else {
                static_assert(std::is_reference_v<T>);
            }
        }

    private:
        T value; // Type T can be a reference to data i.e. V& or a shared_ptr<Sync<V>>

        // Noncopyable
        Locker(const Locker&) = delete;
        Locker& operator=(const Locker&) = delete;
        // Nonmovable
        Locker(Locker&&) = delete;
        Locker& operator=(Locker&&) = delete;
    };

    /**
     * Variable by value without references.   
     * A simple wrapper over data to unify access to it using class Auto
     */
    template <typename V>
    class Value {
    protected:
        V value;
    public:

        Value(V val) : value(val) {
        }

        inline V& operator*() {
            return value;
        };

        inline const V& operator*() const {
            return value;
        };

        inline Locker<V, V&> lock() {
            return Locker<V, V&> (value);
        }

        inline Locker<V, V&> lock() const {
            return lock_const();
        }

        inline const Locker<V, V&> lock_const() const {
            return Locker<V, V&> (value);
        }
    };

    static_assert(std::is_standard_layout_v<Value<int>>);

    /**
     * Reference variable (shared pointer) with optional multi-threaded access control.
     * 
     * By default using template @ref Sync without multithreaded access control.
     * @see @ref SyncSingleThread, @ref SyncTimedMutex, @ref SyncTimedShared  )
     * 
     */

    template <typename V, template <typename> typename S = Sync>
    class Shared : public std::shared_ptr< S<V> > {
    public:

        typedef V ValueType;
        typedef S<V> DataType;
        typedef std::weak_ptr<DataType> WeakType;
        typedef std::shared_ptr<DataType> SharedType;

        Shared() : SharedType(nullptr) {
        }

        Shared(const V & val) : SharedType(std::make_shared<DataType>(val)) {
        }

        Shared(Shared<V, S> &val) : SharedType(val) {
        }

        static Locker<V, SharedType> make_auto(SharedType * shared, bool read_only, const SyncTimeoutType &timeout = SyncTimeoutDeedlock) {
            if constexpr (!std::is_same_v<Sync<V>, DataType>) { // The Sync class is virtual
                if (!shared || !shared->get()) {
                    throw memsafe_error("Object missing (null pointer exception)");
                }
                if (!shared->get()->TryLock(read_only, timeout)) {
                    throw memsafe_error(std::format("try_lock{} timeout", read_only ? " read only" : ""));
                }
            }
            return Locker<V, SharedType> (*shared);
        }

        Locker<V, SharedType> lock(const SyncTimeoutType &timeout = SyncTimeoutDeedlock) {
            return make_auto(this, false, timeout);
        }

        Locker<V, SharedType> lock_const(const SyncTimeoutType &timeout = SyncTimeoutDeedlock) {
            return make_auto(this, true, timeout);
        }

//        template < typename = std::enable_if<std::is_trivially_copyable_v < V >> >
        inline V & operator*() const {
            auto guard_lock = lock_const();
            return *guard_lock;
        }

//        template < typename = std::enable_if<std::is_trivially_copyable_v < V >> >
        inline SharedType & operator=(V && value) {
            auto guard_lock = lock();
            *guard_lock = value;
            return *this;
        }

        inline SharedType & set(V && value, const SyncTimeoutType &timeout = SyncTimeoutDeedlock) {
            auto guard_lock = lock(timeout);
            *guard_lock = value;
            return *this;
        }
        
        //        template < typename = std::enable_if<std::is_trivially_copyable_v < V >> >
        //        inline SharedType & operator=(SharedType & s) {
        //            auto guard_lock = lock_const();
        //            return this->memsafe::shared_ptr<S < V>>::operator*().data;
        //        }
        //

        inline Weak<Shared < V, S >> weak() {
            return Weak<Shared < V, S >> (*this);
        }

        inline explicit operator bool() const noexcept {
            return this->get();
        }
    };

    /**
     * A wrapper template class for storing weak pointers to shared variables
     */

    template <typename T>
    class Weak : public T::WeakType {
    public:

        Weak() : T::WeakType(nullptr) {
        }

        Weak(const T ptr) : T::WeakType(ptr) {
        }

        Weak(Weak & old) : T::WeakType(old) {
        }

        Locker<typename T::ValueType, typename T::SharedType> make_auto(bool read_only, const SyncTimeoutType &timeout = SyncTimeoutDeedlock) {
            typename T::SharedType shared = this->T::WeakType::lock();
            return T::make_auto(&shared, read_only, timeout);
        }

        inline Locker<typename T::ValueType, typename T::SharedType> lock(const SyncTimeoutType &timeout = SyncTimeoutDeedlock) {
            return make_auto(false, timeout);
        }

        inline const Locker<typename T::ValueType, T> lock_const(const SyncTimeoutType &timeout = SyncTimeoutDeedlock) const {
            return make_auto(true, timeout);
        }

//        template < typename = std::enable_if<std::is_trivially_copyable_v < V >> >
        inline T::ValueType & operator*() const {
            auto guard_lock = lock_const();
            return *guard_lock;
        }

//        template < typename = std::enable_if<std::is_trivially_copyable_v < V >> >
        inline Weak<T> & operator=(T::ValueType && value) {
            auto guard_lock = lock();
            *guard_lock = value;
            return *this;
        }

        inline Weak<T> & set(T::ValueType && value, const SyncTimeoutType &timeout = SyncTimeoutDeedlock) {
            auto guard_lock = lock(timeout);
            *guard_lock = value;
            return *this;
        }

        inline explicit operator bool() const noexcept {
            return this->lock();
        }
    };

    /**
     * A class without a synchronization primitive and with the ability to work only in one application thread.   
     * Used to control access to data from only one application thread without creating a synchronization object between threads.
     */
    template <typename V>
    class SyncSingleThread : public Sync<V> {
    public:

        SyncSingleThread(V v) : Sync<V>(v), m_thread_id(std::this_thread::get_id()) {
        }

    protected:

        const std::thread::id m_thread_id;

        inline void check_thread() {
            if (m_thread_id != std::this_thread::get_id()) {
                throw memsafe_error("Using a single thread variable in another thread!");
            }
        }

        inline bool try_lock(const SyncTimeoutType &timeout) override final {
            check_thread();
            Sync<V>::timeout_set_error(timeout);
            return true;
        }

        inline void unlock() override final {
            check_thread();
        }

        inline bool try_lock_const(const SyncTimeoutType &timeout) override final {
            check_thread();
            Sync<V>::timeout_set_error(timeout);
            return true;
        }

        inline void unlock_const() override final {
            check_thread();
        }
    };

    /**
     * Class with timed mutex for simple multithreaded synchronization
     */

    template <typename V>
    class SyncTimedMutex : public Sync<V>, protected std::timed_mutex {
    public:

        SyncTimedMutex(V v) : Sync<V>(v) {
        }

    protected:

        inline bool try_lock(const SyncTimeoutType &timeout) override final {

            return std::timed_mutex::try_lock_for(timeout);
        }

        inline void unlock() override final {

            std::timed_mutex::unlock();
        }

        inline bool try_lock_const(const SyncTimeoutType &timeout) override final {

            return std::timed_mutex::try_lock_for(timeout);
        }

        inline void unlock_const() override final {

            std::timed_mutex::unlock();
        }
    };

    /**
     * Class with shared_timed_mutex for multi-thread synchronization for exclusive read/write lock or shared read-only lock
     */

    template <typename V>
    class SyncTimedShared : public Sync<V>, protected std::shared_timed_mutex {
    public:

        SyncTimedShared<V>(V v) : Sync<V>(v) {
        }

    protected:

        inline bool try_lock(const SyncTimeoutType &timeout) override final {
            return std::shared_timed_mutex::try_lock_for(timeout);
        }

        inline void unlock() override final {
            std::shared_timed_mutex::unlock();
        }

        inline bool try_lock_const(const SyncTimeoutType &timeout) override final {
            return std::shared_timed_mutex::try_lock_shared_for(timeout);
        }

        inline void unlock_const() override final {
            std::shared_timed_mutex::unlock_shared();
        }
    };

    /**
     * Class field reference variable (shared pointer) without multi-threaded access control
     * to store an object of the same class with protection against recursive references at runtime.
     * 
     * Should not be used at all, since the check is performed at runtime and 
     * its execution time depends on the total number of links being checked.
     * 
     * When created, requires the address of the object and the address of the field 
     * in the object to calculate its position in the class field, which must be correct.
     * 
     * To check std::is_standard_layout v the class cannot have virtual methods, 
     * including a destructor, be derived from another class and much more...
     * 
     */

    template <typename V>
    class [[deprecated]] Class {
        public:

        std::shared_ptr<V> m_field; ///< Field data with a pointer to an instance of the class containing this field (to another instance of the same class)
        V * m_instance; ///< A pointer to the object instance that contains this field.
        size_t m_offset; ///< Offset of this field from the pointer to the class instance

        typedef std::weak_ptr<V> WeakType;

        inline bool checkFieldPosInOwner(const V & owner, size_t offset) const {

            return ((size_t) & owner + offset) == (size_t)this;
        }

        inline bool checkCircularReference(const V * owner, const V * tested) const {
            if (!owner || !tested) {
                return true;
            } else if (owner == tested) {
                return false;
            }
            const Class<V> * filed = reinterpret_cast<const Class<V> *> (reinterpret_cast<size_t> (owner) + m_offset);

            return checkCircularReference(filed->m_field.get(), tested);
        }

        Class(V & owner, Class<V> &filed, V *ptr = nullptr) : m_field(std::shared_ptr<V>(ptr)), m_instance(&owner) {

            m_offset = reinterpret_cast<size_t> (& filed) - reinterpret_cast<size_t> (& owner);

            if (!checkCircularReference(m_instance, ptr)) {

                throw memsafe_error("Circular reference exception");
            }

            assert(&owner != ptr);
            assert(checkFieldPosInOwner(*m_instance, m_offset));
        }

        Class<V> & operator=(Class<V> & copy) {
            // Check for a copy of another field in your own object
            if (m_instance == copy.m_instance) {
                throw memsafe_error("Copy of another field exception");
            }
            *this = copy.m_field.get(); // Call Class<V> & operator=(V *cls)

            return *this;
        }

        Class<V> & operator=(V * cls) {
            if (!checkCircularReference(m_instance, cls)) {
                throw memsafe_error("Circular reference exception");
            }
            m_field = std::shared_ptr<V>(cls);

            return *this;
        }

        inline V& operator*() {
            if (V * temp = m_field.get()) {

                return *temp;
            }
            throw memsafe_error("null pointer exception");
        }

        inline const V& operator*() const {
            if (const V * temp = m_field.get()) {

                return *temp;
            }
            throw memsafe_error("null pointer exception");
        }

        Weak<Class < V >> weak() {
            return Weak<Class < V >> (*this, nullptr, nullptr);
        }
    };

#pragma clang attribute push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

    static_assert(std::is_standard_layout_v<std::shared_ptr<int>>);
    static_assert(std::is_standard_layout_v<Class<int>>);

#pragma clang attribute pop

    /**
     * An example of a linked list template implemented using weak pointers 
     * (where strong references between the same data types are not allowed).
     */

    template <typename T>
    struct LinkedWeakNode {
        typedef std::weak_ptr<LinkedWeakNode<T>> WeakType;
        WeakType next;
        T data;

        LinkedWeakNode(const T & value) : data(value) {
        }
    };

    template <typename T>
    class LinkedWeakList {
    public:
        typedef LinkedWeakNode<T> NodeType;
        typedef std::shared_ptr<NodeType> SharedType;

        SharedType m_head;
        std::set<SharedType> m_data;

        LinkedWeakList() : m_head(nullptr) {
        }


        // Function to Insert a new node at the beginning of the list

        void push_front(T && data) {
            SharedType node = std::make_shared<NodeType>(data);
            m_data.insert(node);
            node->next = m_head;
            m_head = node;
        }

        void push_back(T && data) {
            SharedType node = std::make_shared<NodeType>(data);

            m_data.insert(node);

            // If the linked list is empty, update the head to the new node
            if (!m_head) {
                m_head = node;
                return;
            }

            // Traverse to the last node
            SharedType temp = m_head;
            while (temp->next.lock()) {
                temp = temp->next.lock();
            }

            // Update the last node's next to the new node
            temp->next = node;
        }


        // Function to Delete the first node of the list

        void pop_front() {
            if (!m_head) {
                std::cerr << "List is empty." << std::endl;
                return;
            }
            SharedType temp = m_head;
            m_head = m_head->next.lock();
            m_data.erase(temp);
        }

        // Function to Delete the last node of the list

        void pop_back() {
            if (!m_head) {
                std::cerr << "List is empty." << std::endl;
                return;
            }

            SharedType temp = m_head->next.lock();
            if (!temp) {
                m_data.erase(m_head);
                m_head.reset();
                return;
            }

            // Traverse to the second-to-last node
            while (temp->next.lock()->next.lock()) {
                temp = temp->next.lock();
            }

            //  Delete the last node
            m_data.erase(temp->next.lock());
        }

        size_t size() {
            return m_data.size();
        }

        size_t empty() {
            return m_data.empty();
        }

        std::string to_string() {
            if (!m_head) {
                return "nullptr";
            }

            std::string result;

            SharedType temp = m_head;
            while (temp) {
                result += std::to_string(temp->data);
                result += " -> ";
                temp = temp->next.lock();
            }
            return result;
        }
    };


#ifndef DOXYGEN_SHOULD_SKIP_THIS

    /**
     * Helper class for lazy invocation of iterator
     */

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


    //    //decltype(std::declval<decltype(variable)>().method(__VA_ARGS__)) __VA_OPT__(,) DECLTYPE(__VA_ARGS__) 
    //
    //    template<typename T>
    //    struct arg_helper {
    //
    //        static auto type_arg(T arg) {
    //            return arg;
    //        }
    //
    //        static auto type_call(T arg) {
    //            if constexpr (std::is_class_v<T> && std::is_base_of_v<LazyCallerInterface, T>) {
    //                return arg;
    //            } else {
    //                return arg;
    //            }
    //        }
    //    };
    //
    //#define DECLTYPE_EXPAND_TYPE(arg) decltype(arg_helper<decltype(arg)>::type_call(arg))
    //    //    std::conditional<(std::is_class_v<decltype(arg)> && std::is_base_of_v<LazyCallerInterface, decltype(arg)>), decltype(*arg), decltype(arg)>::type
    //    //decltype(arg)
    //    //std::conditional<!(std::is_class_v<decltype(arg)> && std::is_base_of_v<LazyCallerInterface, decltype(arg)>), decltype(arg), decltype(std::declval<decltype(arg)>().operator*())>::type

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

    //
    //#define DECLTYPE_ARG_TYPE(arg) decltype(arg_helper<decltype(arg)>::type_arg(arg))
    //
    //#define DECLTYPE_ARG_0()
    //#define DECLTYPE_ARG_1(arg1) DECLTYPE_ARG_TYPE(arg1)
    //#define DECLTYPE_ARG_2(arg1, arg2)  DECLTYPE_ARG_TYPE(arg1), DECLTYPE_ARG_TYPE(arg2)
    //#define DECLTYPE_ARG_3(arg1, arg2, arg3) DECLTYPE_ARG_TYPE(arg1), DECLTYPE_ARG_TYPE(arg2), DECLTYPE_ARG_TYPE(arg3)
    //#define DECLTYPE_ARG_4(arg1, arg2, arg3, arg4) DECLTYPE_ARG_TYPE(arg1), DECLTYPE_ARG_TYPE(arg2), DECLTYPE_ARG_TYPE(arg3), DECLTYPE_ARG_TYPE(arg4)
    //#define DECLTYPE_ARG_5(arg1, arg2, arg3, arg4, arg5) DECLTYPE_ARG_TYPE(arg1), DECLTYPE_ARG_TYPE(arg2), DECLTYPE_ARG_TYPE(arg3), DECLTYPE_ARG_TYPE(arg4), DECLTYPE_ARG_TYPE(arg5)
    //#define DECLTYPE_ARG_6(arg1, arg2, arg3, arg4, arg5, arg6) DECLTYPE_ARG_TYPE(arg1), DECLTYPE_ARG_TYPE(arg2), DECLTYPE_ARG_TYPE(arg3), DECLTYPE_ARG_TYPE(arg4), DECLTYPE_ARG_TYPE(arg5), DECLTYPE_ARG_TYPE(arg6)
    //#define DECLTYPE_ARG_7(arg1, arg2, arg3, arg4, arg5, arg6, arg7) DECLTYPE_ARG_TYPE(arg1), DECLTYPE_ARG_TYPE(arg2), DECLTYPE_ARG_TYPE(arg3), DECLTYPE_ARG_TYPE(arg4), DECLTYPE_ARG_TYPE(arg5), DECLTYPE_ARG_TYPE(arg6), DECLTYPE_ARG_TYPE(arg7)
    //#define DECLTYPE_ARG_8(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) DECLTYPE_ARG_TYPE(arg1), DECLTYPE_ARG_TYPE(arg2), DECLTYPE_ARG_TYPE(arg3), DECLTYPE_ARG_TYPE(arg4), DECLTYPE_ARG_TYPE(arg5), DECLTYPE_ARG_TYPE(arg6), DECLTYPE_ARG_TYPE(arg7), DECLTYPE_ARG_TYPE(arg8)
    //#define DECLTYPE_ARG_9(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) DECLTYPE_ARG_TYPE(arg1), DECLTYPE_ARG_TYPE(arg2), DECLTYPE_ARG_TYPE(arg3), DECLTYPE_ARG_TYPE(arg4), DECLTYPE_ARG_TYPE(arg5), DECLTYPE_ARG_TYPE(arg6), DECLTYPE_ARG_TYPE(arg7), DECLTYPE_ARG_TYPE(arg8), DECLTYPE_ARG_TYPE(arg9)
    //
    //#define DECLTYPE_ARG_HELPER(count, ...) DECLTYPE_ARG_ ## count (__VA_ARGS__)
    //#define DECLTYPE_ARG(count, ...) DECLTYPE_ARG_HELPER(count, __VA_ARGS__)
    //#define DECLTYPE_ARGS(...) DECLTYPE_ARG(NARGS(__VA_ARGS__) __VA_OPT__(,) __VA_ARGS__)
    //
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


    //    inline std::vector<int> vect;
    //    inline auto check = LAZYCALL(vect, clear);
    //    inline auto reserve = LAZYCALL(vect, reserve, 10UL);
    //
    //    inline std::vector<int> data(1, 1);
    //    inline auto db = LAZYCALL(data, begin);
    //    inline auto de = LAZYCALL(data, end);
    //    inline std::vector<int> test(*db, *de);
    //    //    inline auto call_test = LAZYCALL(vect, assign, db, de);
    //
    //
    //    static_assert(std::is_same<decltype(10), decltype(0)>::value);
    //    static_assert(std::is_same<std::vector<int>, std::vector<int>>::value);
    //    static_assert(std::is_same<std::vector<int>::iterator, decltype(*db)>::value);
    //
    //    static_assert(std::is_base_of_v<LazyCallerInterface, decltype(db)>);
    //
    //    //    #define DECLTYPE_EXPAND_TYPE(arg)  decltype(arg)
    //    //std::conditional<!(std::is_class_v<decltype(arg)> && std::is_base_of_v<LazyCallerInterface, decltype(arg)>), decltype(arg), decltype(std::declval<decltype(arg)>().operator*())>::type
    //
    //    static_assert(std::is_same<std::vector<int>::iterator, std::conditional<(std::is_class_v<decltype(db)> && std::is_base_of_v<LazyCallerInterface, decltype(db)>), decltype(*db), decltype(10)>::type >::value);
    //    static_assert(std::is_same<int, std::conditional<(std::is_class_v<decltype(10)> && std::is_base_of_v<LazyCallerInterface, decltype(10)>), decltype(*db), decltype(10)>::type >::value);
    //
    //    static_assert(std::is_same<decltype(arg_helper<decltype(10)>::type_call(10)), decltype(10)>::value);
    //    //    static_assert(std::is_same<decltype(arg_helper<decltype(db)>::type_call(db)), decltype(*db)>::value);
    //
    //    static_assert(std::is_same<DECLTYPE_EXPAND_TYPE(10), decltype(10)>::value);
    //    //    static_assert(std::is_same<DECLTYPE_EXPAND_TYPE(db), decltype(*db)>::value);
    //
    //    //    static_assert(std::is_same<decltype(arg_type<decltype(db)>::call_type(db)), decltype(db)>::value);
    //
    //    //    static_assert(std::is_same<decltype(10UL), std::conditional<!(std::is_class_v<decltype(10UL)> && std::is_base_of_v<LazyCallerInterface, decltype(10UL)>), decltype(10UL), decltype(*10UL)>::type >::value);
    //
    //    //    static_assert(std::is_base_of_v<LazyCallerInterface, decltype(db)>);
    //
    //
    //    //    static_assert(std::is_same<decltype(std::vector<int>::clear), decltype(std::vector<int>::clear)>::value);
    //    //    static_assert(std::is_same<decltype(LazyCaller<std::vector<int>, decltype(std::declval<std::vector<int>>().clear())>::operator *())), void>::value);
    //
    //
    //
    /*
     * 
     * 
     */

    MEMSAFE_PROFILE(""); // Reset to default profile

    MEMSAFE_ERROR_TYPE("std::auto_ptr");
    MEMSAFE_ERROR_TYPE("std::shared_ptr");

    MEMSAFE_WARNING_TYPE("std::auto_ptr");
    MEMSAFE_WARNING_TYPE("std::shared_ptr");

    MEMSAFE_SHARED_TYPE("std::shared_ptr");
    MEMSAFE_SHARED_TYPE("memsafe::Shared");

    MEMSAFE_AUTO_TYPE("memsafe::Locker");
    MEMSAFE_AUTO_TYPE("__gnu_cxx::__normal_iterator");
    MEMSAFE_AUTO_TYPE("std::reverse_iterator");

    /*
     * For a basic_string_view str, pointers, iterators, and references to elements of str are invalidated 
     * when an operation invalidates a pointer in the range [str.data(), str.data() + str.size()).
     */
    MEMSAFE_AUTO_TYPE("std::basic_string_view");
    /*
     * For a span s, pointers, iterators, and references to elements of s are invalidated 
     * when an operation invalidates a pointer in the range [s.data(), s.data() + s.size()). 
     */
    MEMSAFE_AUTO_TYPE("std::span");


    MEMSAFE_INVALIDATE_FUNC("std::swap");
    MEMSAFE_INVALIDATE_FUNC("std::move");

    // End define memory safety classes
#if !defined(MEMSAFE_DISABLE)    
    MEMSAFE_STATUS("enable"); // Enable memory safety plugin
#endif

} // namespace memsafe


#endif // INCLUDED_MEMSAFE_H_
