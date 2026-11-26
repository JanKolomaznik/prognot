# Regenerating the widget gallery

The README embeds short, looping GIFs using ordinary Markdown image references.
GIF is suitable here because the demonstrations are small, silent, and stored
alongside the README. GitHub supports GIF images as well as uploaded video;
for longer recordings, H.264 MP4 is a useful alternative with playback controls.
See [GitHub's supported media formats](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/attaching-files).

## Generate

Requirements: the project's C++20/CMake/Boost/Qt dependencies, Python 3, FFmpeg
with GIF support, the Qt offscreen platform plugin, and the DejaVu Sans font.
Run from the repository root:

```sh
python3 docs/gallery/generate.py
```

The script builds a capture program in a temporary directory, drives the real
widgets through core progress handles, saves frames, and encodes four GIFs into
`docs/images/`. It does not record the desktop, require a display server, or use
an image-generation model. The stacked and popup captures composite the widget and its
separate popup onto one canvas so both appear in the README.

Each loop lasts 6.4 seconds at 10 frames per second, including short pauses at
the beginning and end. Captures use Qt Fusion, a fixed palette, and a 600 × 280
canvas. FFmpeg builds a palette for each animation; disabling dithering keeps
text and flat UI colors clean and files small. Font/Qt versions can affect pixels.

The gallery demonstrates progress, rather than the timing of actual I/O or CPU
work. The dialog's Cancel button remains available; cancellation behavior is
covered by the Boost.Test suite.
