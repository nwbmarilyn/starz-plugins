# Архитектура

## Карта папки
```
README.md                      витрина для GitHub и установка
install.bat, install.ps1       установка на Windows (в папку VST3, куда смотрит FL)
install.sh                     установка на macOS
CMakeLists.txt                 сборка для macOS и Windows (CMake + VST3 SDK)
.github/workflows/build.yml    сборка mac и windows в GitHub Actions, релиз по тегу v*
dist/windows/                  готовые .vst3 для Windows;  dist/mac/ для бандлов mac
build/build.ps1                сборка и установка на этой машине (zig, без CMake)
build/check_load.py            проверка, что .vst3 загружается
src/core, src/common           DSP и общий мост
src/vocal, src/fx              плагины и их окна
tests/, tools/                 замеры, тестеры, снимки интерфейса
docs/                          документация, docs/img скриншоты
```
## Слои
1. **core** (`src/core`): чистый C++17 без платформенных заголовков.
   - `engine.h`: `ark::Engine`, тюн и рой голосов. Вход `process(inL, inR, outL, outR, n)`, параметры `ark::Params`, `setParams`. Задержка `latency()`.
   - `fx.h`: `ark::Reverb` и `ark::Delay` (параметры `ReverbParams`, `DelayParams`), общие кирпичи `Line`, `Allpass`, `OnePole`, `Ducker`.
2. **bridge** (`src/common/bridge.h`): два чистых интерфейса `gui::Bridge` и `fxgui::Bridge`. Через них окно читает и пишет параметры плагина, не зная про VST3.
3. **plugin** (`src/vocal/plugin.cpp`, `src/fx/fxplugin.cpp`): обвязка VST3 (`SingleComponentEffect`): список параметров, пресеты, состояние, `process`. Реализует Bridge.
4. **gui** (`src/vocal/gui.h`, `src/fx/fxgui.h`, `src/common/brand.h`): окно на GDI+, только `_WIN32`. На других системах `createView` возвращает `nullptr` и хост рисует свой список параметров.

Поток данных: хост → `process()` → `applyParams()` читает `norm[]` (атомики, 0–1) → `engine.setParams` / `fx.set` → DSP → буфер выхода. Окно меняет `norm[]` через `Bridge::edit`.

## Правила (обязательно)
- **Кроссплатформенность сразу.** Любая правка и улучшение должны собираться и работать и на Windows, и на macOS.
  - DSP только в `src/core`: никаких `windows.h`, `wchar_t` для строк параметров, `__declspec`, `GetTickCount`, SIMD-интринсиков без `#if` и запасного пути.
  - Всё платформенное только за `#if defined(_WIN32)` (окна) и отдельными файлами. Для нового платформенного окна: реализовать новый `View` и подключить в `createView` под своей веткой.
  - Строки из VST3 (`String128`, `char16_t`) в `std::wstring` переводить поэлементно, не `reinterpret_cast` (на macOS `wchar_t` 32 бита).
- **Без комментариев и докстрингов в коде** (личное правило владельца).
- **Состояние хранится нормированным.** Меняя диапазон, шаги или порядок параметров, старые проекты поедут. Либо дописывать новые параметры только в конец enum, либо писать миграцию (см. пример `tests/scripts/fix_voices.py`: Voices 12 → 24 потребовало `new = old·11/23`).
- **По умолчанию новые эффекты выключены** (Mix = 0), чтобы старые проекты звучали как раньше.
- **Не прерывать звук**: параметры сглаживать (в fx.h везде есть сглаживание 20 мс), никаких разрывов в буферах.
- Новый DSP сначала проверять тестером без хоста (`tests/`), потом в плагине.

## Идентификаторы
Не менять UID классов (иначе хост видит другой плагин): Vocal `4A8F1C52 7B3E4D90 9A21C6E5 03D7B8F1`, Reverb `...03D7B8F2`, Delay `...03D7B8F3`. Названия (Starz …) от UID не зависят. В 2026-11-17 плагины переименованы из Arkham в Starz с теми же UID.

## Как добавить параметр
1. В `plugin.cpp` / `fxplugin.cpp`: дописать в конец enum `Param` и в `kDefs` (имя, единицы, min, max, default, шаги). Для пресетов добавить значение в каждый пресет.
2. В `applyParams()` прочитать значение и передать в `ark::Params` / `ReverbParams` / `DelayParams`.
3. В DSP (`core`) использовать, со сглаживанием.
4. В окне (`gui.h` / `fxgui.h`): добавить ручку в `layout()` и единицы в `unitOf()`.
5. Тест: ключ в `tests/test_main.cpp` или `tests/fx_test.cpp`, замер скриптом.
6. Документация (`docs/*.md`) и `docs/changelog.md`.

## Как добавить плагин
Новый DSP-класс в `src/core`, новая папка `src/<имя>` с `plugin.cpp`, новый UID, запись в `build/build.ps1` (таблица `$targets`) и в `CMakeLists.txt`, тест в `tests/`.

## Известные ограничения
- Тюн: один голос на экземпляр (дубли на отдельные insert'ы).
- Стерео вход: баланс L/R сохраняется, но рой в одном ухе схлопывается в него.
- Окно только на Windows (на macOS хостовый UI); план в `docs/roadmap.md`.
