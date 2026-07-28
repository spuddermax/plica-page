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


// Third variant: a read/write lock. Loading or destroying a document takes it
// exclusively; rendering only takes it shared, so pages still render in
// parallel. Tests whether the race is specifically load/destroy vs render.
#include <poppler-document.h>
#include <poppler-image.h>
#include <poppler-page.h>
#include <poppler-page-renderer.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

static std::shared_timed_mutex gLock;
static int gMode = 0;   // 0 none, 1 rwlock, 2 full mutex

static void render_loop(const std::string &path, int iterations)
{
    for (int i = 0; i < iterations; ++i) {
        std::unique_ptr<poppler::document> doc;
        {
            if (gMode) { std::unique_lock<std::shared_timed_mutex> w(gLock);
                doc.reset(poppler::document::load_from_file(path)); }
            else doc.reset(poppler::document::load_from_file(path));
        }
        if (!doc || doc->pages() < 1) continue;

        poppler::page_renderer r;
        r.set_render_hint(poppler::page_renderer::antialiasing, true);
        for (int p = 0; p < 3; ++p) {
            std::unique_ptr<poppler::page> pg(doc->create_page(p % doc->pages()));
            if (!pg) continue;
            if (gMode == 2) { std::unique_lock<std::shared_timed_mutex> w(gLock);
                poppler::image im = r.render_page(pg.get(), 150, 150); (void)im.is_valid(); }
            else if (gMode == 1) { std::shared_lock<std::shared_timed_mutex> s(gLock);
                poppler::image im = r.render_page(pg.get(), 150, 150); (void)im.is_valid(); }
            else { poppler::image im = r.render_page(pg.get(), 150, 150); (void)im.is_valid(); }
        }
        // Destroying doc here also needs exclusivity under mode 1.
        if (gMode) { std::unique_lock<std::shared_timed_mutex> w(gLock); doc.reset(); }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 5) { std::fprintf(stderr, "usage: %s <pdf> <threads> <iters> <mode 0|1|2>\n", argv[0]); return 2; }
    const std::string path = argv[1];
    const int threads = std::atoi(argv[2]), iters = std::atoi(argv[3]);
    gMode = std::atoi(argv[4]);
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) pool.emplace_back(render_loop, path, iters);
    for (auto &t : pool) t.join();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    std::printf("ok %lld ms\n", (long long)ms);
    return 0;
}
