# Memsafe - proof-of-concept for memory safety in C++

Many people want to make C++ "safer" so that they don't have to rewrite a lot of code in the latest fancy programming languages.
But making changes to the syntax of a language usually breaks backwards compatibility with older code written before that point.

This project contains code for a library header file and compiler plugin for *safe C++*,
which fixes the major problems of C++ with memory and reference data types without breaking backwards compatibility with older legacy code.

> The global problem of the C and C++ languages ​​is that the pointer to the allocated memory block in the heap is an ordinary address
> in RAM and has no connection with variables - pointers that are in local variables on the stack,
> and whose lifetime is controlled by the compiler.
>
> The second, no less serious problem, which often leads to undefined behavior (Undefined Behavior)
> or data races (Data Races) is access to the same memory area from different threads at the same time.

The [memory safety for C++](https://github.com/rsashka/memsafe) project is based on the idea 
of ​​using strong and weak pointers to an allocated block of memory on the heap 
and managing the lifetime of copies of variables with strong pointers in automatic variables.

The concept of safe memory management is ported to C++ from the [NewLang](https://newlang.net/) language. 
It is a bit similar to [ownership and borrowing in Rust](https://doc.rust-lang.org/book/ch04-00-understanding-ownership.html), 
but is implemented using the standard C++ template classes *shared_ptr* and *weak_ptr*.

The following features are implemented in the C++ [memsafe](https://github.com/rsashka/memsafe) library:
- Automatic allocation and release of memory and resources when creating and destroying objects in the [RAII](https://en.cppreference.com/w/cpp/language/raii) style.
- Checking for invalidation of reference types (iterators, std::span, std::string_view, etc.) when changing data in the original variable.
- Prohibition on creating strong **cyclic/recursive** references (in the form of ordinary variables or class fields).
- It is allowed to create copies of strong references only to automatic variables whose lifetime is controlled by the compiler.
- Automatic protection against data races is implemented when accessing the same variable from different threads simultaneously
(when defining a variable, it is necessary to specify a method for managing access from several threads, 
after which the capture and release of the synchronization object will occur automatically).
*By default, shared variables are created without multi-threaded access control and require 
no additional overhead compared to the standard `shared_ptr` and `weak_ptr`* template classes.

>All the above checks are performed during source code compilation using a compiler plugin,
which is designed only for AST analysis and does not make any changes to the source code.

The main difference when working with reference variables, compared to *shared_ptr* and *weak_ptr*,
is in the way references are dereferenced (obtaining the object address),
which is not done directly using the "**\***" operator, but in two stages.

First, the reference must be captured (saved) in a temporary (automatic) variable,
the lifetime of which is automatically controlled by the compiler, 
and then direct access to the data itself (memory address / object) is obtained through it.

Such an automatic variable is a temporary owner of a strong reference and performs the functions of capturing
an inter-thread synchronization object in the style of [std::lock_guard](https://en.cppreference.com/w/cpp/thread/lock_guard)

The method of marking objects in the source code and configuring the plugin's operation parameters
is performed using C++ attributes, which is very similar to the security profiles
[p3038](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p3038r0.pdf) by Bjarne Stroustrup
and [P3081](https://isocpp.org/files/papers/P3081R0.pdf) by Herb Sutter,
but does not require the creation of a new standard (it is enough to use the existing C++20).


## Examples

Command line for compiling a file with clang and using a plugin

```bash
clang++ -std=c++20 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe _example.cpp
```

### Output of the plugin with  pointer invalidation control:

```cpp
    std::vector<int> vec(100000, 0);
    auto x = vec.begin();
    vec = {};
    vec.shrink_to_fit(); 
    std::sort(x, vec.end()); // malloc(): unaligned tcache chunk detected or Segmentation fault
```

**A fragment of the compiler plugin output with error messages related 
to invalidation of reference variables after changing data in the main variable:**

```bash
_example.cpp:29:17: warning: using main variable 'vect'
   29 |                 vect = {};
      |                 ^
_example.cpp:30:17: warning: using main variable 'vect'
   30 |                 vect.shrink_to_fit();
      |                 ^
_example.cpp:31:27: error: Using the dependent variable 'x' after changing the main variable 'vect'!
   31 |                 std::sort(x, vect.end()); // malloc(): unaligned tcache chunk detected or Segmentation fault 
      |                           ^
```

### Example of plugin output when analyzing shared variables:

```cpp
    void shared_example() {
        Shared<int> var = 1;
        Shared<int> copy;
        copy = var; // Error
        std::swap(var, copy);
        {
            Shared<int> inner = var;
            std::swap(inner, copy); // Error
            copy = inner;
        }
    }
```

**A fragment of the compiler plugin output with error messages for incorrect use of shared variables:**

```bash
_example.cpp:148:9: error: Error copying shared variable due to lifetime extension
  148 |         copy = var; // Error
      |         ^
_example.cpp:152:13: error: Error swap the shared variables with different lifetimes
  152 |             std::swap(inner, copy); // Error
      |             ^
_example.cpp:155:13: error: Error copying shared variable due to lifetime extension
  155 |             copy = inner; // Error
      |             ^
```


### Example of class analysis:

```cpp
    class RecursiveRef {
    public:
        RecursiveRef * ref_pointer; // Error
        std::shared_ptr<RecursiveRef> ref_shared; // Error
        std::weak_ptr<RecursiveRef> ref_weak;

        Auto<int, int&> ref_int; // Error
        Shared<RecursiveRef> recursive; // Error
        Weak<Shared<RecursiveRef>> ref_weak2;
        Class<RecursiveRef> reference;

        MEMSAFE_UNSAFE RecursiveRef * unsafe_pointer; // Unsafe
        MEMSAFE_UNSAFE std::shared_ptr<RecursiveRef> unsafe_shared; // Unsafe
        MEMSAFE_UNSAFE Auto<int, int&> unsafe_ref_int; // Unsafe
        MEMSAFE_UNSAFE Shared<RecursiveRef> unsafe_recursive; // Unsafe
    };
```

**A fragment of the compiler plugin output with error messages when analyzing a class:**

```bash

_example.cpp:179:24: error: Field type raw pointer
  179 |         RecursiveRef * ref_pointer; // Error
      |                        ^
_example.cpp:180:39: error: Error type found 'std::shared_ptr'
  180 |         std::shared_ptr<RecursiveRef> ref_shared; // Error
      |                                       ^
_example.cpp:183:25: error: Create auto variabe as field ref_int:auto-type
  183 |         Auto<int, int&> ref_int; // Error
      |                         ^
_example.cpp:184:30: error: Potentially recursive pointer to ns::RecursiveRef
  184 |         Shared<RecursiveRef> recursive; // Error
      |                              ^
_example.cpp:188:39: warning: UNSAFE field type raw pointer
  188 |         MEMSAFE_UNSAFE RecursiveRef * unsafe_pointer; // Unsafe
      |                                       ^
_example.cpp:189:54: warning: UNSAFE Error type found 'std::shared_ptr'
  189 |         MEMSAFE_UNSAFE std::shared_ptr<RecursiveRef> unsafe_shared; // Unsafe
      |                                                      ^
_example.cpp:190:40: warning: UNSAFE create auto variabe as field unsafe_ref_int:auto-type
  190 |         MEMSAFE_UNSAFE Auto<int, int&> unsafe_ref_int; // Unsafe
      |                                        ^
_example.cpp:191:45: warning: UNSAFE potentially recursive pointer to ns::RecursiveRef
  191 |         MEMSAFE_UNSAFE Shared<RecursiveRef> unsafe_recursive; // Unsafe
      |                                             ^
```


The plugin's message level can be limited using a macro or command line argument,
or after checking the source code, the plugin can be omitted altogether,
since it only parses the AST, but does not make any corrections to it.


### Full output of plugin messages when compiling the test file `_example.cpp`: 

<details>
<summary> Show output: </summary>

```bash
clang++-20 -std=c++26 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe _example.cpp

_example.cpp:20:18: error: Raw pointer type
   20 |             auto pointer = &vect; // Error
      |                  ^
_example.cpp:20:28: error: Operator for address arithmetic
   20 |             auto pointer = &vect; // Error
      |                            ^
_example.cpp:25:18: error: Raw pointer type
   25 |             auto view_iter = view.begin(); // Error
      |                  ^
_example.cpp:24:13: warning: using main variable 'str'
   24 |             str.clear();
      |             ^
_example.cpp:25:30: error: Using the dependent variable 'view' after changing the main variable 'str'!
   25 |             auto view_iter = view.begin(); // Error
      |                              ^
_example.cpp:29:17: warning: using main variable 'vect'
   29 |                 vect = {};
      |                 ^
_example.cpp:30:17: warning: using main variable 'vect'
   30 |                 vect.shrink_to_fit();
      |                 ^
_example.cpp:31:27: error: Using the dependent variable 'beg' after changing the main variable 'vect'!
   31 |                 std::sort(beg, vect.end()); // Error
      |                           ^
_example.cpp:73:17: error: Create auto variabe as static static_fail1:auto-type
   73 |     static auto static_fail1(var_static.take()); // Error
      |                 ^
_example.cpp:74:17: error: Create auto variabe as static static_fail2:auto-type
   74 |     static auto static_fail2 = var_static.take(); // Error
      |                 ^
_example.cpp:94:9: error: Error copying shared variable due to lifetime extension
   94 |         var_shared1 = var_shared1; // Error
      |         ^
_example.cpp:100:13: error: Error copying shared variable due to lifetime extension
  100 |             var_shared1 = var_shared1; // Error
      |             ^
_example.cpp:101:13: error: Error copying shared variable due to lifetime extension
  101 |             var_shared2 = var_shared1; // Error
      |             ^
_example.cpp:108:17: error: Error copying shared variable due to lifetime extension
  108 |                 var_shared1 = var_shared1; // Error
      |                 ^
_example.cpp:109:17: error: Error copying shared variable due to lifetime extension
  109 |                 var_shared2 = var_shared1; // Error
      |                 ^
_example.cpp:115:17: error: Error copying shared variable due to lifetime extension
  115 |                 var_shared4 = var_shared4; // Error
      |                 ^
_example.cpp:120:17: error: Return shared variable
  120 |                 return var_shared4; // Error
      |                 ^
_example.cpp:125:13: error: Error swap the shared variables with different lifetimes
  125 |             std::swap(var_shared1, var_shared3);
      |             ^
_example.cpp:129:13: error: Return shared variable
  129 |             return arg; // Error
      |             ^
_example.cpp:146:30: error: Error type found 'std::shared_ptr'
  146 |         std::shared_ptr<int> old_shared; // Error
      |                              ^
_example.cpp:149:9: error: Error copying shared variable due to lifetime extension
  149 |         copy = var; // Error
      |         ^
_example.cpp:153:13: error: Error swap the shared variables with different lifetimes
  153 |             std::swap(inner, copy); // Error
      |             ^
_example.cpp:155:13: error: Error copying shared variable due to lifetime extension
  155 |             copy = inner; // Error
      |             ^
_example.cpp:163:9: error: Return shared variable
  163 |         return arg; // Error
      |         ^
_example.cpp:180:24: error: Field type raw pointer
  180 |         RecursiveRef * ref_pointer; // Error
      |                        ^
_example.cpp:181:39: error: Error type found 'std::shared_ptr'
  181 |         std::shared_ptr<RecursiveRef> ref_shared; // Error
      |                                       ^
_example.cpp:184:25: error: Create auto variabe as field ref_int:auto-type
  184 |         Auto<int, int&> ref_int; // Error
      |                         ^
_example.cpp:185:30: error: Potentially recursive pointer to ns::RecursiveRef
  185 |         Shared<RecursiveRef> recursive; // Error
      |                              ^
_example.cpp:189:39: warning: UNSAFE field type raw pointer
  189 |         MEMSAFE_UNSAFE RecursiveRef * unsafe_pointer; // Unsafe
      |                                       ^
_example.cpp:190:54: warning: UNSAFE Error type found 'std::shared_ptr'
  190 |         MEMSAFE_UNSAFE std::shared_ptr<RecursiveRef> unsafe_shared; // Unsafe
      |                                                      ^
_example.cpp:191:40: warning: UNSAFE create auto variabe as field unsafe_ref_int:auto-type
  191 |         MEMSAFE_UNSAFE Auto<int, int&> unsafe_ref_int; // Unsafe
      |                                        ^
_example.cpp:192:45: warning: UNSAFE potentially recursive pointer to ns::RecursiveRef
  192 |         MEMSAFE_UNSAFE Shared<RecursiveRef> unsafe_recursive; // Unsafe
      |                                             ^
7 warnings and 25 errors generated.

```
</details>


## Feedback
If you have any suggestions for the development and improvement of the project, join or [write](https://github.com/rsashka/memsafe/discussions).


---


## Библиотека для проверки концепции безопасной работы с памятью в С++

Есть много желающих сделать С++ более "безопасным", чтобы не пришлось переписывать большое количество кода на новых модных языках программирования.
Но внесение изменений в синтаксис языка обычно нарушат обратную совместимость со старым кодом, который был написан до этого момента. 

Данный проект содержит код заголовочного файла библиотеки и плагина компилятора для *безопасного С++*, 
который устраняет основные проблемы С++ при работе с памятью и ссылочными типами данных без нарушения обратной совместимости со старым легаси кодом.

> Глобальная проблема языков C и C++ в том, что указатель на выделенный блок памяти в куче является обычным адресом 
> в оперативной памяти и у него отсутствует связь и переменными - указателями, которые находятся в локальных переменных на стеке,
> и временем жизни которых управляет компилятор. 
> 
> Вторая, не менее серьезная проблема, которая часто приводить к неопределенному поведению (Undefined Behaviour) 
> или гонке данных (Data Races) - это доступ к одной и той же области памяти из разных потоков одновременно.

В основе проекта [безопасной работы с памятью для С++](https://github.com/rsashka/memsafe) лежит идея использования сильных 
и слабых указателей на выделенный блок памяти в куче и контроль времени жизни копий переменных с сильными указателями в автоматических переменных. 

Сама концепция безопасной работы с памятью портирована на С++ из языка [NewLang](https://newlang.net/).
Она немного похожа на [владение и заимствование в Rust](https://doc.rust-lang.org/book/ch04-00-understanding-ownership.html), 
но реализована на базе стандартных шаблонных классов С++ *shared_ptr* и *weak_ptr*.

В C++ библиотеке [memsafe](https://github.com/rsashka/memsafe) реализованы следующие возможности:
- Автоматическое выделение и освобождение памяти и ресурсов при создании и уничтожении объектов в стиле [RAII](https://en.cppreference.com/w/cpp/language/raii).
- Проверка инвалидации ссылочных типов (итераторов, std::span, std::string_view и т.д.) при изменении данных в исходной переменной.
- Запрет на создание сильных **циклических** ссылок (в виде обычных переменных или полей классов).
- Разрешается создание копий сильных ссылок только в автоматические переменные, время жизни которых контролирует компиятор.
- Реализована автоматическая защита от гонок данных при доступе к одной и той же переменной из разных потоков одновременно 
(способ контроля доступа из нескольких потоков требуется указывать при определении переменной, 
после чего захват и освобождение объекта синхронизации будут происходить автоматически).
*По умолчанию общие переменные создаются без контроля многопоточного доступа 
и не имеют дополнительных накладных раскодов по стравнению с стандартными шаблонными классами `shared_ptr` и `weak_ptr`*.

>Все указанные проверки выполняются во время компиляции исходного кода с помощью плагина компилятора,
который предназначен только для анализа AST и не вносит в исходный код никаких изменений.

Основное отличие при работе со ссылочными переменными, по сравнению с обычными *shared_ptr* и *weak_ptr*,
заключается в способе разименования ссылок (получения адреса объекта), 
который происходит не напрямую с помощью оператора "**\***", а в два этапа. 

Сперва ссылку следует захватить (сохранить) во временную переменную, 
время жизни которой контролируется компилятором автоматически,
и уже через неё получается доступ непосредственный к самим данным (объекту). 

Такая автоматическая переменная является временным владельцем сильной ссылки и выполняет функции захвата 
объекта межпотоковой синхронизации в стиле [std::lock_guard](https://en.cppreference.com/w/cpp/thread/lock_guard)

Способ маркировки объектов в исходноом коде и настройка параметров работы плагина
выполняется с помощью  C++ атрибутов, что очень похоже на профили безопасности 
[p3038](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p3038r0.pdf) от Bjarne Stroustrup 
и [P3081](https://isocpp.org/files/papers/P3081R0.pdf) от Herb Sutter, 
но не требует принятия нового стандарта (достаточно использовать уже существующий С++20).


## Примеры

Командная строка компиляции файла с помощью clang с загрузкой плагина

```bash
clang++ -std=c++20 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe _example.cpp
```

### Вывод плагина с контролем инвалидации ссылок:

```cpp
    std::vector<int> vec(100000, 0);
    auto x = vec.begin();
    vec = {};
    vec.shrink_to_fit(); 
    std::sort(x, vec.end()); // malloc(): unaligned tcache chunk detected or Segmentation fault
```

Фрагмент вывода плагина компилятора с сообщениями об ошибках, 
связанных с недействительностью ссылочных переменных после изменения данных в основной переменной:

```bash
_example.cpp:29:17: warning: using main variable 'vect'
   29 |                 vect = {};
      |                 ^
_example.cpp:30:17: warning: using main variable 'vect'
   30 |                 vect.shrink_to_fit();
      |                 ^
_example.cpp:31:27: error: Using the dependent variable 'x' after changing the main variable 'vect'!
   31 |                 std::sort(x, vect.end()); // malloc(): unaligned tcache chunk detected or Segmentation fault 
      |                           ^
```

### Пример вывода плагина при анализе ссылочных переменных:

```cpp
    void shared_example() {
        Shared<int> var = 1;
        Shared<int> copy;
        copy = var; // Error
        std::swap(var, copy);
        {
            Shared<int> inner = var;
            std::swap(inner, copy); // Error
            copy = inner;
        }
    }
```

Фрагмент вывода плагина компилятора с сообщениями об ошибках при неправильном использовании ссылочных переменных:
```bash
_example.cpp:148:9: error: Error copying shared variable due to lifetime extension
  148 |         copy = var; // Error
      |         ^
_example.cpp:152:13: error: Error swap the shared variables with different lifetimes
  152 |             std::swap(inner, copy); // Error
      |             ^
_example.cpp:155:13: error: Error copying shared variable due to lifetime extension
  155 |             copy = inner; // Error
      |             ^
```


### Пример анализа класса:

```cpp
    class RecursiveRef {
    public:
        RecursiveRef * ref_pointer; // Error
        std::shared_ptr<RecursiveRef> ref_shared; // Error
        std::weak_ptr<RecursiveRef> ref_weak;

        Auto<int, int&> ref_int; // Error
        Shared<RecursiveRef> recursive; // Error
        Weak<Shared<RecursiveRef>> ref_weak2;
        Class<RecursiveRef> reference;

        MEMSAFE_UNSAFE RecursiveRef * unsafe_pointer; // Unsafe
        MEMSAFE_UNSAFE std::shared_ptr<RecursiveRef> unsafe_shared; // Unsafe
        MEMSAFE_UNSAFE Auto<int, int&> unsafe_ref_int; // Unsafe
        MEMSAFE_UNSAFE Shared<RecursiveRef> unsafe_recursive; // Unsafe
    };
```

Фрагмент вывода плагина компилятора с сообщениями об ошибках при анализе класса:
```bash

_example.cpp:179:24: error: Field type raw pointer
  179 |         RecursiveRef * ref_pointer; // Error
      |                        ^
_example.cpp:180:39: error: Error type found 'std::shared_ptr'
  180 |         std::shared_ptr<RecursiveRef> ref_shared; // Error
      |                                       ^
_example.cpp:183:25: error: Create auto variabe as field ref_int:auto-type
  183 |         Auto<int, int&> ref_int; // Error
      |                         ^
_example.cpp:184:30: error: Potentially recursive pointer to ns::RecursiveRef
  184 |         Shared<RecursiveRef> recursive; // Error
      |                              ^
_example.cpp:188:39: warning: UNSAFE field type raw pointer
  188 |         MEMSAFE_UNSAFE RecursiveRef * unsafe_pointer; // Unsafe
      |                                       ^
_example.cpp:189:54: warning: UNSAFE Error type found 'std::shared_ptr'
  189 |         MEMSAFE_UNSAFE std::shared_ptr<RecursiveRef> unsafe_shared; // Unsafe
      |                                                      ^
_example.cpp:190:40: warning: UNSAFE create auto variabe as field unsafe_ref_int:auto-type
  190 |         MEMSAFE_UNSAFE Auto<int, int&> unsafe_ref_int; // Unsafe
      |                                        ^
_example.cpp:191:45: warning: UNSAFE potentially recursive pointer to ns::RecursiveRef
  191 |         MEMSAFE_UNSAFE Shared<RecursiveRef> unsafe_recursive; // Unsafe
      |                                             ^
```


Уровень сообщений плагина можно ограничить с помощью макроса или аргумента командной строки, 
либо после проверки исходного кода плагин можно вообще не использовать,  
поскольку он только анализирует AST, но не вносит в него никаких исправлений.


### Полный вывод сообщений плагина при компиляции тестового файла `_example.cpp`:
<details>
<summary>Показать вывод</summary>

```bash
clang++-20 -std=c++20 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe _example.cpp

_example.cpp:20:18: error: Raw pointer type
   20 |             auto pointer = &vect; // Error
      |                  ^
_example.cpp:20:28: error: Operator for address arithmetic
   20 |             auto pointer = &vect; // Error
      |                            ^
_example.cpp:25:18: error: Raw pointer type
   25 |             auto view_iter = view.begin(); // Error
      |                  ^
_example.cpp:24:13: warning: using main variable 'str'
   24 |             str.clear();
      |             ^
_example.cpp:25:30: error: Using the dependent variable 'view' after changing the main variable 'str'!
   25 |             auto view_iter = view.begin(); // Error
      |                              ^
_example.cpp:29:17: warning: using main variable 'vect'
   29 |                 vect = {};
      |                 ^
_example.cpp:30:17: warning: using main variable 'vect'
   30 |                 vect.shrink_to_fit();
      |                 ^
_example.cpp:31:27: error: Using the dependent variable 'beg' after changing the main variable 'vect'!
   31 |                 std::sort(beg, vect.end()); // Error
      |                           ^
_example.cpp:73:17: error: Create auto variabe as static static_fail1:auto-type
   73 |     static auto static_fail1(var_static.take()); // Error
      |                 ^
_example.cpp:74:17: error: Create auto variabe as static static_fail2:auto-type
   74 |     static auto static_fail2 = var_static.take(); // Error
      |                 ^
_example.cpp:94:9: error: Error copying shared variable due to lifetime extension
   94 |         var_shared1 = var_shared1; // Error
      |         ^
_example.cpp:100:13: error: Error copying shared variable due to lifetime extension
  100 |             var_shared1 = var_shared1; // Error
      |             ^
_example.cpp:101:13: error: Error copying shared variable due to lifetime extension
  101 |             var_shared2 = var_shared1; // Error
      |             ^
_example.cpp:108:17: error: Error copying shared variable due to lifetime extension
  108 |                 var_shared1 = var_shared1; // Error
      |                 ^
_example.cpp:109:17: error: Error copying shared variable due to lifetime extension
  109 |                 var_shared2 = var_shared1; // Error
      |                 ^
_example.cpp:115:17: error: Error copying shared variable due to lifetime extension
  115 |                 var_shared4 = var_shared4; // Error
      |                 ^
_example.cpp:120:17: error: Return shared variable
  120 |                 return var_shared4; // Error
      |                 ^
_example.cpp:125:13: error: Error swap the shared variables with different lifetimes
  125 |             std::swap(var_shared1, var_shared3);
      |             ^
_example.cpp:129:13: error: Return shared variable
  129 |             return arg; // Error
      |             ^
_example.cpp:146:30: error: Error type found 'std::shared_ptr'
  146 |         std::shared_ptr<int> old_shared; // Error
      |                              ^
_example.cpp:149:9: error: Error copying shared variable due to lifetime extension
  149 |         copy = var; // Error
      |         ^
_example.cpp:153:13: error: Error swap the shared variables with different lifetimes
  153 |             std::swap(inner, copy); // Error
      |             ^
_example.cpp:155:13: error: Error copying shared variable due to lifetime extension
  155 |             copy = inner; // Error
      |             ^
_example.cpp:163:9: error: Return shared variable
  163 |         return arg; // Error
      |         ^
_example.cpp:180:24: error: Field type raw pointer
  180 |         RecursiveRef * ref_pointer; // Error
      |                        ^
_example.cpp:181:39: error: Error type found 'std::shared_ptr'
  181 |         std::shared_ptr<RecursiveRef> ref_shared; // Error
      |                                       ^
_example.cpp:184:25: error: Create auto variabe as field ref_int:auto-type
  184 |         Auto<int, int&> ref_int; // Error
      |                         ^
_example.cpp:185:30: error: Potentially recursive pointer to ns::RecursiveRef
  185 |         Shared<RecursiveRef> recursive; // Error
      |                              ^
_example.cpp:189:39: warning: UNSAFE field type raw pointer
  189 |         MEMSAFE_UNSAFE RecursiveRef * unsafe_pointer; // Unsafe
      |                                       ^
_example.cpp:190:54: warning: UNSAFE Error type found 'std::shared_ptr'
  190 |         MEMSAFE_UNSAFE std::shared_ptr<RecursiveRef> unsafe_shared; // Unsafe
      |                                                      ^
_example.cpp:191:40: warning: UNSAFE create auto variabe as field unsafe_ref_int:auto-type
  191 |         MEMSAFE_UNSAFE Auto<int, int&> unsafe_ref_int; // Unsafe
      |                                        ^
_example.cpp:192:45: warning: UNSAFE potentially recursive pointer to ns::RecursiveRef
  192 |         MEMSAFE_UNSAFE Shared<RecursiveRef> unsafe_recursive; // Unsafe
      |                                             ^
7 warnings and 25 errors generated.

```
</details>


## Обратная связь
Если у вас есть предложения по развитию и улучшению проекта, присоединяйтесь или [пишите](https://github.com/rsashka/memsafe/discussions)

