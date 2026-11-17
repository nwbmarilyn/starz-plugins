# macOS

## Что сделано
- DSP (`src/core`) без платформенных заголовков.
- Окно на macOS: `src/common/view_mac.mm` (NSView внутри `IPlugView`, `kPlatformTypeNSView`, таймер 33 мс, мышь: перетаскивание, Shift для точной подстройки, правая кнопка или Ctrl+клик сбрасывает, колесо, клики; Retina; NSSavePanel и NSOpenPanel для SAVE и LOAD). Рисует `src/common/gfx_cg.h`: слой с теми же именами, что в GDI+, поверх CoreGraphics и CoreText. Имя класса NSView уникально для каждого плагина (`Starz<Имя>NSView`), чтобы три бандла в одном процессе не конфликтовали.
- Свои пресеты: `~/Library/Application Support/Starz/presets/<плагин>`.
- `CMakeLists.txt` собирает универсальный бинарник arm64 + x86_64, цель `StarzShots` рисует каждое окно без экрана.
- `.github/workflows/build.yml` на `macos-latest`: сборка, снимки окон и проверки мыши (`tools/mac_shots.cpp`), снимки и лог пушатся в ветку `ci-shots` (`mac-vocal.png`, `mac-reverb.png`, `mac-delay.png`, `log.txt`), ad hoc подпись, zip.

## Что проверено и что нет
- Проверено в CI: сборка, отрисовка окон в PNG, логика мыши (перетаскивание, Shift, сброс, колесо, стрелки пресетов) на заглушке хоста.
- Не проверено на живом Mac: работа мыши и Retina внутри FL Studio, диалоги Save и Load, цвета и шрифт (на Mac системный шрифт, не Segoe UI, ширины чуть отличаются).

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
