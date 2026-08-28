#!/usr/bin/env python3
"""Group c89check findings by enclosing function, and list the loop variables
each function needs declared. ANALYSIS ONLY."""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def func_starts(path):
    """(line, text) for every line that looks like the start of a top-level definition."""
    out = []
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        for n, ln in enumerate(f, 1):
            if re.match(r'^[A-Za-z_].*\(', ln) or re.match(r'^static\b.*\(', ln):
                out.append((n, ln.rstrip()))
    return out


def main():
    path = sys.argv[1]
    env = dict(os.environ, DETAIL='1')
    r = subprocess.run([sys.executable, os.path.join(HERE, 'c89check.py'), path],
                       capture_output=True, text=True, env=env)
    fs = func_starts(path)
    groups = {}
    for line in r.stdout.split('\n'):
        m = re.match(r'\s*(\d+)\s+(\S+)\s+(.*)$', line)
        if not m or m.group(2) in ('---', 'summary'):
            continue
        lno, tag, text = int(m.group(1)), m.group(2), m.group(3)
        if tag == 'TOTAL':
            continue
        owner = (0, '<file scope>')
        for (fl, ft) in fs:
            if fl <= lno:
                owner = (fl, ft)
            else:
                break
        groups.setdefault(owner, []).append((lno, tag, text))

    for owner in sorted(groups):
        items = groups[owner]
        loopvars = []
        for _l, tag, text in items:
            if tag == 'for-decl':
                mm = re.match(r'\s*(?:const\s+)?[\w ]*?([A-Za-z_]\w*)\s*=', text)
                if mm and mm.group(1) not in loopvars:
                    loopvars.append(mm.group(1))
        print('### %d: %s' % owner)
        if loopvars:
            print('    loopvars: %s' % ', '.join(loopvars))
        for l, tag, text in items:
            print('    %6d %-16s %s' % (l, tag, text))


main()
