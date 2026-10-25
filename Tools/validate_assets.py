"""Check asset references, plist format-3 frames and atlas pixel integrity."""
import json
import math
import plistlib
import re
from PIL import Image
from prepare_levels import ROOT, RES, COLORS, write_json


def numbers(value):
    return [int(v) for v in re.findall(r'-?\d+',value)]


def main():
    manifest=json.loads((RES/'art/asset_manifest.json').read_text(encoding='utf-8'))
    assets=manifest['assets']
    plist=plistlib.loads((RES/manifest['atlas']).read_bytes())
    atlas=Image.open(RES/manifest['texture']).convert('RGBA')
    assert plist['metadata']['format']==3
    assert plist['metadata']['textureFileName']=='farm_atlas.png'
    assert numbers(plist['metadata']['size'])==list(atlas.size)==manifest['textureSize']
    assert all(n<=2048 and n & (n-1)==0 for n in atlas.size)
    assert set(plist['frames'])=={a['frame'] for a in assets.values()}
    seen_frames=set()
    rectangles=[]
    for key,asset in assets.items():
        frame=asset['frame']
        assert frame not in seen_frames
        seen_frames.add(frame)
        source=Image.open(RES/asset['file']).convert('RGBA')
        assert source.width>0 and source.height>0
        assert list(source.size)==asset['size']
        x,y,w,h=asset['textureRect']
        assert x>=2 and y>=2 and x+w+2<=atlas.width and y+h+2<=atlas.height
        assert numbers(plist['frames'][frame]['textureRect'])==[x,y,w,h]
        assert numbers(plist['frames'][frame]['spriteSize'])==[w,h]
        assert numbers(plist['frames'][frame]['spriteSourceSize'])==[w,h]
        assert numbers(plist['frames'][frame]['spriteOffset'])==[0,0]
        assert not plist['frames'][frame]['textureRotated']
        crop=atlas.crop((x,y,x+w,y+h))
        assert crop.tobytes()==source.tobytes(), key
        # Include extrusion and confirm neighbouring frames cannot share pixels.
        rect=(x-2,y-2,x+w+2,y+h+2)
        for other_key,other in rectangles:
            assert min(rect[2],other[2])<=max(rect[0],other[0]) or min(rect[3],other[3])<=max(rect[1],other[1]), (key,other_key)
        rectangles.append((key,rect))
        assert source.getchannel('A').getextrema()[1]>0
        if key.startswith('vehicle.'):
            tx,ty=asset['capacityTextPosition']
            assert 0<tx<w and 0<ty<h
    index=json.loads((RES/'levels/index.json').read_text(encoding='utf-8'))
    checked=[]
    for entry in index['levels']:
        level=json.loads((RES/entry['path']).read_text(encoding='utf-8'))
        rules=json.loads((RES/level['ruleConfig']).read_text(encoding='utf-8'))
        assert level['rulesVersion']==rules['rulesVersion']
        assert level['id']==entry['id']
        assert [rules['presentation']['designWidth'],rules['presentation']['designHeight']]==level['designSize']
        refs={level['backgroundAssetKey'],level['boardAssetKey']}
        refs.update(v['assetKey'] for v in level['vehicles'])
        groups=([ring['slots'] for ring in level['board']['rings']] if level['mode']=='circular'
                else [chain['slots'] for chain in level['board']['chains']])
        for group in groups:
            for slot in group:
                refs.add('fruit.'+slot['fruit']['color'])
                x,y=slot['center']
                w,h=slot['size']
                radius=math.hypot(w,h)/2
                assert 0<=x-radius and x+radius<=600 and 0<=y-radius and y+radius<=900
        for chain in level['board'].get('chains',[]):
            entrance=chain['entrance']
            if entrance:
                refs.add(entrance['assetKey'])
                refs.update('fruit.'+f['color'] for f in entrance['sequence'])
        assert refs <= assets.keys(), refs-assets.keys()
        # Check parked sprite bounds, including the bottom row at y=42.
        by_id={v['id']:v for v in level['vehicles']}
        for ci,col in enumerate(level['parkingColumns']):
            for ri,vid in enumerate(col):
                w,h=assets[by_id[vid]['assetKey']]['size']
                x,y=level['parkingColumnX'][ci],level['parkingRowY'][ri]
                assert x-w/2>=0 and x+w/2<=600 and y-h/2>=0 and y+h/2<=900
        assert len(level['waitingPositions'])==rules['admission']['waitingCapacity']
        checked.append({'id':level['id'],'resolvedAssetKeys':len(refs),'status':'passed'})
    report={'status':'passed','frameCount':len(assets),'textureSize':list(atlas.size),
            'checks':['all source PNGs match atlas pixels exactly','format-3 plist fields match untrimmed source frames',
                      'frame bounds and extrusion do not overlap','all level references resolve',
                      'rules version and presentation match','fruit and parked vehicle bounds stay on screen'],
            'levels':checked,'runtimeAtlasLoad':'separate check: see Validation/latest-verification.json'}
    write_json(ROOT/'Validation/assets_report.json',report)
    print(json.dumps(report,ensure_ascii=False,indent=2))


if __name__=='__main__':
    main()
