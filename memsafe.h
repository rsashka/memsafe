#pragma once
#ifndef INCLUDED_MEMSAFE_H_
#define INCLUDED_MEMSAFE_H_

#include <iostream>
#include <stdint.h>
#include <stdexcept>
#include <memory>

#include <mutex>
#include <shared_mutex>
#include <thread>

#include <format>
#include <type_traits>

#include <assert.h>
#include <source_location>
#include <stacktrace>


#ifdef BUILD_UNITTEST
// Removing all restrictions on access to protected and private fields for testing cases
#define SCOPE(scope)  public
#else
#define SCOPE(scope)  scope
#endif   




#if defined __has_attribute
#if __has_attribute(memsafe)
#define MEMSAFE_ATTR(...) [[memsafe(__VA_ARGS__)]]
#endif
#endif

#ifndef MEMSAFE_ATTR
#define MEMSAFE_ATTR(...)
#define MEMSAFE_DISABLE
#endif


namespace MEMSAFE_ATTR("define") memsafe { // Begin define memory safety classes

    typedef int INT;

    class runtime_format : public std::runtime_error {
    public:
        template <class... Args>
        runtime_format(std::format_string<Args...> fmt, Args&&... args) : std::runtime_error{std::format(fmt, args...)}
        {
        }
    };

    /*
     *  Base classes of multithreaded synchronization primitives with a uniform interface for future use
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

    /*
     * A class without a synchronization primitive, but with the ability to work only in one application thread
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

    /*
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

    /*
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

    /*
     * 
     * 
     * 
     */

    /**
     * V - Variable value type
     */
    template <typename V>
    class VarInterface {
    public:
        virtual V& operator*() = 0;
        virtual const V& operator*() const = 0;
    };

    /**
     * V - Variable value type
     * T - The type of the specific taked variable
     */
    template <typename V, typename T>
    class MEMSAFE_ATTR("auto") VarAuto : public VarInterface<V> {
    protected:
        T taken; // shared_ptr<V> or shared_ptr<VarGuard<V>>
    public:

        VarAuto(T arg) : taken(arg) {
        }

        inline V& operator*() override final;
        inline const V& operator*() const override final;

        virtual ~VarAuto();
    };

    /**
     * Variable by value without references
     */
    template <typename V>
    class MEMSAFE_ATTR("value") VarValue : public VarInterface<V> {
    protected:
        V value;
    public:

        VarValue(V val) : value(val) {
        }

        inline V& operator*() override final {
            return value;
        };

        inline const V& operator*() const override final {
            return value;
        };

        inline VarAuto<V, V&> take() {
            return VarAuto<V, V&> (value);
        }
    };

    /*
     * Reference variable (strong pointer) without multithreaded access control
     */

    template <typename V, typename T> class VarWeak;

    template <typename V>
    class MEMSAFE_ATTR("share") VarShared : public VarInterface<V>, public std::shared_ptr<V> {
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

        virtual V& operator*() override {
            return this->template shared_ptr<V>::operator*();
        }

        virtual const V& operator*() const override {
            return this->template shared_ptr<V>::operator*();
        }

        VarWeak<V, VarShared<V>> weak();

        virtual ~VarShared() {
        }
    };


    /*
     * Reference variable (strong pointer) with control over multi-threaded access to data
     * By default, it runs in a single application thread without creating a cross-thread access synchronization object.
     */

    template <typename V, template <typename> typename S = VarSyncNone >
    class MEMSAFE_ATTR("share") VarGuard : public VarInterface<V>, public std::atomic<std::shared_ptr<S<V>>>, public std::enable_shared_from_this<VarGuard<V, S>>
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
         */

        //        typedef S<V> GuardType;
        typedef std::atomic<std::shared_ptr<S<V>>> SharedType;
        typedef std::atomic<std::weak_ptr<S<V>>> WeakType;
        //        std::shared_ptr<V> data; // @todo replace to std::atomic<std::shared_ptr<V>>
        //        SharedType guard;

        public:

        VarGuard(V val) : std::atomic<std::shared_ptr<S < V >>> (std::make_shared<S < V >> (val)) { // { //std::make_shared<G>(val)
        }

        virtual V& operator*() override {
            //            static_assert(std::is_trivially_copyable<V>::value, "To be able to directly retrieve a value, the variable type must be trivially copyable.")
            //            guard->sync.lock();
            //            V result = guard->data;
            //            guard->sync.unlock();
            //            return result;
            this->load().get()->lock();
            this->load().get()->unlock();
            return this->load(). template shared_ptr<S < V>>::operator*().data;
        }

        virtual const V& operator*() const override {
            //            static_assert(std::is_trivially_copyable<V>::value, "To be able to directly retrieve a value, the variable type must be trivially copyable.")
            //            guard->sync.lock();
            //            V result = guard->data;
            //            guard->sync.unlock();
            //            return result;
            //            return this->template shared_ptr<V>::operator*();
            this->load().get()->lock();
            this->load().get()->unlock();
            return this->load().template shared_ptr<S < V>>::operator*().data;
            //            return guard->data;
        }




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

    /*
     * 
     * 
     */

    template <typename V, typename T = VarShared<V>>
    class MEMSAFE_ATTR("weak") VarWeak : public T::WeakType {
    public:

        //        T::WeakType weak;

        // Слабый указатель на????
        // Для VarShared на данные
        // Для VarGuard на сам VarGuard ?????


        explicit VarWeak(std::atomic<T> & ptr) : T::WeakType(ptr.load()) {
        }

        VarWeak(T & ptr) : T::WeakType(ptr) {
        }
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
        //        if constexpr (std::is_base_of_v<VarShared<V, T>, T>) {
        //            return VarWeak<V, std::weak_ptr < V >> (*this);
        //        }
        //        static_assert("It is not possible to get a weak reference to a variable by value!");
    }



    // End define memory safety classes
#if !defined(MEMSAFE_DISABLE)    
    namespace MEMSAFE_ATTR("enable") {
    } // Enable memory safety plugin
#endif

    /*
     * 
     * 
     * 
     */



    template <typename T> class Variable;
    template <typename T> class VariableTaken;

    //    struct VariablePair;
    //    typedef std::shared_ptr<Variable> VariablePtr;

    //    /**
    //     * Класс захвата/освобождения объекта межпотоковой синхронизации
    //     * Хранит опции захвата и содерит поле для ссылкы на защищаемую переменную.
    //     */
    //    template <typename T>
    //    class Sync {
    //    protected:
    //        //        friend class VariableTaken;
    //
    //        const RefType m_type;
    //        const std::thread::id m_thread_id;
    //        const std::chrono::milliseconds & m_timeout;
    //
    //        typedef std::variant<std::monostate, std::shared_timed_mutex, std::recursive_timed_mutex> SyncType;
    //        mutable SyncType m_sync;
    //
    //        //        std::variant<std::shared_ptr<T>, std::weak_ptr<T>> m_data;
    //
    //        //        std::shared_ptr<Variable<T>> m_data; ///< Поле данных для использования в классе @ref VariableTaken, чтобы уменьшить размер класса Variable
    //        std::shared_ptr<T> m_data; ///< Поле данных для использования в классе @ref VariableTaken, чтобы уменьшить размер класса Variable
    //
    //    public:
    //
    //        static constexpr std::chrono::milliseconds SyncTimeoutDeedlock = std::chrono::milliseconds(5000);
    //        static constexpr std::chrono::seconds SyncWithoutWait = std::chrono::seconds(0);
    //
    //        //        template <typename S>
    //        //        S VarSyncCreate(RefType type) {
    //        //            assert(m_type != RefType::Value);
    //        //            if (type == RefType::LiteSingle || type == RefType::LiteSingleConst || type == RefType::LiteThread || type == RefType::LiteThreadConst) {
    //        //                assert(std::holds_alternative<std::monostate>(m_sync));
    //        //            } else if (type == RefType::SyncMono || type == RefType::SyncMonoConst) {
    //        //                m_sync.emplace<std::shared_timed_mutex>();
    //        //            } else if (type == RefType::SyncMulti || type == RefType::SyncMultiConst) {
    //        //                m_sync.emplace<std::recursive_timed_mutex>();
    //        //            } else {
    //        //                LOG_RUNTIME("Unknown synchronization type! (%d)", static_cast<int> (type));
    //        //            }
    //        //        }
    //
    //        template <typename D>//, typename = typename std::enable_if<std::is_same_v<D, std::shared_ptr<T>> || std::is_same_v<D, std::weak_ptr<T>>> >
    //        Sync(D data, RefType type, const std::chrono::milliseconds & timeout_duration = SyncTimeoutDeedlock) :
    //        m_type(type), m_thread_id(std::this_thread::get_id()), m_timeout(timeout_duration), m_sync(std::monostate()), m_data(data) {
    //
    //            static_assert(std::is_same_v<D, std::shared_ptr < T>> || std::is_same_v<D, std::weak_ptr < T>>, "Expected type std::shared_ptr<T> or std::weak_ptr<T>");
    //
    //            assert(m_type != RefType::Value);
    //            if (type == RefType::LiteSingle || type == RefType::LiteSingleConst || type == RefType::LiteThread || type == RefType::LiteThreadConst) {
    //                assert(std::holds_alternative<std::monostate>(m_sync));
    //            } else if (type == RefType::SyncMono || type == RefType::SyncMonoConst) {
    //                m_sync.emplace<std::shared_timed_mutex>();
    //            } else if (type == RefType::SyncMulti || type == RefType::SyncMultiConst) {
    //                m_sync.emplace<std::recursive_timed_mutex>();
    //            } else {
    //                LOG_RUNTIME("Unknown synchronization type! (%d)", static_cast<int> (type));
    //            }
    //        }
    //
    //        //        static std::shared_ptr<Sync> CreateSync(const TermPtr &term);
    //        //        static std::shared_ptr<Sync<T>> CreateSync(const std::string_view ref);
    //
    //        static std::shared_ptr<Sync<T>> CreateSync(const std::string_view ref) {
    //            if (!ref.empty()) {
    //                RefType ref_type = RefTypeFromString(ref);
    //                if (!(ref_type == RefType::Value || ref_type == RefType::Shared || ref_type == RefType::SharedConst)) {
    //                    return std::make_shared<Sync < T >> (ref_type);
    //                }
    //            }
    //            return std::shared_ptr<Sync < T >> (nullptr);
    //        }
    //
    //        inline RefType GetRefType() const {
    //            return m_type;
    //        }
    //
    //        inline std::thread::id GetThreadId() const {
    //            return m_thread_id;
    //        }
    //
    //        inline bool SyncLock(bool edit_mode = true, const std::chrono::milliseconds & timeout_duration = Sync::SyncTimeoutDeedlock) {
    //            if (m_type == RefType::LiteSingle || m_type == RefType::LiteSingleConst) {
    //                if (m_thread_id != std::this_thread::get_id()) {
    //                    LOG_RUNTIME("Calling a function on another thread!");
    //                }
    //            } else if (m_type == RefType::SyncMono || m_type == RefType::SyncMonoConst) {
    //                assert(std::holds_alternative<std::shared_timed_mutex>(m_sync));
    //                if (edit_mode) {
    //                    return std::get<std::shared_timed_mutex>(m_sync).try_lock_for(&timeout_duration == &Sync::SyncTimeoutDeedlock ? m_timeout : timeout_duration);
    //                } else {
    //                    return std::get<std::shared_timed_mutex>(m_sync).try_lock_shared_for(&timeout_duration == &Sync::SyncTimeoutDeedlock ? m_timeout : timeout_duration);
    //                }
    //            } else if (m_type == RefType::SyncMulti || m_type == RefType::SyncMultiConst) {
    //                assert(std::holds_alternative<std::recursive_timed_mutex>(m_sync));
    //                return std::get<std::recursive_timed_mutex>(m_sync).try_lock_for(&timeout_duration == &Sync::SyncTimeoutDeedlock ? m_timeout : timeout_duration);
    //            }
    //            return true;
    //        }
    //
    //        inline bool SyncUnLock() {
    //            assert(m_type != RefType::Value);
    //            if (m_type == RefType::LiteSingle || m_type == RefType::LiteSingleConst) {
    //                assert(m_thread_id == std::this_thread::get_id());
    //            } else if (m_type == RefType::SyncMono || m_type == RefType::SyncMonoConst) {
    //                assert(std::holds_alternative<std::shared_timed_mutex>(m_sync));
    //                std::get<std::shared_timed_mutex> (m_sync).unlock();
    //            } else if (m_type == RefType::SyncMulti || m_type == RefType::SyncMultiConst) {
    //                assert(std::holds_alternative<std::recursive_timed_mutex>(m_sync));
    //                std::get<std::recursive_timed_mutex>(m_sync).unlock();
    //            }
    //            return true;
    //        }
    //
    //        virtual ~Sync() {
    //        }
    //    };
    //
    //    /** 
    //     * Класс @ref VariableTaken используется для захвата объекта синхронизации @ref Sync при работе с классом @ref Variable
    //     */
    //    template <typename T>
    //    class VariableTaken : public std::runtime_error {
    //        //        friend class IVariable;
    //
    //        SCOPE(protected) :
    //        std::shared_ptr<Sync<T>> variable;
    //        //        std::variant<Variable<T> *, std::shared_ptr<Variable<T>>, std::shared_ptr<Sync<T>>> variable;
    //        //
    //        //
    //        //        /**
    //        //         * Конструктор захвата объекта синхронизации @ref Sync при работе с классом @ref Variable 
    //        //         * @param var Ссылка на объект @ref Variable .
    //        //         * @param edit_mode Режим редактирования (по умолчанию только для чтения).
    //        //         * @param timeout_duration Время ожидания захвата блокировки до возникнвоения исключения ошибки захвата.
    //        //         */
    //        //        explicit VariableTaken(const Variable & var, bool edit_mode,
    //        //                const std::chrono::milliseconds & timeout_duration = Sync::SyncTimeoutDeedlock,
    //        //                const std::string_view message = "",
    //        //                const std::source_location & location = std::source_location::current());
    //        //
    //        //        static std::string MakeTimeoutMessage(const Variable & var, bool edit_mode,
    //        //                const std::chrono::milliseconds & timeout_duration,
    //        //                const std::string_view message, const std::source_location & location);
    //        //
    //        //    public:
    //        //
    //        //        inline operator bool() const {
    //        //            if (std::holds_alternative<Variable *> (variable)) {
    //        //                return std::get<Variable *> (variable);
    //        //            } else if (std::holds_alternative<std::shared_ptr < Variable >> (variable)) {
    //        //                return std::get<std::shared_ptr < Variable >> (variable).get();
    //        //            }
    //        //            return std::get<std::shared_ptr < Sync >> (variable).get() && std::get<std::shared_ptr < Sync >> (variable)->m_data;
    //        //        }
    //        //
    //        //        inline const Variable & operator*() const {
    //        //            if (std::holds_alternative<Variable *> (variable)) {
    //        //                assert(std::get<Variable *> (variable));
    //        //                return *std::get<Variable *> (variable);
    //        //            } else if (std::holds_alternative<std::shared_ptr < Variable >> (variable)) {
    //        //                assert(std::get<std::shared_ptr < Variable >> (variable));
    //        //                return *std::get<std::shared_ptr < Variable >> (variable);
    //        //            }
    //        //            assert(std::get<std::shared_ptr < Sync >> (variable));
    //        //            assert(std::get<std::shared_ptr < Sync >> (variable)->m_data);
    //        //            return *(std::get<std::shared_ptr < Sync >> (variable))->m_data;
    //        //        }
    //        //
    //        //        inline Variable & operator*() {
    //        //            if (std::holds_alternative<Variable *> (variable)) {
    //        //                assert(std::get<Variable *> (variable));
    //        //                return *std::get<Variable *> (variable);
    //        //            } else if (std::holds_alternative<std::shared_ptr < Variable >> (variable)) {
    //        //                assert(std::get<std::shared_ptr < Variable >> (variable));
    //        //                return *std::get<std::shared_ptr < Variable >> (variable);
    //        //            }
    //        //            assert(std::get<std::shared_ptr < Sync >> (variable));
    //        //            assert(std::get<std::shared_ptr < Sync >> (variable)->m_data);
    //        //            return *std::get<std::shared_ptr < Sync >> (variable)->m_data;
    //        //        }
    //        //
    //        //        VariableTaken & operator=(VariableTaken const& var) noexcept {
    //        //            this->variable = var.variable;
    //        //            return *this;
    //        //        }
    //
    //        virtual ~VariableTaken() {
    //
    //        }
    //    };

    //    template <typename T>
    //    class Variable {
    //        SCOPE(protected) :
    //
    //    public:
    //        typedef std::weak_ptr<T> Ref;
    //
    //        //        template <typename T>
    //
    //        template <class N, typename = typename std::enable_if<!std::is_convertible_v<N*, T*>>>
    //        Variable(N value) {
    //        }
    //
    //        static_assert(std::is_convertible_v<int*, int*>);
    //        static_assert(std::is_convertible_v<int*, Variable<int>*>);
    //
    //        Variable(T value, std::string_view ptr) {
    //        }
    //
    //
    //        mutable std::shared_ptr<Sync> sync;
    //
    //        T & operator*() {
    //
    //        }
    //
    //    };

    //    /**
    //     * Шаблон для реализации произвольной функции, которую можно переопределять с сохранением предыдущей реализации в стеке.
    //     */
    //    template< typename R, typename... Args>
    //    class VirtualFuncImpl : public std::vector<void *> {
    //    public:
    //
    //        typedef R VirtualFuncType(Args... args);
    //
    //        explicit VirtualFuncImpl(VirtualFuncType *func) {
    //            push_back(reinterpret_cast<void *> (func));
    //        }
    //
    //        VirtualFuncImpl(std::initializer_list<VirtualFuncType *> list) {
    //            for (auto func : list) {
    //                push_back(reinterpret_cast<void *> (func));
    //            }
    //        }
    //
    //        inline R operator()(Args... args) {
    //            assert(!empty());
    //            return (*reinterpret_cast<VirtualFuncType *> (back()))(args...);
    //        }
    //
    //        VirtualFuncType * override(VirtualFuncType *func) {
    //            VirtualFuncType * result = reinterpret_cast<VirtualFuncType *> (back());
    //            push_back(reinterpret_cast<void *> (func));
    //            return result;
    //        }
    //
    //        VirtualFuncType * parent(VirtualFuncType *current) {
    //            for (auto it = rbegin(); it != rend(); it++) {
    //                if (reinterpret_cast<void *> (current) == *it) {
    //                    ++it;
    //                    return reinterpret_cast<VirtualFuncType *> (*it);
    //                }
    //            }
    //            return nullptr;
    //        }
    //
    //    };
    //
    //    /**
    //     * Класс с базовыми функциями, которые реализуют базовые операции с переменными и с возможностью их переопределения
    //     */
    //    class VariableOp {
    //    public:
    //        typedef VirtualFuncImpl<Variable &, Variable &, const Variable &> OperatorType;
    //
    //        static VirtualFuncImpl<Variable, const Variable &> copy;
    //        static Variable __copy__(const Variable &copy);
    //
    //        static VirtualFuncImpl<Variable, const Variable &> clone;
    //        static Variable __clone__(const Variable &clone);
    //
    //        static VirtualFuncImpl<int, const Variable &, const Variable &> eq;
    //        static int __eq__(const Variable &self, const Variable &other);
    //
    //        static VirtualFuncImpl<bool, const Variable &, const Variable &> strict_eq;
    //        static bool __strict_eq__(const Variable &self, const Variable &other);
    //
    //
    //#define DEFINE_OPERATOR(name)  static OperatorType name; \
//                               static Variable& __ ## name ## __(Variable &self, const Variable &other)
    //
    //        /*
    //         *      x + y   __add__(self, other)        x += y      __iadd__(self, other)
    //         *      x - y   __sub__(self, other)        x -= y      __isub__(self, other)
    //         *      x * y   __mul__(self, other)        x *= y      __imul__(self, other)
    //         *      x / y   __truediv__(self, other)    x /= y      __itruediv__(self, other)
    //         *      x // y  __floordiv__(self, other)   x //= y     __ifloordiv__(self, other)
    //         *      x % y   __mod__(self, other)        x %= y      __imod__(self, other) 
    //         */
    //
    //        DEFINE_OPERATOR(iadd);
    //        DEFINE_OPERATOR(isub);
    //        DEFINE_OPERATOR(imul);
    //        DEFINE_OPERATOR(itruediv);
    //        DEFINE_OPERATOR(ifloordiv);
    //        DEFINE_OPERATOR(imod);
    //
    //#undef DEFINE_OPERATOR
    //        static Variable& __iadd_only_numbers__(Variable &self, const Variable &other);
    //        static Variable& __imul_only_numbers__(Variable &self, const Variable &other);
    //
    //    };
    //
    //

    /**
     * Структура для хранения данных ссылочной переменной в классе @ref Variable
     */
    //    template <typename T>
    //    struct VariableShared {
    //        std::shared_ptr<Variable<T>> data; // Сильный указатель на данные (владеющая ссылка)
    //        std::shared_ptr<Sync<T>> sync; ///< Объект синхронизации доступа к защищённой переменной
    //    };

    /**
     * Структура для хранения данных слабой ссылки в классе @ref Variable
     */
    //    template <typename T>
    //    struct VariableWeak {
    //        std::weak_ptr<Variable<T>> weak; ///< Слабый указатель на данные (не владеющая ссылка)
    //        std::shared_ptr<Sync<T>> sync; ///< Объект синхронизации доступа к ссылке
    //    };

    //    /**
    //     * Варианты данных для хрения в классе @ref Variable
    //     */
    //    typedef std::variant< std::monostate, // Не инициализированная переменная
    //    VariableShared, // Ссылочная или не захваченная защищенная переменная (владеющая ссылка)
    //    VariableWeak, // Ссылочная переменная (слабая ссылка)
    //    VariableTaken, // Захваченная защищенная или ссылочная переменная
    //    // Далее идут варинаты данных 
    //    ObjPtr, // Универсальный объект
    //    int64_t, double, std::string, std::wstring, Rational //, void * // Оптимизация для типизированных переменных
    //    > VariableVariant;


    //#define VARIABLE_TYPES(_)       \
//            _(EMPTY,    0)      \
//            _(SHARED,   1)      \
//            _(WEAK,     2)      \
//            _(TAKEN,    3)      \
//            \
//            _(OBJECT,   4)      \
//            _(INTEGER,  5)      \
//            _(DOUBLE,   6)      \
//            _(STRING,   7)      \
//            _(WSTRING,  8)      \
//            _(RATIONAL, 9)      
    //    //            _(POINTER,  10)
    //
    //    enum class VariableCase : uint8_t {
    //#define DEFINE_ENUM(name, value) name = static_cast<uint8_t>(value),
    //        VARIABLE_TYPES(DEFINE_ENUM)
    //#undef DEFINE_ENUM
    //    };
    //
    //#define MAKE_TYPE_NAME(type_name)  type_name
    //
    //    inline const char * VariableCaseToString(size_t index) {
    //#define DEFINE_CASE(name, _)                                    \
//    case VariableCase:: name :                                  \
//        return MAKE_TYPE_NAME(":" #name);
    //
    //        switch (static_cast<VariableCase> (index)) {
    //                VARIABLE_TYPES(DEFINE_CASE)
    //
    //            default:
    //                LOG_RUNTIME("UNKNOWN VariableVariant index %d", static_cast<int> (index));
    //        }
    //#undef DEFINE_CASE
    //#undef MAKE_TYPE_NAME
    //    }
    //
    //    static_assert(static_cast<size_t> (VariableCase::EMPTY) == 0);
    //    static_assert(static_cast<size_t> (VariableCase::SHARED) == 1);
    //    static_assert(static_cast<size_t> (VariableCase::WEAK) == 2);
    //    static_assert(static_cast<size_t> (VariableCase::TAKEN) == 3);
    //    static_assert(std::is_same_v<std::monostate, std::variant_alternative_t < static_cast<size_t> (VariableCase::EMPTY), VariableVariant>>);
    //    static_assert(std::is_same_v<VariableShared, std::variant_alternative_t < static_cast<size_t> (VariableCase::SHARED), VariableVariant>>);
    //    static_assert(std::is_same_v<VariableWeak, std::variant_alternative_t < static_cast<size_t> (VariableCase::WEAK), VariableVariant>>);
    //    static_assert(std::is_same_v<VariableTaken, std::variant_alternative_t < static_cast<size_t> (VariableCase::TAKEN), VariableVariant>>);
    //
    //    static_assert(std::is_same_v<ObjPtr, std::variant_alternative_t < static_cast<size_t> (VariableCase::OBJECT), VariableVariant>>);
    //    static_assert(std::is_same_v<int64_t, std::variant_alternative_t < static_cast<size_t> (VariableCase::INTEGER), VariableVariant>>);
    //    static_assert(std::is_same_v<double, std::variant_alternative_t < static_cast<size_t> (VariableCase::DOUBLE), VariableVariant>>);
    //    static_assert(std::is_same_v<std::string, std::variant_alternative_t < static_cast<size_t> (VariableCase::STRING), VariableVariant>>);
    //    static_assert(std::is_same_v<std::wstring, std::variant_alternative_t < static_cast<size_t> (VariableCase::WSTRING), VariableVariant>>);
    //    static_assert(std::is_same_v<Rational, std::variant_alternative_t < static_cast<size_t> (VariableCase::RATIONAL), VariableVariant>>);
    //    //    static_assert(std::is_same_v<void *, std::variant_alternative_t < static_cast<size_t> (VariableCase::POINTER), VariableVariant>>);

    //    template<typename T> struct is_shared_ptr : std::false_type {
    //    };
    //    template<typename T> struct is_shared_ptr<std::shared_ptr < T>> : std::true_type
    //    {
    //    };
    //
    //    /**
    //     * Variable - главный класс для хранения объектов во время выполнения приложения.
    //     * Все операции с объектами выполняются для этой струкутры.
    //     * 
    //     * Variable не зависит от типов переменных или ограничений для них во время компиляции.
    //     * Он хранит не только значение, но и объект синхронизации досутпа к переменной.
    //     * 
    //     * Может хранить строго типизированные данные, тип которых задан во время компиляции или 
    //     * универсальный тип данных (словари, функции, классы и перечисления) в @ref Obj (@ref ObjPtr).
    //     * 
    //     * Для переменных по значению данные хранятся непосредственно в Variable
    //     * Для ссылочных и защищенных переменных в Variable хранится std::shared_ptr<Variable>.
    //     * У ссылок в Variable хранятся слабый указатель на std::shared_ptr<Variable> ссылочной переменной,
    //     * а захваченные переменные содержат @ref VariableTaken.
    //     */
    //
    //    template <typename T>
    //    struct VariableVariant : public std::variant< std::monostate, // Не инициализированная переменная
    //    std::shared_ptr<Sync < T>>, // Ссылочная или не захваченная защищенная переменная (владеющая ссылка)
    //    std::weak_ptr<Sync < T>>, // Ссылочная переменная (слабая ссылка)
    //    //VariableShared<T>, // Ссылочная или не захваченная защищенная переменная (владеющая ссылка)
    //    //VariableWeak<T>, // Ссылочная переменная (слабая ссылка)
    //    VariableTaken<T>, // Захваченная защищенная или ссылочная переменная (получается из Sync<T>)
    //    T>
    //    {
    //        static constexpr size_t index_none = 0;
    //        static constexpr size_t index_shared = 1;
    //        static constexpr size_t index_weak = 2;
    //        static constexpr size_t index_taken = 3;
    //        static constexpr size_t index_value = 4;
    //        //        static_assert(std::is_same_v<std::monostate, std::variant_alternative_t < Variable<T>::index_none, Variable<T>>>);
    //        //        static_assert(std::is_same_v<VariableShared<T>, std::variant_alternative_t < Variable<T>::index_shared, Variable<T>>>);
    //        //        static_assert(std::is_same_v<VariableWeak<T>, std::variant_alternative_t < Variable<T>::index_weak, Variable<T>>>);
    //        //        static_assert(std::is_same_v<VariableTaken<T>, std::variant_alternative_t < Variable<T>::index_taken, Variable<T>>>);
    //        //        static_assert(std::is_same_v<T, std::variant_alternative_t < Variable<T>::index_value, Variable<T>>>);
    //
    //    };
    //
    //    template <typename T>
    //    class Variable : public VariableVariant<T> {
    //
    //        SCOPE(protected) :
    //
    //        //        typedef std::variant< std::monostate, // Не инициализированная переменная
    //        //        VariableShared<T>, // Ссылочная или не захваченная защищенная переменная (владеющая ссылка)
    //        //        VariableWeak<T>, // Ссылочная переменная (слабая ссылка)
    //        //        VariableTaken<T>, // Захваченная защищенная или ссылочная переменная
    //        //        T> Variant;
    //
    //    public:
    //
    //        Variable(T value) : VariableVariant<T>(value) { //[[memsafe("value")]] 
    //            //            static_assert(std::is_convertible_v<T*, V*>);
    //            //            static_assert(std::is_convertible_v<T*, Variable<V>*>);
    //        }
    //
    //        explicit Variable(T value, std::string_view ref) : VariableVariant<T>(value) { //[[memsafe("shared")]]
    //            //            static_assert(std::is_convertible_v<T*, V*>);
    //            //            static_assert(std::is_convertible_v<T*, Variable<V>*>);
    //        }
    //
    //        explicit Variable(T value, RefType ref) : VariableVariant<T>(value) { //[[memsafe("shared")]]
    //            //            static_assert(std::is_convertible_v<T*, V*>);
    //            //            static_assert(std::is_convertible_v<T*, Variable<V>*>);
    //        }
    //
    //        explicit Variable(std::weak_ptr<Sync < T >> weak) : VariableVariant<T>(weak) { //[[memsafe("weak")]]
    //            //            static_assert(std::is_convertible_v<T*, V*>);
    //            //            static_assert(std::is_convertible_v<T*, Variable<V>*>);
    //        }
    //
    //        explicit Variable(VariableTaken<T> taken) : VariableVariant<T>(taken) { //[[memsafe("taken")]]
    //            //            static_assert(std::is_convertible_v<T*, V*>);
    //            //            static_assert(std::is_convertible_v<T*, Variable<V>*>);
    //        }
    //
    //        inline T & operator*() { //[[memsafe("cast")]]
    //            if (this->index() == VariableVariant<T>::index_value) {
    //                return std::get<VariableVariant<T>::index_value>(*this);
    //                //            } else if (Variable<T>::index() == index_value) {
    //            }
    //            LOG_RUNTIME("Variable not initialized!");
    //        }
    //
    //        inline operator T() {
    //            return static_cast<T&> (*this);
    //        }
    //
    //
    //        //        mutable std::shared_ptr<Sync> sync; ///< Объект синхронизации доступа к защищённой переменной
    //
    //        //        Variable(const Variable& var) : VariableVariant(static_cast<VariableVariant> (var)), sync(var.sync) {
    //        //            assert(!sync); // Block copy guard variable
    //        //        }
    //
    //        //        explicit Variable(VariableVariant val, std::shared_ptr<Sync> s) : VariableVariant(val), sync(s) {
    //        //        }
    //
    //        //        Variable& operator=(const Variable& var) noexcept {
    //        //            static_cast<VariableVariant> (*this) = static_cast<VariableVariant> (var);
    //        //            sync = var.sync;
    //        //            assert(!sync); // Block copy guard variable
    //        //            return *this;
    //        //        }
    //        //
    //        //        Variable& operator=(Variable& var) noexcept {
    //        //            static_cast<VariableVariant> (*this) = static_cast<VariableVariant> (var);
    //        //            sync = std::move(var.sync);
    //        //            assert(!sync); // Block copy guard variable
    //        //            return *this;
    //        //        }
    //
    //        //        static bool isShared(const TermPtr & term);
    //        //        //        static std::unique_ptr<Sync> MakeSync(const TermPtr &term);
    //        //        //        static std::unique_ptr<Sync> MakeSync(const std::string_view ref);
    //        //
    //        //        inline Variable copy() {
    //        //            return VariableOp::copy(*this);
    //        //        }
    //        //
    //        //        static VariablePtr UndefinedPtr() {
    //        //            return std::make_shared<Variable>(std::monostate());
    //        //        }
    //        //        explicit Variable(const VarWeak &ref) : sync(nullptr) {
    //        //        }
    //        //
    //        //        explicit Variable(const VarPtr &ptr) : sync(nullptr) {
    //        //        }
    //        //
    //        //        explicit Variable(std::unique_ptr<VarGuard> taken) : sync(nullptr) {
    //        //        }
    //
    //    public:
    //
    //        //        template <class T>
    //        //        typename std::enable_if<is_shared_ptr<decltype(std::declval<T>().value)>::value, void>::type
    //        //        func(T t) {
    //        //            std::cout << "shared ptr" << std::endl;
    //        //        }
    //        //
    //        //        template <class T>
    //        //        typename std::enable_if<!is_shared_ptr<decltype(std::declval<T>().value)>::value, void>::type
    //        //        func(T t) {
    //        //            std::cout << "non shared" << std::endl;
    //        //        }
    //
    //        //        explicit Variable(ObjPtr obj) : VariableVariant(obj) {
    //        //        }
    //
    //        /**
    //         * Template for create variable by value 
    //         * @param value - Base type value (copyable)
    //         */
    //        //        template <class V, typename = typename std::enable_if<!is_shared_ptr<decltype(std::declval<T>())>::value>::type >
    //
    //        //        template <typename T, typename = typename std::enable_if<!is_shared_ptr<decltype(std::declval<T>())>::value>::type >
    //        //        Variable(T value, const TermPtr && term) :
    //        //        VariableVariant(isShared(term) ? static_cast<VariableVariant> (VariableShared(std::make_shared<Variable>(value), Sync::CreateSync(term))) : value) {
    //        //        }
    //        //
    //        //        template <typename T, typename = typename std::enable_if<!is_shared_ptr<decltype(std::declval<T>())>::value>::type >
    //        //        Variable(T value, const std::string_view ref) :
    //        //        VariableVariant(ref.empty() ? value : static_cast<VariableVariant> (VariableShared(std::make_shared<Variable>(value), Sync::CreateSync(ref)))) {
    //        //        }
    //
    //
    //        //        //        Variable(const Variable &&value);
    //        //        //
    //        //        //        template <typename T, typename = typename std::enable_if<!is_shared_ptr<decltype(std::declval<T>())>::value>::type>
    //        //        //        Variable(T shared, std::unique_ptr<Sync> s) : VariableVariant(std::make_shared<T>(value)), sync(std::move(s)) {
    //        //        //        }
    //        //
    //        //
    //        //        //        template <typename D, typename T> static VarPtr Create(D d, T ref = nullptr) {
    //        //        //            return std::make_shared<VarData>(d, ref);
    //        //        //        }
    //        //        //
    //        //
    //        //        //        template <typename D> static VariableType CreatePair(TermPtr term, D d) {
    //        //        //            return std::make_shared<VariablePair>(term, Variable(d));
    //        //        //        }
    //        //
    //        //        static ObjPtr Object(Variable * variable);
    //        //        static ObjPtr Object(VariablePair * pair);
    //        //        //
    //        //        //        static Variable Undefined() {
    //        //        //            return Variable(std::monostate());
    //        //        //        }
    //        //        //
    //        //        //        static VarPtr UndefinedPtr() {
    //        //        //            return std::make_shared<Variable>(std::monostate());
    //        //        //        }
    //        //        //        //        static VarWeak GetWeak(const VarData & data){
    //        //        //        //            return std::weak_ptr<VarData>(data.owner);
    //        //        //        //        }
    //        //
    //        //        inline bool is_undefined() const {
    //        //            return std::holds_alternative<std::monostate>(*this);
    //        //        }
    //        //
    //        //        inline bool is_taked() const {
    //        //            STATIC_assert(static_cast<size_t> (VariableCase::TAKEN) == 3);
    //        //            return this->index() >= static_cast<size_t> (VariableCase::TAKEN) ||
    //        //                    (std::holds_alternative<VariableShared> (*this) && !std::get<VariableShared>(*this).sync);
    //        //        }
    //        //
    //        //        inline bool is_shared() const {
    //        //            return std::holds_alternative<VariableShared> (*this) ||
    //        //                    std::holds_alternative<VariableTaken> (*this) ||
    //        //                    std::holds_alternative<VariableWeak> (*this);
    //        //        }
    //        //
    //        //
    //        //        Variable Ref(const std::string_view ref) const;
    //        //
    //        //        inline Variable Take(bool edit_mode = false, const std::chrono::milliseconds & timeout_duration = Sync::SyncTimeoutDeedlock,
    //        //                const std::string_view message = "", const std::source_location location = std::source_location::current()) const {
    //        //            return VariableTaken(*this, edit_mode, timeout_duration, message, location);
    //        //        }
    //        //
    //        //        inline const Variable & operator*() const {
    //        //            if (is_taked()) {
    //        //                return *this;
    //        //            }
    //        //            return *Take();
    //        //        }
    //        //
    //        //
    //        //        bool GetValueAsBoolean() const;
    //        //        int64_t GetValueAsInteger() const;
    //        //        double GetValueAsNumber() const;
    //        //        std::string & GetValueAsString();
    //        //        std::wstring & GetValueAsStringWide();
    //        //
    //        //        Rational GetValueAsRational() const;
    //        //
    //        //        ObjPtr GetValueAsObject() const;
    //        //        void * GetValueAsPointer() const;
    //        //        std::string toString() const;
    //        //
    //        //        /*
    //        //         *  Cast operators
    //        //         * 
    //        //         */
    //        //        explicit inline operator bool() const {
    //        //            return GetValueAsBoolean();
    //        //        }
    //        //
    //        //#define PLACE_RANGE_CHECK_VALUE(itype, utype)\
////        explicit inline operator itype() const { \
////            int64_t result = GetValueAsInteger(); \
////            if (result > std::numeric_limits<itype>::max() || result < std::numeric_limits<itype>::lowest()) { \
////                LOG_RUNTIME("Value '%s' is out of range of the casting type %s!", toString().c_str(), #itype); \
////            } \
////            return result; \
////        } \
////        explicit inline operator utype() const { \
////            int64_t result = GetValueAsInteger(); \
////            if (result > std::numeric_limits<utype>::max() || result < 0) { \
////                LOG_RUNTIME("Value '%s' is out of range of the casting type %s!", toString().c_str(), #utype); \
////            } \
////            return result; \
////        }
    //        //
    //        //        PLACE_RANGE_CHECK_VALUE(int8_t, uint8_t);
    //        //        PLACE_RANGE_CHECK_VALUE(int16_t, uint16_t);
    //        //        PLACE_RANGE_CHECK_VALUE(int32_t, uint32_t);
    //        //
    //        //        explicit inline operator int64_t() const {
    //        //            return GetValueAsInteger();
    //        //        }
    //        //
    //        //        explicit inline operator uint64_t() const {
    //        //            int64_t result = GetValueAsInteger();
    //        //            if (result < 0) {
    //        //                LOG_RUNTIME("Value '%s' is out of range of the casting type %s!", toString().c_str(), "uint64_t");
    //        //            }
    //        //            return result;
    //        //        }
    //        //
    //        //
    //        //#undef PLACE_RANGE_CHECK_VALUE
    //        //
    //        //        //        explicit inline operator float() const {
    //        //        //            double result = GetValueAsNumber();
    //        //        //            if (result > (double) std::numeric_limits<float>::max()) {
    //        //        //                LOG_RUNTIME("Value1 '%s' %.20f %.20f %.20f is out of range of the casting type float!", toString().c_str(), result, std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest());
    //        //        //            }
    //        //        //
    //        //        //            //    __asm__ volatile ( "; //SOURCE: __FLT_MAX__ ");
    //        //        //            if (result < -__FLT_MAX__) {//(double) std::numeric_limits<float>::lowest()) {
    //        //        //                LOG_RUNTIME("Value2 '%s' %.20f %.20f %.20f is out of range of the casting type float!", toString().c_str(), result, std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest());
    //        //        //            }
    //        //        //            LOG_DEBUG("operator float() '%s' %.20f", toString().c_str(), result);
    //        //        //            return result;
    //        //        //        }
    //        //
    //        //        //        explicit inline operator double() const {
    //        //        //            return GetValueAsNumber();
    //        //        //        }
    //        //
    //        //        explicit inline operator std::string() const {
    //        //            return toString();
    //        //        }
    //        //
    //        //        explicit inline operator std::wstring() const {
    //        //            return newlang::utf8_decode(toString());
    //        //        }
    //        //
    //        //        explicit operator void *() const {
    //        //            return GetValueAsPointer();
    //        //        }
    //        //
    //        //        explicit inline operator Rational() const {
    //        //            return GetValueAsRational();
    //        //        }
    //        //
    //        //
    //        //        //        void set(const ObjPtr & value, bool edit_mode = false, const std::chrono::milliseconds & timeout_duration = Sync::SyncTimeoutDeedlock);
    //        //        //
    //        //        //        template <typename T>
    //        //        //        typename std::enable_if<!std::is_same<T, ObjPtr>::value, void>::type
    //        //        //        set(const T & new_value, bool edit_mode = false, const std::chrono::milliseconds & timeout_duration = Sync::SyncTimeoutDeedlock);
    //        //
    //        //        inline Variable & operator+=(const Variable &other) {
    //        //            return VariableOp::iadd(*this, other);
    //        //        }
    //        //
    //        //        inline const Variable operator+(const Variable &other) {
    //        //            Variable result(VariableOp::copy(other));
    //        //            result += other;
    //        //            return result;
    //        //        }
    //        //
    //        //        inline Variable & operator-=(const Variable &other) {
    //        //            return VariableOp::isub(*this, other);
    //        //        }
    //        //
    //        //        inline const Variable operator-(const Variable &other) {
    //        //            Variable result(VariableOp::copy(other));
    //        //            result += other;
    //        //            return result;
    //        //        }
    //        //
    //        //        inline Variable & operator*=(const Variable &other) {
    //        //            return VariableOp::imul(*this, other);
    //        //        }
    //        //
    //        //        inline const Variable operator*(const Variable &other) {
    //        //            Variable result(VariableOp::copy(other));
    //        //            result += other;
    //        //            return result;
    //        //        }
    //        //
    //        //        inline Variable & operator/=(const Variable &other) {
    //        //            return VariableOp::itruediv(*this, other);
    //        //        }
    //        //
    //        //        inline const Variable operator/(const Variable &other) {
    //        //            Variable result(VariableOp::copy(other));
    //        //            result += other;
    //        //            return result;
    //        //        }
    //        //
    //        //        inline Variable & operator%=(const Variable &other) {
    //        //            return VariableOp::imod(*this, other);
    //        //        }
    //        //
    //        //        inline const Variable operator%(const Variable &other) {
    //        //            Variable result(VariableOp::copy(other));
    //        //            result += other;
    //        //            return result;
    //        //        }
    //        //
    //        //        /*
    //        //         * 
    //        //         * 
    //        //         */
    //        //
    //        //        bool is_object_type() const;
    //        //        bool is_scalar_type() const;
    //        //        bool is_floating_type() const;
    //        //        bool is_complex_type() const;
    //        //        bool is_rational_type() const;
    //        //        bool is_string_type() const;
    //        //
    //        //        /*
    //        //         * 
    //        //         */
    //        //        inline bool operator==(const Variable & other) const {
    //        //            return VariableOp::eq(*this, other) == 0;
    //        //        }
    //        //
    //        //        inline bool operator<=(const Variable & other) const {
    //        //
    //        //            return VariableOp::eq(*this, other) <= 0;
    //        //        }
    //        //
    //        //        inline bool operator<(const Variable &other) const {
    //        //
    //        //            return VariableOp::eq(*this, other) < 0;
    //        //        }
    //        //
    //        //        inline bool operator>=(const Variable &other) const {
    //        //
    //        //            return VariableOp::eq(*this, other) >= 0;
    //        //        }
    //        //
    //        //        inline bool operator>(const Variable &other) const {
    //        //
    //        //            return VariableOp::eq(*this, other) > 0;
    //        //        }
    //        //
    //        //        inline bool operator!=(const Variable &other) const {
    //        //
    //        //            return VariableOp::eq(*this, other) != 0;
    //        //        }
    //        //
    //        //        inline int compare(const Variable &other) const {
    //        //
    //        //            return VariableOp::eq(*this, other);
    //        //        }
    //        //
    //        //        inline bool strict_eq(const Variable &other) const {
    //        //            return VariableOp::strict_eq(*this, other);
    //        //        }
    //    };

} // namespace newlang


#endif // INCLUDED_MEMSAFE_H_
