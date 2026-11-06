"""Check the addon split: every {GUID} reference must resolve inside the
referencing addon or one of its (transitive) dependencies, and asset addons
must not contain scripts, prefabs, layouts or worlds.

Usage: python3 tools/check_addons.py addons   (exit code 1 on problems)
"""
import re, sys, pathlib
root = pathlib.Path(sys.argv[1])
ref_re = re.compile(rb'\{([0-9A-F]{16})\}([A-Za-z0-9_./ -]+)')
addons = {}
for gp in root.glob('*/*.gproj'):
    t = gp.read_text()
    guid = re.search(r'^ GUID "([0-9A-F]{16})"', t, re.M).group(1)
    deps = re.findall(r'"([0-9A-F]{16})"', re.search(r'Dependencies \{(.*?)\}', t, re.S).group(1))
    addons[guid] = dict(name=gp.parent.name, dir=gp.parent, deps=deps, owns={})
for a in addons.values():
    for m in a['dir'].rglob('*.meta'):
        mm = re.search(r'Name "\{([0-9A-F]{16})\}', m.read_text(errors='ignore'))
        if mm: a['owns'][mm.group(1)] = m
owner = {}
bad = 0
for g, a in addons.items():
    for rg in a['owns']:
        if rg in owner: print('DUP GUID', rg, owner[rg], a['name']); bad += 1
        owner[rg] = a['name']
def closure(g, seen=None):
    seen = seen if seen is not None else set()
    if g in seen or g not in addons: return seen
    seen.add(g)
    for d in addons[g]['deps']: closure(d, seen)
    return seen
for g, a in addons.items():
    visible = {addons[x]['name'] for x in closure(g)}
    if a['name'].startswith('ELifeRPG_Assets_'):
        for f in a['dir'].rglob('*'):
            if f.suffix in ('.c', '.et', '.layout', '.ent', '.layer'): print('LOGIC IN ASSET PACK', f); bad += 1
    for f in a['dir'].rglob('*'):
        if not f.is_file() or f.suffix == '.meta': continue
        for rg, path in set(ref_re.findall(f.read_bytes())):
            rg = rg.decode()
            o = owner.get(rg)
            if o and o not in visible:
                print(f'WRONG DIRECTION {a["name"]}: {f.relative_to(root)} -> {o} {{{rg}}}{path.decode()}'); bad += 1
            elif not o:
                # path exists in some ELIFE addon but guid unknown -> stale
                p = path.decode().strip()
                hits = [x['name'] for x in addons.values() if (x['dir']/p).exists()]
                if hits: print(f'GUID MISMATCH {f.relative_to(root)} -> {{{rg}}}{p} (path in {hits})'); bad += 1
print('addons:', {a['name']: len(a['owns']) for a in addons.values()})
print('problems:', bad)
sys.exit(1 if bad else 0)
