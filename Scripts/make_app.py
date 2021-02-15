import argparse
import os
import shutil
import subprocess

from typing import List

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('app_name')
    parser.add_argument('--depends', nargs='*', default=[])
    parser.add_argument('--objs', nargs='*', default=[])
    parser.add_argument('--frameworks',nargs='*', default=[])
    parser.add_argument('--debug', action='store_true')
    parser.add_argument('--winx', default=1200, type=int)
    args = parser.parse_args()
    run(**vars(args))

def run(app_name:str, **kwargs) -> None:
    base_folder = '{}.app'.format(app_name)
    setup_folders(base_folder)
    copy_files(base_folder, app_name)
    build_program(app_name, **kwargs)
    zip_it(app_name)

def setup_folders(base_folder:str) -> None:
    os.makedirs(os.path.join(base_folder, 'Contents/Frameworks'))
    os.makedirs(os.path.join(base_folder, 'Contents/MacOS'))

def copy_framework(base_folder:str, name:str) -> None:
    framework_dir = '/Library/Frameworks'
    fw_name = '{}.framework'.format(name)
    source = os.path.join(framework_dir, fw_name)
    target = os.path.join(base_folder, 'Contents/Frameworks', fw_name)
    os.mkdir(target)
    for f in os.listdir(source):
        p = os.path.join(source, f)
        if os.path.isfile(p):
            shutil.copy(p, os.path.join(target, f))
        if os.path.isdir(p):
            shutil.copytree(p, os.path.join(target, f))

def copy_plist(base_folder:str, app_name:str) -> None:
    plist = os.path.join(app_name, 'Info.plist')
    dest = os.path.join(base_folder, 'Contents/Info.plist')
    shutil.copyfile(plist, dest)

def copy_files(base_folder:str, app_name:str) -> None:
    copy_framework(base_folder, 'SDL2')
    copy_framework(base_folder, 'SDL2_ttf')
    copy_framework(base_folder, 'SDL2_mixer')
    copy_framework(base_folder, 'SDL2_image')
    copy_plist(base_folder, app_name)
    shutil.copytree(
        os.path.join(app_name, 'Resources'),
        os.path.join(base_folder, 'Contents/Resources')
        )
    icondir = next((f for f in os.listdir(os.path.join(app_name, 'Resources')) if f.endswith('.iconset')), None)
    if icondir:
        icon_dst = os.path.join(base_folder, 'Contents/Resources', icondir)
        subprocess.check_call(['iconutil', '-c', 'icns', icon_dst])
        shutil.rmtree(icon_dst)

def build_program(app_name:str, debug:bool, depends:List[str], objs:List[str], frameworks:List[str], winx:int) -> None:
    INCLUDE_DIRS = [
        '.',
        'DataStructure',
        'Allocators',
        'RunTime',
        'Prof',
        'UI',
        'RNG',
        'GUI',
        'Renderer',
        'Dungeons',
        'Utils',
        'RPG',
        'Data',
        'Platform',
        'PythonEmbed',
        'NLP',
        '/Users/drpriver/External/pcre2-10.32/build',
        app_name,
    ]
    INCLUDE_FLAGS = ['-I{}'.format(d) for d in INCLUDE_DIRS]
    # INCLUDE_FLAGS.extend(['-F/Library/Frameworks', '-isystem',  '/Library/Frameworks/Python.framework/Headers', '-framework', 'Python'])
    COMMON_FLAGS=[
        # '-DHAS_PYTHON',
        '-DProf_ENABLED',
        '-DDARWIN',
        '-march=native',
        '--rtlib=compiler-rt',
        '-std=gnu17',
        '-Wno-everything',
    ]
    DEBUG_FLAGS=[
        '-DLOG_LEVEL=5',
        '-DDETERMINISTIC_RNG',
        '-DDEBUG',
        '-fsanitize=undefined',
        '-fsanitize=address',
        '-O0',
        '-g',
    ]
    FAST_FLAGS=[
        '-Ofast',
        '-DLOG_LEVEL=0',
        '-flto',
    ]
    RELATIVE_FLAGS=[
        '-rpath',
        '@executable_path/../Frameworks',
    ]
    SDL_FLAGS=[
        '-F{}.app/Contents/Frameworks'.format(app_name),
        '-framework', 'SDL2',
        '-framework', 'SDL2_image',
        '-framework', 'SDL2_ttf',
        '-framework', 'SDL2_mixer',
    ]
    OBJS=[
        '/Users/drpriver/Downloads/pcre2-10.32/build/libpcre2-8.a',
    ]
    BUILD_FLAGS = INCLUDE_FLAGS + COMMON_FLAGS + (DEBUG_FLAGS if debug else FAST_FLAGS)+ RELATIVE_FLAGS + SDL_FLAGS
    # for depend in depends:
        # subprocess.check_call(['make', depend])
    objs = ['Objs/{}.o'.format(o) for o in objs]
    for obj in objs:
        subprocess.check_call(['make', obj])
    frameworks_ = []
    for fw in frameworks:
        frameworks_.append('-framework')
        frameworks_.append(fw)
    build_command = [
        'clang',
        *BUILD_FLAGS,
        '{}/{}.c'.format(app_name, app_name.lower()),
        '-o', '{0}.app/Contents/MacOS/{0}'.format(app_name),
        '-DWINDOW_NAME="{}"'.format(app_name),
        '-DWINDOW_SIZE_X={}'.format(winx),
        '-DGUIERROR',
        *OBJS, *objs, *frameworks_,
        ]
    print(*build_command)
    subprocess.check_call(build_command)

def zip_it(app_name:str) -> None:
    shutil.make_archive(app_name, 'zip', base_dir='{}.app'.format(app_name))

if __name__ == '__main__':
    main()
