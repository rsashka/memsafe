# C++ Memory safety (memsafe)



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
- Реализация сделана в виде одного заголовочного файла memsafe.h + плагин для компилятора. 
- Синтаксическая проверка корректности использования классов происходит в плагине компилятора, который подключается динамически при компиляции.
- Каждый используемый шаблонный класс помечен атрибутом `[[memsafe(...)]]` в соответствии со своим назначением 
и при проверке лексики поиск имен шаблонных классов выполняется по наличию указанных атрибутов. 
Подобная маркировка с помощью атрибутов очень похожа на предложенную в профилях безопасности 
[p3038](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p3038r0.pdf) от Bjarne Stroustrup и 
[P3081](https://isocpp.org/files/papers/P3081R0.pdf) от Herb Sutter, 
но не требует принятия нового стандарта С++ (достаточно использовать С++20).
- Плагин разработан для Clang (использована актуальная версия clang-19)
- Проверка синтаксических правил активируется автоматически во время при подключении плагина 
за счет использования встроенной функции  `__has_attribute (memsafe)`. 
И если плагин во время компиляции отсутствует, то использование атрибутов отключается с помощью макросов препроцессора 
для подавления предупреждений вида `warning: unknown attribute 'memsafe' ignored [-Wunknown-attributes]`.
- В целях отладки плагин создает копию анализируемого файла с расширением `.memsafe`, 
чтобы добавить в него метки в виде обычных комментариев о применении синтаксических правил для их последующего анализа.
- Для создания копии использует стандартный способ [clang::FixItRewriter](https://habr.com/ru/articles/872268/), 
вывод которого можно использовать в том числе и в любой среде разработки, которая его поддерживает.

### Быстрые примеры:

Исходный файл 
...

Командная строка для подключения плагина `clang++ -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe ... ` 
...

Вывод
...

Выходной файл с отметками плагина о примененными правилами.
...



### Описание и основные команды (атрибуты в С++ коде)
По умолчанию проверка дополнительных лексическая правил отключена. 
Включение или отключение лекстической проверки правил для безопасной работы с памятью производится следующими командами:
- `namespace [[memsafe("define")]] NAME { ... }` - определение области имен `NAME` в которой будут присутствовать шаблонные классы для работы модуля.
- `namespace [[memsafe("enable")]] { }` - команда включения синтаксического анализатора плагина компилятора для шаблонных классов, 
промаркированных атрибутами в области имен `NAME` до конца файла или до команды отключения.
- `namespace [[memsafe("disable")]] { }` - команда выключает синтаксический анализатор плагина компилятора до конца файла или до команды включения.
- `namespace [[memsafe("unsafe")]] { ... }` - определение пространства имен, в том числе анонимного, в котором синтаксический анализатор игнорирует ошибки. 
Возможно использование атрибута `[[memsafe("unsafe")]]` для отдельных операторов, но в настоящий момент это не реализовано. 
(Например сейчас нельзя сделать вот так `[[memsafe("unsafe")]] return nullptr;` для отключения проверки одного конкретного оператора. 
Для этого нужна более новая версия clang с реализацией [Pull requests #110334](https://github.com/llvm/llvm-project/pull/110334))
- `[[memsafe("value")]]`, `[[memsafe("share")]]`, `[[memsafe("guard")]]`, `[[memsafe("auto")]]` - атрибуты 
для маркировки шаблонных классов в [заголовочном файле проекта](https://github.com/rsashka/memsafe/blob/main/memsafe.h).
