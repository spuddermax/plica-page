# poppler concurrent rendering crash - reproducers

Standalone, poppler-cpp only (no Qt, no PlicaPage). They exist to prove where
the crash lives and, later, to prove a fix actually works rather than got lucky.

Nothing here is part of the build — CMake never descends into this directory.
Build them by hand, from this directory:

    g++ -O2 -std=c++14 popplerstressN.cpp $(pkg-config --cflags --libs poppler-cpp) -pthread -o stress

Usage: `./stress <file.pdf> <threads> <iterations> [mode]`
Reproduced against ~/Documents/Primer.pdf (14 MB scanned PDF, poppler 24.02).

| file | what it shows | result |
|---|---|---|
| `popplerstress.cpp`  | 8 threads rendering, documents loaded up front | 0/10 crashes |
| `popplerstress2.cpp` | load/destroy interleaved with rendering | 5/10 crashes |
| `popplerstress3.cpp` | mode 0 none / 1 rw-lock / 2 full mutex | 3/10, 0/10, 0/10 |
| `popplerstress4.cpp` | as 3, 20 renders per load (realistic ratio) | 3/6, 0/6, 0/6 |
| `popplerstress5.cpp` | rendering only, threads released together, mode 1 warms up first | 0/20, 0/20 |

## What these actually established - read this before trusting the table

The first four were read as showing that **document load or destroy racing a
render** was the fault, because the run that only rendered survived and the runs
that interleaved loads did not.

That conclusion was wrong, and the reason is worth remembering: every one of
those reproducers renders concurrently in *all* of its modes. Concurrent
rendering was never the variable under test, so it could never be exonerated,
and locking that happened to serialise it looked like it had fixed the loads.

The crash is **render against render**. What settled it was a core dump from
PlicaPage's own stress test: five worker threads all inside `render_page`, the
main thread asleep in `qWait`, and no load or destroy anywhere in the process.
The abort itself lands in `cmsCloseProfile` under `GfxState`'s constructor -
poppler's lazily built global colour profiles, set up without a guard by
whichever renders reach them first. A double free is reported when something is
freed, not when the heap was corrupted, so an idle main thread at the moment of
the abort proves nothing on its own.

Removing the reloads entirely from PlicaPage's stress test still aborted 4 runs
in 20. Making only the *first* render in the process exclusive - so poppler's
one-time setup completes alone, after which threads render in parallel as
before - took that to 0. That is the fix, in `src/plicapage/popplergate.{h,cpp}`.

`popplerstress5.cpp` is the attempt to show the same thing here, with every
document opened before any thread starts and all threads released from a barrier
together. It does not reproduce at 150 dpi, which is why the PlicaPage-level
test is the one that counts. The standalone case is left in as an honest
negative: these five programs between them cannot distinguish the two
hypotheses, and only the core dump could.

Timings from popplerstress4 (crashed runs excluded, so mode 0 is flattered;
std::shared_timed_mutex is also a poor choice under contention - treat the
locking costs as an upper bound, not a verdict):

    mode 0 no locking   3/6 crashes   4.3 s
    mode 1 rw lock      0/6 crashes   9.6 s
    mode 2 full mutex   0/6 crashes  17.4 s
