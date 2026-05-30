---
VEHIR REBUILD — ДЛИННЫЙ РОАДМАП (Claude на паузе до 2 июня)

0. РОЛИ И ПРОТОКОЛ (применять к КАЖДОМУ шагу)

Роли:
- DeepSeek — пишет код, манифесты, COMMIT_MSG.txt. Git не трогает.
- Gemini 3.5 Flash — (а) собирает ground-truth перед шагом (исходники легаси, схему БД, текущее состояние), (б) проверяет результат (гоняет idea build/idea check, запускает бинарь, читает FINDINGS) и отдаёт вердикт.
- Оператор (ты) — git commit/push, человеческие шаги (аккаунты), релей мне при эскалации.
- Claude (я) — только мозг/эскалация. Не трогаю файлы.

Протокол динамичности (это и есть «что-то не так / изменилось»):
1. Ground-truth вперёд спеки. Спека даёт НАМЕРЕНИЕ + критерий готовности, не точные строки. Gemini собирает реальный легаси-исходник; если он отличается от описания — DeepSeek адаптирует под критерий готовности, расхождение пишет в FINDINGS.
2. Правила расхождений:
  - Легаси-компонента отсутствует/другая → строить от критерия готовности (намерения), отметить в FINDINGS.
  - Сборка падает → чинить по законам кодирования; нет пакета в nixpkgs → overlay, не костыль.
  - Зависимый пакет ещё не готов → объявить интерфейс-заглушку ИЛИ переставить порядок; не блокироваться молча.
  - idea check = incompatible/unknown → разрулить лицензию/ребро; --force только с обоснованием в FINDINGS.
  - Меняется АРХИТЕКТУРНОЕ допущение (спека неверна на уровне дизайна) → эскалация к Claude.
3. Конец каждого шага: idea apply → idea build <pkg> = works → idea check = compatible → FINDINGS (оба чек-листа: SOUL-стандарт + LLM-удобство). Gemini проверяет и отдаёт PASS/FAIL + FINDINGS.
4. Данные священны. memory_entries (~8k), ключи, death_ledger — переносить dump/restore, НИКОГДА не поднимать пустыми поверх.
5. Секреты — через broker (S0.1), не env/CLI/хардкод.
6. Будить Claude ТОЛЬКО при: архитектурном расхождении · вопросе §3.SEC/безопасности · кросс-пакетном дизайн-решении · повторном провале (>2 попыток) · решении, помеченном ниже как ⚠️CLAUDE. Иначе — автономно вперёд.

Инварианты (из SOUL v11): C-only (GPU-исключение для MSA), Nix-only билд, fail-hard, zero-hardcode, declarative>imperative, naming-law, least-privilege, security только усиливать.

---
ФАЗА 0 — фундамент фундамента

S0.1 — secrets/env broker + рефактор forge/mail + ssh (УЖЕ в MIGRATION.md). Делать первым: на него встаёт всё остальное (конфиги, токены, ключи). Done — см. MIGRATION.md.

S0.2 — IPM ROUND 4: compat fail-closed + реальные правила (УЖЕ в ~/ipm/SPEC.md). Без зубов чекера нельзя безопасно тащить пакеты с copyleft-зависимостями. Done — см. ipm/SPEC.md R4.1/R4.2.

---
ФАЗА 1 — рантайм-субстрат (репо vehir-core)

S1.1 — runtime-paths (lib). Динамическое разрешение путей (корень, кэш, сокеты) через /proc/self/exe + конфиг, как легаси vsm_paths.c, но zero-hardcode и совместимо с broker. Ground-truth: src/lib/vsm_paths.c. Done: тул-проба печатает разрешённые пути JSON-ом и в контейнере, и в проекте.

S1.2 — db (lib + опц. прокси-демон). Доступ к Postgres. Ground-truth: src/services/vsm_dbd.c (единственный с libpq, сокет-прокси). Решение ⚠️CLAUDE-lite: сохраняем ли «один процесс с libpq» (демон-прокси) ИЛИ пакеты линкуют libpq напрямую (как IPM)? Дефолт: lib на libpq + конфиг коннекта через broker; прокси-демон — только если изоляция реально требует (обосновать в FINDINGS). Bind-параметры обязательны (не интерполяция). Done: lib делает параметризованный SELECT/INSERT, fail-hard.

S1.3 — postgres-memory (ДАННЫЕ — осторожно). Чистая НОВАЯ схема памяти: memory_entries (halfvec embeddings, мультиязычный FTS), типы записей, importance, слои. Ground-truth: scripts/migrations/000_initial_schema.sql + атлас Gemini §1. Перенос ~8k записей: pg_dump нужных таблиц из легаси → трансформация в новую схему → restore. Никогда не пустую базу. Зависит: S1.2. Done: новая схема применяется через idea init-подобный путь; контрольные N записей перенесены и находятся поиском; счётчики до/после совпадают. ⚠️CLAUDE: финальный список таблиц к переносу (memory_entries — точно; code_index/death_ledger/skills_catalog — подтвердить) ты дашь, когда дойдём.

S1.4 — pain (lib + сигнал). pain_registry + compute_total_pain + маппинг в когнитивный режим (SOUL §5). Ground-truth: src/services/vsm_pain.c, src/tools/pain_check.c. Зависит: S1.2. Done: запись/резолв сигнала, чтение総 боли, порог→режим.

S1.5 — constitution-validator (rebuild soul_hash, чиним хрупкость). Хэширование секций SOUL.md + дрейф. Ground-truth: src/tools/soul_hash.c. Ключевое изменение (динамичность): (а) проверку «referenced item существует» сделать НЕ фатальной для самой конституции (сервис «не активен сейчас» ≠ дрейф конституции) — это был баг; (б) валидировать только дрейф хэшей секций + опционально мягко предупреждать о ссылках; (в) re-baseline хэшей v11. Зависит: S1.2. Done: на v11 SOUL --verify = ok после синка; временно неактивный демон НЕ валит конституцию.

---
ФАЗА 2 — память и анализ (репо vehir-mind)

S2.1 — msa-engine (демон, Python/GPU — задокументированное исключение). MSA-4B как persistent-демон (грузит модель один раз). Ground-truth: scripts/vsm_msa.py vs MSA/src/msa_service.py — сначала установить канон (что реально грузит модель, импортируется ли). Зависит: S1.3, GPU (6GB, шарится с Ollama → lazy-load/idle-unload). Done: сокет-демон, single-instance, 2-стадийный реранк за <30с (тёплый). Тонкие клиенты (msa-search/rerank) — без перезагрузки модели. Динамика: если канон-демон не тот, что в спеке — зафиксировать в FINDINGS и строить на реальном.

S2.2 — lens (анализ-пайплайн). Слои: RAW-парсер (ничего не упустить) → механические правила (risk_score без LLM) → FTS+embedding → MSA-реранк (S2.1) → машиночитаемый отчёт. Ground-truth: vextract.c, code_index (покрытие/свежесть), search_v3.c, smart-search, plain-read, tools/sg. Это консолидация, не write-from-scratch. Зависит: S2.1, S1.3. Done: review <target> выдаёт JSON-находки (спагетти/дубли/мёртвый код/risk) на известном плохом файле находит ≥1 реальную проблему.

S2.3 — search (канонический). Один инструмент: --raw (сырьё) + smart (FTS+pgvector) + --deep (→ MSA-реранк S2.1). Свести дубли поиска к одному. Зависит: S2.2/S2.1. Done: smart даёт JSON top-N; raw — полный файл; deep — реранкованный топ-K.


S3.1 — locks + activity + info (мелкие инфра-пакеты на S1.2): транзакционные локи (reason/ttl/source обязательны), лента событий (event-driven), info/hints из таблицы. Ground-truth: vsm_locks.c/vsm_activity.c/vsm_info.c.

S3.2 — enforcement-checks (статические проверки, 23 чека легаси → чистые C-модули; убрать Python-зависимые проверки или переписать). Ground-truth: src/services/enforcer/checks/. ⚠️CLAUDE: направление sniper-vs-bwrap-vs-позитивный-энфорс — НЕ РЕШЕНО, не трогать enforcement-архитектуру без меня.

S3.3 — positive-enforcement (на S2.2 lens): майнинг tool-call логов → паттерны неэффективности → блеклист/улучшение. Зависит: lens.

S3.4 — task-coordination: ОДНА чистая модель задач/todo/делегации (легаси имеет две — v_core_* и delegated; свести). Ground-truth: схема + vgw_scheduler.c.

---
ФАЗА 4 — рантайм-жизнь (КОНТУР, после Фазы 3)

S4.1 — service-lifecycle (rebuild vsm-init, event-driven, декларативные манифесты в БД). Ground-truth: vsm_init.c, vsm/services/. ⚠️CLAUDE (большой дизайн).
S4.2 — isolation-physics (landlock/bwrap/контейнер — «физика»). ⚠️CLAUDE + §3.SEC: безопасность только усиливать; sniper-vs-bwrap не решён.
S4.3 — swarm (Claude Agent Teams на C+Postgres: leader/worker, shared board, heartbeat).
S4.4 — reliability-cage (adversarial voting, Finch-Zk, SLM-фильтр, logprobs→pain, constrained decoding, мат-верификация поведения).

---
ФАЗА 5 — далёкое (только названия): rsi-safe-modification, blackice, benchmark, end-user-distribution, free-time-engine. Не расписывать до Фазы 4.

---
КОГДА БУДИТЬ CLAUDE (экономия лимита)

1. S1.3 — финальный список таблиц к переносу + трансформация схемы (данные).
2. S1.2 — прокси-демон vs прямой libpq (если изоляция спорна).
3. S3.2 / S4.2 — что угодно про enforcement / isolation / sniper-vs-bwrap / §3.SEC (НЕ РЕШЕНО).
4. S4.1, S4.3, S4.4 — большие архитектурные дизайны.
5. Любое расхождение уровня «спека неверна архитектурно» или повтор провала >2 раз.
6. Финальный список пакетов первого публичного релиза vehir-* репо.

Иначе — автономно: S0.1 → S0.2 → S1.1 → S1.2 → S1.4 → S1.5 → S2.1 → S2.2 → S2.3 → S3.1. Это ~10 пакетов без меня. На каждом: ground-truth → build → idea apply/build/check → FINDINGS → Gemini-вердикт → COMMIT_MSG → ты коммитишь.
