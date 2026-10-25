"""Export normal-speed Cocos framebuffers, then encode them as a silent MP4."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from datetime import datetime
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--ffmpeg', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.ffmpeg = args.ffmpeg.resolve()
    args.output = args.output.resolve()
    run = ROOT/'Validation'/'demo'/datetime.now().strftime('%Y%m%d-%H%M%S')
    run.mkdir(parents=True)
    executable = ROOT/'build-Release'/'bin'/'FarmGame.exe'
    print(f'Exporting actual Cocos frames to {run}', flush=True)
    subprocess.run([str(executable), '--demo', '--report-dir', str(run)],
                   cwd=run, check=True, timeout=660)
    report = json.loads((run/'runtime-report.json').read_text(encoding='utf-8'))
    assert report['status'] == 'passed' and report['mode'] == 'demo', report.get('error')
    expected_count=sum(len(json.loads(p.read_text(encoding='utf-8'))['steps'])
                       for p in (ROOT/'Resources'/'levels').glob('*_solution.json'))
    assert len(report['witnessObservations']) == expected_count
    for mode, total in [('circular_01', 48), ('rectangular_01', 64)]:
        won = next(o for o in report['trafficObservations'] if o['levelId'] == mode)
        assert won['outcome'] == 'won' and won['collected'] == total and won['remaining'] == 0
        expected = json.loads((ROOT/'Resources'/'levels'/(mode+'_solution.json')).read_text(encoding='utf-8'))['steps']
        actual = [s for s in report['witnessObservations'] if s['levelId'] == mode]
        assert len(actual) == len(expected)
        for observed, step in zip(actual, expected):
            assert observed['vehicleId'] == step['vehicleId']
            assert observed['loadedAfter'] == step['loadedAfter']
            assert observed['fruitIds'] == [e['fruit'] for e in step['events']]
            assert observed['slotIndices'] == [e['slotIndex'] for e in step['events']]
    frames = sorted((run/'frames').glob('frame_*.png'))
    assert len(frames) == report['demoFrames'] and len(frames) > 5500
    for i, frame in enumerate(frames):
        assert frame.name == f'frame_{i:06d}.png'
    for i in (0, 60, len(frames)//2, len(frames)-1):
        with Image.open(frames[i]) as im:
            assert im.size == (480, 720)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(args.ffmpeg), '-hide_banner', '-loglevel', 'error', '-y',
                    '-framerate', '30', '-i', str(run/'frames'/'frame_%06d.png'),
                    '-an', '-c:v', 'libx264', '-crf', '20', '-pix_fmt', 'yuv420p',
                    '-movflags', '+faststart', '-metadata',
                    'title=Farm: actual Cocos gameplay, automated inputs, normal speed',
                    str(args.output)], check=True, timeout=180)
    subprocess.run([str(args.ffmpeg), '-v', 'error', '-i', str(args.output),
                    '-f', 'null', '-'], check=True, timeout=120)
    result = dict(status='passed', frames=len(frames), fps=30, durationSeconds=len(frames)/30,
                  resolution=[480, 720], source='actual Cocos framebuffer; automated witness inputs',
                  speed='normal game time, fixed 1/30 second per recorded frame', audio=False,
                  executableSha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                  videoSha256=hashlib.sha256(args.output.read_bytes()).hexdigest(),
                  video=str(args.output), runtimeEvidence=str(run/'runtime-report.json'))
    args.output.with_suffix('.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False, indent=2), flush=True)


if __name__ == '__main__':
    main()
