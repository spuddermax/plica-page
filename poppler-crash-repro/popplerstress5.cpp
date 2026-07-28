/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 *
 * Copyright: 2026 Matthew Daines <spuddermax@gmail.com>
 * Authors:
 *   Matthew Daines <spuddermax@gmail.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */


// Fifth reproducer: no loading or destroying at all while rendering.
//
// Every document is opened up front on the main thread. The worker threads then
// wait on a barrier and all enter render_page() at the same instant. If the
// crash needed a load or a destroy to race a render - which is what the first
// round of investigation concluded - this cannot fail. It does fail, inside
// GfxState's constructor, so the race is render against render.
//
// Mode 1 renders one page on the main thread before the threads start, which
// forces poppler's lazy one-time colour-profile setup to complete while nothing
// else is running. That is the whole fix, and it is what this measures.
//
//   g++ -O2 -std=c++14 popplerstress5.cpp $(pkg-config --cflags --libs poppler-cpp) -pthread -o stress5
//   ./stress5 <file.pdf> <threads> <rounds> [mode 0|1]
#include <poppler-document.h>
#include <poppler-image.h>
#include <poppler-page.h>
#include <poppler-page-renderer.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

static std::atomic<int> gArrived(0);
static std::atomic<bool> gGo(false);

static void render_one(poppler::document *doc, int page)
{
    std::unique_ptr<poppler::page> pg(doc->create_page(page % doc->pages()));
    if (!pg)
        return;
    poppler::page_renderer r;
    r.set_render_hint(poppler::page_renderer::antialiasing, true);
    poppler::image im = r.render_page(pg.get(), 150, 150);
    (void)im.is_valid();
}

static void render_loop(poppler::document *doc, int rounds, int threads)
{
    // Line every thread up so they all call render_page() together. A stagger
    // of even a millisecond is enough to hide a one-time initialisation race.
    ++gArrived;
    while (!gGo.load())
        std::this_thread::yield();

    for (int i = 0; i < rounds; ++i)
        render_one(doc, i);

    (void)threads;
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <pdf> <threads> <rounds> [mode 0|1]\n", argv[0]);
        return 2;
    }
    const std::string path = argv[1];
    const int threads = std::atoi(argv[2]);
    const int rounds  = std::atoi(argv[3]);
    const int mode    = (argc > 4) ? std::atoi(argv[4]) : 0;

    // All loading happens here, before a single thread exists.
    std::vector<poppler::document *> docs;
    for (int t = 0; t < threads; ++t) {
        poppler::document *d = poppler::document::load_from_file(path);
        if (!d || d->pages() < 1) { std::fprintf(stderr, "load failed\n"); return 2; }
        docs.push_back(d);
    }

    if (mode == 1)
        render_one(docs[0], 0);   // warm poppler's globals up while single-threaded

    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back(render_loop, docs[t], rounds, threads);

    while (gArrived.load() < threads)
        std::this_thread::yield();
    gGo.store(true);

    for (auto &t : pool)
        t.join();
    for (auto *d : docs)
        delete d;

    std::printf("ok\n");
    return 0;
}
