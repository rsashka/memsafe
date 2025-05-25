## Memory Safety for C++

There are many projects that want to make C++ a "safer" programming language.
But making changes to the language syntax usually breaks backward compatibility with older code written earlier.

This project contains code for a library header file and compiler plugin for *safe C++*, 
which fixes C++'s core problems with memory and reference data types without breaking backwards compatibility with old legacy code.

### Motivation

> The global problem of the C and C++ languages ​​is that the pointer to the allocated memory block in the heap is an ordinary address
> in RAM and has no connection with variables - pointers that are in local variables on the stack,
> and whose lifetime is controlled by the compiler.
>
> The second, no less serious problem, which often leads to undefined behavior (Undefined Behavior)
> or data races (Data Races) is access to the same memory area from different threads at the same time.

There are many projects that want to make C++ a "safer" programming language.
But making changes to the language's syntax usually breaks backward compatibility with older code written earlier.

This project contains a header only library and a compiler plugin for *safe C++*,
which fixes the main problems of C++ when working with memory and reference data types
without breaking backward compatibility with old legacy code (it uses C++20, but can be downgraded to C++17 or C++11).

The method of marking objects in the source code and configuring the plugin's operation parameters
is performed using C++ attributes, which is very similar to the security profiles
[p3038](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p3038r0.pdf) by Bjarne Stroustrup
and [P3081](https://isocpp.org/files/papers/P3081R0.pdf) by Herb Sutter,
but does not require the creation of a new standard (it is enough to use the existing C++20).


### Concept

The concept of safe memory management consists of implementing the following principles:
- If the program is guaranteed to have no strong cyclic references
(references of an object to itself or cross-references between several objects),
then when implementing the [RAII](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization) principle
*automatic memory release will be performed **always***.
- The absence of cyclic references in the program code can only be guaranteed by prohibiting them at the level of *types* (class definitions).
- The problem of data races when accessing memory from different threads is solved by using inter-thread synchronization objects,
and to prevent errors in logic, only one operator (function call) should be used to capture
the synchronization object and dereference the reference at the same time in one place.

The concept of safe memory management is ported to C++ from the [NewLang](https://newlang.net/) language,
but is implemented using the standard C++ template classes *shared_ptr* and *weak_ptr*.

The main difference when working with reference variables, compared to shared_ptr and weak_ptr, 
is the method of dereferencing references (getting the address of an object), 
and the method of accessing the object, which can be done not only by dereferencing the reference "*", 
but also by capturing (locking) the reference and storing it in a temporary variable, 
the lifetime of which is limited and automatically controlled by the compiler, 
and through it direct access to the data itself (the object) is carried out.

Such an automatic variable is a *temporary* strong reference holder and is similar to
[std::lock_guard](https://en.cppreference.com/w/cpp/thread/lock_guard) - a synchronization object holder 
until the end of the current scope (lifetime), after which it is automatically deleted by the compiler.

### Implementation

The implementation of the concept of safe work with memory for C++ consists of two parts: a plugin for Clang and a header file of the library.

Clang plugin performs static analysis of C++ code during its compilation. 
It checks for invalidation of reference types (iterators, std::span, std::string_view, etc.) 
when data in the original variable is changed and controls strong **cyclic** references 
at the type level (class definitions) of any nesting **\***).

The library file contains template classes that extend the standard *std::shared_ptr* and *std::weak_ptr* 
and add automatic data race protection to them when accessing shared variables from different threads 
(the access control method must be specified when defining the variable, after which the acquisition 
and release of the synchronization object will occur automatically when the reference is renamed).
*By default, shared variables are created without multi-thread access control 
and have no additional overhead compared to the standard template classes `std::shared_ptr` and `std::weak_ptr`*.

The library header file also contains options for controlling the analyzer plugin 
(a list of classes that need to be monitored for invalid reference data types is defined).

---

**\***) - since C++ compiles files separately, and the class (data structure) 
definition may be in a different translation unit due to forward declaration,
two passes may be required for the circular reference analyzer to work correctly.
*First run the plugin with the '--circleref-write -fsyntax-only' option to generate a list of classes with strong references, 
then a second time with the '--circleref-read' option to perform the analysis. 
Or disable the circular reference analyzer completely with the '--circleref-disable' option.*


## Examples

Command line for compiling a file with clang and using a plugin

```bash
clang++ -std=c++20 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe -Xclang -plugin-arg-memsafe -Xclang circleref-disable _example.cpp
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


### Example of analyzing classes with recursive references:

```cpp

    class SharedCross2;

    class SharedCross {
        SharedCross2 *cross2;
    };

    class SharedCross2 {
        SharedCross *cross;
    };
```

A fragment of the compiler plugin output with error messages when analyzing a class:

```bash
_cycles.cpp:53:23: error: The class 'cycles::SharedCross' has a circular reference through class 'cycles::SharedCross2'
   53 |         SharedCross2 *cross2;
      |                       ^
_cycles.cpp:57:22: error: The class 'cycles::SharedCross2' has a circular reference through class 'cycles::SharedCross'
   57 |         SharedCross *cross;
      |                      ^
_cycles.cpp:53:23: error: Field type raw pointer
   53 |         SharedCross2 *cross2;
      |                       ^
_cycles.cpp:53:23: error: The class 'cycles::SharedCross' has a circular reference through class 'cycles::SharedCross2'
_cycles.cpp:57:22: error: Field type raw pointer
   57 |         SharedCross *cross;
      |                      ^
```


The plugin's message level can be limited using a macro or command line argument,
or after checking the source code, the plugin can be omitted altogether,
since it only parses the AST, but does not make any corrections to it.


### Full output of plugin messages when compiling the test file `_example.cpp`: 

<details>
<summary> Show output: </summary>

```bash
clang++-20 -std=c++26 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe -Xclang -plugin-arg-memsafe -Xclang circleref-disable _example.cpp

...

```
</details>


## Feedback
If you have any suggestions for the development and improvement of the project, join or [write](https://github.com/rsashka/memsafe/discussions).


---


## Безопасная работа с памятью для С++

### Мотивация

> Глобальная проблема языка C++ в том, что указатель на выделенный блок памяти в куче является обычным адресом 
> оперативной памяти и у него отсутствует связь с локальными переменными - указателями, которые находятся на стеке,
> и временем жизни которых управляет компилятор. 
> 
> Вторая, не менее серьезная проблема, которая часто приводить к неопределенному поведению (Undefined Behaviour) 
> или гонке данных (Data Races) - это доступ к одной и той же области памяти из разных потоков одновременно.

Есть много проектов, целью которых является превратить С++ более "безопасный" язык программирования.
Но внесение изменений в синтаксис языка обычно нарушает обратную совместимость со старым кодом, который был написан до этого.

Данный проект содержит заголовочный файл библиотеки и плагин компилятора для *безопасного С++*, 
который устраняет основные проблемы С++ при работе с памятью и ссылочными типами данных 
без нарушения обратной совместимости со старым легаси кодом (используется С++20, но можно понизить до С++17 или С++11).

Способ маркировки объектов в исходноом коде и настройка параметров работы плагина
выполняется с помощью  C++ атрибутов, что очень похоже на профили безопасности 
[p3038](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p3038r0.pdf) от Bjarne Stroustrup 
и [P3081](https://isocpp.org/files/papers/P3081R0.pdf) от Herb Sutter, 
но не требует принятия нового стандарта С++.

### Концепция

Концепция безопасной работы с памятью заключается в реализации следующих принципов:   
- Если в программе гарантированно отсутствуют сильные циклические ссылки 
(ссылка объекта на самого себя или перекрестные ссылки между несколькими объектами), 
тогда при реализации принципа [RAII](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization), 
*автоматическое освобождение памяти будет выполнятся **всегда***.
- Гарантировать отсутствие циклических ссылок можно только путем их запрета на уровне *типов* (определений классов).
- Проблема гонок данных при обращении к памяти из разных потоков решается за счет использования объектов межпотоковой синхронизации.
Чтобы исключить ошибки в логике для захвата объекта синхронизации и разименования ссылки используются единый оператор (вызов функции).

Изначальная идея безопасной работы с памятью  была взята из языка [NewLang](https://newlang.net/), 
но реализована на базе стандартных шаблонных классов С++ *shared_ptr* и *weak_ptr*.

Основное отличие новых шаблонов заключается в способе обращения к объекту, 
который может выполняться не только с помощью разименования "**\***", 
но и через захват (блокировку) ссылки с сохранением её во *временную переменную*, 
время жизни которой ограничено и автоматически контролируется компилятором,
и уже через неё получается доступ непосредственный к самим данным (объекту). 

Такая автоматическая переменная является *временным* владельцем сильной ссылки и выполняет функции удержания
объекта межпотоковой синхронизации в стиле [std::lock_guard](https://en.cppreference.com/w/cpp/thread/lock_guard),
время жизни которого ограничено текущей областью видимости и управляется компилятором автоматически.

### Реализация

Реализация концепции безопасной работы с памятью для С++ состоит из двух частей: плагина для Clang и заголовочного файла библиотеки.

С помощью плагина для Clang выполняется статический анализ С++ кода во время его компиляции.
В плагине реализованы проверка инвалидации ссылочных типов (итераторов, std::span, std::string_view и т.д.) при изменении данных в исходной переменной
и контроль сильных **циклических** ссылок на уровне типов (определений классов) любой вложенности **\***).

В файле библиотеки находятся шаблонные классы, расширяющие стандартные *std::shared_ptr* и *std::weak_ptr* 
с автоматической защитой от гонок данных при доступе к общим переменным из разных потоков
(способ контроля доступа требуется указать при определении переменной, 
после чего захват и освобождение объекта синхронизации будут происходить автоматически при разименовании ссылки).
*По умолчанию общие переменные создаются без контроля многопоточного доступа 
и не имеют дополнительных накладных расходов по стравнению с стандартными шаблонными классами `std::shared_ptr` и `std::weak_ptr`*.

Так же в заголовочным файле библиотеки находятся опции для управления плагином анализатора 
(опредляется список классов, которые необходимо отслеживать для инвалидации ссылочных типов данных).

---

**\***) - *поскольку C++ компилирует файлы по отдельности, а определение класса (структуры данных)
может находиться в другой единице трансляции из-за предварительного объявления,
для корректной работы анализатора циклических ссылок может потребоваться два прохода.* 
*Сначала запустить плагин с ключом '--circleref-write -fsyntax-only', чтобы сгенерировать список классов
с сильными ссылками, затем второй раз с ключом '--circleref-read', чтобы выполнить анализ.
Или полностью отключить анализатор циклических ссылок с помощью опции '--circleref-disable'.*

## Примеры

Командная строка компиляции файла с помощью clang с загрузкой плагина

```bash
clang++ -std=c++20 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe -Xclang -plugin-arg-memsafe -Xclang circleref-disable _example.cpp
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

### Пример анализа классов с рекурсивными ссылками:

```cpp

    class SharedCross2;

    class SharedCross {
        SharedCross2 *cross2;
    };

    class SharedCross2 {
        SharedCross *cross;
    };
```

Фрагмент вывода плагина компилятора с сообщениями об ошибках при анализе класса:
```bash
_cycles.cpp:53:23: error: The class 'cycles::SharedCross' has a circular reference through class 'cycles::SharedCross2'
   53 |         SharedCross2 *cross2;
      |                       ^
_cycles.cpp:57:22: error: The class 'cycles::SharedCross2' has a circular reference through class 'cycles::SharedCross'
   57 |         SharedCross *cross;
      |                      ^
_cycles.cpp:53:23: error: Field type raw pointer
   53 |         SharedCross2 *cross2;
      |                       ^
_cycles.cpp:53:23: error: The class 'cycles::SharedCross' has a circular reference through class 'cycles::SharedCross2'
_cycles.cpp:57:22: error: Field type raw pointer
   57 |         SharedCross *cross;
      |                      ^
```


Уровень сообщений плагина можно ограничить с помощью макроса или аргумента командной строки, 
либо после проверки исходного кода плагин можно вообще не использовать,  
поскольку он только анализирует AST, но не вносит в него никаких исправлений.


### Полный вывод сообщений плагина при компиляции тестового файла `_example.cpp`:
<details>
<summary>Показать вывод</summary>

```bash
clang++-20 -std=c++20 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe -Xclang -plugin-arg-memsafe -Xclang circleref-disable _example.cpp


...


```
</details>


## Обратная связь
Если у вас есть предложения по развитию и улучшению проекта, присоединяйтесь или [пишите](https://github.com/rsashka/memsafe/discussions)

