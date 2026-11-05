#!/usr/bin/env python3
"""Compile selected, unmodified production bodies against controlled host fakes.

The application translation units require the XDK/SDL/D3D stack. Extraction keeps
these focused tests on the current implementation without adding test switches
to production. This is not a full application or SDK compatibility build.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import os

ROOT = Path(__file__).resolve().parents[1]
TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true', help='Enable ASan, UBSan and leak checks')
    parser.add_argument('--cxx', default='g++')
    args = parser.parse_args()
    build = ROOT / 'build' / ('returnapp-sanitized' if args.sanitize else 'returnapp-tests')
    build.mkdir(parents=True, exist_ok=True)
    coverage = []

    def source(path):
        try:
            return (ROOT / path).read_text(encoding='utf-8-sig')
        except UnicodeDecodeError:
            # Some upstream headers still use Windows-1252 comments/literals.
            return (ROOT / path).read_text(encoding='cp1252')

    def located(path, start, end):
        text = source(path)
        code = text[start:end]
        line = text.count('\n', 0, start) + 1
        coverage.append({'file': path, 'line': line, 'sha256': hashlib.sha256(code.encode()).hexdigest()})
        return '\n#line %d "%s"\n%s\n' % (line, path, code)

    def block(path, marker, semicolon=False):
        text = source(path)
        if text.count(marker) != 1:
            raise ValueError('Expected one production definition: ' + marker)
        start = text.index(marker)
        opening = text.index('{', start)
        depth = 0
        for token in TOKEN.finditer(text, opening):
            if token.group() == '{':
                depth += 1
            elif token.group() == '}':
                depth -= 1
                if depth == 0:
                    end = token.end()
                    if semicolon:
                        end = text.index(';', end) + 1
                    return located(path, start, end)
        raise ValueError('Unclosed production definition: ' + marker)

    def between(path, begin, end, include_end=False):
        text = source(path)
        if text.count(begin) != 1:
            raise ValueError('Expected one production section: ' + begin)
        start = text.index(begin)
        stop = text.index(end, start + len(begin))
        if include_end:
            stop += len(end)
        return located(path, start, stop)

    types = [
        block('src/const/constant.h', 'enum status_emu', True),
        between('src/const/constant.h', 'typedef enum {JOY_BUTTON_A', '} joystickButtons;', True),
        block('src/io/hotkeys.h', 'typedef enum{', True),
        block('src/menus/gestormenus.h', 'enum TipoOpcion', True),
        block('src/menus/gestormenus.h', 'enum ACTION_ASK', True),
        block('src/menus/gestormenus.h', 'enum CONFIG_STATUS', True),
        block('src/menus/gestormenus.h', 'struct t_option_action', True),
        block('src/io/statesram.h', 'typedef enum {SAVE_STATE', True),
        'namespace cfg {\n' + block('src/const/cfgconst.h', 'typedef enum {emulators', True) + '}\n',
        between('src/beans/structures.h', '#ifdef _XBOX\nstatic const int sdlIndexOfLogicalBtn[]', '#endif', True),
        block('src/beans/structures.h', 'static inline int sdlBtnOf('),
    ]
    (build / 'production_types.inc').write_text(''.join(types))
    menu = between('src/menus/gestormenus.cpp', '\t#ifdef _XBOX\n\tif (!refConfig->configMain[cfg::returnApp]', '\t#endif', True)
    hotkey = between('src/menus/gamemenu.cpp', '\t\tcase HK_EXIT_GAME:', '\t\tcase HK_VIEW_MENU:')
    default = between('src/io/cfgloader.cpp', '\tconfigMain[cfg::returnApp] =', '\n\n')
    functions = [
        block('src/io/statesram.h', 'bool saveSramForExit('),
        block('src/io/hotkeys.cpp', 'Hotkeys::Hotkeys('),
        block('src/io/hotkeys.cpp', 'HOTKEYS_LIST Hotkeys::procesarHotkeys('),
        'void GameMenu::exitHotkeyForTest(int key) { switch (key) {\n' + hotkey + 'default: break; } }\n',
        'void GestorMenus::buildReturnEntryForTest(CfgLoader* refConfig) { (void)refConfig;\n' + menu + '}\n',
        'void CfgLoader::initReturnConfigForTest() {\n' + default + '}\n',
        block('src/menus/gestormenus.cpp', 'std::string GestorMenus::volverAplicacion('),
        block('src/menus/gestormenus.cpp', 'std::string GestorMenus::volverEmulacion('),
        block('src/menus/gestormenus.cpp', 'std::string GestorMenus::salirEmulacion('),
        block('src/io/inputsmenu.h', 'bool processActions('),
        block('src/so/launcher.cpp', 'void Launcher::launchReturnApp('),
        block('src/salvia.cpp', 'void closeGame('),
        '#ifdef _XBOX\n' + block('src/salvia.cpp', 'struct ReturnAppHost', True) +
        block('src/salvia.cpp', 'static void returnToConfiguredApp(') + '\n#endif\n',
        block('src/salvia.cpp', 'static void __declspec(noinline) runGameLoop('),
    ]
    (build / 'production_functions.inc').write_text(''.join(functions))
    flags = ['-Wall', '-Wextra', '-Werror', '-pedantic', '-g', '-Isrc', '-Itests/fakes', '-I' + str(build)]
    if args.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    results = []
    suites = [('portable', 'tests/returnapp_test.cpp', ['-std=c++03']),
              ('xbox-fakes', 'tests/production_exit_test.cpp', ['-std=c++11', '-D_XBOX']),
              ('no-sram-fakes', 'tests/production_exit_test.cpp', ['-std=c++11', '-D_XBOX', '-DNO_SRAM']),
              ('windows-fakes', 'tests/production_exit_test.cpp', ['-std=c++11', '-DWIN'])]
    for name, test, mode in suites:
        binary = build / name
        command = [args.cxx] + mode + flags + [test, '-o', str(binary)]
        if name != 'portable':
            command += ['-Wl,--wrap=fopen,--wrap=fwrite,--wrap=fflush,--wrap=fclose']
        compiled = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        (build / (name + '-build.log')).write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise SystemExit(compiled.stdout + compiled.stderr)
        run = subprocess.run([str(binary), str(build)], cwd=ROOT, env=env, text=True, capture_output=True)
        (build / (name + '-run.log')).write_text(run.stdout + run.stderr)
        if run.returncode:
            raise SystemExit(run.stdout + run.stderr)
        results.append({'suite': name, 'passed': True, 'summary': run.stdout.strip().splitlines()[-1],
                        'compile_command': command})
        print(name + ': ' + results[-1]['summary'], flush=True)
    report = {'sanitizers': ['address', 'undefined', 'leak'] if args.sanitize else [],
              'suites': results, 'extracted_production_sections': coverage,
              'limits': 'Selected source bodies and API fakes; no full app build, real SDL scheduling, core emulation or Xbox API validation.'}
    (build / 'results.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
