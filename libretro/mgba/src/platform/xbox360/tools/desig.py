#!/usr/bin/env python3
"""
desig.py - rewrite C99 designated initializers as positional (C89) ones.

    desig.py <file> <varname> [<line>] -- <clang args>

Uses clang's JSON AST (semantic form of the InitListExpr, which is already
positional) and copies every element's source text, filling the gaps with 0.
The original designator is kept as a comment in front of each element.
"""
import sys, json, subprocess, re

def load(file, name, cargs):
    out = subprocess.run(['clang', '-fsyntax-only', '-Xclang', '-ast-dump=json', '-Xclang', '-ast-dump-filter=' + name] + cargs + [file],
                         capture_output=True, text=True).stdout
    dec = json.JSONDecoder()
    i = 0
    objs = []
    while i < len(out):
        while i < len(out) and out[i].isspace():
            i += 1
        if i >= len(out):
            break
        o, i = dec.raw_decode(out, i)
        objs.append(o)
    return objs


def off(loc, end=False):
    if 'expansionLoc' in loc:
        loc = loc['expansionLoc']
    o = loc.get('offset')
    if o is None:
        return None
    if end:
        o += loc.get('tokLen', 0)
    return o


def find_vars(o, name, acc):
    if o.get('kind') == 'VarDecl' and o.get('name') == name:
        acc.append(o)
    for c in o.get('inner', []):
        find_vars(c, name, acc)


class Rewriter:
    def __init__(self, text):
        self.text = text

    def designator_before(self, start):
        # look back for "[...] =" or ".field =" right before `start`
        t = self.text
        j = start - 1
        while j >= 0 and t[j] in ' \t\n':
            j -= 1
        if j < 0 or t[j] != '=':
            return None
        k = j - 1
        while k >= 0 and t[k] in ' \t':
            k -= 1
        # designator ends at k
        m = k
        depth = 0
        while m >= 0:
            ch = t[m]
            if ch == ']':
                depth += 1
            elif ch == '[':
                depth -= 1
                if depth == 0:
                    # may be a chain like .a[3]
                    m -= 1
                    continue
            elif depth == 0 and ch in ',{\n':
                break
            m -= 1
        d = t[m + 1:k + 1].strip()
        if d.startswith('.') or d.startswith('['):
            return d
        return None

    def elem(self, node, indent, multiline):
        k = node['kind']
        if k == 'ImplicitValueInitExpr':
            t = node.get('type', {}).get('qualType', '')
            agg = ('[' in t) or ((t.startswith('struct ') or t.startswith('union ') or ' struct ' in t) and not t.endswith('*'))
            if not agg and node.get('type', {}).get('desugaredQualType'):
                d = node['type']['desugaredQualType']
                agg = ('[' in d) or ((d.startswith('struct ') or d.startswith('union ')) and not d.endswith('*'))
            return ('{ 0 }' if agg else '0'), None
        if k == 'InitListExpr':
            b, e = off(node['range']['begin']), off(node['range']['end'], True)
            src = self.text[b:e] if b is not None and e is not None else ''
            if src.startswith('{') and not re.search(r'(\.[A-Za-z_]\w*|\[[^\]]*\])\s*=', src):
                return src, self.designator_before(b)
            return self.initlist(node, indent + '\t', False), self.designator_before(b)
        # strip implicit casts that have no range of their own
        r = node.get('range')
        b, e = off(r['begin']), off(r['end'], True)
        return self.text[b:e], self.designator_before(b)

    def initlist(self, node, indent, top):
        inner = node.get('inner')
        if inner is None:
            inner = node.get('array_filler', [None])[1:]
        items = []
        for c in inner:
            items.append(self.elem(c, indent, top))
        # drop trailing implicit zeros
        while items and items[-1][0] in ('0', '{ 0 }') and items[-1][1] is None and len(items) > 1:
            last = inner[len(items) - 1]
            if last['kind'] != 'ImplicitValueInitExpr':
                break
            items.pop()
        if 'field' in node and node['field'].get('name'):
            # union initialised through a member other than the first?
            items_note = node['field']['name']
        multiline = top or len(items) > 6 or any('\n' in it[0] for it in items)
        if not multiline:
            return '{ ' + ', '.join(it[0] for it in items) + ' }'
        lines = []
        for v, d in items:
            v = v.strip()
            if d:
                lines.append('%s\t/* %s */ %s,' % (indent, d, v))
            else:
                lines.append('%s\t%s,' % (indent, v))
        return '{\n' + '\n'.join(lines) + '\n' + indent + '}'


def main():
    argv = sys.argv[1:]
    i = argv.index('--')
    pos, cargs = argv[:i], argv[i + 1:]
    file, name = pos[0], pos[1]
    line = int(pos[2]) if len(pos) > 2 else None
    text = open(file, 'rb').read().decode('latin-1')
    objs = load(file, name, cargs)
    vars_ = []
    for o in objs:
        find_vars(o, name, vars_)
    cand = []
    for v in vars_:
        il = [c for c in v.get('inner', []) if c['kind'] == 'InitListExpr']
        if not il:
            continue
        b = off(il[0]['range']['begin'])
        ln = text.count('\n', 0, b) + 1
        if line and abs(ln - line) > 3:
            continue
        cand.append((b, il[0], v))
    # dedupe same offset
    seen = {}
    for b, il, v in cand:
        seen[b] = (il, v)
    if len(seen) != 1:
        print('ambiguous/no match for %s: %d' % (name, len(seen)), file=sys.stderr)
        return 1
    b, (il, v) = list(seen.items())[0]
    e = off(il['range']['end'], True)
    assert text[b] == '{' and text[e - 1] == '}', (text[b], text[e - 1])
    ls = text.rfind('\n', 0, b) + 1
    indent = re.match(r'[ \t]*', text[ls:]).group(0)
    rw = Rewriter(text)
    new = rw.initlist(il, indent, True)
    text = text[:b] + new + text[e:]
    open(file, 'wb').write(text.encode('latin-1'))
    print('rewrote %s in %s' % (name, file))
    return 0


if __name__ == '__main__':
    sys.exit(main())
