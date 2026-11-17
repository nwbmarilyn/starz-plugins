# macOS

## Что сделано
- DSP (`src/core`) без платформенных заголовков, строки параметров переводятся из `char16_t` поэлементно, окно Windows закрыто `#if defined(_WIN32)`.
- `CMakeLists.txt` под VST3 SDK (макрос `smtg_add_vst3plugin` сам делает бандл `.vst3` с Info.plist, точкой входа `macmain.cpp`, универсальный бинарник arm64 + x86_64).
- `.github/workflows/build.yml` собирает на `macos-14`, ставит ad hoc подпись (`codesign -s -`), упаковывает `ditto` в zip.

## Чего нет и честно о проверке
- **Сборка на macOS ни разу не запускалась**: на этой машине Windows, SDK Apple для кросс-сборки нет (и ставить его нельзя без Apple-устройства по лицензии). Ошибки первой сборки на Mac возможны, править по сообщениям компилятора.
- **Окна на macOS нет**: `createView` возвращает `nullptr`, FL Studio для Mac покажет собственный список параметров (ручки, пресеты, Sync работают). Свой интерфейс на Mac в очереди (`roadmap.md`).
- Не проверялось в FL Studio для macOS и в других DAW.

## Как собрать на Mac (вручную)
1. Xcode Command Line Tools, CMake.
2. `cmake -S . -B build -G Xcode -DVST3_SDK_ROOT=<SDK>` и `cmake --build build --config Release`.
3. Бандлы появятся в `build/VST3/Release/*.vst3`.
4. Положить в `~/Library/Audio/Plug-Ins/VST3/` (или `/Library/Audio/Plug-Ins/VST3/`).
5. Если macOS блокирует: `xattr -rd com.apple.quarantine <бандл>.vst3` и `codesign --force --deep -s - <бандл>.vst3`. Для распространения нужна подпись Developer ID и нотаризация.

## Как собрать без Mac
Залить папку `plugins/starz` как репозиторий на GitHub и запустить workflow `build` (вручную). Скачать артефакт `starz-mac`.

## Правила для будущих правок
Всё, что трогает DSP или параметры, должно оставаться общим. Платформенное только за `_WIN32` / `__APPLE__`, отдельными файлами. Перед выдачей проверять, что `src/core` не включает `windows.h`.
