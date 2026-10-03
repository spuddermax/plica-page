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

## 2026-10 — Paper sizes for applications, defined in the program

The 5 x 8.25 in size was written into the PPD by hand. Sizes are now the user's
to define: Preferences > Paper sizes for applications lists them (name, width,
height in the chosen unit) and, on OK, rewrites every queue on the PlicaPage
backend with the installed PPD plus those sizes, keeping each queue's default
paper. Applications - LibreOffice's printer properties, the system print
dialogs - then offer them by name once restarted.

- `kernel/papersizes.{h,cpp}`: the list, kept in the settings
  (`PaperSizes/User`, points) and starting with 5 x 8.25 in, so nothing the PPD
  used to offer disappears; `ppdWithPaperSizes()` adds each size to PageSize,
  PageRegion, ImageableArea (to the edge) and PaperDimension under a
  `wNNNhNNN` keyword; `installPaperSizes()` hands the result to `lpadmin -P`,
  which members of the lpadmin group may run without a password.
- The shipped PPD no longer carries the 5 x 8.25 in size itself.
- At start-up the program puts the sizes back on any queue that lacks them, so
  a reinstalled queue gets them again without the dialog.
- The build passes the installed PPD's path to the program as
  `CUPS_PPD_TEMPLATE`.

## 2026-10 — Does the whole job fit the printer?

- The printable-area outline is coloured per page: green where the page lies
  within the printer's reach on the side it is printed on, red where it does
  not. A booklet spread colours each half by its own page; other layouts
  colour the sheet red if any of its pages is cut off.
- A bar above the preview reports on the whole job. Green when every page
  fits; otherwise red, naming the pages cut off, the edge - top, bottom, left,
  right, or for a booklet the outer edge and the spine, as the page is seen in
  the preview - and by how much, and which margins to raise to what: the
  printer's limit on each edge some page crosses, since margins at the limit
  fit any page. `Project::pageOverflow()`, `clippedPages()`,
  `hardwareLimit()` and `printableAreaReport()` hold the logic; the page
  tooltip's warning uses the same check.
- When pages are cut off the bar offers "Fit margins to printer"
  (`Project::fitMarginsToPrinter()`): each margin some page crosses goes to the
  printer's limit on that edge - the most of the paper it can reach - and the
  profile is saved. Margins no page crosses are left as they are.

## 2026-10 — Status bar counts pieces of paper

The status bar mixed two meanings of "sheet": the total, "(N sheets)", counted
pieces of paper, while "Sheet X of Y" counted the preview's steps - printed
sides when double-sided, so 8 pages 1-up double-sided read "Sheet 4 of 8"
beside "(4 sheets)", and reading spreads for a booklet, which are not printed
sheets at all. Both now count paper. "Sheet X of Y" is the piece of paper the
current page is printed on, found through the printed layout
(`Project::printSheetIndex()`, `paperOf()`, `paperCount()`), and when printing
double-sided it names the side: front or back, or for a booklet outside or
inside. The page label no longer repeats the paper count beside it.

Hovering a page in the preview for two seconds shows its details: where it
comes from (job and page, or an inserted blank), its size and trimmed size in
the chosen unit, any rotation by hand, the sheet and side it prints on, the
scale it prints at, and a warning when it reaches past the printer's limit for
that side. The sheet wording comes from `Project::sheetDescription()`, shared
with the status bar.

## 2026-10 — Landscape direction per printer, and page order in manual duplex

Two faults found on a real 8-page booklet printed on an HP DeskJet 2700 with
manual duplex.

**The two sheets came out nested the wrong way round.** The HP's PPD has
`*DefaultOutputOrder: Reverse` (a face-up tray), so CUPS reverses every job:
right for a one-sided stack, but it reversed each pass of the manual duplex job
too, and the outer sheet came out last, on top of the inner one. The pairing of
fronts and backs survived because both passes were reversed alike. Manual
double-sided passes, and the duplex wizard's, now go out with
`-o outputorder=normal`, so the sheet order PlicaPage plans is the order printed.
Existing calibrations stay valid: the wizard measures how the second pass meets
the sheets relative to the first, which reversing both passes did not change.

**The printer's limits fell on the wrong edges of landscape sheets.** CUPS turns
a landscape page onto portrait paper in the direction the PPD's
`*LandscapeOrientation` gives - measured through the real filter chain: with
`Plus90` (the Brother) a `/Rotate 90` page lands the right way up and 270 upside
down; with `Minus90` (the HP, and libcups's default when the PPD is silent) the
other way about. PlicaPage assumed `Plus90` everywhere. `PpdOptions` now reads
it, `Printer::pageTurnedOnPaper()` applies it, and both the printable-area
outline (through `sheetGoesThroughTurned()`, which now composes the sheet's
`/Rotate`, any manual pre-rotation and the duplexer) and the print offset use
it. The print offset was negated for the wrong landscape sheets on `Minus90`
printers. The Margins tab now states the limits in the frame the margins are
entered in.

## 2026-10 — Show where the printer stops printing

Margins could be set inside the area the printer can reach without any sign
that the edge would be cut off. Nothing is enforced now either - some printers
reach further than their driver admits - but the limit is shown:

- `Printer::hardwareMargins()` reads the PPD's imageable area for the profile's
  paper size (the queue default when it names none), in the sheet's portrait
  frame; `printableRect()` is the paper less those margins.
- The preview outlines that area in a red dashed line, through
  `PreviewWidget::sheetToWidget()`, which maps the sheet frame to the screen the
  same way `pageRect()` places pages. On the half-sheets that open and close a
  booklet it is clipped to the half shown.
- Double-sided printing can send a side through the printer turned round, and
  its limits then fall on the opposite edges: the backs of an automatic
  long-edge duplex, and the first pass of a manual job that
  `calcDuplexPasses()` pre-rotates. `sheetGoesThroughTurned()` (duplex.cpp)
  decides it from the same settings printing uses, and
  `Project::hardwareMargins(sheet)` turns the limits for those sheets. The
  preview shows reading spreads, whose two halves come from different printed
  sides, so the project maps each page to its printed sheet and each half of a
  booklet spread gets the limits of its own side.
- The Margins tab states the four limits in the current unit and names any
  margin set inside them - inside either orientation, when some sides go
  through turned.

## 2026-10 — Select several pages and delete them together

Pages could only be deleted one at a time, or from one page to the end of its
job. The preview now selects pages the way a file manager does: a click selects
one page, Ctrl+click adds or removes one, and Shift+click selects the run from
the last page clicked, across sheets; Ctrl+Shift+click adds the run instead.
Selected pages are outlined in blue. Delete, or "Delete N selected pages" in the
page menus, removes them in one step (`Project::deletePages()`), and Escape or a
click off the pages clears the selection. The selection lives in the project and
is pruned on every update, so it never outlives a deleted page or job.

## 2026-10 — Undo every page deletion at once

Deleted pages could only be brought back one at a time, from a submenu with
an entry per page. Both undo-delete submenus now start with an item that
restores them all: "All deleted pages" in the page menu, across every job,
and "All deleted pages in this job" in the job menu. `Project::undoDeletePages()`
shows them with a single re-layout and makes the first restored page current.

## 2026-10 — Fold line for booklets

Boomaga marked the fold of a booklet only in the on-screen preview, as a guide
overlay that never reached the paper.

- New fold-line option, shown in the Layout panel while Booklet is selected:
  none, solid, dotted, or dotted with cross marks for stitching
  at a spacing the user sets (default 2 in). Stored as `Project/FoldLine` and
  `Project/StitchSpacing`.
- Drawn by `TmpPdfFile::foldLineStream()` into the sheet layer, inside the print
  offset so it stays registered with the pages, which puts it in the print, the
  PDF export and the preview alike; the preview's own guide steps aside while a
  line is chosen. It goes through the middle of the gap between the two pages,
  so it follows the Internal margin and any unequal outer margins, and runs the
  full width of the paper. Stitch marks spread out from the centre of the fold
  and stop where the pages end.
- The line is drawn on both faces of every booklet sheet, and never on the
  preview sheet that joins two sub-booklets, which has no fold. Stitch marks can
  go on all faces, only the inside of each folded sheet (the stitch side), or
  only the centre spread of each booklet, where the stitches pass through every
  sheet (`Project/StitchMarks`). With all faces or the stitch side, faces
  without marks keep the dotted line; with the centre only, no other face
  gets a line at all. The
  layout tags printed sheets with new `HintInsideFace` and `HintCenterSpread`
  hints to tell them apart. A reading spread in the preview holds one page from
  an inside face and one from an outside face, so the preview shows "stitch
  side" marks on every spread; only the centre spread is exact there.
- The line and its marks are drawn in a colour the user picks (default 60%
  gray, `Project/FoldLineColor`), set once as the stroke colour for both.

## 2026-10 — Source page margins from the print dialog

The queue's PPD described a physical printer: every page size carried an
unprintable border of 0.25 in at the sides and 0.5 in top and bottom, and custom
page sizes were not accepted. Applications take that border as the smallest
margin they may offer, so a document could not be laid out any closer to the
edge before being sent to PlicaPage — and the real printer's margins were then
added on top when the page was placed on the sheet.

- Every `*ImageableArea` is now the full page. PlicaPage is not a device and has
  no border of its own; the margins of the real printer still apply, in the
  printer profile.
- Custom page sizes are accepted (`*CustomPageSize`, `*ParamCustomPageSize`,
  `*HWMargins: 0 0 0 0`), from 1 in to 200 in on either side. Dialogs that attach
  margins to a custom size need this before they will offer one.
- A named 5 x 8.25 in size (`w360h594`): half a Letter sheet less 1/8 in at the
  top, bottom and outer edge and 3/8 in at the spine. A document laid out at
  this size fills a booklet page at 100%. It is named because LibreOffice's
  printer properties and the browser previews list only named sizes.

A queue keeps the copy of the PPD it was created with, so an existing queue has
to be refreshed after upgrading — see INSTALL.txt.

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
