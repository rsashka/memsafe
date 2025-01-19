# C++ Memory safety (memsafe)

## Safe Development in C++

C++'s ambition to become a "safer" programming language does not fit well with the requirements of the language standard.

Any C++ standard must provide backward compatibility with old legacy code, 
which automatically nullifies any attempts to add any keywords at the C++ standard level.

And since the current state of C++ cannot guarantee secure development at the standards level, it turns out that:
- Adopting new C++ standards with a change in the language vocabulary for secure development 
will necessarily break backward compatibility with existing legacy code.
- Rewriting the entire existing C++ code base (if such standards were adopted) 
is no cheaper than rewriting the same code in a new fashionable programming language.

And the way out of this situation is to implement *safe C++ syntax* that would satisfy both of these requirements. 
Moreover, the best option would be not to adopt any new C++ standards at all to change the vocabulary, 
but to try to use the existing ones, adopted earlier.

*Additional syntactic analysis by means of third-party applications (static analyzers, 
for example, based on Clang-Tidy) **is not considered**, since any solution external 
to the compiler will always contain at least one significant drawback - the need for synchronous support 
and use of the same modes of compilation of program source texts, 
which for C++ with its preprocessor can be a very non-trivial task.*


## Proof of Concept

This repository contains code for an example of marking C++ objects (types, templates, etc.)
using custom attributes and then lexically analyzing them using a compiler plugin.
It is based on the [memory safety in C++](https://github.com/rsashka/memsafe/blob/main/memsafe_concept.md) concept prototype.

The method of marking objects in C++ program code using attributes is very similar to the one proposed in the security profiles
[p3038](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p3038r0.pdf) by Bjarne Stroustrup
and [P3081](https://isocpp.org/files/papers/P3081R0.pdf) by Herb Sutter,
but does not require the development of a new C++ standard (it is enough to use the existing C++20).

Since such additional checks will most likely not work for old legacy code,
therefore their implementation using disableable compiler plugins is most preferable.

The project on safe work with memory in C++ is currently implemented **partially**:
- Standard development tools are used in the form of a plugin for the compiler
(the plugin is made for Clang, but GCC also allows using plugins)
- Does not break backward compatibility with the old source code, since it does not change the C++ dictionary,
and the new code can be compiled by any compiler without using a plugin (at least the C++20 standard)
- Allows you to write new code with strict checking of safe work with memory based on formalized syntax rules,
and the dictionary checking rules can be quickly supplemented as they are clarified and without changing the compiler itself.
- A detailed description of the concept of safe work with memory in C++ is given [here](https://github.com/rsashka/memsafe/blob/main/memsafe_concept.md).
It is based on the idea of ​​using strong and weak pointers to an allocated block of memory on the heap
and managing the lifetime of copies of variables using strong pointers.
It is a bit similar to the concept of [ownership and borrowing from the Rust language](https://doc.rust-lang.org/book/ch04-00-understanding-ownership.html),
but is implemented based on strong and weak references (standard C++ mechanisms *shared_ptr* and *weak_ptr*),
and therefore is fully compatible with the latter at a low level.


## Disclaimer

##### *Checking of lexical rules of copying and borrowing is implemented partially and this code is not intended for industrial use, but serves only to demonstrate the functionality of the concept of checking lexical rules using a compiler plugin without breaking backward compatibility with existing C++ code!*

### Implementation details:
- The implementation is implemented as a single header file [memsafe.h](https://github.com/rsashka/memsafe/blob/main/memsafe.h) +
[compiler plugin](https://github.com/rsashka/memsafe/blob/main/memsafe_clang.cpp).
- Syntax checking of the correctness of class usage occurs in the plugin, which is dynamically connected during compilation.
- Each used template class is marked with the `[[memsafe(...)]]` attribute according to its purpose
and when checking the dictionary, the names of template classes are searched for the presence of the specified attributes.
- The plugin is designed for Clang (the current version of clang-19 is used)
- Checking of syntax rules is activated automatically when the plugin is connected
using the built-in function `__has_attribute(memsafe)`.

If the plugin is missing during compilation, then the use of attributes is disabled using preprocessor macros
to suppress warnings like `warning: unknown attribute 'memsafe' passed [-Wunknown-attributes]`.
- For testing and debugging purposes, the plugin can create a copy of the analyzed file,
with the addition of marks in the form of ordinary comments about the use of syntax rules or errors found for their subsequent analysis.
- The standard [clang::FixItRewriter](https://clang.llvm.org/doxygen/classclang_1_1FixItRewriter.html) method is used to create a copy of the file,
the output of which can be used in any development environment that supports it.

### Quick examples:

<details>
<summary>A fragment of correct C++ code: </summary>
   
```cpp
    #include "memsafe.h"

    using namespace memsafe;

    namespace MEMSAFE_ATTR("unsafe") {
        // Plugin checks are ignored
        VarShared<int> var_unsafe1(1);
        memsafe::VarShared<int> var_unsafe2(2);
        memsafe::VarShared<int> var_unsafe3(3);
    }
    
    memsafe::VarValue<int> var_value(1);
    memsafe::VarShared<int> var_share(1);
    memsafe::VarGuard<int, memsafe::VarSyncMutex> var_guard(1);

    static memsafe::VarValue<int> var_static(1);
    static auto static_fail1(var_static.take()); // Correct C++ code, but incorrect for memsafe
    
    memsafe::VarShared<int> stub_function(memsafe::VarShared<int> arg, memsafe::VarValue<int> arg_val) {

        memsafe::VarShared<int> var_shared1(1);
        memsafe::VarShared<int> var_shared2(1);

        var_shared1 = var_shared2; // Correct C++ code, but incorrect for memsafe
        {
            memsafe::VarShared<int> var_shared3(3);
            var_shared3 = var_shared1; // OK

            std::swap(var_shared1, var_shared3); // Correct C++ code, but incorrect for memsafe
        }

        return 777; // OK
    }

    memsafe::VarShared<int> stub_function8(memsafe::VarShared<int> arg) {
        return arg; // Correct C++ code, but incorrect for memsafe
    }

```
</details>


Command line for connecting the plugin `clang++ -std=c++20 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe _example.cpp`

<details>
<summary>The output of the compiler plugin with error messages about using the memsafe library classes: </summary>

```bash
clang++-19 -std=c++20   -c -g -DBUILD_UNITTEST -I. -std=c++20 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe -MMD -MP -MF "build/UNITTEST/CLang-19-Linux/_example.o.d" -o build/UNITTEST/CLang-19-Linux/_example.o _example.cpp

Register template class 'memsafe::VarAuto' and 'VarAuto' with 'auto' attribute!
Register template class 'memsafe::VarValue' and 'VarValue' with 'value' attribute!
Register template class 'memsafe::VarShared' and 'VarShared' with 'shared' attribute!
Register template class 'memsafe::VarGuard' and 'VarGuard' with 'shared' attribute!
Register template class 'memsafe::VarWeak' and 'VarWeak' with 'weak' attribute!
In file included from _example.cpp:1:
./memsafe.h:645:38: remark: Memory safety plugin is enabled!
  645 |     namespace MEMSAFE_ATTR("enable") {
      |                                      ^
_example.cpp:25:17: error: Create auto variabe as static
   25 |     static auto static_fail1(var_static.take());
      |                 ^
      |                 /* [[memsafe(error, 3002)]] */ 
_example.cpp:26:17: error: Create auto variabe as static
   26 |     static auto static_fail2 = var_static.take();
      |                 ^
      |                 /* [[memsafe(error, 3003)]] */ 
_example.cpp:52:21: error: Cannot copy a shared variable to an equal or higher lexical level
   52 |         var_shared1 = var_shared1;
      |                     ^
      |                     /* [[memsafe(error, 4024)]] */ 
_example.cpp:53:21: error: Cannot copy a shared variable to an equal or higher lexical level
   53 |         var_shared1 = var_shared2;
      |                     ^
      |                     /* [[memsafe(error, 4025)]] */ 
_example.cpp:57:25: error: Cannot copy a shared variable to an equal or higher lexical level
   57 |             var_shared1 = var_shared1;
      |                         ^
      |                         /* [[memsafe(error, 4029)]] */ 
_example.cpp:58:25: error: Cannot copy a shared variable to an equal or higher lexical level
   58 |             var_shared2 = var_shared1;
      |                         ^
      |                         /* [[memsafe(error, 4030)]] */ 
_example.cpp:59:25: error: Cannot copy a shared variable to an equal or higher lexical level
   59 |             var_shared3 = var_shared1;
      |                         ^
      |                         /* [[memsafe(error, 4031)]] */ 
_example.cpp:65:29: error: Cannot copy a shared variable to an equal or higher lexical level
   65 |                 var_shared1 = var_shared1;
      |                             ^
      |                             /* [[memsafe(error, 4037)]] */ 
_example.cpp:66:29: error: Cannot copy a shared variable to an equal or higher lexical level
   66 |                 var_shared2 = var_shared1;
      |                             ^
      |                             /* [[memsafe(error, 4038)]] */ 
_example.cpp:67:29: error: Cannot copy a shared variable to an equal or higher lexical level
   67 |                 var_shared3 = var_shared1;
      |                             ^
      |                             /* [[memsafe(error, 4039)]] */ 
_example.cpp:69:29: error: Cannot copy a shared variable to an equal or higher lexical level
   69 |                 var_shared4 = var_shared1;
      |                             ^
      |                             /* [[memsafe(error, 4041)]] */ 
_example.cpp:70:29: error: Cannot copy a shared variable to an equal or higher lexical level
   70 |                 var_shared4 = var_shared3;
      |                             ^
      |                             /* [[memsafe(error, 4042)]] */ 
_example.cpp:72:29: error: Cannot copy a shared variable to an equal or higher lexical level
   72 |                 var_shared4 = var_shared4;
      |                             ^
      |                             /* [[memsafe(error, 4044)]] */ 
_example.cpp:77:13: error: Cannot swap a shared variables of different lexical levels
   77 |             std::swap(var_shared1, var_shared3);
      |             ^
      |             /* [[memsafe(error, 4049)]] */ 
_example.cpp:95:16: error: Return share type
   95 |         return arg;
      |                ^
      |                /* [[memsafe(error, 5004)]] */ 
15 errors generated.
```
</details>


### Description and main commands (attributes in C++ code)
By default, checking of additional lexical rules is disabled.

The following commands enable or disable lexical checking of memory safety rules:
- `namespace [[memsafe("define")]] NAME { ... }` - defines the namespace `NAME`, in which the module's template classes will be located.
- `[[memsafe("value")]]`, `[[memsafe("shared")]]`, `[[memsafe("guard")]]`, `[[memsafe("auto")]]` and `[[memsafe("weak")]]` - attributes
for marking template classes in the [project header file](https://github.com/rsashka/memsafe/blob/main/memsafe.h).
- `namespace [[memsafe("enable")]] { }` - command to enable the compiler plugin parser for template classes,
marked with attributes in the `NAME` namespace until the end of the file or until the disable command.
- `namespace [[memsafe("disable")]] { }` - command to disable the compiler plugin parser until the end of the file or until the enable command.
- `namespace [[memsafe("unsafe")]] { ... }` - definition of a namespace, including anonymous, in which the parser
ignores safe usage errors of memsafe classes.
It is possible to use the `[[memsafe("unsafe")]]` attribute for individual operators, but this is not currently implemented.
(For example, you can't currently do something like `[[memsafe("unsafe")]] return nullptr;` to disable the check for one specific statement.
This requires a newer version of clang with an implementation of [Pull requests #110334](https://github.com/llvm/llvm-project/pull/110334))

### Currently (for the purpose of demonstrating the concept functionality), the plugin implements the following dictionary checks:
- Disable copying of reference and protected variables within the same level (marked with `[[memsafe("shared")]]`)
- Disable sharing values ​​between two reference variables of different levels (marked with `[[memsafe("shared")]]`)
- Disable creating static captured variables, marked with `[[memsafe("auto")]]`
- Disable returning captured (automatic) variables from a function (marked with `[[memsafe("auto")]]`)
- Prevent returning reference and protected variables from a function, except for those created directly in the return statement

## Feedback
If you have any suggestions for the development and improvement of the project, join or [write](https://github.com/rsashka/memsafe/discussions).


---


## Безопасная разработка на С++

Стремление С++ стать  более "безопасным" языком программирования плохо сочетается с требования к стандарту языка. 
Ведь любой стандарт должен обеспечивать обратную совместимость со старым легаси кодом, 
что автоматически сводит на нет любые попытки внедрения какой либо новой лексики на уровне единого стандарта С++. 
А раз текущее состояние С++ не может гарантировать безопасную разработку на уровне стандартов, то выходит, что:
- Принятие новых стандартов С++ с изменением лексики для безопасной разработки обязательно нарушат обратную совместимость с существующим легаси кодом.
- Переписать всю имеющуюся кодовую базу С++ под новую безопасную лексику (если бы такие стандартны были приняты), 
ничуть не дешевле, чем переписать этот же код на новом модном языке программирования.

И выходом из данной ситуации является реализация такого синтаксиса *безопасного С++*, 
который бы позволил удовлетворить оба этих требования. Причем самый лучший вариант, 
вообще не принимать какие либо новые стандарты С++ для изменения лексики, 
а попробовать использовать уже существующие принятые ранее.

*Дополнительный синтаксический анализ за счет сторонних приложений (статических анализаторов, 
например, на базе Clang-Tidy) **не рассматривается**, так как любое внешнее по отношению к компилятору решение, 
всегда будет содержать как минимум один существенный недостаток - 
необходимость синхронно поддерживать и использовать одни и те же режимы компиляции исходных текстов программ, 
что для С++ может быть очень не тривиальной задачей с его препроцессором.*

## Proof of Concept

Данный репозиторий содержит код примера маркировки объектов С++ (типов, шаблонов и т.д.) 
с помощью пользовательских атрибутов и их последующего лексического анализа с помощью плагина компилятора. 
В качестве основы был использован прототип концепции [безопасной работы с памятью на С++](https://github.com/rsashka/memsafe/blob/main/memsafe_concept.md).

Способ маркировки объектов в программном коде С++ с помощью атрибутов очень похож на предложенный в профилях безопасности 
[p3038](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p3038r0.pdf) от Bjarne Stroustrup 
и [P3081](https://isocpp.org/files/papers/P3081R0.pdf) от Herb Sutter, 
но не требует разработки нового стандарта С++ (достаточно использовать уже существующий С++20).

Ведь подобные дополнительные проверки с высокой долей вероятности не будут проходить для старого легаси кода, 
поэтому их реализация с помощью отключаемых плагинов компилятора наиболее предпочтительна.

Проект для безопасной работы с памятью на С++ сейчас реализован **частично**:
- Используются стандартные средства разработки в виде плагина для компилятора 
(плагин сделан под Clang, но GCC тоже позволяет использовать плагины)
- Не нарушает обратной совместимости со старым исходным кодом, так как не изменяет лексику С++, 
а новый код может быть собран любым компилятором без применения плагина (минимальный стандарт C++20)
- Позволяет писать новый код со строгой проверкой безопасной работы с памятью на основе формализованных синтаксических правил, 
причем правила для проверки лексики могут быть оперативно дополнены по мере их уточнения и без изменения самого компилятора.
- Подробное описание концепции безопасной работы с памятью С++ приводится [тут](https://github.com/rsashka/memsafe/blob/main/memsafe_concept.md). 
В её основе лежит идея использования сильных и слабых указателей на выделенный блок памяти в куче 
и контроль времени жизни копий переменных с сильными указателями. 
Это немного похоже на концепцию [владения и заимствования из языка Rust](https://doc.rust-lang.org/book/ch04-00-understanding-ownership.html), 
но реализованную на базе сильных и слабых ссылок (стандартных механизмов С++ *shared_ptr* и *weak_ptr*), 
и поэтому полностью совместима с последним на низком уровне.

### Отказ от ответственности

##### *Проверка лексических правил копирования и заимствования реализована частично и данный код не предназначен для промышленного использования, а служит только для демонстрации работоспособности концепции проверки лексики с помощью плагина компилятора без нарушения обратной совместимости с существующим С++ кодом!*


### Детали реализации:
- Реализация сделана в виде одного заголовочного файла [memsafe.h](https://github.com/rsashka/memsafe/blob/main/memsafe.h) + 
[плагин для компилятора](https://github.com/rsashka/memsafe/blob/main/memsafe_clang.cpp). 
- Синтаксическая проверка корректности использования классов происходит в плагине, который подключается динамически при компиляции.
- Каждый используемый шаблонный класс помечен атрибутом `[[memsafe(...)]]` в соответствии со своим назначением 
и при проверке лексики поиск имен шаблонных классов выполняется по наличию указанных атрибутов. 
- Плагин разработан для Clang (использована актуальная версия clang-19)
- Проверка синтаксических правил активируется автоматически во время при подключении плагина 
за счет использования встроенной функции  `__has_attribute (memsafe)`. 
Если плагин во время компиляции отсутствует, то использование атрибутов отключается с помощью макросов препроцессора 
для подавления предупреждений вида `warning: unknown attribute 'memsafe' ignored [-Wunknown-attributes]`.
- Для целей тестирования и отладки плагин может создавать копию анализируемого файла, 
с добавлением в него меток в виде обычных комментариев о применении синтаксических правил или найденных ошибках для их последующего анализа. 
- Для создания копии файла использует стандартный способ [clang::FixItRewriter](https://clang.llvm.org/doxygen/classclang_1_1FixItRewriter.html), 
вывод которого можно использовать в том числе и в любой среде разработки, которая его поддерживает.


### Быстрые примеры:

<details>
  <summary>Фрагмент корректного С++ кода: </summary>
   
```cpp
    #include "memsafe.h"

    using namespace memsafe;

    namespace MEMSAFE_ATTR("unsafe") {
        // Проверки плагина игнорируются
        VarShared<int> var_unsafe1(1);
        memsafe::VarShared<int> var_unsafe2(2);
        memsafe::VarShared<int> var_unsafe3(3);
    }
    
    memsafe::VarValue<int> var_value(1);
    memsafe::VarShared<int> var_share(1);
    memsafe::VarGuard<int, memsafe::VarSyncMutex> var_guard(1);

    static memsafe::VarValue<int> var_static(1);
    static auto static_fail1(var_static.take()); // Корректный С++ код, но неправлиный с точки зрения memsafe
    
    memsafe::VarShared<int> stub_function(memsafe::VarShared<int> arg, memsafe::VarValue<int> arg_val) {

        memsafe::VarShared<int> var_shared1(1);
        memsafe::VarShared<int> var_shared2(1);

        var_shared1 = var_shared2; // Корректный С++ код, но неправлиный с точки зрения memsafe
        {
            memsafe::VarShared<int> var_shared3(3);
            var_shared3 = var_shared1; // OK

            std::swap(var_shared1, var_shared3); // Корректный С++ код, но неправлиный с точки зрения memsafe
        }

        return 777; // OK
    }

    memsafe::VarShared<int> stub_function8(memsafe::VarShared<int> arg) {
        return arg; // Корректный С++ код, но неправлиный с точки зрения memsafe
    }

```
</details>

Командная строка для подключения плагина `clang++ -std=c++20 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe _example.cpp` 

<details> 
  <summary>Вывод плагина компилятора с сообщениями об ошибках использования классов библиотеки memsafe: </summary>

```bash
clang++-19 -std=c++20   -c -g -DBUILD_UNITTEST -I. -std=c++20 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe -MMD -MP -MF "build/UNITTEST/CLang-19-Linux/_example.o.d" -o build/UNITTEST/CLang-19-Linux/_example.o _example.cpp

Register template class 'memsafe::VarAuto' and 'VarAuto' with 'auto' attribute!
Register template class 'memsafe::VarValue' and 'VarValue' with 'value' attribute!
Register template class 'memsafe::VarShared' and 'VarShared' with 'shared' attribute!
Register template class 'memsafe::VarGuard' and 'VarGuard' with 'shared' attribute!
Register template class 'memsafe::VarWeak' and 'VarWeak' with 'weak' attribute!
In file included from _example.cpp:1:
./memsafe.h:645:38: remark: Memory safety plugin is enabled!
  645 |     namespace MEMSAFE_ATTR("enable") {
      |                                      ^
_example.cpp:25:17: error: Create auto variabe as static
   25 |     static auto static_fail1(var_static.take());
      |                 ^
      |                 /* [[memsafe(error, 3002)]] */ 
_example.cpp:26:17: error: Create auto variabe as static
   26 |     static auto static_fail2 = var_static.take();
      |                 ^
      |                 /* [[memsafe(error, 3003)]] */ 
_example.cpp:52:21: error: Cannot copy a shared variable to an equal or higher lexical level
   52 |         var_shared1 = var_shared1;
      |                     ^
      |                     /* [[memsafe(error, 4024)]] */ 
_example.cpp:53:21: error: Cannot copy a shared variable to an equal or higher lexical level
   53 |         var_shared1 = var_shared2;
      |                     ^
      |                     /* [[memsafe(error, 4025)]] */ 
_example.cpp:57:25: error: Cannot copy a shared variable to an equal or higher lexical level
   57 |             var_shared1 = var_shared1;
      |                         ^
      |                         /* [[memsafe(error, 4029)]] */ 
_example.cpp:58:25: error: Cannot copy a shared variable to an equal or higher lexical level
   58 |             var_shared2 = var_shared1;
      |                         ^
      |                         /* [[memsafe(error, 4030)]] */ 
_example.cpp:59:25: error: Cannot copy a shared variable to an equal or higher lexical level
   59 |             var_shared3 = var_shared1;
      |                         ^
      |                         /* [[memsafe(error, 4031)]] */ 
_example.cpp:65:29: error: Cannot copy a shared variable to an equal or higher lexical level
   65 |                 var_shared1 = var_shared1;
      |                             ^
      |                             /* [[memsafe(error, 4037)]] */ 
_example.cpp:66:29: error: Cannot copy a shared variable to an equal or higher lexical level
   66 |                 var_shared2 = var_shared1;
      |                             ^
      |                             /* [[memsafe(error, 4038)]] */ 
_example.cpp:67:29: error: Cannot copy a shared variable to an equal or higher lexical level
   67 |                 var_shared3 = var_shared1;
      |                             ^
      |                             /* [[memsafe(error, 4039)]] */ 
_example.cpp:69:29: error: Cannot copy a shared variable to an equal or higher lexical level
   69 |                 var_shared4 = var_shared1;
      |                             ^
      |                             /* [[memsafe(error, 4041)]] */ 
_example.cpp:70:29: error: Cannot copy a shared variable to an equal or higher lexical level
   70 |                 var_shared4 = var_shared3;
      |                             ^
      |                             /* [[memsafe(error, 4042)]] */ 
_example.cpp:72:29: error: Cannot copy a shared variable to an equal or higher lexical level
   72 |                 var_shared4 = var_shared4;
      |                             ^
      |                             /* [[memsafe(error, 4044)]] */ 
_example.cpp:77:13: error: Cannot swap a shared variables of different lexical levels
   77 |             std::swap(var_shared1, var_shared3);
      |             ^
      |             /* [[memsafe(error, 4049)]] */ 
_example.cpp:95:16: error: Return share type
   95 |         return arg;
      |                ^
      |                /* [[memsafe(error, 5004)]] */ 
15 errors generated.
```
</details>

### Описание и основные команды (атрибуты в С++ коде)
По умолчанию проверка дополнительных лексическая правил отключена. 
Включение или отключение лекстической проверки правил для безопасной работы с памятью производится следующими командами:
- `namespace [[memsafe("define")]] NAME { ... }` - определение области имен `NAME` в которой будут присутствовать шаблонные классы для работы модуля.
- `[[memsafe("value")]]`, `[[memsafe("shared")]]`, `[[memsafe("guard")]]`, `[[memsafe("auto")]]` и `[[memsafe("weak")]]` - атрибуты 
для маркировки шаблонных классов в [заголовочном файле проекта](https://github.com/rsashka/memsafe/blob/main/memsafe.h).
- `namespace [[memsafe("enable")]] { }` - команда включения синтаксического анализатора плагина компилятора для шаблонных классов, 
промаркированных атрибутами в области имен `NAME` до конца файла или до команды отключения.
- `namespace [[memsafe("disable")]] { }` - команда выключает синтаксический анализатор плагина компилятора до конца файла или до команды включения.
- `namespace [[memsafe("unsafe")]] { ... }` - определение пространства имен, в том числе анонимного, в котором синтаксический анализатор 
игнорирует ошибки безопасного использования memsafe классов. 
Возможно использование атрибута `[[memsafe("unsafe")]]` и для отдельных операторов, но в настоящий момент это не реализовано. 
(Например, сейчас нельзя сделать вот так `[[memsafe("unsafe")]] return nullptr;` для отключения проверки одного конкретного оператора. 
Для этого нужна более новая версия clang с реализацией [Pull requests #110334](https://github.com/llvm/llvm-project/pull/110334))

### В настоящий момент (для целей демонстрации работоспособности концепции) в плагине реализованы следующие проверки лексики:
- Запрет копирования ссылочных и защищенных переменных в рамках одного уровня (отмеченных `[[memsafe("shared")]]`)
- Запрет обмена значениями между двумя ссылочными переменными разных уровней (отмеченных `[[memsafe("shared")]]`)
- Запрет создания статических захваченных переменных отмеченных `[[memsafe("auto")]]`
- Запрет возврата из функции захваченных (автоматических) переменных (отмеченных `[[memsafe("auto")]]`)
- Запрет возврата из функции ссылочных и защищенных переменных, кроме созданных непостредственно в операторе возврата


## Обратная связь
Если у вас есть предложения по развитию и улучшению проекта, присоединяйтесь или [пишите](https://github.com/rsashka/memsafe/discussions)

