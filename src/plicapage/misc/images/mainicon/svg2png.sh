#!/bin/bash
# Regenerate the icon raster set from plicapage.svg.
#
# Uses rsvg-convert rather than Inkscape: the old script was written for
# Inkscape 0.x, whose -z/-e flags were removed in 1.0, so it silently produced
# nothing on a modern system.
set -e

PNG_NAME=plicapage
SVG_FILE=plicapage.svg

render() { rsvg-convert -w "$1" -h "$1" -o "$2" "${SVG_FILE}"; }

for size in 16 32 48 64 128 256 512; do
    render ${size} ${PNG_NAME}-${size}x${size}.png
done

mkdir -p ${PNG_NAME}.iconset
for size in 16 32 128 256 512; do
    render ${size}          ${PNG_NAME}.iconset/icon_${size}x${size}.png
    render $((size * 2))    ${PNG_NAME}.iconset/icon_${size}x${size}@2x.png
done

echo "Regenerated $(ls -1 ${PNG_NAME}-*.png ${PNG_NAME}.iconset/*.png | wc -l) files."
