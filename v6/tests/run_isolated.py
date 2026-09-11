"""Run a test in a disposable save directory; never touch a player's slots."""
from pathlib import Path
import subprocess
import sys
import tempfile
binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='huedunit-test-') as directory:
    result = subprocess.run([str(binary)], cwd=directory)
sys.exit(result.returncode)
