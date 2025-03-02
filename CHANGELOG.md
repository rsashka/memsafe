# Version history

## Release v0.2 (02.03.2025)
- Added invalidation analysis of reference data types
- Memory safety library adapted for C++

## Miscellaneous
- Switched to clang-20 (added support for custom attributes in expressions)
- Add expanded diagnostics and plugin development methods (MEMSAFE_PRINT_DUMP and MEMSAFE_PRINT_AST macros)
- Removed automated testing with FixIt and reorganized plugin log output to indicate where the message was created for use in tests.

------

## First prototype (19.01.2025)

The first prototype of safe memory library from the [NewLang](https://newlang.net/), 
adapted for C++ and a clang plugin for analyzing source code when compiling a C++ program.

The method of marking objects in C++ program code using attributes is very similar to the one proposed in the security profiles
[p3038](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p3038r0.pdf) by Bjarne Stroustrup
and [P3081](https://isocpp.org/files/papers/P3081R0.pdf) by Herb Sutter,
but does not require the development of a new C++ standard (it is enough to use the existing C++20).