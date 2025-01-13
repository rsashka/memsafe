# C++ Memory safety (memsafe)

A prototype of the concept of safe work with memory in the style of
strong reference control and ownership transition (somewhat similar to Rust),
but for the C++ language based on the template classes std::shared_ptr and std::weak_ptr
and without breaking the backward compatibility of the code.

The implementation is made in the form of a header file and a plugin for the compiler (Clang),
with the help of which the checking of the rules of ownership and borrowing during compilation is implemented.

The code uses marking of the class and its individual methods [C++ attributes](https://en.cppreference.com/w/cpp/language/attributes),
therefore, the minimum supported version of the C++ compiler standard cannot be less than C++11 (C++23 is used for development).

This code can be compiled without a special plugin,
but in this case the compiler will issue warnings about an unknown attribute
(warning: unknown attribute 'memsafe' ignored [-Wunknown-attributes]),
and reference and ownership transfer control will not be performed.

---

Прототип [концепции безопасной работы с памятью](https://habr.com/ru/articles/853646/) в стиле
контроля сильных ссылок и перехода владения (чем-то похоже на Rust),
но для языка C++ на основе шаблонных классов std::shared_ptr и std::weak_ptr
и без нарушения обратной совместимости кода.

Реализация выполнена в виде заголововчного файла и плагина для компилятора (Clang), 
с помощью которого и рализуется проверка правил владения и заимствования во время копмиляции.

В коде используются маркировка класса и его отдельных методов [атрубитами С++](https://en.cppreference.com/w/cpp/language/attributes),
поэтому минимальная поддерживаемая версия стандарта С++ компилятора не может быть меньше С++11 (для разработки используется C++23).

Данный код можно компилировать без специального плагина, 
просто в этом случае компилятор будет выдавать предупреждения о неизвестном атрибуте 
(warning: unknown attribute 'memsafe' ignored [-Wunknown-attributes]),
а контроль ссылок и перехода владений осуществляться не будет.
