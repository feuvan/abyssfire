"""Where the engine-generated Python stub (unreal.py) lives. It is a git-ignored cache written by the editor Python
tooling (unreal/Scripts/.cache/unreal_stub/unreal.py), so a secondary git worktree does not have it: look in the main
checkout too. ABYSS_UNREAL_STUB overrides."""
import os
import subprocess

REL = os.path.join('Scripts', '.cache', 'unreal_stub', 'unreal.py')


def find_stub(unreal_dir):
    env = os.environ.get('ABYSS_UNREAL_STUB')
    if env:
        return env
    local = os.path.join(unreal_dir, REL)
    if os.path.exists(local):
        return local
    try:
        common = subprocess.run(['git', '-C', unreal_dir, 'rev-parse', '--path-format=absolute', '--git-common-dir'],
                                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=10).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        common = ''
    if common:
        main = os.path.join(os.path.dirname(common), 'unreal', REL)
        if os.path.exists(main):
            return main
    return local
