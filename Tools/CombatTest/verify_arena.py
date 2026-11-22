"""Geometry and runtime evidence for the new arena. Run inside Unreal Python."""
import unreal,json,pathlib,math
ROOT=pathlib.Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
world=worlds[0] if worlds else unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ignore=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Pawn)
report={'world':world.get_path_name(),'pie':bool(worlds),'checks':{},'samples':{}}
def vec(v):return [float(v.x),float(v.y),float(v.z)]
def trace(a,b):
    hit=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(*a),unreal.Vector(*b),unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignore,unreal.DrawDebugTrace.NONE,True)
    return hit.to_tuple() if hit else None
def ground(x,y,top=800):
    hit=trace((x,y,top),(x,y,-500))
    return float(hit[5].z) if hit and hit[0] else None
samples=[]
for r in (0,250,500,650,699):
    for i in range(16 if r else 1):
        a=i*math.tau/16;x=r*math.cos(a);y=r*math.sin(a)
        z=ground(x,y)
        samples.append({'x':round(x,2),'y':round(y,2),'z':z})
report['samples']['combat_floor']=samples
report['checks']['flat_clear_14m_floor']=all(s['z'] is not None and abs(s['z'])<2 for s in samples)
gate=trace((550,0,150),(1150,0,150))
report['checks']['gate_open_at_player_height']=not (gate and gate[0])
balcony=[ground(*[v*825 for v in (math.cos(math.radians(a)),math.sin(math.radians(a)))],top=260) for a in (-110,-90,-70,-50)]
report['samples']['balcony_z']=balcony
report['checks']['balcony_deck_at_2m']=all(z is not None and abs(z-200)<3 for z in balcony)
east=[ground(875*math.cos(math.radians(a)),875*math.sin(math.radians(a)),top=60) for a in (65,85,105,115)]
report['samples']['east_path_z']=east
report['checks']['east_walkway_at_court_level']=all(z is not None and abs(z)<2 for z in east)
bridge=[ground(s*math.cos(math.radians(150)),s*math.sin(math.radians(150)),top=100) for s in (1020,1150,1350,1550,1750)]
report['samples']['bridge_z']=bridge
report['checks']['bridge_walkable_surface_continuous']=all(z is not None and abs(z)<2 for z in bridge)
stairs=[ground(-679+i*30,-735,top=240) for i in range(12)]
report['samples']['stair_z']=stairs
report['checks']['stairs_have_correct_rises']=all(z is not None and abs(z-(i+1)*200/12)<3 for i,z in enumerate(stairs))
if worlds:
    hero=unreal.GameplayStatics.get_player_character(world,0)
    bosses=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.load_class(None,'/Game/_Game/Enemies/BP_EnemyBase.BP_EnemyBase_C'))
    report['hero']={'position':vec(hero.get_actor_location()),'mesh':hero.get_component_by_class(unreal.SkeletalMeshComponent).get_skeletal_mesh_asset().get_name(),'falling':hero.get_component_by_class(unreal.CharacterMovementComponent).is_falling()} if hero else None
    report['bosses']=[{'position':vec(b.get_actor_location()),'mesh':b.get_component_by_class(unreal.SkeletalMeshComponent).get_skeletal_mesh_asset().get_name()} for b in bosses]
    report['checks']['hero_spawned_with_existing_mesh']=bool(report['hero'] and report['hero']['mesh']=='SK_Hero')
    report['checks']['boss_spawned_with_existing_mesh']=bool(report['bosses'] and report['bosses'][0]['mesh']=='SK_Boss')
    report['checks']['hero_grounded']=bool(hero and not report['hero']['falling'] and 45<report['hero']['position'][2]<65)
    ds=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.load_class(None,'/Script/Parry_Parcour_Game.SwordCombatTest'))
    report['combat_directors']=len(ds)
    unreal.AutomationLibrary.take_high_res_screenshot(1520,856,str(ROOT/'Saved/ArenaSetup/arena-gameplay.png'))
report['passed']=all(report['checks'].values())
(ROOT/'Saved/ArenaSetup'/('runtime-report.json' if worlds else 'geometry-report.json')).write_text(json.dumps(report,indent=2))
unreal.log('ARENA_VERIFICATION '+str(report['checks']))
