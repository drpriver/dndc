def install_qt():
    import sys
    import subprocess
    from ctypes import windll
    import time
    MB_YESNO = 0x00000004
    MB_ICONEXCLAMATION = 0x00000030L
    response = windll.user32.MessageBoxW(0, 'Qt is not installed. Install Qt?', 'Install Qt?', MB_YESNO | MB_ICONEXCLAMATION)
    IDYES = 6
    if response != IDYES:
        return
    instance = windll.user32.GetModuleInstance()
    WMUSER = 1024
    PBM_SETMARQUEE = WM_USER + 10

    class ProgressBar:
        def __init__(self):
            PBS_MARQUEE = 8
            CW_USEDEFAULT = 0x80000000
            self.bar = windll.user32.CreateWindowExW(0, "msctls_progress32", "Installing Qt", 0x80000000 | PBS_MARQUEE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0)
            windll.user32.SendMessageW(self.bar, PBM_SETMARQUEE, 1, 0)
        def close(self):
            windll.user32.CloseWindow(self.bar)

    pb = ProgressBar()
    command = [sys.executable, '-m', 'pip', 'install', 'PySide2==5.15.2']
    process = subprocess.Popen(command)
    process = subprocess.run(command, check=True)
    # while process.poll() is None:
        # time.sleep(0.1)
    pb.close()
    MB_OK = 0
    windll.user32.MessageBoxW(0, 'Qt was installed successfully', 'Success!', MB_OK  | MB_ICON_EXCLAMATION)

try:
    import PySide2
except ImportError:
    install_qt()
