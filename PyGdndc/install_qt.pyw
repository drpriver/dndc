def install_qt() -> None:
    import sys
    import subprocess
    from ctypes import windll
    from ctypes import pointer
    import time
    MB_YESNO = 0x00000004
    MB_ICONEXCLAMATION = 0x00000030
    response = windll.user32.MessageBoxW(0, 'Qt is not installed. Install Qt?', 'Install Qt?', MB_YESNO | MB_ICONEXCLAMATION)
    IDYES = 6
    if response != IDYES:
        return
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
            self.bar = windll.user32.CreateWindowExW(0, "msctls_progress32", "Installing Qt",  style, 500, 500, 300, 40, 0, 0, instance, 0)
            result = windll.user32.SendMessageW(self.bar, PBM_SETMARQUEE, 1, 10)
        def close(self):
            windll.user32.CloseWindow(self.bar)
    def pump_messages():
        msg = MSG()
        pmsg = pointer(msg)
        PM_REMOVE = 0x0001
        while windll.user32.PeekMessageW(pmsg, 0, 0, 0, PM_REMOVE) != 0:
            windll.user32.TranslateMessage(pmsg)
            windll.user32.DispatchMessageW(pmsg)


    pb = ProgressBar()
    command = [sys.executable, '-m', 'pip', 'install', 'PySide2==5.15.2']
    process = subprocess.Popen(command)
    while process.poll() is None:
        time.sleep(0.01)
        pump_messages()
        # pb.step()
    pb.close()
    MB_OK = 0
    windll.user32.MessageBoxW(0, 'Qt was installed successfully', 'Success!', MB_OK)

def already_installed() -> None:
    from ctypes import windll
    MB_OK = 0
    windll.user32.MessageBoxW(0, 'Qt was already installed', 'Already Installed', MB_OK)

try:
    import PySide2
except ImportError:
    install_qt()
else:
    already_installed()

