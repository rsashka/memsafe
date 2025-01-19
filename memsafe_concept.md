# Memory Safety сoncept in C++

> The global problem of C and C++ languages ​​is that the pointer to the allocated memory block on the heap is an address
in RAM and there is no connection between it and the variables - pointers that are in local variables on the stack.

> The second, no less serious problem, which often leads to undefined behavior (Undefined Behavior) 
or data races (Data Races) is access to the same memory area from different threads at the same time.

[Safe Memory for C++](https://github.com/rsashka/memsafe) is based on the idea of ​​using strong and weak pointers 
to the allocated memory block on the heap and managing the lifetime of copies of variables using strong pointers.

This is a bit similar to [Rust's concept of ownership and borrowing](https://doc.rust-lang.org/book/ch04-00-understanding-ownership.html), 
but which is implemented based on strong and weak references (standard C++ patterns *shared_ptr* and *weak_ptr*), 
and is therefore fully compatible with the latter at a low level.

Any operations with the variable's data by reference are possible only after its dereferencing, 
i.e. after converting a weak reference (*weak_ptr*) to a strong one (*shared_ptr*), 
and the capture of the inter-thread synchronization object happens automatically 
in the reference dereferencing operator (if the variable uses one).

This mechanism is implemented at the **syntax level** and is performed **at compile time** by the compiler plugin,
while at runtime only the thread ID is checked for single-threaded references.


### Variable Types
It is impossible to allocate or free a memory area manually. 
Memory allocation and freeing for an object occurs automatically when variables are created/deleted 
(on the stack or on the heap), which is why variables in a program are divided into two types by lifetime:
- Permanent variables - are global and static variables that are created statically or on the heap 
and retain their value after the end of the code block in which they were created or defined (after exiting the current scope).
- Temporary (automatic) variables - are function arguments and local variables that are created 
and destroyed by the compiler automatically. Temporary objects are accessible only from the lexical context in which they were defined.
Such variables are placed on the stack and destroyed when exiting the code block in which they were created.

The implemented concept of safe memory management uses several template classes, 
each of which is responsible for its own type of variables or a specific action:

- VarValue (variable by value) — data is stored directly in the variable itself.
These are variables in their classical sense, when a copy of a variable creates a duplicate of the original value, 
and changing a copy of the variable does not affect the original in any way.
*You cannot create a reference to a variable by value (for this you need to use a shared or guard variable)*.

- VarShared (shared variable) — the variable contains only a strong (owning) pointer to a variable by value, 
while memory for data is allocated statically or in a shared heap. 
A copy of a reference variable creates a copy of the pointer and **increases the ownership counter**.
*A reference variable is intended for use only in the current thread and does not have a built-in mechanism for synchronizing inter-thread access.*

- VarGuarg (guard variable) — also a shared variable, but with a built-in mechanism for synchronizing inter-thread access. 
Making a copy of a guard variable copies only the strong pointer and **increments the ownership counter**.
*Intended for data that can be accessed from different threads.*

- VarWeak (pointer variable) - the variable contains only a weak (non-owning) pointer to the reference variable.
Making a copy of a reference variable copies only the weak pointer **without incrementing the ownership counter**.

- VarAuto - a helper object for accessing data from the above variable types, 
[which provides a RAII-style mechanism for owning a captured value for the duration of a bounded block](https://en.cppreference.com/w/cpp/thread/lock_guard). 
*Lock access (if necessary) and converting a weak pointer to a strong one (i.e. effectively
[dereferencing a pointer](https://www.gnu.org/software/c-intro-and-ref/manual/html_node/Pointer-Dereference.html)) are performed as a single operation.*

### Rules for copying and processing variables
A formalized list of rules that are checked (must be checked) in the lexical analyzer based on the included compiler plugin.
- Variables by value and weak pointers can be copied from one variable to another without any restrictions.
- Shared and guard variables can only be copied to automatic variables of a lower lexical level or passed as an argument to a function when it is called.
- Returning guard and shared variables from functions passed as arguments or having a reference ownership count greater than one is prohibited.
- Store captured values in VarAuto ​​as static variables is prohibited.
- The [swap](https://en.cppreference.com/w/cpp/algorithm/swap) operation for guard and shared variables 
is only allowed between variables of the same lexical level.


---


## Концепция безопасной работы с памятью для С++

> Глобальная проблема языков C и C++ в том, что указатель на выделенный блок памяти в куче является адресом 
> в оперативной памяти и отсутствует его связь и переменными - указателями, которые находятся в локальных переменных на стеке. 
> 
> Вторая, не менее серьезная проблема, которая часто приводить к неопределенному поведению (Undefined Behaviour) 
> или гонке данных (Data Races) - это доступ к одной и той же области памяти из разных потоков одновременно.

В основе [проекта безопасной работы с памятью для С++](https://github.com/rsashka/memsafe)  лежит идея использования сильных 
и слабых указателей на выделенный блок памяти в куче и контроль времени жизни копий переменных с сильными указателями. 
Это немного похоже на концепцию [владения и заимствования из языка Rust](https://doc.rust-lang.org/book/ch04-00-understanding-ownership.html), 
но которая реализована на базе сильных и слабых ссылок (стандартных механизмов С++ *shared_ptr* и *weak_ptr*), 
и поэтому полностью совместима с последним на низком уровне.

Любые операции с данными для переменной по ссылке возможны только после её захвата, 
т.е. после преобразования слабой ссылки (*weak_ptr*) в сильную (*shared_ptr*), 
причем захват объекта межпотоковой синхронизации происходит автоматически в операторе захвата ссылки (если у переменной он используется).

Данный механизм реализован на **уровне синтаксиса** и выполняется **во время компиляции** с помощью плагина, 
тогда как в рантайме происходит только контроль идентификатора потока для однопоточных ссылок. 

### Виды переменных
Выделить или освободить участок памяти вручную нельзя. 
Выделение и освобождение памяти под объект происходит автоматически при создании/удалении переменных (на стеке или в куче), 
из-за чего переменные в программе разделятся на на два вида по времени жизни:
- Постоянные переменные  - это глобальные и статические переменные, которые создаются статически или в куче 
и сохраняют свое значение после завершения блока кода, в котором они были созданы или определены (после выхода из текущей области видимости).
- Временные (автоматические) переменные - это аргументы функций и локальные переменные, 
которые создаются и уничтожаются компилятором в автоматическом режиме. 
Временные объекты доступные только изнутри того лексического контекста, в котором они были определены. 
Такие переменные размещаются на стеке и уничтожается при выходе из блока кода, где были созданы.

В реализованной концепции безопасной работы с памятью используется несколько шаблонных классов, 
каждый из которых отвечает за свой тип переменных или определенное действие:

- VarValue - переменная по значению (variable by value) - данные хранятся непосредственно в самой переменной. 
Это переменные в их классическом понимании, когда копия переменной создает дубликат исходного значения, 
а изменение копии переменной никак не влияет на исходный оригинал. 
*Создать ссылку на переменную по значению нельзя (для этих целей нужно использовать ссылочную или защищенную переменную)*.

- VarShared - ссылочная переменная (reference variable) - в переменой находится только сильный (владеющий) 
указатель на переменную по значению, тогда как память под данные выделена статически или в общей куче. 
Копия ссылочной переменной создает копию указателя и **увеличивает счетчик владений**. 
*Ссылочная переменная предназначена для использования только в текущем потоке и не имеет встроенного механизма межпотоковой синхронизации доступа.*

- VarGuarg - защитная переменная (guard variable) - тоже ссылочная переменная, но со встроенным механизмом межпотоковой синхронизации. 
Создание копии защищенной переменной копирует только сильный указатель и **увеличивает счетчик владений**. 
*Предназначена для данных к которым можно получить доступ из разных потоков.*

- VarWeak - Переменная ссылка (link variable) - в переменой находится только слабый (не владеющий) указатель на ссылочную переменную. 
Создание копии переменной-ссылки копирует только слабый указатель **без увеличения счетчика владений**. 

- VarAuto - вспомогательный объект для доступа к данным у перечисленных выше видов переменных, 
[который предоставляет механизм в стиле RAII для владения захваченным значением на время действия ограниченного блока](https://en.cppreference.com/w/cpp/thread/lock_guard). 
*Блокировка доступа (если она требуется) и преобразование слабого указателя в сильный (т.е. фактически 
[разименование указателя](https://www.gnu.org/software/c-intro-and-ref/manual/html_node/Pointer-Dereference.html)), выполняются как одна операция.*

### Правила копирования и обработки переменных
Формализованный список правил, которые проверяются (должны проверяться) в лексическом анализаторе на основе подлючаемого плагина компилятора.
- Переменные по значению и слабые ссылки могут копироваться из одной переменной в другую без каких либо ограничений.
- Защищенная и ссылочная переменная могут быть скопированы только в локальные переменные 
более низкого лексического уровня или переданы в качестве аргумента в функцию при её вызове.
- Запрещен возврат из функций защищенных и ссылочных переменных, 
которые были переданы в качестве аргументов или со счетчиком ссылок более единицы.
- Запрещено сохранение захваченных значений как статических переменных.
- Операция [обмена значениями (swap)](https://en.cppreference.com/w/cpp/algorithm/swap) для защищенных 
и ссылочных переменных разрешены только между переменными одного лексического уровня.
  
