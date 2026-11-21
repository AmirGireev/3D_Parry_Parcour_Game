import unreal
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
existing=sub.get_all_level_actors()
for a in existing:
    if isinstance(a,unreal.PointLight):
        a.point_light_component.set_editor_property('light_color',unreal.Color(r=255,g=168,b=70,a=255))
        a.point_light_component.set_editor_property('intensity',15)
    if isinstance(a,unreal.DirectionalLight):
        a.light_component.set_editor_property('light_color',unreal.Color(r=255,g=234,b=201,a=255))
    if isinstance(a,unreal.SkeletalMeshActor) and 'editor preview' in a.get_actor_label():
        a.set_actor_hidden_in_game(True)
        a.set_actor_enable_collision(False)
if not any(a.get_actor_label()=='Fort exposure' for a in existing):
    p=sub.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector(0,0,0))
    p.set_actor_label('Fort exposure')
    p.set_editor_property('unbound',True)
    settings=p.get_editor_property('settings')
    settings.set_editor_property('override_auto_exposure_bias',True)
    settings.set_editor_property('auto_exposure_bias',-.6)
    p.set_editor_property('settings',settings)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
