# UI Graphics Toolkit

Небольшая C++‑библиотека для работы с цветами, графикой и ресурсами интерфейса под Windows (MFC/Win32).  
Проект предоставляет менеджеры цветов и изображений, а также набор вспомогательных классов для рисования и оптимизированного выделения памяти.

## Возможности

- **Управление цветами и темами**
  - Класс `CUIColorManager` и связанные типы (`CUIPaintManagerColor`, `CUIPaintManagerColorGradient`).  
  - Получение системных цветов (`GetColor`, `GetLunaColor`, `GetXtremeColor`, `GetMSO2003Color`).  
  - Поддержка тем Windows XP/Vista (Luna Blue/Olive/Silver, Royale, Aero) и Office 2003‑стиля.  
  - Генерация градиентов и вычисление производных цветов (осветление, затемнение, смешивание).

- **Рисование и вспомогательные GDI‑утилиты**
  - Класс `CUIDrawHelpers` и набор маленьких helper‑классов (`CUIBufferDC`, `CUIBitmapDC`, `CUIFontDC`, `CUIPenDC`, `CUIBrushDC` и др.).  
  - Градиентная заливка (через `GradientFill`/эмуляцию), работа с прозрачностью, преобразование RGB↔HSL.  
  - Хуки мыши, расчёт координат/областей, поддержка RTL‑раскладки и DPI‑утилиты.

- **Управление изображениями и иконками**
  - Класс `CUIImageManager` и вспомогательные типы (`CUIImageManagerIcon`, `CUIImageManagerIconSet`, `CUIImageManagerImageList`).  
  - Загрузка иконок и битмапов из файлов и ресурсов (в т.ч. PNG, 32‑bpp с альфой).  
  - Создание состояний иконок: normal, hot, pressed, checked, disabled, faded, shadow.  
  - Альфа‑смешивание, прозрачный blit, создание bitmap’ов, сплит sprite‑sheet’ов и работа с `HIMAGELIST`.

- **Кастомные аллокаторы и строки**
  - `CUIHeapAllocatorT` / `CUIHeapObjectT` — шаблоны для размещения объектов в отдельном heap’е с поддержкой LFH (Low Fragmentation Heap).  
  - `CUIBatchAllocObjT` — batch‑аллокатор, размещающий объекты пачками для уменьшения фрагментации.  
  - `CUIHeapString` / `CUIHeapStringT` — строковые классы, использующие указанные аллокаторы (альтернатива `CString`).

## Структура проекта

Основные файлы, которые вы сейчас видите в репозитории:

- `UIColorManager.h`, `UIColorManager.cpp`  
  Реализация класса `CUIColorManager` и вспомогательных типов для управления палитрой и темами.

- `UIDrawHelpers.cpp`  
  Набор функций и классов для рисования: градиенты, буферные DC, шрифты, линии, конвертация цветов и т.п.

- `UIImageManager.cpp`  
  Большой менеджер изображений и иконок: загрузка, хранение, генерирование различных состояний и отрисовка.

- `UICustomHeap.h`  
  Шаблоны аллокаторов (`CUIHeapAllocatorT`, `CUIBatchAllocObjT`, `CUIHeapObjectT`, `CUIHeapStringT`) и связанные макросы.

- `UIDLLExports.h` (если присутствует)  
  Экспортируемые функции/интерфейсы DLL‑библиотеки.

Рекомендуемая целевая структура каталога:

```text
src/
  UIColorManager.cpp
  UIDrawHelpers.cpp
  UIImageManager.cpp
include/
  UIColorManager.h
  UIDLLExports.h
  UICustomHeap.h
docs/
  architecture.md
  changelog.md
```

## Системные требования

- Windows (минимум Windows 2000/XP; часть функций использует темы XP/Vista и Office 2003‑стиль).  
- C++ (MFC/Win32), компилятор уровня Visual C++ 8.0 и выше.  
- Подключённые заголовки и библиотеки:
  - Windows SDK (`windows.h`, GDI, COMCTL32, THEMES, UxTheme и др.);  
  - MFC (`afxwin.h` и связанные заголовки);  
  - `msimg32.dll` для `GradientFill`, `AlphaBlend`, `TransparentBlt` (загружается динамически).

## Подключение в проект

1. Добавьте `.cpp`‑файлы библиотеки в ваш проект или соберите их в отдельную статическую/динамическую библиотеку.  
2. Подключите нужные заголовки:

```cpp
#include "UIColorManager.h"
#include "UIDrawHelpers.h"
#include "UIImageManager.h"
#include "UICustomHeap.h"
```

3. Убедитесь, что включены нужные модули и библиотечные зависимости (`msimg32.lib`, `comctl32.lib` и т.д.).  
4. Инициализация, как правило, сводится к использованию статических синглтонов:

```cpp
COLORREF cr = GetXtremeColor(XPCOLOR_HIGHLIGHT);
CUIColorManager& cm = UIColorManager();
```

Дополнительные детали использования (пример инициализации палитр, интеграция с вашей оконной иерархией, настройка аллокаторов) можно вынести в `docs/architecture.md`.

## Статус проекта

Сейчас код взят как самостоятельный UI‑utility‑слой и может требовать адаптации под конкретный проект (namespaces, отключение лишних частей, привязка к вашей системе логирования и т.п.).

---

Если покажете, как именно вы хотите использовать эту библиотеку (отдельная DLL, статическая либка или просто набор исходников в приложении), я смогу дополнить `README` разделом “Примеры использования” с конкретными код‑сниппетами.