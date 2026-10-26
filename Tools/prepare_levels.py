"""Author the two v1 levels. Coordinates use the Cocos bottom-left origin."""
from pathlib import Path
from collections import Counter
import json
import math
import shutil

ROOT = Path(__file__).resolve().parents[1]
RES = ROOT / 'Resources'
COLORS = ['green', 'yellow', 'orange', 'purple']
CAPACITIES = {'small': 4, 'medium': 8, 'large': 12}

def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')

def vehicle(identifier, color, size):
    return {'id': identifier, 'color': color, 'size': size,
            'capacity': CAPACITIES[size], 'assetKey': f'vehicle.{color}.{size}'}

def common(identifier, mode, title):
    return {
        'schemaVersion': 1, 'rulesVersion': '1.3.0', 'id': identifier,
        'mode': mode, 'title': title, 'designSize': [600, 900],
        'ruleConfig': 'config/rules.v1.json',
        'backgroundAssetKey': 'background.farm',
        'boardAssetKey': f'board.{mode}',
        'waitingPositions': [[100+i*100, 300] for i in range(5)],
        'parkingColumnX': [195, 300, 405],
        'parkingRowY': [198, 120, 42],
        'goal': {'type': 'collect_all_fruit_including_entrances'},
    }

def circular():
    level = common('circular_01', 'circular', '环形农场')
    level['tutorial'] = '先发小车，再用回场车辆继续收集外圈。'
    level['road'] = {
        'type': 'circle', 'center': [300, 585], 'radius': 102,
        'width': 38, 'startAngleDegrees': -90,
        'direction': 'clockwise', 'lapDurationSeconds': 10,
    }
    patterns = [
        ['green']*4+['yellow']*4+['orange']*4+['purple']*4,
        ['yellow','green','purple','orange']*4,
        ['purple','orange','green','yellow']*4,
    ]
    rings = []
    for ring, (radius, size) in enumerate(zip([135, 168, 204], [28,32,36])):
        slots = []
        for slot in range(16):
            progress = (slot+0.5)/16
            angle = -math.pi/2-progress*2*math.pi
            slots.append({
                'id': f'c_slot_{ring}_{slot:02d}', 'slotIndex': slot,
                'center': [round(300+radius*math.cos(angle),4), round(585+radius*math.sin(angle),4)],
                'size': [size,size], 'rotationDegrees': round(progress*360,4),
                'contactProgress': progress if ring==0 else None,
                'fruit': {'id': f'c_fruit_{ring}_{slot:02d}', 'color': patterns[ring][slot]},
            })
        rings.append({'layer':ring, 'radius':radius, 'slots':slots})
    level['board'] = {'type':'concentric_rings', 'rings':rings}
    level['vehicles'] = [
        vehicle('c_green_s','green','small'), vehicle('c_green_m','green','medium'),
        vehicle('c_yellow_s','yellow','small'), vehicle('c_yellow_m','yellow','medium'),
        vehicle('c_orange_l','orange','large'), vehicle('c_purple_l','purple','large'),
    ]
    level['parkingColumns'] = [
        ['c_green_s','c_purple_l'], ['c_yellow_s','c_green_m'], ['c_orange_l','c_yellow_m'],
    ]
    level['expectedTotals'] = {'initialFruit':48, 'entranceFruit':0, 'totalFruit':48,
                               'byColor':{color:12 for color in COLORS}}
    return level

def rectangular():
    level = common('rectangular_01','rectangular','折返农场')
    level['tutorial'] = '横向连续收同色，异色阻挡；未满到终点回等待槽。'
    level['road'] = {
        'type':'open_polyline', 'points':[[452,390],[172,590],[452,790]],
        'width':52, 'direction':'forward', 'lapDurationSeconds':10,
        'outerBoundary':{'left':68,'right':532,'bottom':360,'top':810},
    }
    chains = []
    entrance_rows = {(1,'left'),(2,'right'),(5,'right'),(6,'left')}
    for row in range(8):
        y=415+50*row
        x=452-1.4*(y-390) if y<590 else 172+1.4*(y-590)
        # 76 horizontal units clear the sloping 52-wide road over a 44-high tile.
        left_depth=min(6,math.floor((x-76-68)/44))
        colors=[COLORS[(3*row+j//2)%4] for j in range(6)]
        for region, count, offset in [('left',left_depth,0),('right',6-left_depth,left_depth)]:
            if not count:
                continue
            normal=[-1,0] if region=='left' else [1,0]
            anchor=[round(x+76*normal[0],4),y]
            identifier = f'r_row{row}_{region}'
            slots=[]
            for depth in range(count):
                color = colors[offset+depth]
                slots.append({
                    'id':f'{identifier}_slot_{depth}', 'depth':depth,
                    'center':[anchor[0]+normal[0]*(22+44*depth), y],
                    'size':[44,44],
                    'fruit':{'id':f'{identifier}_fruit_{depth}', 'color':color},
                })
            entrance = None
            if (row,region) in entrance_rows:
                entrance = {
                    'id':f'{identifier}_entrance', 'assetKey':'icon.entrance',
                    'position':[anchor[0]+normal[0]*(44*count+24),y],
                    'inwardDirection':[-normal[0],-normal[1]],
                    'candidatePool':COLORS,
                    'generation':'finite_random_bag_without_replacement',
                    'spawnOrigin':'farthest_horizontal_slot',
                    'sequence':[{'id':f'{identifier}_spawn_{i}', 'color':COLORS[(row+i)%4]} for i in range(4)],
                }
            chains.append({
                'id':identifier,'region':region,'slotOrder':'front_to_back',
                'contactAnchor':anchor,'outwardNormal':normal,
                'contactProgress':(y-390)/400, 'slots':slots, 'entrance':entrance,
            })
    level['board']={'type':'direct_contact_chains','randomSeed':20261026,'chains':chains}
    level['vehicles']=[
        vehicle('r_green_s','green','small'), vehicle('r_green_l','green','large'),
        vehicle('r_yellow_s','yellow','small'), vehicle('r_yellow_l','yellow','large'),
        vehicle('r_orange_a','orange','medium'), vehicle('r_orange_b','orange','medium'),
        vehicle('r_purple_a','purple','medium'), vehicle('r_purple_b','purple','medium'),
    ]
    level['parkingColumns']=[
        ['r_green_s','r_orange_a','r_yellow_l'],
        ['r_yellow_s','r_purple_a','r_orange_b'],
        ['r_green_l','r_purple_b'],
    ]
    level['expectedTotals']={'initialFruit':48,'entranceFruit':16,'totalFruit':64,
                              'byColor':{color:16 for color in COLORS}}
    return level

if __name__=='__main__':
    (RES/'config').mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT.parent/'首版规则配置.json',RES/'config'/'rules.v1.json')
    levels=[circular(),rectangular()]
    for level in levels:
        write_json(RES/'levels'/f'{level["id"]}.json',level)
        print(level['id'],level['expectedTotals'],flush=True)
    write_json(RES/'levels'/'index.json',{'schemaVersion':1,'levels':[
        {'id':level['id'],'title':level['title'],'path':f'levels/{level["id"]}.json'} for level in levels]})
