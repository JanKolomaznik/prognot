#!/usr/bin/env python3
"""Render the Qt widgets offscreen and encode small, looping README GIFs."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(__file__).resolve().parent
output = root / 'docs' / 'images'
output.mkdir(exist_ok=True)

def run(*args, **kwargs):
    subprocess.run(args, check=True, **kwargs)

with tempfile.TemporaryDirectory(prefix='prognot-gallery-') as temporary:
    work = Path(temporary)
    build = work / 'build'
    frames = work / 'frames'
    frames.mkdir()
    run('cmake', '-S', str(source), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release')
    run('cmake', '--build', str(build), '--target', 'capture', '-j2')
    environment = dict(os.environ, QT_QPA_PLATFORM='offscreen', QT_SCALE_FACTOR='1')
    run(str(build / 'capture'), str(frames), env=environment)
    for scene in ('progress-bar', 'progress-dialog', 'stacked-progress'):
        run('ffmpeg', '-hide_banner', '-loglevel', 'error', '-y',
            '-framerate', '10', '-i', str(frames / (scene + '-%03d.png')),
            '-filter_complex',
            '[0:v]split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=none',
            '-loop', '0', str(output / (scene + '.gif')))
        print(f'{scene}.gif: {(output / (scene + ".gif")).stat().st_size:,} bytes')
