# PlicaPage

A virtual printer for CUPS. Print to it from any application and, instead of
paper, you get a preview window where you can rearrange, combine and impose the
document before it reaches a real printer.

*Plica* is Latin for a fold — which is most of what this program does to a page.

---

## What it does

Print to the **PlicaPage** queue from any application and its window opens with
the document loaded. From there you can:

- **Trim whitespace.** Detect the ink on each page, throw away the blank margins
  and scale what remains to fill the sheet. On a wide-margined PDF printed 4-up
  this is the difference between readable and not.
- **Impose** 1, 2, 4 or 8 pages per sheet, or fold the document into a booklet.
- **Print both sides** on a printer without a duplexer — it will tell you when to
  turn the stack over.
- **Combine documents.** Print a second document and it is appended to the first,
  so several sources can go out as one job.
- **Reorder, rotate and hide** individual pages before printing.
- **Export to PDF** instead of printing.

Lengths are shown in millimetres or inches, following your locale by default and
switchable in Preferences.

## Relationship to Boomaga

PlicaPage is a fork of [Boomaga](https://github.com/Boomaga/boomaga) by Alexander
Sokoloff and the Boomaga team, taken at commit `7f7ad47` (2022-02-21) — the last
commit upstream made. Boomaga's README carries the maintainer's own note that he
no longer has time for the project.

**The great majority of this program is still their work**, and their copyright
headers are intact in every file. What changed is recorded in
[CHANGES-FROM-UPSTREAM.md](CHANGES-FROM-UPSTREAM.md).

PlicaPage is built to install **alongside** a stock Boomaga package rather than
replace it, so you can print the same document to both queues and compare. The
two share no backend, D-Bus name, spool directory, settings file or installed
path.

Files saved by Boomaga (`*.boo`) still open. PlicaPage saves its own projects as
`*.plica` so the two do not fight over the file association.

## How it works

CUPS hands the print job to `/usr/lib/cups/backend/plicapage`, which runs as root,
writes the job into `/var/cache/plicapage/<user>/`, drops to the printing user and
asks the GUI to pick it up over D-Bus (starting it if it is not already running).
Pages are rendered with [poppler](https://poppler.freedesktop.org/); Ghostscript
is used only to convert PostScript input to PDF.

## Installing

### Build dependencies

On Debian/Ubuntu/Mint:

```bash
sudo apt install build-essential cmake pkg-config \
                 qtbase5-dev qttools5-dev qttools5-dev-tools \
                 libcups2-dev libpoppler-cpp-dev zlib1g-dev ghostscript
```

### Build and install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j$(nproc)
sudo cmake --install build
```

`/usr` matters: the CUPS backend prepends `GUI_DIR` to `PATH` and launches the GUI
by name, so installing to the default `/usr/local` while another build sits in
`/usr` would launch the wrong one.

### Register the printer

```bash
sudo ./scripts/installPrinter.sh
```

Or add it by hand in your printer settings: choose the local printer
**PlicaPage (Virtual PlicaPage printer)** and the `plicapage.ppd` driver.

### Running the tests

```bash
cmake -S . -B build -DBUILD_TESTS=Yes
cmake --build build -j$(nproc)
./build/src/plicapage/tests/plicapage_test
```

They run headless; no display required.

## Known quirks

- Each app lists the other's queue as a printable target, because the printer
  list only filters out its *own* backend URI. Handy for comparing the two,
  but it does mean you can round-trip a job through both by accident.
- The CUPS backend finds the user's session bus by scanning `/proc`. It is
  effective but not elegant, and inherited from upstream — if the window does
  not appear after printing, that is the first place to look. Submitting jobs
  to both PlicaPage and Boomaga in the same instant has been observed to leave
  one window unopened; the job itself is not lost, and printing again brings it
  up. Leaving a second or two between the two jobs avoids it.

## Licence

LGPL-2.1-or-later. See [COPYING](COPYING) for the full picture, including the
GPL-licensed PPD and third-party assets.
