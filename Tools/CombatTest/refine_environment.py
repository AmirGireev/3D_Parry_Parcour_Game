import unreal,random
random.seed(18)
lib=unreal.MaterialEditingLibrary
root='/Game/_Game/Art/CombatTest/Environment'
for path in unreal.EditorAssetLibrary.list_assets(root,recursive=False,include_folder=False):
    mat=unreal.load_asset(path)
    if not isinstance(mat,unreal.Material):continue
    name=mat.get_name()
    if not ('FortStone' in name or 'WeatheredTimber' in name):continue
    # World-space variation gives prototype blocks a weathered surface without external textures.
    idx=int(name.rsplit('_',1)[-1])
    rgb=(.15+idx*.012,.175+idx*.012,.145+idx*.012) if 'Stone' in name else (.13+idx*.016,.076+idx*.010,.038+idx*.006)
    low=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-600,-200)
    high=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-600,-60)
    low.set_editor_property('constant',unreal.LinearColor(rgb[0]*.45,rgb[1]*.45,rgb[2]*.45,1))
    high.set_editor_property('constant',unreal.LinearColor(rgb[0]*1.2,rgb[1]*1.2,rgb[2]*1.2,1))
    noise=lib.create_material_expression(mat,unreal.MaterialExpressionNoise,-600,130)
    noise.set_editor_property('scale',.055 if 'Stone' in name else .025)
    noise.set_editor_property('levels',3)
    noise.set_editor_property('output_min',0)
    noise.set_editor_property('output_max',1)
    blend=lib.create_material_expression(mat,unreal.MaterialExpressionLinearInterpolate,-250,0)
    lib.connect_material_expressions(low,'',blend,'A')
    lib.connect_material_expressions(high,'',blend,'B')
    lib.connect_material_expressions(noise,'',blend,'Alpha')
    lib.connect_material_property(blend,'',unreal.MaterialProperty.MP_BASE_COLOR)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors=sub.get_all_level_actors()
if not any(a.get_actor_label()=='Mossbound raised balcony' for a in all_actors):
    cube=unreal.load_asset('/Engine/BasicShapes/Cube')
    cone=unreal.load_asset('/Engine/BasicShapes/Cone')
    wood=unreal.load_asset(root+'/M_WeatheredTimber_0')
    green=unreal.load_asset(root+'/M_Moss')
    def block(name,loc,scale,mat,mesh=cube):
        a=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc))
        a.set_actor_label(name);a.set_folder_path('Fort/Balcony and undergrowth')
        a.static_mesh_component.set_static_mesh(mesh);a.static_mesh_component.set_material(0,mat)
        a.set_actor_scale3d(unreal.Vector(*scale))
    block('Mossbound raised balcony',(500,-740,120),(5.0,3.0,.22),wood)
    for x in (265,735):
        for y in (-870,-610):block('Balcony oak support',(x,y,56),(.24,.24,1.2),wood)
    for i in range(8):block('Balcony access step',(155+i*27,-740,7.5+i*15),(.28,1.5,.15),wood)
    for x in range(275,760,120):
        block('Balcony railing post',(x,-885,176),(.14,.14,1.2),wood)
        block('Balcony rail',(x+50,-885,220),(1.0,.10,.12),wood)
    for i in range(100):
        x=random.uniform(-850,850); y=random.choice((-1,1))*random.uniform(840,895)
        block('Wild grass',(x,y,random.uniform(10,25)),(.07,.07,random.uniform(.15,.4)),green,cone)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
