"""Check fresh Cocos framebuffers and exact C++ witness observations."""
import argparse
import json
from pathlib import Path
from PIL import Image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    report = json.loads((args.directory/'runtime-report.json').read_text(encoding='utf-8'))
    assert report['status'] == 'passed', report
    assert report['mode'] == 'smoke' and report['frames'] >= 180
    assert len(report['trafficChecks']) == 10 and len(report['gameplayChecks']) == 21
    for text in ('same-color batch', 'simultaneous left/right', 'endpoint waits', 'random bag', 'tail pop'):
        assert any(text in check for check in report['gameplayChecks']), text
    assert report['trafficInputs'] == 0, 'External clicks must not change automatic input'
    assert report['assetFrames'] == 33 and report['levelCount'] == 2
    assert report['menuVisits'] == 3 and report['reloads'] == 1
    assert report['keyEvents'] == 5 and not report['lastKeyPressed']
    assert report['currentScene'] == 'mode_selection' and not report['error']
    assert report['glRenderer'] not in ('', 'unavailable')
    assert report['glVersion'] not in ('', 'unavailable')
    expected = [('circular_01', 48, 6, 0), ('circular_01', 48, 6, 0), ('rectangular_01', 48, 8, 4)]
    assert len(report['views']) == len(expected)
    for view, (identifier, fruit, vehicles, entrances) in zip(report['views'], expected):
        assert view == dict(levelId=identifier, fruitSprites=fruit, vehicleSprites=vehicles,
                            waitingSprites=5, entranceSprites=entrances)
    names = ['01_mode_selection.png', '02_circular.png', 'circular_collecting.png',
             'circular_refilling.png', 'circular_partial_waiting.png', 'circular_won.png',
             'circular_reloaded.png', 'rectangular_initial.png', 'rectangular_collecting.png',
             'rectangular_refilling.png', 'rectangular_spawning.png', 'rectangular_partial_waiting.png', 'rectangular_won.png']
    assert set(report['screenshots']) == set(names) and len(report['screenshots'])==len(names), report['screenshots']
    images = {}
    for name in names:
        image = Image.open(args.directory/name).convert('RGB')
        assert image.size == (480, 720)
        assert len(image.getcolors(image.width*image.height)) > 100, 'Blank render'
        images[name] = image
    assert images['02_circular.png'].tobytes() == images['circular_reloaded.png'].tobytes()
    observations = {o['capture']: o for o in report['trafficObservations']}
    assert observations['rectangular_spawning.png']['spawning']>0
    assert set(observations) == set(names[1:])
    for mode, total, cursors in [('circular', 48, []), ('rectangular', 64, [4, 4, 4, 4])]:
        flight = observations[mode+'_collecting.png']
        assert flight['reserved'] > 0 and flight['roadUsed'] > 0
        waiting = observations[mode+'_partial_waiting.png']
        assert waiting['roadUsed'] == 0 and waiting['waitingUsed'] > 0
        assert any(v['phase'] == 'waiting' and v['loaded'] > 0 for v in waiting['vehicles'])
        won = observations[mode+'_won.png']
        assert won['outcome'] == 'won' and won['remaining'] == 0 and won['collected'] == total
        assert won['pickups'] == total and won['roadUsed'] == won['waitingUsed'] == won['reserved'] == 0
        assert won['entranceCursors'] == cursors
        assert all(v['phase'] == 'completed' for v in won['vehicles'])
    project = Path(__file__).resolve().parents[1]
    for mode in ('circular_01', 'rectangular_01'):
        witness = json.loads((project/'Resources'/'levels'/(mode+'_solution.json')).read_text(encoding='utf-8'))
        actual = [o for o in report['witnessObservations'] if o['levelId'] == mode]
        assert len(actual) == len(witness['steps'])
        for o, w in zip(actual, witness['steps']):
            for key in ('vehicleId', 'loadedBefore', 'loadedAfter', 'remainingTargets'):
                assert o[key] == w[key], (mode, key, o, w)
            assert o['waitingSlot'] == w['waitingSlot']-1
            assert o['phase'] == ('completed' if w['result'] == 'completed_exit' else 'waiting')
            assert o['fruitIds'] == [e['fruit'] for e in w['events']]
            assert o['slotIndices'] == [e['slotIndex'] for e in w['events']]
    result = dict(status='passed', checks=[
        '33 atlas frames loaded; both configured layouts instantiated',
        'five Cocos navigation events; reload restores identical framebuffer',
        '13 nonblank framebuffers including collection, refill, tail pop, partial return and two wins',
        f'{len(report["witnessObservations"])} scene launches match exact fruit IDs/slots, loads, bindings and remaining targets',
        '48 circular and 64 rectangular targets collected; finite entrances exhausted',
        '10 C++ traffic groups and 21 gameplay groups including seeded random bags and tail pop'],
        scope='C++ gameplay and rendered scene validation; Cocos key routing')
    (args.directory/'evidence-check.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
