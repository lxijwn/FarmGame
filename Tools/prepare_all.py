"""Rebuild authored data, witnesses, original art, previews and validation reports."""
import runpy
from pathlib import Path

if __name__ == '__main__':
    here=Path(__file__).resolve().parent
    for name in ['prepare_levels.py','validate_levels.py','prepare_art.py','validate_assets.py']:
        print(f'Running {name}',flush=True)
        runpy.run_path(str(here/name),run_name='__main__')
    print('All data and asset checks passed. C++ integration is the next stage.',flush=True)
