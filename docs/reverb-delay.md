# Starz Reverb и Starz Delay

Один исходник `src/fx/fxplugin.cpp`, режим выбирается флагом компиляции `ARK_REVERB` или `ARK_DELAY`. DSP в `src/core/fx.h`. Версия 0.2.0.

## Starz Reverb
FDN из 8 перекрёстных линий задержки (матрица Хаусхолдера), на входе диффузия из 4 allpass на канал, линии с медленной модуляцией, фильтр-демпфер в каждой линии, на выходе срез низа и верха и расширение стерео. Время затухания задаётся и точно совпадает с замером (калибровка ×1.17 в `update()`). Хвост приглушается, пока идёт голос (Duck).

Параметры (10): Preset, Mix, Dry, Decay 0.2–10 с, Pre-delay 0–150 мс, Damp 1.5–16 кГц, Low Cut 20–800 Гц, Width, Duck, Motion (глубина модуляции).
Пресеты: Klyukva Air (0.9 с, яркий, широкий), Vocal Plate (1.4), Tight Room (0.45), Dark Hall (2.8), Huge Bright (5), Ghost (4, тёмный, сильный Duck), Cathedral Dream (8), Send Only (Mix 100, Dry 0).
Замеры: RT60 1.82/3.97/0.78 с при цели 1.8/4/0.8; корреляция L/R около 0.

## Starz Delay
Стерео пинг-понг, в петле фильтры Low/High Cut и мягкое насыщение (Drive), Wobble качает время, Duck приглушает под голос. Время можно брать от темпа хоста (`processContext->tempo`): Sync Off, 1/4, 1/8, 1/8 dotted, 1/16, 1/8 triplet. При отсутствии темпа берётся 120.

Параметры (12): Preset, Mix, Dry, Sync, Time 20–1500 мс, Feedback 0–95 %, Low Cut 20–1000, High Cut 1.5–18 кГц, Ping-Pong 0–100 %, Duck, Wobble, Drive.
Пресеты: Eighth Ping-Pong, Slapback (95 мс), Dark Tape, Quarter Echo, Long Wash, Triplet Bounce, Wide Doubler (45 мс), Send Only.

## Общее
- Dry остаётся 100 %, Mix добавляет wet. Для send: Send Only (Mix 100, Dry 0).
- Стерео: сухой сигнал сохраняет L/R; у дилея с Ping-Pong 100 % эхо уходит в другое ухо по замыслу.
- Выбор пресета ставит все параметры, кроме Preset. Пресеты собраны по цифрам и замерам, на слух оценивает владелец; его значения после правки брать из диффа проекта.
- Грабли: чтение линии задержки делать целыми индексами с ограничением (иначе выход за буфер давал вечный хвост).

## Тесты
`tests/fx_test.cpp`: `fx_test reverb|delay in.wav|impulse out.wav [ключи]` (mix dry decay pre damp lowcut width duck mod time feed low high ping wobble drive pan). Скрипты: `fx_ir.py` (RT60 и спектр), `fx_voice.py`, `fx_presets.py` (все пресеты на голосе).
