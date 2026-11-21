import unreal, math, random, json, pathlib, traceback
ROOT=pathlib.Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
(ROOT/'Saved/CombatSetup').mkdir(parents=True,exist_ok=True)
LEVEL='/Game/_Game/Levels/L_MossboundFort'
random.seed(21)
report={}
try:
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    # This level is new; the existing parkour map is untouched.
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        levels.load_level(LEVEL)
        report={'level':LEVEL,'existing':True}
        raise StopIteration
    else:
        levels.new_level(LEVEL)
    def material(name,color,rough=.85):
        path='/Game/_Game/Art/CombatTest/Environment/'+name
        if unreal.EditorAssetLibrary.does_asset_exist(path):return unreal.load_asset(path)
        mat=tools.create_asset(name,'/Game/_Game/Art/CombatTest/Environment',unreal.Material,unreal.MaterialFactoryNew())
        lib=unreal.MaterialEditingLibrary
        base=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-400,0)
        base.set_editor_property('constant',unreal.LinearColor(*color,1))
        lib.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
        r=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-400,180)
        r.set_editor_property('r',rough)
        lib.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS)
        lib.recompile_material(mat)
        unreal.EditorAssetLibrary.save_loaded_asset(mat)
        return mat
    stone=[material('M_FortStone_'+str(i),(.15+i*.012,.175+i*.012,.145+i*.012)) for i in range(5)]
    wood=[material('M_WeatheredTimber_'+str(i),(.13+i*.016,.076+i*.010,.038+i*.006)) for i in range(4)]
    moss=material('M_Moss',(.075,.15,.035))
    dark=material('M_ShadowStone',(.055,.065,.05))
    leaf=[material('M_Greenery_'+str(i),(.045+i*.015,.12+i*.02,.025+i*.008)) for i in range(3)]
    iron=material('M_Iron',(.045,.05,.047),.38)
    banner=material('M_OchreBanner',(.36,.19,.065))
    water=material('M_RavineWater',(.035,.09,.075),.24)
    shapes={n:unreal.load_asset('/Engine/BasicShapes/'+n) for n in ('Cube','Cylinder','Cone','Sphere')}
    count=0
    def shape(name,loc,scale,mat,kind='Cube',rot=(0,0,0),folder='Fort'):
        global count
        obj=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc),unreal.Rotator(pitch=rot[0],yaw=rot[1],roll=rot[2]))
        obj.set_actor_label(name)
        obj.set_folder_path(folder)
        comp=obj.static_mesh_component
        comp.set_static_mesh(shapes[kind])
        comp.set_material(0,mat)
        obj.set_actor_scale3d(unreal.Vector(*scale))
        count+=1
        return obj
    shape('Raised stone foundation',(0,0,-115),(20,18,2.3),dark)
    # The center remains an unobstructed combat floor; seams are narrow enough for dash movement.
    for row in range(14):
        for col in range(12):
            x=-715+col*130+(30 if row%2 else 0)
            y=-585+row*90
            shape('Worn courtyard paving',(x,y,-10),(1.285,.885,.20),random.choice(stone),folder='Fort/Flagstones')
    for side in (-1,1):
        for i in range(32):
            shape('Timber fighting deck',(-850+i*55,side*765,-8),(.53,2.8,.16),random.choice(wood),folder='Fort/Timber decks')
        for x in range(-850,1000,230):
            shape('Deck cross beam',(x,side*765,-34),(.22,3.15,.35),wood[0],folder='Fort/Timber decks')
            shape('Timber support',(x,side*855,-240),(.32,.32,4.6),wood[0],folder='Fort/Timber decks')
        # Intentional openings between railing sections expose the ledge for tests.
        for x in (-780,-560,100,320,540,760):
            shape('Railing post',(x,side*900,53),(.18,.18,1.25),wood[1],folder='Fort/Railings')
            for z in (42,92): shape('Railing rail',(x+85,side*900,z),(1.7,.10,.12),wood[1],folder='Fort/Railings')
    # Stone walls and crenellations behind the boss.
    for y in range(-900,1000,150):
        for z in range(50,450,80):
            if abs(y)>210 or z>330:
                shape('Mossed castle brick',(990,y,z),(1.10,1.47,.77),random.choice(stone),folder='Fort/Gatehouse')
        if abs(y)>230: shape('Battlement',(990,y,480),(1.25,.75,.9),stone[1],folder='Fort/Gatehouse')
    for side in (-1,1):
        for x in (780,1080):
            shape('Fort tower',(x,side*850,200),(3.6,3.6,6.5),stone[1],'Cylinder',folder='Fort/Towers')
            shape('Tower timber crown',(x,side*850,550),(4.3,4.3,.25),wood[0],'Cylinder',folder='Fort/Towers')
            for a in range(8):
                ang=a*math.pi/4
                shape('Tower crenel',(x+175*math.cos(ang),side*850+175*math.sin(ang),595),(.72,.72,1.0),stone[2],folder='Fort/Towers')
        shape('Gate oak pillar',(890,side*200,155),(.45,.45,3.4),wood[0],folder='Fort/Gatehouse')
        shape('Faded fort banner',(875,side*295,240),(.035,1.0,2.2),banner,folder='Fort/Gatehouse')
        for j in range(7):
            shape('Moss on wall',(925,side*(400+j*65),random.uniform(50,290)),(.10,.55,.30),moss,'Sphere',folder='Fort/Greenery')
    shape('Gate oak lintel',(890,0,340),(.6,4.55,.55),wood[0],folder='Fort/Gatehouse')
    for j in range(7):
        angle=math.pi*j/6
        shape('Stone gateway arch',(955,205*math.cos(angle),280+155*math.sin(angle)),(1.3,.65,.65),stone[j%5],rot=(0,0,math.degrees(angle)-90),folder='Fort/Gatehouse')
    # Bridge approach over a deep green ravine.
    for i in range(18):shape('Approach bridge plank',(-1050-i*48,0,-8),(.46,3.2,.20),random.choice(wood),folder='Fort/Bridge')
    for side in (-1,1):
        shape('Bridge stringer',(-1450,side*135,-45),(8.8,.25,.55),wood[0],folder='Fort/Bridge')
        for x in range(-1850,-1000,180):
            shape('Bridge post',(x,side*165,55),(.18,.18,1.4),wood[0],folder='Fort/Bridge')
        for z in (50,100):shape('Bridge side rail',(-1440,side*165,z),(9,.12,.12),wood[1],folder='Fort/Bridge')
    shape('Ravine water',(0,0,-660),(180,150,.1),water,folder='Surroundings')
    # Irregular cliffs, stylized tree clusters and creeping moss soften the modular fort.
    for i in range(45):
        x=random.uniform(-3600,3600); y=random.choice((-1,1))*random.uniform(1400,3600)
        height=random.uniform(400,1400)
        shape('Ravine cliff',(x,y,-500+height*.25),(random.uniform(4,10),random.uniform(4,9),height/100),random.choice(stone),'Sphere',rot=(random.uniform(-10,10),random.uniform(0,180),random.uniform(-10,10)),folder='Surroundings/Cliffs')
        if i%2==0:
            top=-500+height*.75
            shape('Forest trunk',(x,y,top+170),(.45,.45,3.8),wood[0],'Cylinder',folder='Surroundings/Forest')
            for j in range(3):
                shape('Forest canopy',(x+random.uniform(-100,100),y+random.uniform(-100,100),top+330+j*70),(3.2,3.0,2.8),random.choice(leaf),'Sphere',folder='Surroundings/Forest')
    for i in range(50):
        x=random.uniform(-900,900); side=random.choice((-1,1)); y=side*random.uniform(650,875)
        shape('Moss tufts at deck edges',(x,y,5),(random.uniform(.4,1.0),random.uniform(.3,.6),.12),moss,'Sphere',folder='Fort/Greenery')
    # Lighting: warm grazing sun, cool ambient fill and light atmospheric haze.
    sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,1500),unreal.Rotator(pitch=-28,yaw=-35,roll=0))
    sun.set_actor_label('Late afternoon sun')
    sun.light_component.set_editor_property('intensity',2.0)
    sun.light_component.set_editor_property('light_color',unreal.Color(255,234,201,255))
    sun.light_component.set_editor_property('atmosphere_sun_light',True)
    sky=actors.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector(0,0,-500))
    skylight=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,1000))
    skylight.light_component.set_editor_property('intensity',1.0)
    skylight.light_component.set_editor_property('real_time_capture',True)
    fog=actors.spawn_actor_from_class(unreal.ExponentialHeightFog,unreal.Vector(0,0,-200))
    fog.component.set_editor_property('fog_density',.018)
    fog.component.set_editor_property('fog_height_falloff',.25)
    for side in (-1,1):
        shape('Gate lantern bracket',(820,side*250,180),(.5,.16,.16),iron,folder='Fort/Lanterns')
        light=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(795,side*250,160))
        light.point_light_component.set_editor_property('intensity',65)
        light.point_light_component.set_editor_property('attenuation_radius',350)
        light.point_light_component.set_editor_property('light_color',unreal.Color(255,168,70,255))
    world=unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,'/Game/_Game/Core/BP_HeroGameMode.BP_HeroGameMode_C'))
    start=actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-240,0,65),unreal.Rotator(0,0,0))
    start.set_actor_label('Combat player start')
    director=actors.spawn_actor_from_class(unreal.load_class(None,'/Script/Parry_Parcour_Game.SwordCombatTest'),unreal.Vector(0,0,0))
    director.set_actor_label('Sword combat test - select to tune traces')
    for who,x in [('Hero',-240),('Boss',180)]:
        obj=actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(x,0,0),unreal.Rotator(pitch=0,yaw=-90 if who=='Hero' else 90,roll=0))
        obj.set_actor_label(who+' editor preview (not spawned in game)')
        obj.set_editor_property('is_editor_only_actor',True)
        obj.set_actor_hidden_in_game(True)
        obj.set_actor_enable_collision(False)
        obj.skeletal_mesh_component.set_skeletal_mesh_asset(unreal.load_asset('/Game/_Game/Art/CombatTest/'+who+'/SK_'+who))
        obj.skeletal_mesh_component.override_animation_data(unreal.load_asset('/Game/_Game/Art/CombatTest/'+who+'/Anim_'+who+'_Idle'),True,True,0,1)
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-1450,-1850,1050),unreal.Rotator(pitch=-24,yaw=48,roll=0))
    levels.save_current_level()
    report={'level':LEVEL,'geometry_actors':count,'director':director.get_path_name(),'height_ratio':'100 cm : 300 cm'}
except StopIteration:
    pass
except Exception:
    report['error']=traceback.format_exc()
(ROOT/'Saved/CombatSetup/fort-report.json').write_text(json.dumps(report,indent=2))
unreal.log('FORT_SETUP '+str(report))
