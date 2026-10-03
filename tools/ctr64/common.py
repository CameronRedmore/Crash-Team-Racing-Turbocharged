import json, shlex, os
import clang.cindex as ci
ROOT = os.path.realpath('/home/cameron/Games/CTR Turbocharged/src/worktrees/64bit')
def args_for(build='/tmp/b64', file='main.c'):
    d = json.load(open(build + '/compile_commands.json'))
    for e in d:
        if e['file'].endswith('/' + file) and '/externals/' not in e['file']:
            a = shlex.split(e['command'])
            out = []
            skip = False
            for x in a[1:]:
                if skip: skip = False; continue
                if x == '-o': skip = True; continue
                if x == '-c': continue
                if x == a[-1]: continue
                out.append(x)
            import subprocess
            rd=subprocess.check_output(['clang','-print-resource-dir']).decode().strip()
            out += ['-isystem', rd+'/include','-ferror-limit=0','-Wno-everything']
            return out
def parse(build='/tmp/b64', file='main.c'):
    idx = ci.Index.create()
    a = args_for(build, file)
    return idx.parse(os.path.join(ROOT, file), args=a,
                     options=ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
def in_project(path):
    if not path: return False
    p = os.path.realpath(path)
    return p.startswith(ROOT + '/') and '/externals/' not in p

def convert_scope(path):
    r = os.path.relpath(os.path.realpath(path), ROOT)
    if r.startswith('include/'):
        for skip in ('include/psx/','include/psn00bsdk/','include/platform/','include/platform.h'):
            if r.startswith(skip): return False
        return True
    return False
