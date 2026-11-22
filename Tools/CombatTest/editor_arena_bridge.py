"""Temporary local editor job runner; stops after 30 minutes or a stop request."""
import unreal
import pathlib
import json
import traceback
import time

_arena_root=pathlib.Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
_arena_jobs=_arena_root/'Saved/ArenaSetup'
_arena_jobs.mkdir(parents=True,exist_ok=True)
_arena_seen=None
_arena_started=time.monotonic()
_arena_handle=None
_arena_allowed={'inspect_arena.py','build_arena_level.py','verify_arena.py','refine_arena.py'}

def _arena_tick(delta):
    global _arena_seen,_arena_handle
    p=_arena_jobs/'request.json'
    if time.monotonic()-_arena_started>1800:
        unreal.unregister_slate_post_tick_callback(_arena_handle)
        return
    if not p.exists():return
    request=json.loads(p.read_text(encoding='utf-8-sig'))
    if request.get('id')==_arena_seen:return
    _arena_seen=request['id']
    result={'id':_arena_seen}
    try:
        if request.get('script')=='stop':
            unreal.unregister_slate_post_tick_callback(_arena_handle)
            result['stopped']=True
        else:
            name=request['script']
            if name not in _arena_allowed:raise ValueError('Unknown arena job')
            source=_arena_root/'Tools/CombatTest'/name
            namespace={'__file__':str(source),'__name__':'__main__'}
            exec(compile(source.read_text(encoding='utf-8'),str(source),'exec'),namespace)
            result['success']=True
    except Exception:
        result['error']=traceback.format_exc()
        unreal.log_error(result['error'])
    (_arena_jobs/'response.json').write_text(json.dumps(result,indent=2),encoding='utf-8')

_arena_handle=unreal.register_slate_post_tick_callback(_arena_tick)
(_arena_jobs/'bridge-ready.json').write_text(json.dumps({'ready':True}))
unreal.log('Arena editor job runner ready')
