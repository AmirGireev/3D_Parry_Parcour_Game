import unreal,json,pathlib
root=pathlib.Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
data={'project':str(root),'world':w.get_path_name(),'pie':bool(unreal.EditorLevelLibrary.get_pie_worlds(False)),'assets':{},'api':{}}
for n in ('StaticMeshEditorSubsystem','SkyLight','SkyAtmosphere','ExponentialHeightFog','PostProcessSettings'):
    data['api'][n]=bool(getattr(unreal,n,None))
for p in ('/Game/_Game/Art/CombatTest/Hero/SK_Hero','/Game/_Game/Art/CombatTest/Boss/SK_Boss','/Game/_Game/Core/BP_HeroGameMode','/Script/Parry_Parcour_Game.SwordCombatTest'):
    data['assets'][p]=bool(unreal.load_class(None,p) if p.startswith('/Script') else unreal.load_asset(p))
(root/'Saved/ArenaSetup/inspection.json').write_text(json.dumps(data,indent=2))

hit=unreal.SystemLibrary.line_trace_single(w,unreal.Vector(0,0,800),unreal.Vector(0,0,-500),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[],unreal.DrawDebugTrace.NONE,True)
data['hit_dir']=dir(hit)
data['hit_tuple']=str(hit.to_tuple())
data['hit_str']=str(hit)
(root/'Saved/ArenaSetup/inspection.json').write_text(json.dumps(data,indent=2))
