"""Import authored v2 landmark meshes and assemble the first playable art pass.

Run through Unreal Python with the current project open. The old combat map
is preserved. Re-running only replaces tagged actors in this new arena map.
"""
import unreal
import json
import pathlib
import math

ROOT=pathlib.Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE=ROOT/'test-assets/mossbound-arena'
REPORT=ROOT/'Saved/ArenaSetup'
LEVEL='/Game/_Game/Levels/L_MossboundArena'
DEST='/Game/_Game/Art/MossboundArena'
TAG='MossboundArenaV2'
manifest=json.loads((SOURCE/'manifest.json').read_text())
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
if unreal.EditorLevelLibrary.get_pie_worlds(False):
    raise RuntimeError('Stop Play before building the arena')
# Save current work before opening the new environment map.
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)

materials={}
for name,rgba in manifest['materials'].items():
    path=DEST+'/Materials/M_MB_'+name
    mat=unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('M_MB_'+name,DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    lib=unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(mat)
    base=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-400,0)
    base.set_editor_property('constant',unreal.LinearColor(*rgba))
    lib.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-400,180)
    rough.set_editor_property('r',.88 if name!='Iron' else .62)
    lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    if name=='Iron':
        metal=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-400,280)
        metal.set_editor_property('r',.5)
        lib.connect_material_property(metal,'',unreal.MaterialProperty.MP_METALLIC)
    lib.recompile_material(mat)
    assets.save_loaded_asset(mat)
    materials[name]=mat

tasks=[]
for item in manifest['groups']:
    ui=unreal.FbxImportUI()
    for k,v in {'import_mesh':True,'import_as_skeletal':False,'mesh_type_to_import':unreal.FBXImportType.FBXIT_STATIC_MESH,'automated_import_should_detect_type':False,'import_materials':False,'import_textures':False,'import_animations':False}.items():ui.set_editor_property(k,v)
    data=ui.get_editor_property('static_mesh_import_data')
    for k,v in {'combine_meshes':True,'auto_generate_collision':False,'generate_lightmap_u_vs':False,'force_front_x_axis':False,'convert_scene':True,'convert_scene_unit':True,'import_uniform_scale':1.0}.items():data.set_editor_property(k,v)
    task=unreal.AssetImportTask()
    for k,v in {'filename':str(SOURCE/(item['mesh']+'.fbx')),'destination_path':DEST+'/Meshes','destination_name':item['mesh'],'replace_existing':True,'automated':True,'save':True,'options':ui}.items():task.set_editor_property(k,v)
    tasks.append(task)
tools.import_asset_tasks(tasks)

if assets.does_asset_exist(LEVEL):
    if not levels.load_level(LEVEL):raise RuntimeError('Cannot load arena')
else:
    if not levels.new_level(LEVEL):raise RuntimeError('Cannot create arena')
for a in actors.get_all_level_actors():
    if TAG in [str(t) for t in a.tags]:actors.destroy_actor(a)

def actor(cls,name,loc=(0,0,0),folder='Arena/Architecture',rot=(0,0,0)):
    a=actors.spawn_actor_from_class(cls,unreal.Vector(*loc),unreal.Rotator(pitch=rot[0],yaw=rot[1],roll=rot[2]))
    a.set_actor_label(name);a.set_folder_path(folder);a.tags=[TAG]
    return a

report={'level':LEVEL,'landmarks':{},'warnings':[]}
for item in manifest['groups']:
    mesh=unreal.load_asset(DEST+'/Meshes/'+item['mesh'])
    if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError('Mesh failed to import: '+item['mesh'])
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        name=str(slot.get_editor_property('imported_material_slot_name'))
        if name not in materials:
            # FBX exporters may suffix material display names; use stable prefix.
            name=next((n for n in materials if name.startswith(n)),None)
        if name:mesh.set_material(i,materials[name])
        else:raise RuntimeError('Unknown material slot '+str(slot))
    body=mesh.get_editor_property('body_setup')
    if body:body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    assets.save_loaded_asset(mesh)
    folder='Arena/Surroundings' if item['name']=='Ravine' else 'Arena/Architecture'
    if item['name'] in ('Moss','SouthwestTree','Banners'):folder='Arena/Dressing'
    a=actor(unreal.StaticMeshActor,'MB '+item['name'],folder=folder)
    a.static_mesh_component.set_static_mesh(mesh)
    a.static_mesh_component.set_collision_profile_name('BlockAll' if item['collision'] else 'NoCollision')
    a.static_mesh_component.set_editor_property('cast_shadow',item['name']!='Moss')
    b=mesh.get_bounds()
    report['landmarks'][item['name']]={'mesh':mesh.get_path_name(),'origin':list(b.origin.to_tuple()),'extent':list(b.box_extent.to_tuple()),'collision':item['collision']}

# A flat low-contrast background hemisphere provides diffuse skylight and a
# quiet mist-grey horizon, independent of the old map's bright blue sky.
sky_path=DEST+'/Materials/M_MB_OvercastSky'
sky=unreal.load_asset(sky_path) if assets.does_asset_exist(sky_path) else tools.create_asset('M_MB_OvercastSky',DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
lib=unreal.MaterialEditingLibrary
lib.delete_all_material_expressions(sky)
sky.set_editor_property('two_sided',True)
sky.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
sky.set_editor_property('is_sky',False)
horizon=lib.create_material_expression(sky,unreal.MaterialExpressionConstant3Vector,-500,-150)
horizon.set_editor_property('constant',unreal.LinearColor(.24,.31,.37,1))
zenith=lib.create_material_expression(sky,unreal.MaterialExpressionConstant3Vector,-500,-20)
zenith.set_editor_property('constant',unreal.LinearColor(.075,.12,.19,1))
position=lib.create_material_expression(sky,unreal.MaterialExpressionWorldPosition,-1000,170)
mask=lib.create_material_expression(sky,unreal.MaterialExpressionComponentMask,-800,170)
for k,v in {'r':False,'g':False,'b':True,'a':False}.items():mask.set_editor_property(k,v)
lib.connect_material_expressions(position,'',mask,'')
height=lib.create_material_expression(sky,unreal.MaterialExpressionMultiply,-600,180)
height.set_editor_property('const_b',.0000125)
lib.connect_material_expressions(mask,'',height,'A')
clamp=lib.create_material_expression(sky,unreal.MaterialExpressionSaturate,-400,180)
lib.connect_material_expressions(height,'',clamp,'')
blend=lib.create_material_expression(sky,unreal.MaterialExpressionLinearInterpolate,-150,0)
lib.connect_material_expressions(horizon,'',blend,'A')
lib.connect_material_expressions(zenith,'',blend,'B')
lib.connect_material_expressions(clamp,'',blend,'Alpha')
lib.connect_material_property(blend,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(sky);assets.save_loaded_asset(sky)
dome=actor(unreal.StaticMeshActor,'MB overcast sky dome',folder='Arena/Lighting')
dome.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'))
dome.static_mesh_component.set_material(0,sky)
dome.set_actor_scale3d(unreal.Vector(1600,1600,1600))
dome.static_mesh_component.set_collision_profile_name('NoCollision')
dome.static_mesh_component.set_editor_property('cast_shadow',False)

sun=actor(unreal.DirectionalLight,'MB pale afternoon',loc=(0,0,1600),folder='Arena/Lighting',rot=(-38,-28,0))
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sun.light_component.set_editor_property('intensity',2.4)
sun.light_component.set_editor_property('light_color',unreal.Color(r=205,g=220,b=235,a=255))
sun.light_component.set_editor_property('light_source_angle',5.0)
fill=actor(unreal.SkyLight,'MB soft overcast fill',folder='Arena/Lighting')
fill.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
fill.light_component.set_editor_property('lower_hemisphere_is_black',False)
fill.light_component.set_editor_property('sky_distance_threshold',8000.0)
fill.light_component.set_editor_property('intensity',1.1)
fill.light_component.set_editor_property('real_time_capture',False)
fill.light_component.recapture_sky()
fog=actor(unreal.ExponentialHeightFog,'MB blue ravine mist',loc=(0,0,-80),folder='Arena/Lighting')
fog.component.set_editor_property('fog_density',.045)
fog.component.set_editor_property('fog_height_falloff',.22)
fog.component.set_editor_property('fog_inscattering_luminance',unreal.LinearColor(.23,.30,.36,1))
fog.component.set_editor_property('start_distance',800.0)
post=actor(unreal.PostProcessVolume,'MB restrained exposure',folder='Arena/Lighting')
post.set_editor_property('unbound',True)
s=post.get_editor_property('settings')
for k,v in {'override_auto_exposure_method':True,'auto_exposure_method':unreal.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_bias':True,'auto_exposure_bias':0.0,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_bloom_intensity':True,'bloom_intensity':.12,'override_vignette_intensity':True,'vignette_intensity':.16,'override_motion_blur_amount':True,'motion_blur_amount':0.0}.items():s.set_editor_property(k,v)
post.set_editor_property('settings',s)

world=editor.get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,'/Game/_Game/Core/BP_HeroGameMode.BP_HeroGameMode_C'))
world.get_world_settings().set_editor_property('kill_z',-2500.0)
start=actor(unreal.PlayerStart,'MB player start',(-430,220,65),'Arena/Combat')
director=actor(unreal.load_class(None,'/Script/Parry_Parcour_Game.SwordCombatTest'),'MB existing sword combat',folder='Arena/Combat')
for who,loc,yaw in [('Hero',(-430,220,0),-90),('Boss',(180,0,0),90)]:
    p=actor(unreal.SkeletalMeshActor,'MB '+who+' scale preview',loc,'Arena/Scale previews',rot=(0,yaw,0))
    p.set_editor_property('is_editor_only_actor',True)
    p.set_actor_hidden_in_game(True);p.set_actor_enable_collision(False)
    comp=p.skeletal_mesh_component
    comp.set_skeletal_mesh_asset(unreal.load_asset('/Game/_Game/Art/CombatTest/'+who+'/SK_'+who))
    comp.override_animation_data(unreal.load_asset('/Game/_Game/Art/CombatTest/'+who+'/Anim_'+who+'_Idle'),True,True,0,1)

for name,loc,rot in [
    ('MB camera - south overview',(-1750,100,900),(-22,-4,0)),
    ('MB camera - overhead',(-350,100,3400),(-90,0,0)),
    ('MB camera - north reverse',(820,0,610),(-25,180,0)),
]:
    camera=actor(unreal.CameraActor,name,loc,'Arena/Reference cameras',rot)
    camera.camera_component.set_editor_property('field_of_view',90.0)
    camera.set_editor_property('is_editor_only_actor',True)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-1750,100,900),unreal.Rotator(pitch=-22,yaw=-4,roll=0))
actors.clear_actor_selection_set()
levels.save_current_level()
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
atmosphere_script=ROOT/'Tools/CombatTest/refine_arena.py'
exec(compile(atmosphere_script.read_text(encoding='utf-8'),str(atmosphere_script),'exec'),{'__file__':str(atmosphere_script)})
report['actor_count']=len(actors.get_all_level_actors())
report['saved']=True
(REPORT/'build-report.json').write_text(json.dumps(report,indent=2))
unreal.log('MOSSBOUND_ARENA_BUILT '+LEVEL)
