# Starz Vocal

Автотюн в реальном времени и рой голосов («Mr Kitty», как в After Dark). Файлы: `src/core/engine.h`, `src/vocal/plugin.cpp`, `src/vocal/gui.h`. Версия 0.4.0.

## Алгоритм
- **Детектор высоты**: YIN на 2x децимации, шаг 128 отсчётов, окно J адаптивное (160–520), tauMax = sd/70 (низкие голоса не отрезаются), уточнение периода нормированной автокорреляцией ±3 отсчёта на полной частоте. Удержание f0 на 3 кадра при пропаданиях (`gHold`). «Продолжение по предыдущей высоте» (`gCont`) выключено (давало скачки).
- **Привязка к ноте**: гамма Key/Scale, гистерезис 22 цента (`gHyst`), скорость подтяжки Retune Speed, режим Stepped (расстройка и дрейф голосов применяются до привязки).
- **Синтез**: PSOLA с зёрнами Hann, нормировка по сумме окон, смесь с сухим по smoothstep. Задержка L = ceil(latK·sr/Lowest)+64 отсчётов, зерно не должно обрезаться слева (иначе щелчок), для нот ниже Lowest зерно делится (nsub).
- **Рой**: эффективное число голосов 1+(Voices−1)·Swarm, фейд голосов по блоку, сглаживание 80 мс, Spread/Drift/Flange/Delay Spread, голос 0 без задержки, очистка буферов выключенных голосов.
- **Стерео вход**: сухой сигнал идёт как есть; wet получает баланс L/R входа (огибающие 20 мс, выровнен по задержке, equal-power).
- **Компенсация роя**: wet умножается на (1+`gMake`·widen) и поднимается мягкой полкой от `gShelfHz` (5 кГц) на `gShelf`·widen; widen = доля включённого роя. Подобрано замером: полосы 0.2–16 кГц в пределах ±0.6 дБ от Auto-Tune.

## Параметры (21, порядок не менять)
Preset (Auto-Tune, Auto-Tune Hard (Artist), Mr Kitty), Mix %, Key (C…B, 7 = G), Scale (Minor, Major, Chromatic), Reference 415–466 Гц (440), Retune Speed 0–300 мс, Tune on/off, Voices 1–24, Spread 0–100 центов, Drift 0–150 центов, Drift Rate 0.1–6 Гц, Flange 0–15 мс, Flange Depth 0–4 мс, Delay Spread 0–30 мс, Stereo (Wide / L-C-R), Stepped Voices, Lowest Voice 60–250 Гц, Output ±12 дБ, Variation 1–16 (зерно), Swarm 0–100 %, Latency Mode (Clean 2 периода, Balanced 1.5, Live 1).

Пресеты: Auto-Tune (Retune 20, 1 голос), Auto-Tune Hard (Retune 0, 1 голос), Mr Kitty (Retune 4, 8 голосов, Spread 25, Drift 40, Flange 7, Depth 1, Delay Spread 12, Stepped On, Swarm 100).

Stereo: **Wide** голоса по очереди то слева, то справа (центр пуст); **L-C-R** по кругу центр, лево, право (центр занят, плотнее, надёжнее в моно).

## Поведение
- Один голос на экземпляр. Несколько дублей на одном канале тюнятся как смесь: хуже (разброс 30–40 центов между дублями не лечится общим сдвигом). Дубли разводить по insert'ам (skill vocal-split-takes).
- Сравнение с Auto-Tune Artist на одном голосе: попадание в 10 центов от ноты 78 % против 79 %, средняя ошибка 3.3 против 6.7 цента.
- Задержка: Balanced с Lowest 110 около 15 мс; Live меньше. Для записи вживую Live или Balanced.
- Вход жёстко в одном ухе: рой уходит в это ухо.

## Грабли (чтобы не повторять)
- Обрезка зерна слева даёт щелчок: нужен запас задержки latK.
- tauMax нельзя привязывать к Lowest (пропадут ноты ниже), «защита от октав» через 2·tau вредна.
- Буферы выключенных голосов очищать (`dirty`).
- Изменение диапазона Voices сдвигает старые проекты (нормированное состояние).

## Тесты
`tests/test_main.cpp` (`ark_test in.wav out.wav artist|kitty|dry [block] [ключи]`): ключи swarm, sweep, solo, hyst, latk, voices, spread, drift, retune, minf0, ref, presnap, stereo, seed, gate, thr, fall, cont, hold, pan, make, shelf. Скрипты замеров в `tests/scripts/`.
