MOSSBOUND FORT - combat prototype

Open /Game/_Game/Levels/L_MossboundFort and press Play.
The map contains a stone courtyard, timber side decks, a raised balcony,
a bridge approach, greenery and open ledges. Environment art is a blockout.

Characters use the textured moss hero and fallen knight, including weapons
rigidly weighted to WeaponGrip bones. Reference heights are 100 and 300 cm.
BladeBase and BladeTip mark the cutting portion of each visible weapon.

Controls:
WASD / mouse: existing player movement and camera
Space: existing double jump
Right mouse: existing timed parry window
Shift: existing normal dash
Left mouse: existing counter-dash after a successful parry
R: restart this test level
F1: show/hide blade traces
F2: diagnostic automatic guard (swords only; does not auto-parry projectiles)

Select 'Sword combat test - select to tune traces' in the Outliner.
Combat test properties expose blade radius, attack interval, approach distance,
normalized active start/end frames and the hero's guard direction in mesh space.
Training Mode supplies 1000 player health so repeated hits do not trigger the
original player's hard-coded parkour death travel. Body-hit counters still work.
The original player and enemy Blueprint assets are not modified.

Collision rules:
During active melee frames, the test samples blade capsules between frames.
A successful parry requires both a real blade-to-blade intersection and the
player's existing parry window. A body hit outside the blade contact still damages
the player. A valid parry calls the original OnParrySuccess / OnParried events,
sets DashTarget, plays a metallic sound and draws a short contact burst.
Four sound variants are available for both metallic parries and clashes.
Every fourth boss action uses the project's existing projectile Blueprint.

Animation is a prototype. CombatPreviewAnimInstance crossfades imported clips
and corrects the guard wrist in component space, keeping the hand, fingers and
sword together. Boss attack grips were adjusted for the much smaller target.
Blender source clips are baked in-place for locomotion, dash and jump; gameplay
movement remains controlled by the original player Blueprint.

Rebuild/import:
1. test-assets/export_combat_assets.py runs in Blender against the saved preview.
2. Tools/CombatTest/import_assets.py runs in Unreal Python and imports source FBXs
   from test-assets/unreal-combat. Animation FBXs contain skin to retain the extra
   weapon bones' bind pose, but Unreal imports only animation from those files.
3. build_fort.py creates the level only if it does not already exist.
4. refine_environment.py and refine_lighting.py apply the blockout dressing.

The saved preview .blend and previously downloaded raw Mixamo packs are not
overwritten by the exporter. The new Unreal classes are isolated test adapters.
The project already reports a GameFeatureData Asset Manager configuration warning
at startup; this prototype does not change unrelated project/plugin settings.
