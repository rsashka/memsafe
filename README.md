# C++ Memory safety (memsafe)

## Motivation

C++ is a programming language with the ability to develop code at any level of abstraction,
from high-level templates and OOP to the lowest level in assembler
with unlimited opportunities for optimization (no need to pay for what is not used).

However, nothing comes for free, and the great capabilities of C++ come at a price in the form of using address arithmetic.
Direct manipulation of memory addresses is a fundamental concept in C++ and is used when working with various types of pointers:
iterators, arrays, strings, dynamic memory allocation, etc.

However, pointers in C++ are implemented as regular numbers and have no connection to variables,
which are on the stack and whose lifetime is managed by the compiler.

And the possibility of creating uninitialized variables and the lack of control (Undefined Behavior)
when dereferencing an invalid address (e.g. when accessing via a null pointer, even in some STL classes)
is a big minefield for various bugs when working with almost any reference data types.

There are many initiatives to make C++ "safer", but they don't fit well with the strict language standards
due to breaking backwards compatibility with old legacy code written over the previous decades.

This project contains code for a library header and compiler plugin for *safe C++*,
which reduces errors when working with reference data types and dynamic memory, while maintaining backward compatibility with old code.

The method of marking objects in C++ program code using attributes is very similar to the one proposed in the security profiles
[p3038](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p3038r0.pdf) by Bjarne Stroustrup
and [P3081](https://isocpp.org/files/papers/P3081R0.pdf) by Herb Sutter,
but does not require the development of a new C++ standard (it is enough to use the existing C++20).

For safe memory mamagment, the [memory safety concept from the NewLang language, adapted for C++](https://github.com/rsashka/memsafe/blob/main/memsafe_concept.md) 
was used, which was supplemented with control over invalidation of reference data types.

## Quick example

```cpp
    std::vector<int> vec(100000, 0);
    auto x = vec.begin();
    auto y = vec.end();
    vec = {};
    vec.shrink_to_fit(); 
    std::sort(x, y); // malloc(): unaligned tcache chunk detected or Segmentation fault 
```

Command line to run compiler plugin `clang++-20 -std=c++20 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe _example.cpp` 

A fragment of the compiler plugin's output messages due to invalidation 
of reference variables after changing data in the main variable:

```bash
clang++-20 -std=c++20 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe _example.cpp

_example.cpp:29:17: warning: using main variable 'vect'
   29 |                 vect = {};
      |                 ^
_example.cpp:30:17: warning: using main variable 'vect'
   30 |                 vect.shrink_to_fit();
      |                 ^
_example.cpp:31:27: error: Using the dependent variable 'beg' after changing the main variable 'vect'!
   31 |                 std::sort(beg, vect.end()); // malloc(): unaligned tcache chunk detected or Segmentation fault 
      |                           ^

```

<details>
<summary>Full output of plugin messages when compiling the test file `_example.cpp`: </summary>

```bash
clang++-20 -std=c++26 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe _example.cpp

_example.cpp:20:28: error: Operator for address arithmetic
   20 |             auto pointer = &vect; // Error
      |                            ^
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
_example.cpp:94:9: error: Cannot copy a shared variable to an equal or higher lexical level
   94 |         var_shared1 = var_shared1; // Error
      |         ^
_example.cpp:100:13: error: Cannot copy a shared variable to an equal or higher lexical level
  100 |             var_shared1 = var_shared1; // Error
      |             ^
_example.cpp:101:13: error: Cannot copy a shared variable to an equal or higher lexical level
  101 |             var_shared2 = var_shared1; // Error
      |             ^
_example.cpp:102:13: error: Cannot copy a shared variable to an equal or higher lexical level
  102 |             var_shared3 = var_shared1; // Error
      |             ^
_example.cpp:108:17: error: Cannot copy a shared variable to an equal or higher lexical level
  108 |                 var_shared1 = var_shared1; // Error
      |                 ^
_example.cpp:109:17: error: Cannot copy a shared variable to an equal or higher lexical level
  109 |                 var_shared2 = var_shared1; // Error
      |                 ^
_example.cpp:110:17: error: Cannot copy a shared variable to an equal or higher lexical level
  110 |                 var_shared3 = var_shared1; // Error
      |                 ^
_example.cpp:112:17: error: Cannot copy a shared variable to an equal or higher lexical level
  112 |                 var_shared4 = var_shared1; // Error
      |                 ^
_example.cpp:113:17: error: Cannot copy a shared variable to an equal or higher lexical level
  113 |                 var_shared4 = var_shared3; // Error
      |                 ^
_example.cpp:115:17: error: Cannot copy a shared variable to an equal or higher lexical level
  115 |                 var_shared4 = var_shared4; // Error
      |                 ^
_example.cpp:120:17: error: Return shared variable
  120 |                 return var_shared4; // Error
      |                 ^
_example.cpp:125:13: error: Error to swap the shared variables of different lexical levels
  125 |             std::swap(var_shared1, var_shared3);
      |             ^
_example.cpp:129:13: error: Return shared variable
  129 |             return arg; // Error
      |             ^
_example.cpp:147:9: error: Return shared variable
  147 |         return arg; // Error
      |         ^
3 warnings and 19 errors generated.

```
</details>


The plugin's message level can be limited using a macro or command line argument,
or after checking the source code, the plugin can be omitted altogether,
since it only parses the AST, but does not make any corrections to it.


## Feedback
If you have any suggestions for the development and improvement of the project, join or [write](https://github.com/rsashka/memsafe/discussions).


---


## Мотивация

С++ - это язык программирования с возможностью разработывать код на любом уровне абстракции,
от высокоуровневых шаблонов и ООП до самого низкого уровня на ассаемблере
с неограниченными возможностями для оптимизации (не нужно платить за то, что не используется).

Однако ничего не дается даром, и за большие возможности С++ берется плата в виде использования адресной арифметики.
Прямое оперирование адресами памяти является фундаментальной концепцией в С++ и применяется при работе с различного рода указателями: 
итераторами, массивами, строками, динамическим выделением памяти и т.д. 

Однако сами указатели в С++ реализованы как обычные числа и у них нет связи с переменными,
которые находятся на стеке и временем жизни которых управляет компилятор.

А возможность создания не инициализированных переменных и отсутствие контроля (Undefined Behaviour) 
при разименовании недействительного адреса (например, при доступе по нулевому указателю даже в некоторых классах STL), 
представляет собой большое минное поле для различных ошибок при работе практически с любыми ссылочными типами данных.

Есть много инициатив, желающих сделать С++ более "безопасным", но они плохо сочетаются со строгими стандартами языка
из-за нарушения обратной совместимость со старым легаси кодом, который был написан за предыдущие десятилетия. 

Данный проект содержит код заголовочного файла библиотеки и плагина компилятора для *безопасного С++*, 
который уменьшает ошибки при работе со ссылочными типами данных и динамиечской памятью без нарушения обратной совместимости со старым кодом. 

Способ маркировки объектов в программном коде С++ реазован с помощью атрибутов и очень похож на предложенный в профилях безопасности 
[p3038](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p3038r0.pdf) от Bjarne Stroustrup 
и [P3081](https://isocpp.org/files/papers/P3081R0.pdf) от Herb Sutter, 
но не требует разработки нового стандарта С++ (достаточно использовать уже существующий С++20).

Для безопасной работы с памятью был использована [концепция безопасной работы с памятью из языка NewLang, адаптированная для С++](https://github.com/rsashka/memsafe/blob/main/memsafe_concept.md),
который был дополнен контролем инвалидации ссылочных типов данных.

## Быстрый пример

```cpp
    std::vector<int> vec(100000, 0);
    auto x = vec.begin();
    auto y = vec.end();
    vec = {};
    vec.shrink_to_fit(); 
    std::sort(x, y); // malloc(): unaligned tcache chunk detected or Segmentation fault 
```

Командная строка для запуска плагина компилятора `clang++ -std=c++20 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe _example.cpp` 

Фрагмент вывода плагина компилятора с сообщениями об ошибках, связанных с недействительностью ссылочных переменных после изменения данных в основной переменной:

```bash
_example.cpp:29:17: warning: using main variable 'vect'
   29 |                 vect = {};
      |                 ^
_example.cpp:30:17: warning: using main variable 'vect'
   30 |                 vect.shrink_to_fit();
      |                 ^
_example.cpp:31:27: error: Using the dependent variable 'beg' after changing the main variable 'vect'!
   31 |                 std::sort(beg, vect.end()); // malloc(): unaligned tcache chunk detected or Segmentation fault 
      |                           ^
```

<details>
<summary>Полный вывод сообщений плагина при компиляции тестового файла `_example.cpp`: </summary>

```bash
clang++-20 -std=c++20 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe _example.cpp

_example.cpp:20:28: error: Operator for address arithmetic
   20 |             auto pointer = &vect; // Error
      |                            ^
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
_example.cpp:94:9: error: Cannot copy a shared variable to an equal or higher lexical level
   94 |         var_shared1 = var_shared1; // Error
      |         ^
_example.cpp:100:13: error: Cannot copy a shared variable to an equal or higher lexical level
  100 |             var_shared1 = var_shared1; // Error
      |             ^
_example.cpp:101:13: error: Cannot copy a shared variable to an equal or higher lexical level
  101 |             var_shared2 = var_shared1; // Error
      |             ^
_example.cpp:102:13: error: Cannot copy a shared variable to an equal or higher lexical level
  102 |             var_shared3 = var_shared1; // Error
      |             ^
_example.cpp:108:17: error: Cannot copy a shared variable to an equal or higher lexical level
  108 |                 var_shared1 = var_shared1; // Error
      |                 ^
_example.cpp:109:17: error: Cannot copy a shared variable to an equal or higher lexical level
  109 |                 var_shared2 = var_shared1; // Error
      |                 ^
_example.cpp:110:17: error: Cannot copy a shared variable to an equal or higher lexical level
  110 |                 var_shared3 = var_shared1; // Error
      |                 ^
_example.cpp:112:17: error: Cannot copy a shared variable to an equal or higher lexical level
  112 |                 var_shared4 = var_shared1; // Error
      |                 ^
_example.cpp:113:17: error: Cannot copy a shared variable to an equal or higher lexical level
  113 |                 var_shared4 = var_shared3; // Error
      |                 ^
_example.cpp:115:17: error: Cannot copy a shared variable to an equal or higher lexical level
  115 |                 var_shared4 = var_shared4; // Error
      |                 ^
_example.cpp:120:17: error: Return shared variable
  120 |                 return var_shared4; // Error
      |                 ^
_example.cpp:125:13: error: Error to swap the shared variables of different lexical levels
  125 |             std::swap(var_shared1, var_shared3);
      |             ^
_example.cpp:129:13: error: Return shared variable
  129 |             return arg; // Error
      |             ^
_example.cpp:147:9: error: Return shared variable
  147 |         return arg; // Error
      |         ^
3 warnings and 19 errors generated.

```
</details>


Уровень сообщений плагина можно ограничить с помощью макроса или аргумента командной строки, 
либо после проверки исходного кода плагин можно вообще не использовать,  
поскольку он только анализирует AST, но не вносит в него никаких исправлений.


## Обратная связь
Если у вас есть предложения по развитию и улучшению проекта, присоединяйтесь или [пишите](https://github.com/rsashka/memsafe/discussions)

