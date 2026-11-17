# Сборка и установка

## Windows, на этой машине (zig, без CMake)
Нужны: VST3 SDK (по умолчанию `_работа\toolchain\vst3sdk`, версия 3.8.1) и `python -m ziglang` (`_работа\toolchain\venv`). Пути можно переопределить параметрами `-Sdk`, `-Zig` или переменными `VST3_SDK`, `ZIG_PY`.
```
powershell -File build\build.ps1 -Plugin all          vocal | reverb | delay | all
powershell -File build\build.ps1 -Plugin all -Tests   ещё соберёт D:\arktest\ark_test.exe и fx_test.exe
powershell -File build\build.ps1 -Plugin all -Install ставит в C:\Program Files\Common Files\VST3, удаляет старые Arkham*.vst3
python build\check_load.py build\out\StarzVocal.vst3  проверка загрузки
```
Результат в `build\out\`. Объектные файлы SDK кэшируются (`build\obj_*`), свои исходники пересобираются всегда.
Важно:
- `build.ps1` хранить в UTF-8 **с BOM**: PowerShell 5.1 иначе портит кириллицу в путях.
- Установка при открытом FL невозможна (файл занят): закрыть FL. После установки в проекте плагин удалить и вставить заново.
- Кириллицу не передавать в argv консольных exe (ломается): тестовые файлы держать в `D:\arktest`.

## CMake (Windows и macOS)
```
cmake -S . -B build -DVST3_SDK_ROOT=<путь к SDK>      (или без параметра: SDK скачается с GitHub)
cmake --build build --config Release
```
`CMakeLists.txt` создаёт три цели StarzVocal, StarzReverb, StarzDelay. На macOS собирается универсальный бинарник (arm64 + x86_64), минимальная система 10.13. Подробнее и про подпись: `docs/mac.md`.

## GitHub Actions
`.github/workflows/build.yml` (запуск вручную или по тегу `v*`): задачи mac (Xcode, ad hoc подпись, zip) и windows, артефакты `starz-mac`, `starz-windows`. Работает, если папка `starz` лежит как корень репозитория.
