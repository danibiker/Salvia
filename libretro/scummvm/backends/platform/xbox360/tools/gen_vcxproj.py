#!/usr/bin/env python3
"""Generate backends/platform/xbox360/scummvm/scummvm.vcxproj (+ .filters, .sln) for Visual Studio 2010.
usage: gen_vcxproj.py <srclist> <outdir>"""
import sys, os, uuid

SRCLIST, OUT = sys.argv[1], sys.argv[2]
GUID = '{4A410493-85D2-4EEF-9AFC-24262B1C1D00}'
UP = '..\\..\\..\\..\\'   # from backends/platform/xbox360/scummvm to the ScummVM root

EXCLUDE_PREFIX = (
    'backends/fs/chroot/', 'backends/fs/posix', 'backends/plugins/posix/', 'backends/saves/posix/',
    'backends/taskbar/unity/', 'backends/dialogs/gtk/',
    'backends/midi/alsa.cpp', 'backends/midi/dmedia.cpp', 'backends/midi/seq.cpp', 'backends/midi/sndio.cpp',
    'backends/midi/stmidi.cpp', 'backends/midi/timidity.cpp',
    'common/std/std.cpp', 'base/test_new_standards.cpp',
    # Windows desktop (SDL backend) only, not used by the libretro core
    'backends/audiocd/win32/', 'backends/dialogs/win32/', 'backends/taskbar/win32/', 'backends/updates/win32/',
    'backends/printing/win32/', 'backends/plugins/win32/', 'backends/saves/windows/',
)
# libretro-common parts that are only needed in some configurations
EXTRA = []

files = [l.strip() for l in open(SRCLIST) if l.strip()]
files = [f for f in files if not f.startswith(EXCLUDE_PREFIX)]
files = sorted(set(files + EXTRA))

ENGINES = 'ENABLE_SCUMM=STATIC_PLUGIN;ENABLE_SCUMM_7_8;ENABLE_HE;ENABLE_BLADERUNNER=STATIC_PLUGIN;ENABLE_STARK=STATIC_PLUGIN;ENABLE_GRIM=STATIC_PLUGIN;ENABLE_MONKEY4;ENABLE_SCI=STATIC_PLUGIN;ENABLE_SCI32'
# Engines built only for Win32: on Xbox 360 they run too slowly for now.
# Their sources are excluded from the Xbox 360 build and their ENABLE_* defines
# are not set there, so neither the engine nor its detection is linked in.
WIN32_ONLY_ENGINES = [
    ('engines/bladerunner/', 'ENABLE_BLADERUNNER=STATIC_PLUGIN'),
    ('engines/grim/', 'ENABLE_GRIM=STATIC_PLUGIN;ENABLE_MONKEY4'),
    ('engines/stark/', 'ENABLE_STARK=STATIC_PLUGIN'),
]
ENGINES_WIN = ENGINES
_xbox_off = set(d for _, ds in WIN32_ONLY_ENGINES for d in ds.split(';'))
ENGINES_XBOX = ';'.join(d for d in ENGINES.split(';') if d not in _xbox_off)
FEATURES = ('__LIBRETRO__;NONSTANDARD_PORT;HAVE_CONFIG_H;DETECTION_STATIC;USE_LIBCO;RELEASE_BUILD;'
            'USE_RGB_COLOR;USE_OSD;USE_TRANSLATION;ENABLE_VKEYBD;USE_HIGHRES;USE_ZLIB;USE_MIDI;'
            'USE_BINK;USE_INDEO3;USE_INDEO45;USE_CDTOONS;USE_JYV1;USE_TRUEMOTION1;USE_XAN;'
            'USE_FMTOWNS_PC98_AUDIO;USE_SID_AUDIO;USE_VGMTRANS_AUDIO;'
            'USE_MAD;FPM_DEFAULT;USE_VORBIS;USE_OGG;USE_FLAC;FLAC__NO_DLL;'
            'USE_TINYGL;USE_FREETYPE2;FT2_BUILD_LIBRARY;USE_PNG;SCUMMVM_PNG_PREFIX;PNG_NO_CONFIG_H;'
            'SIZEOF_SIZE_T=4;CORE_NAME="ScummVM";CORE_EXTENSIONS="scummvm";'
            'SCUMMVM_SYSTEM_SUBDIR="scummvm";SCUMMVM_EXTRA_SUBDIR="extra";SCUMMVM_THEME_SUBDIR="theme";'
            '_CRT_SECURE_NO_WARNINGS;_CRT_NONSTDC_NO_WARNINGS')
WIN_DEFS = 'WIN32;_WINDOWS;NOMINMAX;_LIB'
XBOX_DEFS = '_XBOX;_XBOX360;_LIB;MSB_FIRST'  # MSB_FIRST: big-endian PPC, used by libmad and libFLAC

INCS = ';'.join([
    '$(SCUMMVM_ROOT)backends\\platform\\libretro\\include',
    '$(SCUMMVM_ROOT)',
    '$(SCUMMVM_ROOT)engines',
    '$(SCUMMVM_ROOT)backends\\vkeybd',
    '$(SCUMMVM_ROOT)backends\\platform\\libretro\\deps\\libretro-common\\include',
    '$(ZLIB_INCLUDE_DIR)',
    '$(SCUMMVM_ROOT)backends\\platform\\libretro\\deps\\libretro-deps\\libogg\\include',
    '$(SCUMMVM_ROOT)backends\\platform\\libretro\\deps\\libretro-deps\\libvorbis\\include',
    '$(SCUMMVM_ROOT)backends\\platform\\libretro\\deps\\libretro-deps\\libFLAC\\include',
    '$(SCUMMVM_ROOT)backends\\platform\\libretro\\deps\\libretro-deps\\freetype\\include',
    '$(SCUMMVM_ROOT)backends\\platform\\libretro\\deps\\libretro-deps\\libmad',
    '$(SCUMMVM_ROOT)backends\\platform\\libretro\\deps\\libretro-deps\\libpng',
])

CONFIGS = [('Debug', 'Win32'), ('Release', 'Win32'), ('Debug', 'Xbox 360'), ('Release', 'Xbox 360')]

WIN32_ONLY = ('backends/platform/libretro/deps/libretro-deps/libFLAC/share/win_utf8_io/',
              'backends/platform/sdl/win32/') + tuple(d for d, _ in WIN32_ONLY_ENGINES)

def objname(f):
    base = os.path.splitext(f)[0].replace('/', '_').replace('-', '_')
    return '$(IntDir)' + base + '.obj'

def cl_settings(cfg, plat):
    debug = cfg == 'Debug'
    win = plat == 'Win32'
    d = []
    d.append('<PrecompiledHeader>NotUsing</PrecompiledHeader>')
    # By file extension (.c as C, .cpp as C++): the Xbox 360 platform defaults to /TP otherwise
    d.append('<CompileAs>Default</CompileAs>')
    d.append('<WarningLevel>Level2</WarningLevel>')
    defs = (WIN_DEFS if win else XBOX_DEFS) + ';NDEBUG;' + FEATURES + ';' + (ENGINES_WIN if win else ENGINES_XBOX)
    d.append('<PreprocessorDefinitions>%s;%%(PreprocessorDefinitions)</PreprocessorDefinitions>' % defs)
    d.append('<AdditionalIncludeDirectories>%s;%%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>' % INCS)
    d.append('<Optimization>%s</Optimization>' % ('Disabled' if debug else 'MaxSpeed'))
    if debug:
        d.append('<DebugInformationFormat>ProgramDatabase</DebugInformationFormat>')
    else:
        d.append('<InlineFunctionExpansion>AnySuitable</InlineFunctionExpansion>')
        d.append('<IntrinsicFunctions>true</IntrinsicFunctions>')
        d.append('<FavorSizeOrSpeed>Speed</FavorSizeOrSpeed>')
        d.append('<WholeProgramOptimization>false</WholeProgramOptimization>')
    d.append('<RuntimeLibrary>MultiThreaded</RuntimeLibrary>')
    d.append('<MultiProcessorCompilation>true</MultiProcessorCompilation>')
    d.append('<MinimalRebuild>false</MinimalRebuild>')
    d.append('<StringPooling>true</StringPooling>')
    d.append('<FunctionLevelLinking>true</FunctionLevelLinking>')
    d.append('<BufferSecurityCheck>false</BufferSecurityCheck>')
    d.append('<FloatingPointModel>Fast</FloatingPointModel>')
    d.append('<RuntimeTypeInfo>true</RuntimeTypeInfo>')
    d.append('<ExceptionHandling>Sync</ExceptionHandling>')
    d.append('<DisableSpecificWarnings>4481;4482;4480;4351;4355;4244;4267;4305;4800;4996;4068;4146;4200;4065;4554;4018;4099;4005;4250;4103;%(DisableSpecificWarnings)</DisableSpecificWarnings>')
    opts = '/Zm200 '
    if win:
        opts += '/bigobj '
    d.append('<AdditionalOptions>%s%%(AdditionalOptions)</AdditionalOptions>' % opts)
    if not win:
        d.append('<PreschedulingOptimization>false</PreschedulingOptimization>')
        d.append('<TrapIntegerDividesOptimization>false</TrapIntegerDividesOptimization>')
        d.append('<InlineAssemblyOptimization>false</InlineAssemblyOptimization>')
    return '\n      '.join(d)

out = []
w = out.append
w('<?xml version="1.0" encoding="utf-8"?>')
w('<Project DefaultTargets="Build" ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">')
w('  <ItemGroup Label="ProjectConfigurations">')
for c, p in CONFIGS:
    w('    <ProjectConfiguration Include="%s|%s">' % (c, p))
    w('      <Configuration>%s</Configuration>' % c)
    w('      <Platform>%s</Platform>' % p)
    w('    </ProjectConfiguration>')
w('  </ItemGroup>')
w('  <PropertyGroup Label="Globals">')
w('    <ProjectGuid>%s</ProjectGuid>' % GUID)
w('    <Keyword>Win32Proj</Keyword>')
w('    <RootNamespace>scummvm</RootNamespace>')
w('  </PropertyGroup>')
w('  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />')
for c, p in CONFIGS:
    w('  <PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'%s|%s\'" Label="Configuration">' % (c, p))
    w('    <ConfigurationType>StaticLibrary</ConfigurationType>')
    w('    <UseDebugLibraries>%s</UseDebugLibraries>' % ('true' if c == 'Debug' else 'false'))
    w('    <WholeProgramOptimization>false</WholeProgramOptimization>')
    w('    <CharacterSet>%s</CharacterSet>' % ('Unicode' if p == 'Win32' else 'MultiByte'))
    w('  </PropertyGroup>')
w('  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />')
w('  <ImportGroup Label="ExtensionSettings">')
w('  </ImportGroup>')
for c, p in CONFIGS:
    w('  <ImportGroup Label="PropertySheets" Condition="\'$(Configuration)|$(Platform)\'==\'%s|%s\'">' % (c, p))
    w('    <Import Project="$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props" Condition="exists(\'$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\')" Label="LocalAppDataPlatform" />')
    w('  </ImportGroup>')
w('  <PropertyGroup Label="UserMacros">')
w('    <!-- ScummVM source root (this project lives in backends\\platform\\xbox360\\scummvm) -->')
w('    <SCUMMVM_ROOT>$(ProjectDir)%s</SCUMMVM_ROOT>' % UP)
w('    <!-- zlib headers: the zlib library itself is linked by the Salvia frontend (zlib.lib) -->')
w('    <ZLIB_INCLUDE_DIR Condition="\'$(ZLIB_INCLUDE_DIR)\'==\'\'">$(SCUMMVM_ROOT)..\\..\\libs\\zlib</ZLIB_INCLUDE_DIR>')
w('  </PropertyGroup>')
for c, p in CONFIGS:
    w('  <PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'%s|%s\'">' % (c, p))
    w('    <OutDir>$(ProjectDir)$(Configuration)\\$(Platform)\\</OutDir>')
    w('    <IntDir>$(ProjectDir)$(Configuration)\\$(Platform)\\obj\\</IntDir>')
    w('    <TargetName>scummvm</TargetName>')
    if p != 'Win32':
        w('    <OutputFile>$(OutDir)$(TargetName)$(TargetExt)</OutputFile>')
    w('  </PropertyGroup>')
for c, p in CONFIGS:
    w('  <ItemDefinitionGroup Condition="\'$(Configuration)|$(Platform)\'==\'%s|%s\'">' % (c, p))
    w('    <ClCompile>')
    w('      ' + cl_settings(c, p))
    w('    </ClCompile>')
    w('    <Lib>')
    w('      <OutputFile>$(OutDir)$(TargetName)$(TargetExt)</OutputFile>')
    w('    </Lib>')
    w('  </ItemDefinitionGroup>')
w('  <ItemGroup>')
for f in files:
    path = UP + f.replace('/', '\\')
    w('    <ClCompile Include="%s">' % path)
    w('      <ObjectFileName>%s</ObjectFileName>' % objname(f))
    if f.startswith(WIN32_ONLY):
        w('      <ExcludedFromBuild Condition="\'$(Platform)\'==\'Xbox 360\'">true</ExcludedFromBuild>')
    w('    </ClCompile>')
w('  </ItemGroup>')
w('  <ItemGroup>')
for h in ['backends/platform/libretro/include/msvc2010_compat.h', 'backends/platform/libretro/include/portdefs.h',
          'backends/platform/libretro/include/config.h']:
    w('    <ClInclude Include="%s" />' % (UP + h.replace('/', '\\')))
w('  </ItemGroup>')
w('  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />')
w('  <ImportGroup Label="ExtensionTargets">')
w('  </ImportGroup>')
w('</Project>')
open(os.path.join(OUT, 'scummvm.vcxproj'), 'w', newline='\r\n', encoding='utf-8-sig').write('\n'.join(out) + '\n')

# filters
filt = {}
def fdir(f):
    d = os.path.dirname(f)
    if d.startswith('backends/platform/libretro/deps/libretro-common'):
        d = d.replace('backends/platform/libretro/deps/', 'libretro/')
    elif d.startswith('backends/platform/libretro'):
        d = d.replace('backends/platform/libretro', 'libretro')
    return d.replace('/', '\\')
dirs = set()
for f in files:
    d = fdir(f)
    parts = d.split('\\')
    for i in range(1, len(parts) + 1):
        dirs.add('\\'.join(parts[:i]))
o = []
o.append('<?xml version="1.0" encoding="utf-8"?>')
o.append('<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">')
o.append('  <ItemGroup>')
for d in sorted(dirs):
    o.append('    <Filter Include="%s">' % d)
    o.append('      <UniqueIdentifier>{%s}</UniqueIdentifier>' % str(uuid.uuid5(uuid.NAMESPACE_URL, 'scummvm/' + d)).upper())
    o.append('    </Filter>')
o.append('    <Filter Include="compat">')
o.append('      <UniqueIdentifier>{%s}</UniqueIdentifier>' % str(uuid.uuid5(uuid.NAMESPACE_URL, 'scummvm/compat')).upper())
o.append('    </Filter>')
o.append('  </ItemGroup>')
o.append('  <ItemGroup>')
for f in files:
    o.append('    <ClCompile Include="%s">' % (UP + f.replace('/', '\\')))
    o.append('      <Filter>%s</Filter>' % fdir(f))
    o.append('    </ClCompile>')
o.append('  </ItemGroup>')
o.append('  <ItemGroup>')
for h in ['backends/platform/libretro/include/msvc2010_compat.h', 'backends/platform/libretro/include/portdefs.h',
          'backends/platform/libretro/include/config.h']:
    o.append('    <ClInclude Include="%s">' % (UP + h.replace('/', '\\')))
    o.append('      <Filter>compat</Filter>')
    o.append('    </ClInclude>')
o.append('  </ItemGroup>')
o.append('</Project>')
open(os.path.join(OUT, 'scummvm.vcxproj.filters'), 'w', newline='\r\n', encoding='utf-8-sig').write('\n'.join(o) + '\n')

# solution
sln = ['', 'Microsoft Visual Studio Solution File, Format Version 11.00', '# Visual Studio 2010',
       'Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "scummvm", "scummvm.vcxproj", "%s"' % GUID, 'EndProject', 'Global',
       '\tGlobalSection(SolutionConfigurationPlatforms) = preSolution']
for c, p in CONFIGS:
    sln.append('\t\t%s|%s = %s|%s' % (c, p, c, p))
sln.append('\tEndGlobalSection')
sln.append('\tGlobalSection(ProjectConfigurationPlatforms) = postSolution')
for c, p in CONFIGS:
    sln.append('\t\t%s.%s|%s.ActiveCfg = %s|%s' % (GUID, c, p, c, p))
    sln.append('\t\t%s.%s|%s.Build.0 = %s|%s' % (GUID, c, p, c, p))
sln.append('\tEndGlobalSection')
sln.append('\tGlobalSection(SolutionProperties) = preSolution')
sln.append('\t\tHideSolutionNode = FALSE')
sln.append('\tEndGlobalSection')
sln.append('EndGlobal')
open(os.path.join(OUT, 'scummvm.sln'), 'w', newline='\r\n', encoding='utf-8-sig').write('\n'.join(sln) + '\n')
print(len(files), 'files')
