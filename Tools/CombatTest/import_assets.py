import unreal, pathlib, json, traceback
ROOT=pathlib.Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
(ROOT/'Saved/CombatSetup').mkdir(parents=True,exist_ok=True)
DEST='/Game/_Game/Art/CombatTest'
report={}
try:
    for who in ('Hero','Boss'):
        folder=DEST+'/'+who
        mesh_path=folder+'/SK_'+who
        if not unreal.EditorAssetLibrary.does_asset_exist(mesh_path):
            ui=unreal.FbxImportUI()
            ui.set_editor_property('import_mesh',True)
            ui.set_editor_property('import_as_skeletal',True)
            ui.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_SKELETAL_MESH)
            ui.set_editor_property('automated_import_should_detect_type',False)
            ui.set_editor_property('import_materials',True)
            ui.set_editor_property('import_textures',True)
            ui.set_editor_property('import_animations',False)
            ui.set_editor_property('create_physics_asset',True)
            task=unreal.AssetImportTask()
            for k,v in {'filename':str(ROOT/'test-assets/unreal-combat'/('SK_'+who+'.fbx')),'destination_path':folder,'destination_name':'SK_'+who,'automated':True,'save':True,'options':ui}.items():task.set_editor_property(k,v)
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        mesh=unreal.load_asset(mesh_path)
        skeleton=mesh.get_editor_property('skeleton')
        tasks=[]
        for source in (ROOT/'test-assets/unreal-combat').glob('Anim_'+who+'_*.fbx'):
            ui=unreal.FbxImportUI()
            for k,v in {'import_mesh':False,'import_as_skeletal':True,'mesh_type_to_import':unreal.FBXImportType.FBXIT_ANIMATION,'automated_import_should_detect_type':False,'import_materials':False,'import_textures':False,'import_animations':True,'skeleton':skeleton}.items():ui.set_editor_property(k,v)
            task=unreal.AssetImportTask()
            for k,v in {'filename':str(source),'destination_path':folder,'destination_name':source.stem,'replace_existing':True,'automated':True,'save':True,'options':ui}.items():task.set_editor_property(k,v)
            tasks.append(task)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        bounds=mesh.get_bounds()
        report[who]={'mesh':mesh.get_path_name(),'bounds_origin':str(bounds.origin),'bounds_extent':str(bounds.box_extent),'skeleton':skeleton.get_path_name()}
    tasks=[]
    for group,glob in [('parry-steel-v3','steel_parry_0[1-4].wav'),('sfx/clash','sword_clash_0[1-4].wav')]:
        for source in (ROOT/'test-assets/audio'/group).glob(glob):
            if unreal.EditorAssetLibrary.does_asset_exist('/Game/_Game/Audio/CombatTest/'+source.stem):continue
            task=unreal.AssetImportTask()
            for k,v in {'filename':str(source),'destination_path':'/Game/_Game/Audio/CombatTest','automated':True,'save':True}.items():task.set_editor_property(k,v)
            tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    unreal.EditorAssetLibrary.save_directory(DEST)
except Exception:
    report['error']=traceback.format_exc()
(ROOT/'Saved/CombatSetup/import-report.json').write_text(json.dumps(report,indent=2))
unreal.log('COMBAT_IMPORT_REPORT '+str(report))
