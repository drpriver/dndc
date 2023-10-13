#!/usr/bin/env python3
# Copyright © 2021-2023, David Priver <david@davidpriver.com>
#
import sys
pydndcver = '1.2.1'
pydndeditver = '1.0.2'
def win_install_deps() -> bool:
    import sys
    assert sys.platform == 'win32'
    import subprocess
    from ctypes import windll # type: ignore
    from ctypes import pointer
    import time
    MB_YESNO = 0x00000004
    MB_ICONEXCLAMATION = 0x00000030
    response = windll.user32.MessageBoxW(0, 'Dependencies are not installed. Install Dependencies?\n\nThis will take up around 150mb.', 'Install Dependencies?', MB_YESNO | MB_ICONEXCLAMATION)
    IDYES = 6
    if response != IDYES:
        return False
    instance = windll.kernel32.GetModuleHandleW(0)
    WM_USER = 1024
    PBM_SETMARQUEE = WM_USER + 10
    from ctypes.wintypes import MSG

    class ProgressBar:
        def __init__(self):
            PBS_MARQUEE = 8
            CW_USEDEFAULT = 0x80000000
            WS_VISIBLE = 0x10000000
            WS_CAPTION = 0x00C00000
            WS_THICKFRAME = 0x00040000
            WS_BORDER = 0x00800000
            WS_POPUP = 0x80000000
            style = WS_THICKFRAME | WS_POPUP | WS_VISIBLE | PBS_MARQUEE
            self.bar = windll.user32.CreateWindowExW(0, "msctls_progress32", "Installing Dependencies",  style, 500, 500, 300, 40, 0, 0, instance, 0)
            result = windll.user32.SendMessageW(self.bar, PBM_SETMARQUEE, 1, 10)
        def close(self):
            windll.user32.CloseWindow(self.bar)

    def pump_messages() -> None:
        # The progress bar gets messages from our message loop, so we need
        # to ensure the messages are getting dispatched.
        msg = MSG()
        pmsg = pointer(msg)
        PM_REMOVE = 0x0001
        PeekMessageW = windll.user32.PeekMessageW
        TranslateMessage = windll.user32.TranslateMessage
        DispatchMessageW = windll.user32.DispatchMessageW
        while PeekMessageW(pmsg, 0, 0, 0, PM_REMOVE) != 0:
            TranslateMessage(pmsg)
            DispatchMessageW(pmsg)

    pb = ProgressBar()
    command = [sys.executable, '-m', 'pip', 'install', 'PySide6', f'pydndc=={pydndcver}', f'PyDndEdit=={pydndeditver}', '-U']
    process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    while process.poll() is None:
        time.sleep(0.01)
        pump_messages()
        # pb.step()
    pb.close()
    MB_OK = 0
    if process.returncode != 0:
        windll.user32.MessageBoxW(0, 'Installing Dependencies Failed', 'Fail!', MB_OK)
        return False

    windll.user32.MessageBoxW(0, 'Dependencies installed successfully', 'Success!', MB_OK)
    return True

def win_already_installed() -> None:
    from ctypes import windll # type: ignore
    MB_OK = 0
    windll.user32.MessageBoxW(0, 'Dependencies are already installed', 'Already Installed', MB_OK)

def unix_install_deps() -> bool:
    import subprocess
    import sys
    try:
        response = input('Install dependencies? Y/n')
    except:
        return False
    if response and not response.strip().lower().startswith('y'):
        return False
    command = [sys.executable, '-m', 'pip', 'install', 'PySide6', f'pydndc=={pydndcver}', f'PyDndEdit=={pydndeditver}', '-U']
    process = subprocess.run(command, check=True)
    return True

def install_deps() -> bool:
    if sys.platform == 'win32':
        return win_install_deps()
    else:
        return unix_install_deps()

def unix_already_installed() -> None:
    print('Dependencies are already installed')


def ensure_deps() -> None:
    import sys
    try:
        import PySide6
        import pydndc
        if pydndc.version[:2] != (1, 0):
            if not install_deps():
                return
        import PyDndEdit
    except ImportError as e:
        if not install_deps():
            return
    import PyDndEdit.dndedit

if __name__ == '__main__':
    ensure_deps()
