import unreal,pathlib,json
root=pathlib.Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world,'ShowFlag.Fog 1')
unreal.SystemLibrary.execute_console_command(world,'ShowFlag.VolumetricFog 1')
sky=unreal.load_asset('/Game/_Game/Art/MossboundArena/Materials/M_MB_OvercastSky')
# A custom unlit dome does not need SkyAtmosphere's special sky shader flag.
sky.set_editor_property('is_sky',False)
expressions={o.get_class().get_name():o for o in unreal.ObjectIterator(unreal.MaterialExpression) if o.get_outer()==sky}
ml=unreal.MaterialEditingLibrary
assert ml.connect_material_expressions(expressions['MaterialExpressionWorldPosition'],'',expressions['MaterialExpressionComponentMask'],'')
assert ml.connect_material_expressions(expressions['MaterialExpressionMultiply'],'',expressions['MaterialExpressionSaturate'],'')
unreal.MaterialEditingLibrary.recompile_material(sky)
unreal.EditorAssetLibrary.save_loaded_asset(sky)
lib=unreal.MaterialEditingLibrary
for name,rgb in [('Floor',(.16,.19,.215)),('FloorAlt',(.145,.17,.19))]:
    mat=unreal.load_asset('/Game/_Game/Art/MossboundArena/Materials/M_MB_'+name)
    lib.delete_all_material_expressions(mat)
    colour=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-300,0)
    colour.set_editor_property('constant',unreal.LinearColor(*rgb,1))
    lib.connect_material_property(colour,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-300,150)
    rough.set_editor_property('r',.9)
    lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
for a in actors.get_all_level_actors():
    if a.get_actor_label()=='MB soft overcast fill':a.light_component.recapture_sky()
    if a.get_actor_label()=='MB camera - overhead':
        a.set_actor_location(unreal.Vector(-350,100,3400),False,False)
    if a.get_actor_label()=='MB pale afternoon':
        a.light_component.set_editor_property('intensity',1.6)
    if a.get_actor_label()=='MB blue ravine mist':
        fog=a.component
        a.set_actor_location(unreal.Vector(0,0,150),False,False)
        fog.set_fog_density(.12)
        fog.set_fog_height_falloff(.3)
        fog.set_start_distance(250)
        fog.set_volumetric_fog(True)
        fog.set_volumetric_fog_scattering_distribution(.15)
        fog.set_volumetric_fog_extinction_scale(.65)
        fog.set_volumetric_fog_albedo(unreal.Color(r=185,g=204,b=220,a=255))
        fog.set_volumetric_fog_emissive(unreal.LinearColor(.008,.014,.022,1))
        fog.set_volumetric_fog_distance(7500)
        fog.set_volumetric_fog_start_distance(250)
        fog.set_volumetric_fog_near_fade_in_distance(350)

existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
def atmosphere_actor(cls,label,loc,rot=(0,0,0)):
    a=existing.get(label)
    if not a:
        a=actors.spawn_actor_from_class(cls,unreal.Vector(*loc),unreal.Rotator(pitch=rot[0],yaw=rot[1],roll=rot[2]))
        a.set_actor_label(label);a.set_folder_path('Arena/Atmosphere');a.tags=['MossboundArenaV2']
    return a
for name,loc,scale,density in [
    ('MB mist - north gateway',(1550,0,-60),1.5,.6),
    ('MB mist - east ravine',(0,1900,-100),2.5,1.3),
    ('MB mist - west ravine',(0,-1900,-100),2.5,1.3),
    ('MB mist - south ravine',(-1900,0,-100),2.5,1.3),
]:
    a=atmosphere_actor(unreal.LocalFogVolume,name,loc)
    a.set_actor_location(unreal.Vector(*loc),False,False)
    a.set_actor_scale3d(unreal.Vector(scale,scale,scale))
    c=a.get_component_by_class(unreal.LocalFogVolumeComponent)
    c.set_radial_fog_extinction(.65)
    c.set_height_fog_extinction(density)
    c.set_height_fog_falloff(4)
    c.set_height_fog_offset(.1)
    c.set_fog_albedo(unreal.LinearColor(.55,.68,.78,1))
    c.set_fog_emissive(unreal.LinearColor(.035,.055,.08,1))
    c.set_fog_phase_g(.1)
shaft=atmosphere_actor(unreal.SpotLight,'MB cold light through gate',(1140,0,425),(-18,180,0))
c=shaft.light_component
c.set_mobility(unreal.ComponentMobility.MOVABLE)
c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
c.set_editor_property('intensity',700)
c.set_editor_property('light_color',unreal.Color(r=172,g=205,b=230,a=255))
c.set_editor_property('attenuation_radius',2300)
c.set_editor_property('inner_cone_angle',10)
c.set_editor_property('outer_cone_angle',24)
c.set_editor_property('source_radius',25)
c.set_editor_property('volumetric_scattering_intensity',4.0)
c.set_editor_property('cast_volumetric_shadow',True)
if hasattr(unreal.EditorLevelLibrary,'editor_set_game_view'):
    unreal.EditorLevelLibrary.editor_set_game_view(True)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-1750,100,900),unreal.Rotator(pitch=-22,yaw=-4,roll=0))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
data={'game_view_api':hasattr(unreal.EditorLevelLibrary,'editor_set_game_view'),'local_mist_volumes':4,'volumetric_fog':True,'gate_light_shaft':True}
data['render_settings']={n:unreal.SystemLibrary.get_console_variable_int_value(n) for n in ['r.Fog','r.VolumetricFog','r.LocalFogVolume','r.SupportLocalFogVolumes','ShowFlag.Fog','ShowFlag.VolumetricFog','sg.EffectsQuality']}
(root/'Saved/ArenaSetup/refine-report.json').write_text(json.dumps(data,indent=2))
