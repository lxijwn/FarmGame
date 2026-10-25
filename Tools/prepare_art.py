"""Original procedural placeholder art, atlas, and config-driven previews.

All drawing coordinates below use top-left origin; exported level positions
retain Cocos bottom-left origin. No third-party images or font files bundled.
"""
from pathlib import Path
import json
import math
import plistlib
from PIL import Image, ImageDraw, ImageFont
from prepare_levels import ROOT, RES, COLORS, write_json

ART = RES/'art'
SOURCE = ART/'source'
PREVIEW = ROOT/'Previews'
S = 4
PALETTE = {'green':'#66B650','yellow':'#F6CD4A','orange':'#ED8849','purple':'#9B75BD'}
DARK = {'green':'#377541','yellow':'#AE7C20','orange':'#A04E31','purple':'#634C86'}
INK = '#324D42'
CREAM = '#FCF9E9'
assets = {}


class Canvas:
    def __init__(self,w,h,bg=(0,0,0,0)):
        self.size = (w,h)
        self.im = Image.new('RGBA',(w*S,h*S),bg)
        self.d = ImageDraw.Draw(self.im)

    def box(self,xy,fill,outline=None,width=1,radius=0):
        bounds = tuple(round(n*S) for n in xy)
        if radius:
            self.d.rounded_rectangle(bounds,radius=round(radius*S),fill=fill,outline=outline,width=max(1,round(width*S)))
        else:
            self.d.rectangle(bounds,fill=fill,outline=outline,width=max(1,round(width*S)))

    def ellipse(self,xy,fill,outline=None,width=1):
        self.d.ellipse(tuple(round(n*S) for n in xy),fill=fill,outline=outline,width=max(1,round(width*S)))

    def line(self,points,fill,width=1):
        self.d.line([(round(x*S),round(y*S)) for x,y in points],fill=fill,width=max(1,round(width*S)),joint='curve')

    def polygon(self,points,fill):
        self.d.polygon([(round(x*S),round(y*S)) for x,y in points],fill=fill)

    def text(self,xy,text,size=16,fill=INK,anchor='mm',bold=False):
        font_path = Path('C:/Windows/Fonts/msyhbd.ttc' if bold else 'C:/Windows/Fonts/msyh.ttc')
        font = ImageFont.truetype(str(font_path),round(size*S))
        self.d.text((round(xy[0]*S),round(xy[1]*S)),text,font=font,fill=fill,anchor=anchor)

    def finish(self):
        return self.im.resize(self.size,Image.Resampling.LANCZOS)


def save(key,canvas,**extra):
    im = canvas.finish() if isinstance(canvas,Canvas) else canvas
    frame = key.replace('.','_')+'.png'
    im.save(SOURCE/frame)
    assets[key] = {'frame':frame,'file':f'art/source/{frame}',
                   'size':list(im.size),'anchor':[.5,.5],**extra}
    return im


def fruit(color):
    c = Canvas(64,64)
    shade,base = DARK[color],PALETTE[color]
    c.ellipse([7,48,57,61],(36,65,41,40))
    c.box([3,3,61,59],CREAM,outline='#D4DEC0',width=1.2,radius=13)
    c.box([6,6,58,55],base,radius=11)
    if color=='green':  # cabbage, three overlapping leaves
        c.ellipse([14,21,49,51],'#87D86A',shade,1.5)
        c.ellipse([11,16,37,48],'#A7E581',shade,1.5)
        c.ellipse([27,15,53,48],'#9ADD73',shade,1.5)
        c.line([(32,47),(32,25)],shade,1.7)
        c.line([(32,36),(23,28)],shade,1.4)
        c.line([(32,40),(42,30)],shade,1.4)
    elif color=='yellow':  # corn cob
        c.polygon([(30,49),(13,35),(13,20),(27,29)],'#5C9F48')
        c.polygon([(35,49),(53,33),(52,21),(38,29)],'#73B84B')
        c.ellipse([22,12,43,50],'#FFF095',shade,1.3)
        for x in [27,33,39]:
            for y in [21,28,35,42]:
                c.ellipse([x-1.5,y-2,x+1.5,y+2],'#F0BE36')
    elif color=='orange':  # carrot with leafy crown
        c.polygon([(20,24),(42,19),(39,36),(25,51),(23,41)],'#FFBE73')
        c.line([(20,24),(23,41),(25,51),(39,36),(42,19),(20,24)],shade,1.5)
        c.line([(26,29),(33,27)],'#D47B35',1.7)
        c.line([(27,37),(33,35)],'#D47B35',1.7)
        c.line([(32,21),(28,12)],'#4E9145',4)
        c.line([(34,20),(39,10)],'#5EAC4C',4)
        c.line([(37,20),(47,15)],'#74BB58',4)
    else:  # grape bunch
        c.line([(33,19),(38,10)],'#457E3E',3)
        c.ellipse([17,10,34,20],'#79BA57')
        for x,y in [(24,25),(39,25),(18,35),(32,35),(45,35),(26,45),(38,45),(32,52)]:
            c.ellipse([x-6.3,y-6.3,x+6.3,y+6.3],'#C6A4E1',shade,1.1)
            c.ellipse([x-3,y-3.8,x-.5,y-1.3],'#E7D5F5')
    save(f'fruit.{color}',c,semantic={'green':'cabbage','yellow':'corn','orange':'carrot','purple':'grapes'}[color])


def vehicle(color,size):
    w,h = {'small':(54,64),'medium':(60,72),'large':(66,80)}[size]
    c=Canvas(w,h)
    cx=w/2
    c.ellipse([4,h-15,w-4,h-2],(30,50,40,50))
    for x in [4,w-12]:
        for y in [13,h-23]:
            c.box([x,y,x+8,y+13],'#34483F',radius=3)
            c.line([(x+2,y+3),(x+2,y+9)],'#789083',1)
    c.box([8,6,w-8,h-7],DARK[color],radius=9)
    c.box([9,4,w-9,h-10],PALETTE[color],outline='#FCF6DC',width=1.5,radius=8)
    c.box([14,8,w-14,24],'#B6E0D9',outline=DARK[color],width=1.5,radius=4)
    c.line([(17,11),(w-18,11)],'#E5F4EF',2)
    c.box([14,27,w-14,h-15],DARK[color],radius=4)
    c.box([17,30,w-17,h-18],'#FAF2D5',radius=3)
    # Direction arrow on the roof. Capacity digits are drawn at runtime.
    c.polygon([(cx,15),(cx-4,20),(cx+4,20)],'#FCF8E5')
    for x in [12,w-17]:
        c.box([x,5,x+5,8],'#FFF1AA',radius=1)
    stripes = {'small':1,'medium':2,'large':3}[size]
    for i in range(stripes):
        x = cx+(i-(stripes-1)/2)*5
        c.line([(x,h-13),(x,h-10)],'#FFEDBB',2)
    save(f'vehicle.{color}.{size}',c,
         facing='up',capacityTextPosition=[w/2,h-(30+h-18)/2],capacityTextOrigin='bottom_left',
         capacityTextFontSize=16,collisionSize=[w-16,h-12])


def background():
    c=Canvas(600,900,'#E8EFCE')
    for y in range(900):
        t=y/900
        c.line([(0,y),(600,y)],(round(235-11*t),round(242-13*t),round(214-22*t),255))
    for x,y in [(28,168),(555,180),(26,493),(571,546),(52,846),(550,840),(48,674),(540,712)]:
        c.line([(x,y+7),(x-3,y-2)],'#C2D29C',2)
        c.line([(x,y+7),(x+4,y)],'#C2D29C',2)
    # Lower yard and queue guides are decorative, not movement boundaries.
    c.box([34,640,566,889],'#D9D3AB',outline='#BFC392',width=2,radius=26)
    for x in [195,300,405]:
        c.box([x-43,664,x+43,884],'#EAE4BE',radius=17)
    c.line([(58,643),(542,643)],'#F8F4D7',3)
    save('background.farm',c)


def road_circle():
    c=Canvas(260,260)
    c.ellipse([6,9,254,257],(50,77,47,38))
    c.ellipse([9,9,251,251],'#BDBE9C')
    c.ellipse([11,11,249,249],'#697C72')
    c.ellipse([47,47,213,213],'#D4E1B5')
    c.ellipse([49,49,211,211],'#DDE9C1')
    for a in range(0,360,18):
        points=[]
        for da in [0,1,2,3,4,5,6]:
            r=math.radians(a+da)
            points.append((130+102*math.cos(r),130+102*math.sin(r)))
        c.line(points,'#E8E9CE',2)
    c.ellipse([84,86,176,178],'#CBDCA8')
    c.line([(107,116),(117,103),(130,119),(144,102),(155,115)],'#9CBA78',3)
    c.box([94,126,166,149],'#F8F1D5',radius=5)
    c.text((130,138),'FARM',15,bold=True)
    save('board.circular',c,worldCenter=[300,585],roadOnly=True)


def road_rect():
    c=Canvas(500,460)
    c.box([8,3,492,457],'#97B56D',outline='#739453',width=3,radius=20)
    c.box([16,8,484,451],'#EFE4BA',radius=14)
    # Image coordinates invert world Y. Open start -> bend -> finish.
    points=[(402,425),(122,225),(402,25)]
    c.line(points,'#B6A77C',58)
    c.line(points,'#FFF5D8',52)
    for a,b in zip(points,points[1:]):
        dx,dy=b[0]-a[0],b[1]-a[1]
        length=math.hypot(dx,dy);ux,uy=dx/length,dy/length
        for t in [.18,.4,.62,.84]:
            x,y=a[0]+dx*t,a[1]+dy*t
            c.line([(x-ux*5-uy*4,y-uy*5+ux*4),(x+ux*4,y+uy*4),
                    (x-ux*5+uy*4,y-uy*5-ux*4)],'#CCB987',2.5)
    for x,y,text in [(402,425,'入口'),(402,25,'终点')]:
        c.box([x-24,y-13,x+24,y+13],'#608952',radius=8)
        c.text((x,y),text,13,fill=CREAM,bold=True)
    save('board.rectangular',c,worldCenter=[300,585],roadOnly=True)


def misc():
    for state,base,line in [('free','#E7EBD2','#A6B88E'),('reserved','#F2E8C6','#C7AC65'),('docked','#D5E5B6','#83A564')]:
        c=Canvas(80,76)
        c.box([2,3,78,73],base,outline=line,width=2,radius=13)
        c.box([10,10,70,66],None,outline=line,width=1,radius=8)
        if state=='reserved':
            c.ellipse([31,28,49,46],None,line,2)
            c.line([(40,32),(40,38),(44,40)],line,2)
        save('waiting.'+state,c)
    c=Canvas(30,30)
    c.ellipse([1,1,29,29],'#FEF5CF','#C2AC70',1.5)
    c.line([(15,22),(15,9)],INK,3)
    c.line([(9,15),(15,9),(21,15)],INK,3)
    save('icon.entrance',c,facing='up')
    for name in ['pause','restart','back','play']:
        c=Canvas(48,48)
        c.box([2,3,46,47],'#C5CEA2',radius=13)
        c.box([2,1,46,44],CREAM,'#B9C59C',1.5,radius=13)
        if name=='pause':
            for x in [17,27]:
                c.box([x,14,x+5,31],INK,radius=2)
        elif name=='back':
            c.line([(29,13),(18,23),(29,33)],INK,3)
        elif name=='play':
            c.polygon([(18,12),(34,23),(18,34)],INK)
        else:
            points=[(24+10*math.cos(math.radians(a)),23+10*math.sin(math.radians(a))) for a in range(-75,231,5)]
            c.line(points,INK,3)
            c.polygon([(13,18),(13,29),(22,25)],INK)
        save('icon.'+name,c)
    c=Canvas(240,56)
    c.box([2,5,238,55],'#6D933D',radius=15)
    c.box([2,1,238,51],'#8AB653','#C4D29A',1.5,radius=15)
    save('button.primary',c,labelBaked=False)
    c=Canvas(480,300)
    c.box([3,7,477,299],(52,72,40,50),radius=24)
    c.box([3,2,477,292],CREAM,'#C2CDA1',2,radius=24)
    save('panel.dialog',c)
    c=Canvas(32,32)
    pts=[]
    for i in range(8):
        a=math.pi*i/4
        r=14 if i%2==0 else 4
        pts.append((16+math.cos(a)*r,16+math.sin(a)*r))
    c.polygon(pts,'#FFF3BC')
    save('fx.sparkle',c)
    c=Canvas(48,48)
    c.ellipse([3,3,45,45],None,'#F9E79A',3)
    save('fx.collect_ring',c)
    c=Canvas(52,52)
    c.ellipse([2,2,50,50],'#8EB852','#FCF3D6',2)
    c.line([(14,26),(22,34),(38,17)],CREAM,4)
    save('icon.success',c)
    c=Canvas(52,52)
    c.ellipse([2,2,50,50],'#E2A85B','#FCF3D6',2)
    c.line([(26,13),(26,29)],CREAM,4)
    c.ellipse([24,35,28,39],CREAM)
    save('icon.blocked',c)


def pack():
    padding,extrude,width=4,2,2048
    ordered=sorted(assets.items(),key=lambda kv:(-kv[1]['size'][1],kv[0]))
    placements={}
    x=y=row_h=0
    for key,entry in ordered:
        w,h=entry['size']
        rw,rh=w+padding*2,h+padding*2
        if x+rw>width:
            y+=row_h
            x=row_h=0
        placements[key]=(x+padding,y+padding,w,h)
        x+=rw
        row_h=max(row_h,rh)
    required=y+row_h
    height=2**math.ceil(math.log2(required))
    assert height<=2048
    atlas=Image.new('RGBA',(width,height))
    frames={}
    def pair(a,b): return '{'+f'{a},{b}'+'}'
    for key,entry in assets.items():
        px,py,w,h=placements[key]
        im=Image.open(RES/entry['file']).convert('RGBA')
        atlas.paste(im,(px,py))
        # Extrude all edges and corners; leave two transparent pixels before
        # neighbouring allocations. PNG is straight alpha, premultiply on load.
        atlas.paste(im.crop((0,0,w,1)).resize((w,extrude)),(px,py-extrude))
        atlas.paste(im.crop((0,h-1,w,h)).resize((w,extrude)),(px,py+h))
        atlas.paste(im.crop((0,0,1,h)).resize((extrude,h)),(px-extrude,py))
        atlas.paste(im.crop((w-1,0,w,h)).resize((extrude,h)),(px+w,py))
        for dx,dy,sx,sy in [(-extrude,-extrude,0,0),(w,-extrude,w-1,0),(-extrude,h,0,h-1),(w,h,w-1,h-1)]:
            atlas.paste(im.getpixel((sx,sy)),(px+dx,py+dy,px+dx+extrude,py+dy+extrude))
        frames[entry['frame']]={'spriteOffset':pair(0,0),'spriteSize':pair(w,h),
            'spriteSourceSize':pair(w,h),'textureRect':'{'+pair(px,py)+','+pair(w,h)+'}',
            'textureRotated':False,'aliases':[]}
        entry['textureRect']=[px,py,w,h]
        entry['atlas']='art/farm_atlas.plist'
    atlas.save(ART/'farm_atlas.png')
    plist={'frames':frames,'metadata':{'format':3,'pixelFormat':'RGBA8888',
        'premultiplyAlpha':False,'realTextureFileName':'farm_atlas.png',
        'textureFileName':'farm_atlas.png','size':pair(width,height)}}
    (ART/'farm_atlas.plist').write_bytes(plistlib.dumps(plist,sort_keys=False))
    write_json(ART/'asset_manifest.json',{'schemaVersion':1,'style':'original_procedural_placeholder',
        'atlas':'art/farm_atlas.plist','texture':'art/farm_atlas.png','textureSize':[width,height],
        'frameCoordinates':'top_left_pixels','worldCoordinates':'bottom_left_design_units',
        'defaultSampling':'linear','extrusionPixels':extrude,'allocationPaddingPixels':padding,
        'fontPolicy':'Use system font at runtime; no font file is distributed.',
        'vehicleNumber':'capacity-loaded-reserved (runtime text)',
        'assets':assets})


def paste_world(base,key,position,size=None,rotation=0):
    im=Image.open(RES/assets[key]['file']).convert('RGBA')
    if size:
        im=im.resize(tuple(size),Image.Resampling.LANCZOS)
    if rotation:
        # Pillow positive CCW is the opposite of Cocos clockwise rotation.
        im=im.rotate(-rotation,resample=Image.Resampling.BICUBIC,expand=True)
    x,y=position
    base.alpha_composite(im,(round(x-im.width/2),round(base.height-y-im.height/2)))


def overlay_text(im,xy,label,size=16,fill=INK,bold=False,anchor='mm'):
    c=Canvas(*im.size)
    c.text(xy,label,size,fill,anchor,bold)
    im.alpha_composite(c.finish())


def preview(level):
    im=Image.open(RES/assets[level['backgroundAssetKey']]['file']).convert('RGBA')
    board=assets[level['boardAssetKey']]
    paste_world(im,level['boardAssetKey'],board['worldCenter'])
    if level['mode']=='circular':
        for ring in reversed(level['board']['rings']):
            for slot in ring['slots']:
                paste_world(im,'fruit.'+slot['fruit']['color'],slot['center'],slot['size'],slot['rotationDegrees'])
    else:
        angles={(0,1):0,(1,0):90,(0,-1):180,(-1,0):270}
        for chain in level['board']['chains']:
            for slot in reversed(chain['slots']):
                paste_world(im,'fruit.'+slot['fruit']['color'],slot['center'],slot['size'])
            if chain['entrance']:
                entrance=chain['entrance']
                paste_world(im,entrance['assetKey'],entrance['position'],rotation=angles[tuple(entrance['inwardDirection'])])
    for i,p in enumerate(level['waitingPositions']):
        paste_world(im,'waiting.free',p)
        overlay_text(im,(p[0],900-p[1]),str(i+1),18,'#A6B88E')
    vs={v['id']:v for v in level['vehicles']}
    for ci,col in enumerate(level['parkingColumns']):
        for ri,vid in reversed(list(enumerate(col))):
            v=vs[vid]
            x,y=level['parkingColumnX'][ci],level['parkingRowY'][ri]
            paste_world(im,v['assetKey'],(x,y))
            entry=assets[v['assetKey']]
            tx,ty=entry['capacityTextPosition']
            overlay_text(im,(x+tx-entry['size'][0]/2,900-y-(ty-entry['size'][1]/2)),str(v['capacity']),16,bold=True)
    # HUD preview uses runtime-style labels; none of these strings are baked in sprites.
    c=Canvas(600,900)
    c.box([25,18,575,85],CREAM,'#C5CFA6',1.5,radius=18)
    c.text((47,42),level['title'],23,anchor='lm',bold=True)
    c.text((47,67),f'目标  {level["expectedTotals"]["totalFruit"]}    道路  0 / 5',13,anchor='lm')
    if level['mode']=='circular':
        c.text((300,351),'点击队首发车',12)
        c.text((300,370),'未满回场 · 点击再次发车',10,'#7E8E64')
    else:
        c.text((300,552),'横向收同色 · 异色阻挡 · 未满到终点回槽',11,'#7E8E64')
    c.text((300,651),'停车场 · 每列队首可点击',14)
    im.alpha_composite(c.finish())
    paste_world(im,'icon.restart',(488,849))
    paste_world(im,'icon.pause',(545,849))
    im.save(PREVIEW/(level['id']+'.png'))
    return im


def contact_sheet():
    keys=[key for key in assets if not key.startswith(('board.','background.'))]
    c=Canvas(960,math.ceil(len(keys)/6)*142+92,'#EDF1DD')
    c.text((30,33),'FARM / 原创首版资源',23,anchor='lm',bold=True)
    c.text((30,65),'四色果蔬 · 三种规格车辆 · 等待位 · 操作按钮与反馈',14,anchor='lm')
    im=c.finish()
    for i,key in enumerate(keys):
        col,row=i%6,i//6
        x,y=80+160*col,132+142*row
        source=Image.open(RES/assets[key]['file']).convert('RGBA')
        source.thumbnail((140,86),Image.Resampling.LANCZOS)
        im.alpha_composite(source,(round(x-source.width/2),round(y-source.height/2)))
        overlay_text(im,(x,y+57),key,11)
    im.save(PREVIEW/'资源总览.png')


def main():
    SOURCE.mkdir(parents=True,exist_ok=True)
    PREVIEW.mkdir(parents=True,exist_ok=True)
    background()
    road_circle()
    road_rect()
    for color in COLORS:
        fruit(color)
        for size in ['small','medium','large']:
            vehicle(color,size)
    misc()
    pack()
    previews=[]
    for path in sorted((RES/'levels').glob('*_01.json')):
        previews.append(preview(json.loads(path.read_text(encoding='utf-8'))))
    comparison=Image.new('RGB',(1236,924),'#C5D3AA')
    for i,im in enumerate(previews):
        comparison.paste(im,(12+i*612,12),im)
    comparison.save(PREVIEW/'两关预览.png')
    contact_sheet()
    print(f'Created {len(assets)} frames, atlas, two config-driven previews and contact sheet.')


if __name__=='__main__':
    main()
