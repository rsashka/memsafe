# Version history

## Release v0.4 (07.04.2025)
- The theses that guarantee safe memory management are formulated.
- The plugin now supports analysis of cyclic references between classes.
- The name of the automatic variable class has been renamed to Locker to match the logic of the functionality it performs.
- Disabled ownership checks and strong reference borrowing as it is no longer required.

------


## Release v0.3 (17.03.2025)
- Refactor the library interface to simplify inheritance:    
    - Сombined *shared* and *guard* variables into one template class
    - Pointer data types with invalidation control are merged with automatic data types
- Add restriction on using reference types as the data type of variables has been added to templates.
- The plugin has new checks for reference types of class fields
- Fixed checking for use of unwanted (warning) and forbidden (error) classes
- Added control of reference types (pointers)
- New tests added and project description reworked

------


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