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


// Minimal reproducer: poppler-cpp only, no Qt, no PlicaPage.
//
// Mirrors what render.cpp does - N threads, each with its OWN document,
// rendering pages concurrently. If this crashes, the fault is in poppler (or
// its lcms2 colour management), not in the application using it.
//
// Build:
//   g++ -O2 -std=c++11 popplerstress.cpp $(pkg-config --cflags --libs poppler-cpp) -pthread -o popplerstress
// Run:
//   ./popplerstress <file.pdf> <threads> <iterations> [serialise]

#include <poppler-document.h>
#include <poppler-image.h>
#include <poppler-page.h>
#include <poppler-page-renderer.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static std::mutex gRenderMutex;
static bool gSerialise = false;

static void worker(const std::string &path, int iterations, int seed)
{
    std::unique_ptr<poppler::document> doc(
        poppler::document::load_from_file(path));
    if (!doc)
        return;

    const int pages = doc->pages();
    if (pages < 1)
        return;

    poppler::page_renderer renderer;
    renderer.set_render_hint(poppler::page_renderer::antialiasing, true);
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing, true);

    for (int i = 0; i < iterations; ++i)
    {
        std::unique_ptr<poppler::page> page(doc->create_page((seed + i) % pages));
        if (!page)
            continue;

        if (gSerialise)
        {
            std::lock_guard<std::mutex> lock(gRenderMutex);
            poppler::image img = renderer.render_page(page.get(), 150, 150);
            (void)img.is_valid();
        }
        else
        {
            poppler::image img = renderer.render_page(page.get(), 150, 150);
            (void)img.is_valid();
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::fprintf(stderr, "usage: %s <file.pdf> <threads> <iterations> [serialise]\n", argv[0]);
        return 2;
    }

    const std::string path = argv[1];
    const int threads    = std::atoi(argv[2]);
    const int iterations = std::atoi(argv[3]);
    gSerialise = (argc > 4);

    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back(worker, path, iterations, t * 7);

    for (auto &t : pool)
        t.join();

    std::printf("ok\n");
    return 0;
}
