"""Validate authored levels and find/replay serial, idle-between-launch solutions.

This is a data model, not the C++ runtime. Contact gaps are checked against the
collect/refill duration, so immediate settled-state transitions are equivalent
for these serial witness inputs. Concurrent traffic remains a runtime test.
"""
from collections import Counter
from pathlib import Path
import hashlib
import json
import math
from prepare_levels import RES, ROOT, COLORS, CAPACITIES, write_json


class Model:
    def __init__(self, level):
        self.level = level
        self.rules = json.loads((RES / level['ruleConfig']).read_text(encoding='utf-8'))
        self.vehicles = level['vehicles']
        self.vi = {v['id']: i for i, v in enumerate(self.vehicles)}
        self.columns = [[self.vi[v] for v in col] for col in level['parkingColumns']]
        self.circular = level['mode'] == 'circular'
        self.groups = ([r['slots'] for r in level['board']['rings']] if self.circular
                       else [c['slots'] for c in level['board']['chains']])
        self.entities = {}
        self.initial = []
        self.group_indices = []
        self.entrances = []
        self.ids = set()
        for group in self.groups:
            indices = []
            for slot in group:
                self.unique(slot['id'])
                fruit = slot['fruit']
                self.unique(fruit['id'])
                self.entities[fruit['id']] = fruit['color']
                indices.append(len(self.initial))
                self.initial.append(fruit['id'])
            self.group_indices.append(indices)
        for chain in ([] if self.circular else level['board']['chains']):
            self.unique(chain['id'])
            entrance = chain['entrance']
            sequence = []
            if entrance:
                self.unique(entrance['id'])
                for fruit in entrance['sequence']:
                    self.unique(fruit['id'])
                    assert fruit['color'] in entrance['candidatePool']
                    self.entities[fruit['id']] = fruit['color']
                    sequence.append(fruit['id'])
            self.entrances.append(sequence)
        for v in self.vehicles:
            self.unique(v['id'])
            assert v['color'] in COLORS
            assert v['capacity'] == CAPACITIES[v['size']]
        queued = [v for col in self.columns for v in col]
        assert len(queued) == len(set(queued)) == len(self.vehicles)
        assert set(queued) == set(range(len(self.vehicles)))
        assert len(self.columns) == self.rules['parking']['queueColumns']
        actual = Counter(self.entities.values())
        capacities = Counter()
        for v in self.vehicles:
            capacities[v['color']] += v['capacity']
        assert actual == capacities == Counter(level['expectedTotals']['byColor'])
        assert len(self.initial) == level['expectedTotals']['initialFruit']
        assert sum(map(len, self.entrances)) == level['expectedTotals']['entranceFruit']
        assert len(self.entities) == level['expectedTotals']['totalFruit']
        self.contacts = ([(s['contactProgress'], i) for i, s in enumerate(self.groups[0])]
                         if self.circular else [(c['contactProgress'], self.group_indices[i][0])
                                                for i, c in enumerate(level['board']['chains'])])
        self.contacts.sort()
        lap = self.rules['vehicleTrip']['lapDurationSeconds']
        progress = sorted(set(p for p,_ in self.contacts))
        self.min_gap = min((b-a)*lap for a,b in zip(progress,progress[1:]))
        self.settle = self.rules['animations']['collectSeconds'] + self.rules['animations']['refillSeconds']
        assert self.min_gap > self.settle, 'Use an event-driven simulator for tighter contact spacing'
        self.check_geometry()

    def unique(self, identifier):
        assert identifier not in self.ids, identifier
        self.ids.add(identifier)

    def check_geometry(self):
        if self.circular:
            assert len(self.groups) == self.rules['circular']['defaultRingCount']
            assert all(len(g) == 16 for g in self.groups)
            # Local squares rotate with their radial/tangential axes. Their outer
            # radial extent must fit before the next ring's inner extent.
            rings = self.level['board']['rings']
            for a,b in zip(rings,rings[1:]):
                assert b['radius']-a['radius'] >= (a['slots'][0]['size'][0]+b['slots'][0]['size'][0])/2
            for ring in rings:
                size = ring['slots'][0]['size'][0]
                assert 2*ring['radius']*math.sin(math.pi/16) > size*math.sqrt(2)
            assert rings[0]['radius']-self.groups[0][0]['size'][0]/2 == self.level['road']['radius']+self.level['road']['width']/2
            return
        boundary = self.level['road']['outerBoundary']
        assert self.level['road']['type']=='open_polyline'
        points=self.level['road']['points']
        assert all(b[1]>a[1] for a,b in zip(points,points[1:]))
        for chain in self.level['board']['chains']:
            anchor, normal = chain['contactAnchor'], chain['outwardNormal']
            assert normal in ([-1,0],[1,0])
            road_edge = anchor[0]
            p=chain['contactProgress']
            lengths=[math.dist(a,b) for a,b in zip(points,points[1:])]
            distance=p*sum(lengths)
            for a,b,length in zip(points,points[1:],lengths):
                if distance<=length:
                    road=[a[i]+(b[i]-a[i])*distance/length for i in range(2)]
                    break
                distance-=length
            assert math.isclose(road[1],anchor[1])
            assert math.isclose(anchor[0]-road[0],normal[0]*76)
            for depth,slot in enumerate(chain['slots']):
                half = slot['size'][0]/2
                near_edge = slot['center'][0] - normal[0]*half
                assert math.isclose(abs(near_edge-road_edge), depth*44)
                assert (abs(near_edge-road_edge) < 0.0001) == (depth == 0)
                assert slot['center'][1]==anchor[1]
                assert boundary['left']<=slot['center'][0]-half
                assert slot['center'][0]+half<=boundary['right']
                # Every square corner stays outside the sloping road corridor.
                for cx in (slot['center'][0]-half,slot['center'][0]+half):
                    for cy in (slot['center'][1]-half,slot['center'][1]+half):
                        for a,b in zip(points,points[1:]):
                            dx,dy=b[0]-a[0],b[1]-a[1]
                            t=max(0,min(1,((cx-a[0])*dx+(cy-a[1])*dy)/(dx*dx+dy*dy)))
                            assert math.hypot(cx-a[0]-t*dx,cy-a[1]-t*dy)>self.level['road']['width']/2

    def start(self):
        return (tuple(self.initial), tuple(0 for _ in self.entrances), tuple(-1 for _ in self.vehicles))

    def available(self, state):
        _,_,loads = state
        active = [i for i,v in enumerate(self.vehicles) if 0 <= loads[i] < v['capacity']]
        options = list(active)
        if len(active) < self.rules['admission']['waitingCapacity']:
            for col in self.columns:
                head = next((i for i in col if loads[i] == -1), None)
                if head is not None:
                    options.append(head)
        return options

    def lap(self, state, vi):
        assert vi in self.available(state), 'Illegal launch'
        board, cursors, loads = map(list,state)
        vehicle = self.vehicles[vi]
        before = max(loads[vi],0)
        loads[vi] = before
        events = []
        for progress,idx in self.contacts:
            fruit = board[idx]
            if not fruit or self.entities[fruit] != vehicle['color']:
                continue
            gi = next(i for i,g in enumerate(self.group_indices) if idx in g)
            candidates=[idx] if self.circular else self.group_indices[gi]
            batch=[]
            for slot in candidates:
                entity=board[slot]
                if not entity or self.entities[entity]!=vehicle['color'] or loads[vi]==vehicle['capacity']:
                    break
                loads[vi]+=1
                board[slot]=''
                batch.append({'progress':progress,'fruit':entity,'slotIndex':slot,'moved':[],'generated':[]})
            moved, generated = [], []
            if self.circular:
                column = self.group_indices[0].index(idx)
                ray = [ring[column] for ring in self.group_indices]
                remaining = [(j, board[j]) for j in ray if board[j]]
                for j in ray:
                    board[j] = ''
                for dest, (src, entity) in zip(ray, remaining):
                    board[dest] = entity
                    if src != dest:
                        moved.append({'id':entity, 'from':src, 'to':dest})
            else:
                group = self.group_indices[gi]
                remaining = [(j,board[j]) for j in group if board[j]]
                for j in group:
                    board[j] = ''
                for dest,(src,entity) in zip(group,remaining):
                    board[dest] = entity
                    if src != dest:
                        moved.append({'id':entity,'from':src,'to':dest})
                for dest in group:
                    if not board[dest] and cursors[gi] < len(self.entrances[gi]):
                        entity = self.entrances[gi][cursors[gi]]
                        cursors[gi] += 1
                        board[dest] = entity
                        generated.append({'id':entity,'to':dest})
            batch[-1]['moved']=moved
            batch[-1]['generated']=generated
            events.extend(batch)
            if loads[vi] == vehicle['capacity']:
                break
        new = (tuple(board),tuple(cursors),tuple(loads))
        return new, events, loads[vi]-before

    def won(self, state):
        board,cursors,loads = state
        return (not any(board) and all(c == len(e) for c,e in zip(cursors,self.entrances))
                and all(load == v['capacity'] for load,v in zip(loads,self.vehicles)))

    def solve(self):
        seen = set()
        def visit(state):
            if self.won(state):
                return []
            if state in seen:
                return None
            seen.add(state)
            choices = []
            for vi in self.available(state):
                next_state,_,picked = self.lap(state,vi)
                if next_state == state:
                    continue
                full = next_state[2][vi] == self.vehicles[vi]['capacity']
                choices.append(((full,picked,state[2][vi]>=0,-vi),vi,next_state))
            choices.sort(reverse=True)
            for _,vi,next_state in choices:
                suffix = visit(next_state)
                if suffix is not None:
                    return [vi]+suffix
            return None
        solution = visit(self.start())
        assert solution is not None, f'No serial solution: {self.level["id"]}'
        return solution, len(seen)

    def replay(self, actions):
        state = self.start()
        steps, collected = [], set()
        slots = {}
        peak, elapsed = 0, 0.0
        for step,vi in enumerate(actions,1):
            previous = state
            is_new = state[2][vi] == -1
            if is_new:
                free = next(s for s in range(5) if s not in slots.values())
                slots[vi] = free
            peak = max(peak,len(slots))
            state,events,picked = self.lap(state,vi)
            for event in events:
                assert event['fruit'] not in collected
                collected.add(event['fruit'])
            full = state[2][vi] == self.vehicles[vi]['capacity']
            board,cursors,loads = state
            pending = [f for i,seq in enumerate(self.entrances) for f in seq[cursors[i]:]]
            present = [f for f in board if f] + pending
            assert len(present) == len(set(present))
            assert not set(present) & collected
            assert set(present) | collected == set(self.entities)
            assert sum(max(load,0) for load in loads) == len(collected)
            assert len(slots) <= 5
            # All transactions finish before the next input. Final-contact refill
            # may outlive the lap; the wait includes it and return/exit animation.
            last = events[-1]['progress']*10 if events else 0
            duration = .25 + (last+.48 if full else max(10.3,last+self.settle))
            item = {
                'step':step,'vehicleId':self.vehicles[vi]['id'],
                'action':'launch_from_parking' if is_new else 'relaunch_from_waiting',
                'waitingSlot':slots[vi]+1,'loadedBefore':max(previous[2][vi],0),
                'collectedThisTrip':picked,'loadedAfter':loads[vi],
                'capacity':self.vehicles[vi]['capacity'],
                'result':'completed_exit' if full else 'returned_to_waiting',
                'waitFor':'road_board_and_parking_idle',
                'earliestNextInputSeconds':round(elapsed+duration,3),
                'remainingTargets':len(present),'events':events,
            }
            steps.append(item)
            elapsed += duration
            if full:
                del slots[vi]
        assert self.won(state) and not slots
        assert len(collected) == len(self.entities)
        return steps, {'status':'passed','collected':len(collected),'launchCount':len(steps),
                       'relaunchCount':sum(s['action']=='relaunch_from_waiting' for s in steps),
                       'peakBoundWaitingSlots':peak,'minContactGapSeconds':round(self.min_gap,4),
                       'collectPlusRefillSeconds':self.settle,'estimatedSeconds':round(elapsed,2)}


def main():
    report = {'scope':'serial data witness; independent C++ scene validation is recorded in latest-verification.json','levels':[]}
    lines = ['# 关卡通关操作（数据模型回放）','',
             '每次点击后，等待车辆回场或驶出、棋盘补位和队列前移全部结束，再执行下一步。',
             '等待位从左到右为1—5；“首次”点击停车场队首，“再发”点击对应等待车。',
             '这是数据模型生成的输入；C++实际场景逐步回放结果见同目录 latest-verification.json。','']
    for path in sorted((RES/'levels').glob('*_01.json')):
        level = json.loads(path.read_text(encoding='utf-8'))
        model = Model(level)
        actions, visited = model.solve()
        steps, result = model.replay(actions)
        solution = {'schemaVersion':1,'levelId':level['id'],
                    'levelSha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                    'scope':report['scope'],'inputPolicy':'one_vehicle_at_a_time_wait_until_all_idle',
                    'validation':result,'steps':steps}
        write_json(path.with_name(level['id']+'_solution.json'),solution)
        report['levels'].append({'id':level['id'],'searchedStates':visited,**result})
        lines += [f'## {level["title"]}（{len(steps)}次发车）','',
                  '| 步骤 | 车辆 | 操作 | 等待位 | 本圈收集 | 装载/容量 | 结果 | 剩余目标 |',
                  '| --- | --- | --- | --- | --- | --- | --- | --- |']
        for s in steps:
            label = '首次' if s['action']=='launch_from_parking' else '再发'
            outcome = '满载驶出' if s['result']=='completed_exit' else '回场'
            lines.append(f'| {s["step"]} | {s["vehicleId"]} | {label} | {s["waitingSlot"]} | {s["collectedThisTrip"]} | {s["loadedAfter"]}/{s["capacity"]} | {outcome} | {s["remainingTargets"]} |')
        lines += ['']
        print(level['id'],result,'searchedStates',visited,flush=True)
    write_json(ROOT/'Validation'/'levels_report.json',report)
    (ROOT/'Validation'/'通关操作.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')


if __name__ == '__main__':
    main()
