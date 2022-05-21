import argparse
import os
import sys
import shutil

def main() -> None:
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    if sys.platform == 'win32':
        default = '~/_vim'
    else:
        default = '~/.vim'
    default = os.path.expanduser(default)
    parser.add_argument('--install-directory', type=str, default=default, help='Where to install the vim files to')
    args = parser.parse_args()
    run(**vars(args))

def run(install_directory:str) -> None:
    thisdir = os.path.dirname(os.path.abspath(__file__))
    if not os.path.isdir(install_directory):
        raise ValueError(f'The given install directory does not exist: {install_directory!r}')
    syndir = os.path.join(install_directory, 'syntax')
    ftdetect = os.path.join(install_directory, 'ftdetect')
    ftplugin = os.path.join(install_directory, 'ftplugin')
    os.makedirs(syndir, exist_ok=True)
    os.makedirs(ftdetect, exist_ok=True)
    os.makedirs(ftplugin, exist_ok=True)
    pj = os.path.join
    shutil.copy(pj(thisdir, 'syntax',   'dnd.vim'), pj(syndir,   'dnd.vim'))
    shutil.copy(pj(thisdir, 'ftdetect', 'dnd.vim'), pj(ftdetect, 'dnd.vim'))
    shutil.copy(pj(thisdir, 'ftplugin', 'dnd.vim'), pj(ftplugin, 'dnd.vim'))

if __name__ == '__main__':
    main()
