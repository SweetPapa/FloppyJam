#!/usr/bin/env python3
"""Import the original authored soundtrack into the offline native bundles.
Usage: python3 mobile/tools/import_soundtrack.py /path/to/magLava
Requires ffmpeg. Sources are left untouched; AAC is supported on both platforms.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

source = Path(sys.argv[1]).expanduser() / 'src/assets/audio/songsforgame'
out = Path(__file__).resolve().parents[1] / 'assets/Audio'
tracks = ['magLava-main-theme'] + [f'magLava-game-bg-{i}' for i in range(1, 4)]
manifest = []
for track in tracks:
    original = source / (track + '.ogg')
    destination = out / (track + '.m4a')
    subprocess.run(['ffmpeg', '-v', 'error', '-nostdin', '-y', '-i', str(original),
                    '-c:a', 'aac', '-b:a', '128k', '-ar', '44100', '-ac', '2', '-movflags', '+faststart', str(destination)], check=True)
    info = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-show_entries',
                                               'format=duration', '-of', 'json', str(destination)]))
    manifest.append({'file': destination.name, 'source': 'magLava/src/assets/audio/songsforgame/' + original.name,
                     'source_sha256': hashlib.sha256(original.read_bytes()).hexdigest(),
                     'sha256': hashlib.sha256(destination.read_bytes()).hexdigest(),
                     'duration_seconds': float(info['format']['duration']),
                     'codec': 'AAC-LC', 'target_bitrate': 128000, 'bytes': destination.stat().st_size})
(out / 'soundtrack.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(manifest, indent=2))
