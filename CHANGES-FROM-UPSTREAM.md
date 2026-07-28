# Changes from Boomaga

PlicaPage is a fork of [Boomaga](https://github.com/Boomaga/boomaga).

**Fork point:** commit `7f7ad47`, tag `v3.0.0-13-g7f7ad47`, 2022-02-21 — the last
commit upstream made. Boomaga's own README says the maintainer no longer has time
for the project.

This file records what PlicaPage changed, which is both useful history and what
LGPL-2.1 §2(a) asks for ("cause the files modified to carry prominent notices
stating that you changed the files and the date of any change"). The git history
from the fork point onward is the authoritative record; this is the summary.

The great majority of this program is still Boomaga's work, by Alexander Sokoloff
and the Boomaga team. Their copyright headers are intact in every file.

**Version line.** PlicaPage starts at 1.0.0. Boomaga's 21 release tags
(`v0.3.0` … `v3.0.0`) are deliberately *not* carried into this repository — its
line already reached 3.x, and a `v1.0.0` existed there in 2013, so keeping them
beside PlicaPage's own 1.x would be ambiguous. The full commit history is
preserved; only the tags were dropped, and they remain in the `upstream` remote
(`git fetch upstream --tags` if you want them).

---

## 2026-07 — The intermittent double-free crash

Boomaga aborts with `double free or corruption (fasttop)`, or segfaults, on
roughly one launch in five with a large scanned document, taking the loaded jobs
with it. Stock 3.0.0 does it too, so this is inherited rather than introduced
here.

The fault is in poppler, not in either program: its global colour-management
profiles are built lazily, without a guard, by whichever render arrives first.
Two renders reaching that setup together corrupt the heap, and the abort surfaces
later in `cmsCloseProfile` under `GfxState`'s constructor — often with every
thread apparently just rendering, because a double free is reported when
something is freed rather than when the heap was damaged.

- New `PopplerGate` (`src/plicapage/popplergate.{h,cpp}`), a process-wide gate
  that every poppler call goes through. Rendering takes it *shared*, so the
  preview stays parallel; the first render in the process takes it exclusively,
  which is what forces poppler's one-time setup to complete on its own. Opening
  and closing documents take it exclusively too — never shown to be the culprit,
  but never shown to be safe either, and rare enough that excluding them costs
  nothing.
- It has to be process-wide: PlicaPage runs two `Render` pools and `PageTrimmer`
  opens documents on the main thread, and poppler's globals belong to none of
  them.
- `Render::setFileName()` stopped destroying its workers one at a time. It quit,
  waited for and deleted each worker in turn, which freed the first worker's
  document while the rest of the pool was still inside poppler; and it started
  each new thread while later workers were still opening their documents. Both
  are now two passes — stop all, then delete all; build all, then start all.
- `RenderWorker::mBusy` was written by the worker thread and read by the main
  one. Besides being a data race, it meant a burst of dispatches all landed on
  the first worker, because none had flipped its flag yet: eight threads
  rendered a preview one page at a time. It is now written only by `Render` on
  the main thread, when a job is handed out and when the result comes back.
- Workers whose document failed to open are no longer handed jobs. They never
  answer, so they would have been marked busy permanently.

Tested by `test_render.cpp`, whose stress tests abort on the unfixed code:
`test_RenderNoReloadStress` — one load and nothing but concurrent rendering —
failed 4 runs in 20 before the fix, which is what identified the real cause after
document lifetime turned out to be a red herring. Reproducers and the full
account are in `poppler-crash-repro/`.

## 2026-07 — Trim whitespace and scale to fit

Boomaga always scaled the *whole* source page into its cell on the sheet,
margins included, which wastes paper on N-up and booklet layouts.

- New `PageTrimmer` (`src/plicapage/kernel/pagetrimmer.{h,cpp}`) rasterises each
  source page through poppler and finds its ink bounding box. Ink is grouped into
  connected blobs so isolated speckles and scanner lid shadows are rejected
  rather than pinning the box to the full page.
- `ProjectPage::trimRect()` is a new accessor kept deliberately separate from
  `rect()`. `rect()` still returns the CropBox because it also drives the
  landscape and rotation decisions; a trimmed box with a different aspect ratio
  would otherwise silently rotate whole sheets.
- `TmpPdfFile::getPageStream()` now emits a clipping path before drawing each
  page. Boomaga emitted none, so content outside the drawn box could bleed across
  neighbouring cells — harmless at 1:1, not harmless once a page is scaled up.
- Per-page and uniform trim modes, with configurable padding, in the main window.
- `TmpPdfFile` now always writes a real page tree for the source pages. Upstream
  only did this behind the `BOOMAGAMERGER_DEBUGPAGES` environment variable, and
  that code path wrote PDF rectangles as `[left top width height]` instead of
  `[x1 y1 x2 y2]`. Fixed, since the trimmer depends on it.

## 2026-07 — Measurement units

Upstream had a `Unit` enum with `UnitInch` commented out and a `mUnit` member
threaded through the printer-margins dialog but hardcoded to millimetres.

- `UnitInch` enabled; conversions moved to `plicapagetypes.cpp` and exposed.
- App-wide measurement-unit preference, defaulting from the system locale, which
  drives both the new trim padding control and the existing margins dialog (whose
  tab now retitles between "Margins (mm)" and "Margins (in)").
- The millimetre ratio was derived from rounded A4 dimensions (842/297) and was
  0.02% off, which made a value drift visibly when switching units — 1.000 in
  came back as 0.999 in. Conversions are now exact.

## 2026-07 — Fork rename and coexistence

PlicaPage is designed to install *alongside* a stock Boomaga package so the two
can be compared. Every shared identity was made distinct:

- CUPS backend binary and URI scheme (`plicapage:/`), PPD file and directory,
  queue name, and PPD identity fields.
- D-Bus well-known name, object path, interface and activation service. Not
  cosmetic: sharing `org.boomaga` would deliver a job printed to PlicaPage into a
  running Boomaga, which would then delete the spool file.
- Spool directory `/var/cache/plicapage`. Both programs delete any spool file
  they load, so a shared directory means one can destroy the other's job.
- QSettings path, scratch-file prefix, environment file, autosave directory.
- Desktop entry, MIME type, icons, man page, translation catalogue — each of
  which is a package file-level conflict otherwise.

Also fixed `setByDefault()` in `cmake/tools.cmake`, which passed the macro's
literal argument to `add_definitions()` instead of the resolved variable. Every
`CUPS_BACKEND_*` override was silently ignored by the compiled code while still
affecting install paths.

### File format

Saved projects use `*.plica` and write `@PJL PLICAPAGE_PROJECT`, but Boomaga's
tokens — including its `BOOMAGA_PROGECT` typo — and its `CUPS_BOOMAGA` spool
magic are still accepted on read, and the desktop entry declares Boomaga's MIME
type. Existing `.boo` files continue to open.

### Assets

The application icon was **replaced**, not renamed. Boomaga's was a Flaticon
image whose licence forbids "offering Flaticon Contents designs for download",
which is what publishing it in a source repository does. PlicaPage's icon is
original work under the project licence. See
`src/plicapage/misc/images/mainicon/AUTHORS.md`.

The About dialog's "Thanks" tab, which carries the attribution the Icons8
CC BY-ND licence requires, was hidden upstream (`setVisible(false)`). It is now
shown.

`PJL_Technical_Reference_Manual.pdf` (3 MB of HP's copyrighted manual, carried in
the repo with no licence grant) was removed.

The macOS Sparkle `SUFeedURL` pointed at `boomaga.org`; it was removed rather
than repointed, since this project has no update infrastructure.
