"""Bake the saved Blender grip preview into self-contained Unreal test assets."""
import bpy, json
import math
from pathlib import Path
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'unreal-combat'
OUT.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(ROOT / 'blender-preview/characters-weapon-preview.blend'))
scene = bpy.context.scene
metadata = {}
clips = {'Hero': [('Idle',361,480),('Run',241,360),('Jump',121,240),('Dash',61,120),('Parry',1,60)],
         'Boss': [('Idle',1,120),('Combo',481,600),('Sweep',601,690),('Walk',691,810),('Cast',811,900)]}

for prefix in ('Hero','Boss'):
    rig = bpy.data.objects[prefix+'_Rig']
    socket = bpy.data.objects[prefix+'_Sword_HandSocket']
    swords = [o for o in bpy.data.objects if o.type=='MESH' and o.parent==socket]
    bodies = [o for o in bpy.data.objects if o.type=='MESH' and any(m.type=='ARMATURE' and m.object==rig for m in o.modifiers)]
    scene.frame_set(10)
    bpy.context.view_layer.update()
    relative = (rig.matrix_world @ rig.pose.bones['mixamorig:RightHand'].matrix).inverted() @ socket.matrix_world
    rest_socket = rig.data.bones['mixamorig:RightHand'].matrix_local @ relative
    low = Vector(tuple(min((rig.matrix_world.inverted() @ o.matrix_world @ Vector(c))[i] for o in bodies for c in o.bound_box) for i in range(3)))
    high = Vector(tuple(max((rig.matrix_world.inverted() @ o.matrix_world @ Vector(c))[i] for o in bodies for c in o.bound_box) for i in range(3)))
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.object.mode_set(mode='EDIT')
    grip=rig.data.edit_bones.new('WeaponGrip')
    grip.parent=rig.data.edit_bones['mixamorig:RightHand']
    grip.matrix=rest_socket
    grip.length=.05
    for name,z in [('BladeBase',.12),('BladeTip',1.0)]:
        # Endpoint positions are updated below from the actual normalized sword geometry.
        bone=rig.data.edit_bones.new(name)
        bone.parent=grip
        bone.matrix=rest_socket @ Matrix.Translation((0,0,z))
        bone.length=.025
    bpy.ops.object.mode_set(mode='OBJECT')
    sword_points=[]
    for sword in swords:
        local=sword.matrix_basis.copy()
        sword.parent=rig
        sword.parent_type='OBJECT'
        sword.matrix_parent_inverse=Matrix.Identity(4)
        sword.matrix_basis=rest_socket @ local
        vg=sword.vertex_groups.new(name='WeaponGrip')
        vg.add(list(range(len(sword.data.vertices))),1.0,'REPLACE')
        modifier=sword.modifiers.new('Rigid weapon grip','ARMATURE')
        modifier.object=rig
        sword_points += [local @ Vector(c) for c in sword.bound_box]
    zmin=min(p.z for p in sword_points)
    zmax=max(p.z for p in sword_points)
    length=zmax-zmin
    base=zmin+length*.28
    tip=zmax-length*.02
    bpy.context.view_layer.objects.active=rig
    bpy.ops.object.mode_set(mode='EDIT')
    for name,z in [('BladeBase',base),('BladeTip',tip)]:
        rig.data.edit_bones[name].matrix=rest_socket @ Matrix.Translation((0,0,z))
        rig.data.edit_bones[name].length=.025
    bpy.ops.object.mode_set(mode='OBJECT')
    # Keep the existing normalized 1 m / 3 m world scale, but place each export at the origin.
    rig.parent.location.x=0
    scene.frame_set(10)
    bpy.context.view_layer.update()
    objects=[rig]+bodies+swords
    def select_export(meshes):
        bpy.ops.object.select_all(action='DESELECT')
        for obj in ([rig]+meshes): obj.select_set(True)
        bpy.context.view_layer.objects.active=rig
    select_export(bodies+swords)
    rig.data.pose_position='REST'
    bpy.ops.export_scene.fbx(filepath=str(OUT/('SK_'+prefix+'.fbx')),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,path_mode='COPY',embed_textures=True,use_armature_deform_only=False)
    rig.data.pose_position='POSE'
    # Author readable height-matched attacks and a guard using the existing hand controls.
    # The rigid weapon and supporting hand follow the grip, so contact remains tied to geometry.
    control=bpy.data.objects[prefix+'_RightHand_MotionTarget'] if prefix=='Boss' else bpy.data.objects['Hero_Parry_HandTarget']
    hand_relation=relative.copy()
    authored=[('Combo',481,600),('Sweep',601,690)] if prefix=='Boss' else [('Parry',1,60)]
    for label,a,b in authored:
        playback=next(t for t in rig.animation_data.nla_tracks if t.name.startswith('PLAYBACK'))
        source=next(s for s in playback.strips if abs(s.frame_start-a)<.1)
        b=min(b,int(source.frame_end))
        original=[]
        for frame in range(a,b+1):
            scene.frame_set(frame)
            original.append(control.matrix_world.copy())
        for frame,original_matrix in zip(range(a,b+1),original):
            scene.frame_set(frame)
            t=(frame-a)/max(1,b-a)
            if prefix=='Boss':
                weight=min(1.,max(0.,(t-.10)/.16),max(0.,(1.-t)/.18))
                sweep=max(0.,min(1.,(t-.26)/.4))
                grip_point=Vector((-.3+.6*sweep,-.62,1.50))
                aim_point=Vector((.4-.8*sweep,-1.80,.64))
            else:
                weight=min(1.,max(0.,t/.25))
                grip_point=Vector((.22,-.28,.72))
                aim_point=Vector((.75,-.55,1.10))
            desired_socket=Matrix.LocRotScale(grip_point,(aim_point-grip_point).to_track_quat('Z','Y'),rig.matrix_world.to_scale())
            wanted=desired_socket @ hand_relation.inverted()
            loc=original_matrix.translation.lerp(wanted.translation,weight)
            quat=original_matrix.to_quaternion().slerp(wanted.to_quaternion(),weight)
            control.matrix_world=Matrix.LocRotScale(loc,quat,original_matrix.to_scale())
            control.keyframe_insert('location',frame=frame)
            control.keyframe_insert('rotation_quaternion',frame=frame)
            if prefix=='Hero' and weight>.99:
                for correction_pass in range(8):
                    bpy.context.view_layer.update()
                    current=(rig.matrix_world @ rig.pose.bones['WeaponGrip'].matrix).to_3x3().col[2].normalized()
                    delta=current.rotation_difference((aim_point-grip_point).normalized())
                    world=control.matrix_world.copy()
                    control.matrix_world=Matrix.LocRotScale(world.translation,delta @ world.to_quaternion(),world.to_scale())
                    control.keyframe_insert('rotation_quaternion',frame=frame)
            control.keyframe_insert('location',frame=frame)
            control.keyframe_insert('rotation_quaternion',frame=frame)
    for label,start,end in clips[prefix]:
        playback=next(t for t in rig.animation_data.nla_tracks if t.name.startswith('PLAYBACK'))
        source=next(s for s in playback.strips if abs(s.frame_start-start)<.1)
        end=min(end,int(source.frame_end))
        scene.frame_start=start
        scene.frame_end=end
        placement=rig.parent
        placement.animation_data_clear()
        placement.location.x=0
        placement.location.y=0
        if label in ('Run','Walk','Dash','Jump'):
            samples=[]
            for frame in range(start,end+1):
                scene.frame_set(frame)
                samples.append((rig.matrix_world @ rig.pose.bones['mixamorig:Hips'].matrix).translation.copy())
            origin=samples[0]
            for frame,point in zip(range(start,end+1),samples):
                placement.location.x=origin.x-point.x
                placement.location.y=origin.y-point.y
                placement.location.z=origin.z-point.z if label=='Jump' else 0
                placement.keyframe_insert('location',frame=frame)
        # Include skin to preserve a valid FBX bind pose for the extra weapon bones.
        # Unreal imports only animation from these files into the existing skeleton.
        select_export(bodies+swords)
        bpy.ops.export_scene.fbx(filepath=str(OUT/('Anim_'+prefix+'_'+label+'.fbx')),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=True,bake_anim_use_all_bones=True,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,bake_anim_force_startend_keying=True,bake_anim_step=1,bake_anim_simplify_factor=0,use_armature_deform_only=False)
        placement.animation_data_clear()
        placement.location=(0,0,0)
    metadata[prefix]={'reference_height_m':1 if prefix=='Hero' else 3,'target_height_cm':100 if prefix=='Hero' else 300,'blade_length_m':(tip-base)*rig.matrix_world.to_scale().x,'clips':[c[0] for c in clips[prefix]]}
    scene.frame_set(10 if prefix=='Hero' else 500)
    bpy.context.view_layer.update()
    print('GRIP_SAMPLE',prefix,list(socket.matrix_world.translation),list((rig.matrix_world @ rig.pose.bones['WeaponGrip'].matrix).translation),list(control.matrix_world.translation),flush=True)
    print('BLADE_SAMPLE',prefix,list((rig.matrix_world @ rig.pose.bones['BladeBase'].matrix).translation),list((rig.matrix_world @ rig.pose.bones['BladeTip'].matrix).translation),flush=True)
    print('EXPORTED',prefix,metadata[prefix],flush=True)
(OUT/'measurements.json').write_text(json.dumps(metadata,indent=2))
