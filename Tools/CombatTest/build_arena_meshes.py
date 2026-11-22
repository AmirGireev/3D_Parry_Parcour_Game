"""Blender source generator for the approved Mossbound arena v2.

Run in Blender background. Exports one editable static mesh per landmark.
Coordinates are centimetres, Unreal north +X and east +Y. Standard FBX scene
conversion plus export handedness compensation retain these axes in Unreal.
"""
import bpy
import math
import random
import json
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'test-assets' / 'mossbound-arena'
OUT.mkdir(parents=True, exist_ok=True)
random.seed(41)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = .01

COLORS = {
    'Slate': (.115, .151, .17, 1),
    'StoneEdge': (.18, .215, .225, 1),
    'Floor': (.16, .19, .215, 1),
    'FloorAlt': (.145, .17, .19, 1),
    'Wood': (.105, .078, .065, 1),
    'WoodEdge': (.17, .125, .09, 1),
    'Moss': (.18, .225, .105, 1),
    'Burgundy': (.155, .035, .045, 1),
    'Iron': (.045, .055, .062, 1),
    'Bark': (.095, .083, .075, 1),
    'Cliff': (.09, .125, .14, 1),
}
MATS = {}
for name, color in COLORS.items():
    m = bpy.data.materials.new(name)
    m.diffuse_color = color
    MATS[name] = m

GROUPS = {}
group = None

def register(obj, mat, bevel=0):
    obj.data.materials.append(MATS[mat])
    if bevel:
        mod = obj.modifiers.new('Soft sculptural edges', 'BEVEL')
        mod.width = bevel
        mod.segments = 3
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
        norm = obj.modifiers.new('Weighted broad normals', 'WEIGHTED_NORMAL')
        norm.keep_sharp = True
        bpy.ops.object.modifier_apply(modifier=norm.name)
    GROUPS.setdefault(group, []).append(obj)
    return obj

def box(name, loc, size, mat='Slate', bevel=6, yaw=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.object
    o.name = name
    o.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    o.rotation_euler.z = math.radians(yaw)
    return register(o, mat, bevel)

def mesh(name, verts, faces, mat='Slate', bevel=0):
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    o = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(o)
    bpy.context.view_layer.objects.active = o
    o.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    return register(o, mat, bevel)

def prism(name, points, bottom, top, mat='Floor', bevel=0):
    n = len(points)
    v = [(x,y,bottom) for x,y in points] + [(x,y,top) for x,y in points]
    faces = [tuple(reversed(range(n))), tuple(range(n,2*n))]
    faces += [(i, (i+1)%n, (i+1)%n+n, i+n) for i in range(n)]
    return mesh(name, v, faces, mat, bevel)

def xy(r, a):
    a = math.radians(a)
    return (r*math.cos(a), r*math.sin(a))

def sector(name, inner, outer, a, b, bottom, top, mat, bevel=0):
    steps = max(2, int(abs(b-a)/4))
    angles = [a+(b-a)*i/steps for i in range(steps+1)]
    pts = [xy(outer,t) for t in angles]
    pts += [xy(inner,t) for t in reversed(angles)] if inner else [(0,0)]
    return prism(name, pts, bottom, top, mat, bevel)

def beam(name, a, b, width, mat='Wood', width2=None):
    d = Vector(b)-Vector(a)
    o = box(name, (Vector(a)+Vector(b))/2, (width,width2 or width,d.length),mat, min(3,width*.15))
    o.rotation_euler = d.to_track_quat('Z','Y').to_euler()
    return o

def branch(name, points, radii, mat='Bark'):
    for i in range(len(points)-1):
        a,b = Vector(points[i]),Vector(points[i+1])
        bpy.ops.mesh.primitive_cone_add(vertices=10, radius1=radii[i],radius2=radii[i+1],depth=(b-a).length,location=(a+b)/2)
        o=bpy.context.object;o.name=name
        o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler()
        for p in o.data.polygons:p.use_smooth=True
        register(o,mat)

def arch(name, center, inner=125, thickness=48, spring=235, depth=70, yaw=0, broken=False):
    # Local X is wall depth; local Y horizontal span; Z vertical.
    c=Vector(center); ang=math.radians(yaw)
    def transform(p):
        x,y,z=p
        return (c.x+x*math.cos(ang)-y*math.sin(ang), c.y+x*math.sin(ang)+y*math.cos(ang), c.z+z)
    for side in (-1,1):
        local=(0,side*(inner+thickness/2),spring/2)
        box(name+' pier',transform(local),(depth,thickness,spring),'Slate',8,yaw)
    for i in range(11):
        if broken and i in (4,5,6):continue
        a=math.pi*i/11+.008;b=math.pi*(i+1)/11-.008
        pts=[(inner*math.cos(a),spring+inner*math.sin(a)),((inner+thickness)*math.cos(a),spring+(inner+thickness)*math.sin(a)),((inner+thickness)*math.cos(b),spring+(inner+thickness)*math.sin(b)),(inner*math.cos(b),spring+inner*math.sin(b))]
        v=[transform((x,y,z)) for x in (-depth/2,depth/2) for y,z in pts]
        mesh(name+' arch segment',v,[(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],'StoneEdge',3)

group='Court'
sector('Continuous collision foundation',0,1000,0,360,-85,-.5,'Slate')
sector('Centre medallion',0,215,0,360,-20,0,'Floor',1)
for i in range(8):
    sector('Broad radial slab',218,699,i*45+.12,(i+1)*45-.12,-20,0,'Floor' if i%2 else 'FloorAlt',1)
for i in range(16):
    sector('Perimeter paving',702,998,i*22.5+.1,(i+1)*22.5-.1,-20,0,'FloorAlt',1)

group='Foundation'
for i in range(16):
    sector('Thick ancient plinth',0,998,i*22.5,(i+1)*22.5,-260,-80,'Slate',8)
    a=i*22.5+11.25;x,y=xy(965,a)
    box('Foundation buttress',(x,y,-200),(110,115,340),'Slate',12,a)

group='NorthGate'
for side in (-1,1):
    box('Gatehouse mass',(850,side*225,260),(300,150,520),'Slate',14)
    box('Pier foot',(730,side*222,35),(80,165,70),'StoneEdge',8)
    box('Tower crown',(850,side*225,662),(310,165,76),'StoneEdge',12)
box('Upper gatehouse',(850,0,600),(300,600,160),'Slate',12)
arch('Main gateway',(850,0,0),150,70,300,300)
for y in (-270,-90,90,270):box('Crenellation',(850,y,730),(300,85,65),'Slate',9)
for y in range(-125,126,42):
    box('Raised iron gate tooth',(720,y,430),(14,12,95),'Iron',2)
for z in (437,471):box('Portcullis crossbar',(720,0,z),(14,290,12),'Iron',2)

group='Banners'
def banner(name, origin, width, height, yaw=0):
    # Thick two-sided cloth with a simple torn hem, no simulation dependency.
    x,y,z=origin
    pts=[(-width/2,0),(width/2,0),(width/2,-height*.77),(width*.28,-height*.70),(width*.25,-height*.94),(width*.06,-height*.83),(-width*.08,-height),(-width*.2,-height*.81),(-width*.36,-height*.92),(-width/2,-height*.76)]
    a=math.radians(yaw)
    verts=[]
    for d in (-1,1):
        for u,v in pts:
            verts.append((x+d*math.cos(a)-u*math.sin(a), y+d*math.sin(a)+u*math.cos(a), z+v))
    n=len(pts)
    faces=[tuple(reversed(range(n))),tuple(range(n,n*2))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh(name,verts,faces,'Burgundy')
banner('Gate banner',(687,0,715),110,205)

group='WestBalcony'
for j in range(42):
    a=-114+j*2;b=a+1.9
    sector('Balcony curved plank',700,950,a,b,180,200,'WoodEdge',1)
for a in range(-114,-29,14):
    for r in (725,930):
        x,y=xy(r,a);box('Balcony support',(x,y,86),(24,24,185),'Wood',3,a)
    x,y=xy(715,a);box('Inner rail post',(x,y,252),(14,14,130),'WoodEdge',2,a)
    x,y=xy(932,a);box('Outer rail post',(x,y,252),(16,16,130),'Wood',2,a)
    x0,y0=xy(725,a);x1,y1=xy(835,a+7)
    beam('Balcony diagonal brace',(x0,y0,65),(x1,y1,179),16)
for j in range(14):
    a=-114+j*6;b=a+6
    for r in (715,932):
        for z in (255,305):beam('Curved balcony rail',(*xy(r,a),z),(*xy(r,b),z),11,'WoodEdge')
for a in (-100,-62):
    x,y=xy(698,a);banner('Balcony cloth',(x,y,285),80,138,a)

# Staircase hugs southwest perimeter, bottom at x=-694 and top at -334.
# y=-735 and width150 fit outside clear radius700. Landing joins balcony.
group='WestStairs'
for i in range(12):
    h=(i+1)*200/12
    box('Balcony stair %02d'%i,(-679+i*30,-735,h/2),(30.5,150,h),'WoodEdge',1.5)
box('Upper landing',(-305,-735,190),(100,150,20),'WoodEdge',2)
for side in (-1,1):
    y=-735+side*69
    for i in (0,4,8,11):
        x=-679+i*30;h=(i+1)*200/12
        box('Stair rail post',(x,y,h+52),(12,12,105),'Wood',2)
    beam('Stair handrail',(-694,y,110),(-334,y,310),10,'WoodEdge')

group='EastWalkway'
for j in range(30):sector('Court level east timber',800,950,60+j*2,61.9+j*2,-18,0,'WoodEdge',1)
for a in range(60,121,15):
    x,y=xy(948,a);box('East rail post',(x,y,53),(14,14,106),'Wood',2,a)
for a in range(60,120,5):
    for z in (55,100):beam('East rail',(*xy(948,a),z),(*xy(948,a+5),z),10,'WoodEdge')

group='RuinedArcades'
for a in (-47,-24,27,49,73,100):
    x,y=xy(985,a)
    arch('Ruined outer arcade',(x,y,0),108,42,235,65,a,broken=a in (-47,100))
for a in (-115,-93,-72,132,178,-164):
    x,y=xy(980,a);h=random.uniform(55,115)
    box('Broken perimeter wall',(x,y,h/2),(55,190,h),'Slate',10,a)

group='Bridge'
u=Vector((*xy(1,150),0));v=Vector((-u.y,u.x,0))
def bridgepos(s,w,z):return u*s+v*w+Vector((0,0,z))
for j in range(8):
    box('Bridge flagstone',bridgepos(1030+j*100,0,-20),(101,300,40),'FloorAlt',4,150)
for side in (-1,1):
    for j in (0,1,3,4,6,7):
        box('Broken bridge parapet',bridgepos(1030+j*100,side*137,42),(85,26,84),'StoneEdge',6,150)
# Extruded arch spine below bridge, retaining an actual open arch.
for i in range(16):
    a=math.pi*i/16;b=math.pi*(i+1)/16
    pts=[(330*math.cos(a),-390+330*math.sin(a)),(390*math.cos(a),-390+390*math.sin(a)),(390*math.cos(b),-390+390*math.sin(b)),(330*math.cos(b),-390+330*math.sin(b))]
    vv=[bridgepos(1380+s,w,z) for w in (-125,125) for s,z in pts]
    mesh('Bridge arch voussoir',vv,[(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],'Slate',3)
for s in (1030,1730):box('Bridge pier',bridgepos(s,0,-350),(100,280,600),'Slate',10,150)

group='SouthwestTree'
base=Vector((-830,-510,0))
def tp(points):return [base+Vector(p) for p in points]
branch('Ancient bent trunk',tp([(0,0,-30),(10,-4,130),(-25,20,285),(-10,45,440),(-80,65,570)]),[65,52,38,25,8])
branch('Tree reaching arm',tp([(-25,20,280),(60,15,370),(150,30,400),(245,15,440)]),[34,24,15,2])
branch('Tree upper fork',tp([(-10,45,435),(55,45,500),(110,15,585),(100,30,650)]),[23,17,9,1])
branch('Bare left branch',tp([(-25,20,280),(-130,0,350),(-205,-20,430),(-240,-10,455)]),[26,20,9,1])
for i in range(8):
    a=i*45;r=random.uniform(100,180)
    dx,dy=xy(r,a)
    branch('Broad exposed root',tp([(0,0,25),(dx*.5,dy*.5,16),(dx,dy,3)]),[22,14,1])
for a in (-140,-45,10,110):
    d=Vector((*xy(1,a),0));start=base+Vector((110,20,460))
    branch('Sparse fine twig',[start,start+d*70+Vector((0,0,60)),start+d*100+Vector((0,0,110))],[8,4,0.5])
arch('Southwest broken entrance',(-810,-380,0),95,42,205,70,-140,True)

group='Moss'
for i in range(65):
    a=random.uniform(-180,180);r=random.uniform(880,1005)
    x,y=xy(r,a)
    if 138<a<162 or (abs(a)<19):continue
    bpy.ops.mesh.primitive_uv_sphere_add(segments=10,ring_count=6,location=(x,y,2))
    o=bpy.context.object;o.name='Broad quiet moss patch';o.scale=(random.uniform(25,65),random.uniform(22,55),random.uniform(3,7))
    for p in o.data.polygons:p.use_smooth=True
    register(o,'Moss')
for side in (-1,1):
    box('Gate crown moss',(850,side*225,705),(280,150,12),'Moss',5)

group='Ravine'
for i in range(26):
    a=i*360/26;r=random.uniform(3400,6500);x,y=xy(r,a)
    # Bridge approach receives a landing cliff, never cliff obstacles on court.
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=1,location=(x,y,-800))
    o=bpy.context.object;o.name='Soft distant cliff';o.scale=(random.uniform(350,650),random.uniform(350,650),random.uniform(550,1000))
    register(o,'Cliff')
    if i%2==0:
        z=-800+o.scale.z*.7
        branch('Distant bare tree',[(x,y,z),(x+25,y,z+330),(x-30,y,z+620)],[25,16,1])
        branch('Distant fork',[(x+20,y,z+300),(x+115,y+20,z+450),(x+155,y+10,z+540)],[14,7,1])
group='BridgeLanding'
box('Bridge landing rock',bridgepos(1940,0,-210),(380,520,420),'Cliff',35,150)

manifest={'version':2,'units':'centimetres','north':'+X','east':'+Y','court_diameter_cm':2000,'clear_combat_diameter_cm':1400,'groups':[],'materials':COLORS}
for name, objects in GROUPS.items():
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.object.join()
    o=bpy.context.object;o.name='SM_MB_'+name
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    bpy.context.scene.cursor.location=(0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    # Mirror Y explicitly to compensate the FBX scene handedness conversion.
    # Import uses force_front_x_axis=False; UE's Y flip restores authored +Y.
    o.scale.y=-1
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.normals_make_consistent(inside=False);bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.export_scene.fbx(filepath=str(OUT/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',global_scale=1,apply_unit_scale=True,bake_anim=False,use_mesh_modifiers=True,mesh_smooth_type='FACE')
    manifest['groups'].append({'name':name,'mesh':o.name,'collision':name not in ('Moss','Banners','Ravine','SouthwestTree'),'vertices':len(o.data.vertices),'material_slots':[m.name for m in o.data.materials]})
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'mossbound-arena-v2.blend'))
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2))
print('ARENA_SOURCE_READY',len(GROUPS),'landmarks',flush=True)
