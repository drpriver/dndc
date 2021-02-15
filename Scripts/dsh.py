#!/usr/bin/env python3
from __future__ import annotations
import time
import os
import shlex
import shutil
import subprocess
import sys
from glob import glob
from typing import NamedTuple, Optional, MutableMapping, Callable, List, Any, TypeVar, Dict, Tuple
import types
import textwrap
import pathlib
import datetime
import string
from enum import Enum
import builtins

# Readline doesn't handle escape codes properly on macos in that it
# doesn't strip them out when calculating the length of the prompt.
# That sucks. So here's a reimplentation of the parts of it I actually
# use just using raw escape codes.  Seems pretty simple?
if sys.platform == 'darwin':
    import tty
    import termios
    def escapeless_len(s:str) -> int:
        # This is incorrect, but works for the color codes
        # that I actually output, so whatever.
        n = 0
        in_escape = False
        for c in s:
            if in_escape:
                if c == 'm':
                    in_escape = False
                continue
            if c == '\033':
                in_escape = True
                continue
            n += 1
        return n

    def get_cursor_pos() -> Tuple[int, int]:
        sys.stdout.write('\033[6n') # get cursor pos
        sys.stdout.flush()
        b = ''
        # Terminal responds on stdin (as if user typed it) with
        # \e[row;colR
        # where row and col are 1-based integers.
        while True:
            c = sys.stdin.read(1)
            if c == '\033':
                continue
            if c == '[':
                continue
            if c == 'R':
                break
            b += c
        row, col = b.split(';')
        return int(row), int(col)

    class MyReadline:
        def __init__(self) -> None:
            self.history = ['']
            self.index = 0
            self.buff = ['']
            self.buff.pop()
            self.prompt = ''
            self.pos = 0
            self.complete_state = 0
            self.completer:Optional[Callable[[str, int], Optional[str]]] = None
            self.pre_complete = ''
            self.prompt_row = 0
            self.display_height = 1
        def _dec(self) -> None:
            """
            Go back one in history.
            """
            if self.index == len(self.history) - 1:
                self.history[self.index] = ''.join(self.buff)
            self.index -= 1
            if self.index < 0:
                self.index = 0
            self.buff = list(self.history[self.index])
            self.pos = len(self.buff)
        def _inc(self) -> None:
            """
            Go forward one in history.
            """
            if self.index == len(self.history) - 1:
                self.history[self.index] = ''.join(self.buff)
            self.index += 1
            if self.index >= len(self.history):
                self.index = len(self.history) - 1
            self.buff = list(self.history[self.index])
            self.pos = len(self.buff)
        def _clear_buff(self) -> None:
            """
            Clear input.
            """
            self.buff.clear()
            self.pos = 0
        def _back_delete_word(self) -> None:
            """
            Delete a word from the input
            """
            if not self.pos:
                return
            # get rid of all of our current whitespace
            while self.buff[self.pos-1] == ' ':
                self.pos -= 1
                self.buff.pop(self.pos)
                if not self.pos:
                    return
            # ok, now delete until we hit a whitespace
            while self.buff[self.pos-1] != ' ':
                self.pos -= 1
                self.buff.pop(self.pos)
                if not self.pos:
                    return
        def _append(self) -> None:
            """
            Add the current line to the history (if not duplicate entry).
            """
            if self.buff:
                new_line = ''.join(self.buff)
                if len(self.history) == 1 or self.history[-2] != new_line:
                    self.history[-1] = new_line
                    self.index = len(self.history)
                    self.history.append('')
                else:
                    self.index = len(self.history) - 1
        def display(self) -> None:
            """
            Display the command prompt and what the user has inputted so far.
            """
            write = sys.stdout.write
            write('\033[?25l') # hide cursor
            cols = shutil.get_terminal_size().columns
            pl = escapeless_len(self.prompt)
            bl = len(self.buff)
            rows = (bl + pl) // cols + ((bl + pl % cols) != 0)
            write('\033[{};1H'.format(self.prompt_row))
            for _ in range(rows - self.display_height):
                write('\033[1S') # scroll whole page up 1 line
                self.prompt_row -= 1
            write('\033[J') # Erase in display (implicit 0) clear from cursor to end of screen.
            self.display_height = rows
            col_pos = (self.pos+pl) % cols
            write(self.prompt)
            text = ''.join(self.buff)
            first_line_remainder = text[:cols-pl]
            write(first_line_remainder)
            last_index = cols-pl
            for i in range(rows-1):
                write('\033[E') # cursor next line
                write(text[last_index:last_index+cols])
                last_index += cols
            write('\033[{}G'.format(col_pos+1)) # cursor right
            write('\033[?25h') # show cursor
            sys.stdout.flush()
        def _inc_pos(self) -> None:
            """
            Advance the input cursor 1 character.
            """
            self.pos += 1
            if self.pos > len(self.buff):
                self.pos = len(self.buff)
        def _dec_pos(self) -> None:
            """
            Go back 1 character in the current input.
            """
            self.pos -= 1
            if self.pos < 0:
                self.pos = 0
        def _seek_home(self) -> None:
            """
            Go to beginning of input.
            """
            self.pos = 0
        def _seek_end(self) -> None:
            """
            Go to end of input.
            """
            self.pos = len(self.buff)
        def request_completion(self) -> None:
            """
            ???
            """
            if self.completer:
                if self.complete_state == 0:
                    self.pre_complete = ''.join(self.buff)
                completion = self.completer(self.pre_complete, self.complete_state)
                self.complete_state += 1
                if completion is not None:
                    self.buff = list(completion)
                else:
                    self.buff = list(self.pre_complete)
                    self.complete_state = 0
                self.pos = len(self.buff)
        def input(self, prompt='') -> str:
            """
            Loops until the user hits enter.
            """
            self.prompt = prompt
            if not sys.stdin.isatty():
                import builtins
                return builtins.input(prompt)
            fd = sys.stdin.fileno()
            old_settings = termios.tcgetattr(fd)
            tty.setcbreak(sys.stdin)
            self.prompt_row = get_cursor_pos()[0]
            self.display_height = 1
            sys.stdout.flush()
            try:
                while True:
                    self.display()
                    q = sys.stdin.read(1)
                    if q != '\t':
                        self.complete_state = 0
                    if q == '\033':
                        q2 = sys.stdin.read(1)
                        if q2 == '[':
                            q3 = sys.stdin.read(1)
                            if q3 == 'A':
                                self._dec()
                            elif q3 == 'B':
                                self._inc()
                            elif q3 == 'C':
                                self._inc_pos()
                            elif q3 == 'D':
                                self._dec_pos()
                            elif q3 == '1':
                                q4 = sys.stdin.read(1)
                                if q4 == ';':
                                    q5 = sys.stdin.read(1)
                                    if q5 == '5':
                                        q6 = sys.stdin.read(1)
                                        # I think this actually supposed to be seek word?
                                        if q6 == 'D':
                                            self._seek_home()
                                        elif q6 == 'C':
                                            self._seek_end()
                    elif q == '\x08':
                        if self.pos:
                            self.buff.pop(self.pos-1)
                            self._dec_pos()
                    elif q == '\x7f':
                        if self.pos < len(self.buff):
                            self.buff.pop(self.pos)
                        elif self.buff:
                            self.buff.pop(self.pos-1)
                            self._dec_pos()
                    elif q == '\x01': # c-a?
                        self._seek_home()
                    elif q == '\x05': # c-e?
                        self._seek_end()
                    elif q == '\x06': #c-f?
                        self._inc_pos()
                    elif q == '\x02': #c-b?
                        self._dec_pos()
                    elif q == '\x04': #c-d?
                        raise EOFError
                    elif q == '\x17': #c-w?
                        self._back_delete_word()
                        pass
                    elif q == '\x10': #c-p
                        self._dec()
                    elif q == '\x0e': #c-n
                        self._inc()
                    elif q =='\n':
                        sys.stdout.write('\033[?25l\033[S\033[E') # hide cursor, scroll up, cursor next line
                        sys.stdout.flush()
                        self._append()
                        result = ''.join(self.buff)
                        self._clear_buff()
                        return result
                    elif q == '\t':
                        self.request_completion()
                    elif q.isprintable():
                        self.buff.insert(self.pos, q)
                        self.pos += 1
                    else:
                        print(repr(q))
                        pass
            finally:
                self._clear_buff()
                termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    myreadline = MyReadline()
    input = myreadline.input

CommandFunc = Callable[[str], bool]
CmdsMap = MutableMapping[str, CommandFunc]
cmds:CmdsMap = {}
T = TypeVar('T', bound=Callable)
def command(func:T, name:Optional[str]=None) -> T:
    if name is None:
        name = func.__name__
    def wrapped(arg:str='') -> bool:
        if arg:
            func(arg)
        else:
            func()
        return False
    cmds[name] = wrapped
    return func

def namedcommand(name:str, func:T) -> T:
    return command(func, name)

def namedshellcommand(name:str, func:T) -> T:
    return shellcommand(func, name)

def shellcommand(func:T, name:Optional[str]=None) -> T:
    if name is None:
        name = func.__name__
    def wrapped(arg:str='') -> bool:
        args = shlex.split(arg)
        func(*args)
        return False
    wrapped.__doc__ = func.__doc__
    assert isinstance(name, str)

    cmds[name] = wrapped
    return func

@shellcommand
def cd(where:str='..', prev:List[str]=[]) -> None:
    if where.startswith('-'):
        if where == '-':
            back = -1
        else:
            back = int(where[1:])
        where = prev[back]
    prev.append(os.getcwd())
    os.chdir(os.path.expanduser(where))

@shellcommand
def run(*args:str) -> None:
    if '>' in args:
        if len(args) < 3:
            raise Exception('Need to redirect to a file')
        if args[-2] != '>':
            raise Exception('Only one argument after the \'>\'')
        filename = args[-1]
        if filename == '/dev/null':
            subprocess.run(args[:-2], stdout=subprocess.DEVNULL)
        else:
            with builtins.open(filename, 'w') as fp:
                subprocess.run(args[:-2], stdout=fp)
        return
    subprocess.run(args)

@command
def pwd() -> None:
    print(os.getcwd())

@shellcommand
def vim(f:str='') -> None:
    command = [vimexec]
    if f:
        command.append(f)
    if sys.platform == 'win32':
        command.insert(1, '--remote')
        subprocess.Popen(command)
    else:
        subprocess.run(command)

class Git:
    def __init__(self):
        self.current_branch:Optional[str] = None
        self._determine_git_branch()
    def get_git_branch(self) -> Optional[str]:
        return self.current_branch
    def _determine_git_branch(self) -> None:
        # command = [gitexec, 'branch', '--show-current']
        command = [gitexec, 'rev-parse', '--abbrev-ref', 'HEAD']
        try:
            out = subprocess.run(command, capture_output=True)
            self.current_branch = out.stdout.decode('utf-8').strip()
        except Exception:
            self.current_branch = None
    def gitcommand(self, *args:str) -> None:
        """
        Calls git.
        """
        while args and args[0] == 'git':
            args = args[1:]
        command = [gitexec, *args]
        subprocess.run(command)
        self._determine_git_branch()
        if sys.platform == 'win32':
            # For some reason, git diff fucks up the windows vt mode
            # stuff so we have to re-enable it every time.
            enable_vt()

@shellcommand
def make(*args:str) -> None:
    command = [makeexec, *args]
    subprocess.run(command, check=True)

@shellcommand
def mypy(*args:str) -> None:
    if not args:
        raise ValueError("Need some args")
    command = [sys.executable, '-m', 'mypy', '--namespace-packages']
    if not args[-1].endswith('.py'):
        command.append('-m')
    command.extend(args)
    subprocess.run(command)

@command
def clean() -> None:
    make('clean')

@shellcommand
def open(*args:str) -> None:
    command = []
    if openexec:
        command.append(openexec)
    command.extend(args)
    subprocess.run(command)

@command
def pydoc(arg:str) -> None:
    subprocess.run([sys.executable, '-m', 'pydoc', arg])
@shellcommand
def export(*args:str) -> None:
    new_vars = {}
    for arg in args:
        key, value = arg.split('=')
        new_vars[key] = value
    os.environ.update(new_vars)

class Target:
    def __init__(self, what:Optional[str]=None) -> None:
        self.what = what
    def target(self, what:Optional[str]=None) -> None:
        """
        Sets target for builds.
        """
        self.what = what
    def build(self, what:Optional[str]=None) -> None:
        if what is not None:
            self.target(what)
        if self.what is None:
            raise Exception('must set a target at least once')
        what = self.what
        command = [makeexec, what, '-j']
        result = subprocess.run(command)
        if result.returncode:
            raise Exception(f"Building '{what}' failed")
    def debug(self, what:Optional[str]=None) -> None:
        if what is not None:
            self.target(what)
        assert self.what is not None
        self.build()
        run('lldb', '-o', 'run', os.path.join('Bin', self.what))
    def play(self, what:Optional[str]=None) -> None:
        if what is not None:
            self.target(what)
        self.build()
        assert self.what is not None
        run(os.path.join('Bin', self.what))

class ExecCommand:
    def __init__(self, interp:'MyCmd'):
        self.state:Dict[str, Any] = {'interp':interp}
    def import_(self, arg:str='') -> bool:
        arg = 'from '+arg+' import *'
        exec(arg, self.state, self.state)
        return False
    def execcommand(self, arg:str='') -> bool:
        self.state['interp'].show_cursor()
        try:
            line = arg
            lines = []
            if line:
                lines.append(line)
            if (line and line.endswith(':')) or not line:
                while True:
                    line = input(str(ColoredText('py) ', Colors.green)))
                    lines.append(line)
                    if not line: break
            while lines and not lines[-1]:
                lines.pop()
            if len(lines) == 1:
                try:
                    result = eval(lines[0], self.state, self.state)
                    if result is not None:
                        print(repr(result))
                except SyntaxError:
                    pass
                else:
                    return False
            exec('\n'.join(lines), self.state, self.state)
        finally:
            self.state['interp'].hide_cursor()
        return False

def rm(*args:str, recursive=False) -> None:
    if recursive:
        for a in args:
            shutil.rmtree(a)
    else:
        for a in args:
            if os.path.isfile(a):
                os.unlink(a)
            else:
                raise ValueError(a)

def rmcommand(arg:str) -> bool:
    args = shlex.split(arg)
    real_args = []
    for arg in args:
        if '*' in arg and '\'' not in arg and '"' not in arg:
            real_args.extend(glob(arg))
        else:
            real_args.append(arg)
    args = real_args
    if '-r' in args:
        args.remove('-r')
        try:
            rm(*args, recursive=True)
        except ValueError as e:
            raise ValueError('No such file or directory: {}'.format(e.args[0])) from None
    else:
        try:
            rm(*args)
        except ValueError as e:
            raise ValueError('No such file or directory: {}'.format(e.args[0])) from None
    return False
cmds['rm'] = rmcommand

@shellcommand
def mv(*args:str) -> None:
    """
    mv src dst
    """
    if len(args) != 2:
        raise TypeError("shutil.move only supports two arguments")
    src, dst = args
    if '*' in dst:
        raise Exception('* not supported in dst')
    if '*' in src:
        sources = glob(src)
        for source in sources:
            shutil.move(source, dst)
        return
    shutil.move(src, dst)


if sys.platform == 'win32':
    from ctypes import wintypes
    import msvcrt
    import ctypes
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    def _check_bool(result:bool, func:Any, args:Any) -> Any:
        if not result:
            raise ctypes.WinError(ctypes.get_last_error())
        return args

    class DoVt:
        ERROR_INVALID_PARAMETER = 0x0057
        ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004

        LPDWORD = ctypes.POINTER(wintypes.DWORD)
        kernel32.GetConsoleMode.errcheck = _check_bool # type: ignore
        kernel32.GetConsoleMode.argtypes = (wintypes.HANDLE, LPDWORD)
        kernel32.SetConsoleMode.errcheck = _check_bool # type: ignore
        kernel32.SetConsoleMode.argtypes = (wintypes.HANDLE, wintypes.DWORD)

        def set_conout_mode(self, new_mode, mask:int=0xffffffff) -> Any:
            # don't assume StandardOutput is a console.
            # open CONOUT$ instead
            fdout = os.open('CONOUT$', os.O_RDWR)
            try:
                hout = msvcrt.get_osfhandle(fdout)
                old_mode = wintypes.DWORD()
                kernel32.GetConsoleMode(hout, ctypes.byref(old_mode))
                mode = (new_mode & mask) | (old_mode.value & ~mask)
                kernel32.SetConsoleMode(hout, mode)
                return old_mode.value
            finally:
                os.close(fdout)

        def enable_vt_mode(self) -> Any:
            mode = mask = self.ENABLE_VIRTUAL_TERMINAL_PROCESSING
            try:
                return self.set_conout_mode(mode, mask)
            except WindowsError as e:
                if e.winerror == self.ERROR_INVALID_PARAMETER:
                    raise NotImplementedError
                raise
else:
    class DoVt:
        def enable_vt_mode(self) -> None:
            pass

enable_vt = command(DoVt().enable_vt_mode, 'vt')

class ParsedLine(NamedTuple):
    command: Optional[str]
    args: Optional[str]
    line: str

class Colors(Enum):
    green     = '\033[38;5;10m'
    yellow    = '\033[38;5;226m\033[48;5;0m'
    orange    = '\033[38;5;202m'
    red       = '\033[38;5;9m'
    cyan      = '\033[36m'
    under     = '\033[4m'
    de_under  = '\033[24m'
    reset     = '\033[0m'
    resetfgbg = '\033[39;49m'

class ColoredText(NamedTuple):
    text: str
    color: Colors
    def __len__(self) -> int:
        return len(self.text)
    def __str__(self) -> str:
        if sys.platform == 'linux':
            return '\001{}\002{}\001{}\002'.format(self.color.value, self.text, Colors.reset.value)
        else:
            return '{}{}{}'.format(self.color.value, self.text, Colors.reset.value)

    def ljust(self, amount:int) -> ColoredText:
        return ColoredText(self.text.ljust(amount), self.color)

class MyCmd:
    prompt = 'dsh) '
    identchars = string.ascii_letters + string.digits + '_'
    ruler = '='
    lastcmd = ''
    intro = None
    doc_leader = ""
    doc_header = "Documented commands (type help <topic>):"
    misc_header = "Miscellaneous help topics:"
    undoc_header = "Undocumented commands:"
    nohelp = "*** No help on %s"
    usecolors = True

    def __init__(self, cmds:CmdsMap, aliases:List[Tuple[str, str]]=[], native_commands:List[str]=[]) -> None:
        self.stdin = sys.stdin
        self.stdout = sys.stdout
        self.cmdqueue:List[str] = []
        self.completekey = 'tab'
        execcommand = ExecCommand(self)
        cmds['q'] = lambda arg: True
        cmds['quit'] = lambda arg: True
        cmds['exit'] = lambda arg: True
        cmds['help'] = self.help
        cmds['h'] = self.help
        cmds['queue'] = self.queue
        cmds['register'] = self.register
        cmds['alias'] = self.alias
        cmds['sh'] = cmds['run']
        cmds['time'] = self.time
        cmds['shell'] = execcommand.execcommand
        cmds['py'] = execcommand.execcommand
        cmds['import'] = execcommand.import_
        self.native_commands = set(native_commands)
        self.cmds = cmds
        for a, b in aliases:
            self._alias(a, b)
        if sys.platform != 'win32':
            self._register('mv', 'mv') # use the actual mv program instead of our implementation
        self.platform_prompt = {
                'win32':'W',
                'darwin':'M',
                'linux':'L'
            }[sys.platform]
        if self.usecolors:
            self.platform_prompt = str(ColoredText(self.platform_prompt, Colors.green))

    def alias(self, arg:str) -> bool:
        """
        alias name1 <- name2

        Makes name1 an alias of name2.
        """
        self._alias(*(arg.split()))
        return False

    def _alias(self, name:str, name2:str) -> None:
        self.cmds[name] = self.cmds[name2]

    def time(self, arg:str) -> bool:
        if not arg:
            raise Exception('Must time something')
        before = time.time()
        result = self.onecmd(arg, False)
        after = time.time()
        self.stdout.write(f'Took: {after-before:.4f}s\n')
        return result

    def register(self, arg:str) -> bool:
        """
        Adds a command to the interpreter (which is then
        passed to run basically).
        """
        args = shlex.split(arg)
        self._register(*args)
        return False

    def _register(self, *args:str) -> None:
        if len(args) < 2:
            raise TypeError('register requires at least two arguments')

        def runcommand(arg:str) -> bool:
            subargs = shlex.split(arg)
            run(*args[1:], *subargs)
            return False
        self.cmds[args[0]] = runcommand

    def cmd(self, arg:str) -> None:
        self.cmdqueue.append(arg)

    def queue(self, arg:Optional[str]) -> bool:
        """
        Queue a sequence of commands. After this returns, the
        cmd interpreter will procede to execute the commands
        in sequence.
        """
        queue:List[str] = []
        if arg:
            queue.append(arg)
        line: Optional[str]
        while True:
            line = input(' q) ')
            if not line:
                break
            queue.append(line)
        # TODO: refactor into a method?
        try:
            for line in queue:
                cmd, arg, line = self.parseline(line)
                if not line:
                    continue
                if not cmd:
                    raise Exception('Unknown command {}'.format(line))
                try:
                    func = self.cmds[cmd]
                except KeyError as ke:
                    raise Exception("Unknown command {}".format(ke)) from None
                if arg is None:
                    arg = ''
                func(arg)
        except Exception as e:
            if self.usecolors:
                text = str(ColoredText(str(e), Colors.orange))
            else:
                text = str(e)
            self.stdout.write(text+'\n')
        return False

    def cmdloop(self) -> None:
        self.preloop()
        if sys.platform != 'win32':
            import readline
            old_completer = readline.get_completer()
            readline.set_completer(self.complete)
            readline.parse_and_bind(self.completekey+": complete")
        self.hide_cursor()
        try:
            self.cmdloop_interior()
        finally:
            if sys.platform != 'win32':
                readline.set_completer(old_completer)
            self.show_cursor()
        self.postloop()

    def cmdloop_interior(self) -> None:
        if self.intro:
            self.stdout.write(str(self.intro)+"\n")
        stop = False
        while not stop:
            try:
                self.show_cursor()
                line = self.getline()
                self.hide_cursor()
                if line == 'EOF':
                    return
                line = self.precmd(line)
                stop = self.onecmd(line, True)
                stop = self.postcmd(stop, line)
            except KeyboardInterrupt:
                self.stdout.write('^C\n')
                continue

    def getline(self) -> str:
        if self.cmdqueue:
            return self.cmdqueue.pop(0)
        try:
            if sys.platform != 'win32':
                fmt = '%-I:%M%p'
            else:
                fmt = '%I:%M%p'
            now = datetime.datetime.now().strftime(fmt)
            branch = get_git_branch()
            if branch is None:
                branch = ''
            elif self.usecolors:
                branch = str(ColoredText(branch, Colors.yellow))
            if branch:
                branch = '<{}> '.format(branch)
            if self.usecolors:
                now = str(ColoredText(now, Colors.cyan))
            prompt = '{} {} {}{}'.format(now, self.platform_prompt, branch, self.prompt)
            line = input(prompt)
        except EOFError:
            line = 'EOF'
        return line

    def precmd(self, line:str) -> str:
        return line

    def postcmd(self, stop:bool, line) -> bool:
        return stop

    def preloop(self) -> None:
        pass

    def postloop(self) -> None:
        pass

    def parseline(self, line:str) -> ParsedLine:
        line = line.strip()
        if not line:
            return ParsedLine(None, None, line)
        elif line[0] == '?':
            line = 'help ' + line[1:]
        elif line[0] == ':':
            line = 'py ' + line[1:]
        elif line[0] == '!':
            line = 'run ' + line[1:]
        i, n = 0, len(line)
        while i < n and line[i] in self.identchars:
            i += 1
        cmd, arg = line[:i], line[i:].strip()
        return ParsedLine(cmd, arg, line)

    def onecmd(self, line:str, store:bool) -> bool:
        """
        Interpret the argument as though it had been typed in
        response to the prompt.

        This may be overridden, but should not normally need
        to be; see the precmd() and postcmd() methods for
        useful execution hooks.  The return value is a flag
        indicating whether interpretation of commands by the
        interpreter should stop.
        """
        cmd, arg, line = self.parseline(line)
        if not line:
            return self.emptyline()
        if cmd is None:
            return self.default(line)
        if store:
            self.lastcmd = line
            if line == 'EOF' :
                self.lastcmd = ''
        if cmd in self.native_commands:
            arg = '{} {}'.format(cmd, arg)
            cmd = 'run'
        if cmd == '':
            return self.default(line)
        else:
            try:
                func = self.cmds[cmd]
            except KeyError:
                return self.default(line)
            try:
                if arg is None:
                    arg = ''
                return func(arg)
            except Exception as e:
                if self.usecolors:
                    text = str(ColoredText(str(e), Colors.orange))
                else:
                    text = str(e)
                self.stdout.write(text+'\n')
                return False

    def emptyline(self) -> bool:
        """
        Called when an empty line is entered in response to
        the prompt.
        """
        if self.lastcmd:
            return self.onecmd(self.lastcmd, False)
        return False

    def default(self, line:str) -> bool:
        """
        Called on an input line when the command prefix is not
        recognized.
        """
        cmd, arg, _ = self.parseline(line)
        if shutil.which(cmd):
            run(*shlex.split(line))
        else:
            self.stdout.write('*** Unknown command: %s\n'%line)
        return False

    def completedefault(self, *ignored) -> List[str]:
        """
        Method called to complete an input line when no
        command-specific complete_*() method is available.
        """
        return []

    def completenames(self, text:str, *ignored) -> List[str]:
        return [a for a in self.cmds if a.startswith(text)]

    def complete(self, text:str, state:int) -> Optional[str]:
        """
        Return the next possible completion for 'text'.

        If a command has not been entered, then complete
        against command list.  Otherwise try to call
        complete_<command> to get list of completions.
        """
        if sys.platform == 'win32':
            return None
        if state == 0:
            if sys.platform == 'linux':
                import readline
                origline = readline.get_line_buffer()
                line = origline.lstrip()
                stripped = len(origline) - len(line)
                begidx = readline.get_begidx() - stripped
                endidx = readline.get_endidx() - stripped
            else:
                #FIXME: this is wrong
                line = text
                begidx = line.find(' ')+1
                endidx = len(line)
            if not line:
                self.help('')
            if begidx>0:
                cmd, args, foo = self.parseline(line)
                if cmd == '':
                    compfunc = self.completedefault
                else:
                    if cmd is None:
                        cmd = ''
                    try:
                        compfunc = getattr(self, 'complete_' + cmd)
                    except AttributeError:
                        compfunc = self.completedefault
            else:
                compfunc = self.completenames  # type: ignore
            self.completion_matches = compfunc(text, line, begidx, endidx)
        try:
            return self.completion_matches[state]
        except IndexError:
            return None

    def get_names(self) -> List[str]:
        return sorted(k for k in self.cmds.keys() if len(k) > 1)

    def complete_help(self, *args) -> List[str]:
        commands = set(self.completenames(*args))
        topics = set(a[5:] for a in self.get_names()
                     if a.startswith('help_' + args[0]))
        return list(commands | topics)

    def help(self, arg:str) -> bool:
        '''
        List available commands with "help" or detailed help
        with "help cmd".
        '''
        if arg:
            # XXX check arg syntax
            try:
                func = getattr(self, 'help_' + arg)
            except AttributeError:
                try:
                    doc = self.cmds.get(arg).__doc__
                    if doc:
                        self.stdout.write(textwrap.dedent(str(doc)).lstrip('\r\n')+'\n')
                        return False
                except AttributeError:
                    pass
                self.stdout.write("%s\n"%str(self.nohelp % (arg,)))
                return False
            func()
        else:
            names = self.get_names()
            cmds_doc = []
            cmds_undoc = []
            help = {}
            for name in names:
                if name[:5] == 'help_':
                    help[name[5:]]=1
            names.sort()
            # There can be duplicates if routines overridden
            for name in names:
                cmd=name
                if cmd in help:
                    cmds_doc.append(cmd)
                    del help[cmd]
                else:
                    func = self.cmds.get(name)
                    if func and func.__doc__:
                        cmds_doc.append(cmd)
                    else:
                        cmds_undoc.append(cmd)
            self.stdout.write("%s\n"%str(self.doc_leader))
            self.print_topics(self.doc_header, cmds_doc)
            self.print_topics(self.misc_header, list(help.keys()))
            self.print_topics(self.undoc_header, cmds_undoc)
        return False

    def print_topics(self, header:str, cmds:List[str]) -> None:
        maxcol = shutil.get_terminal_size().columns
        if cmds:
            self.stdout.write("%s\n"%str(header))
            if self.ruler:
                self.stdout.write("%s\n"%str(self.ruler * len(header)))
            columnize(self.stdout, cmds, maxcol-1)
            self.stdout.write("\n")

    def hide_cursor(self) -> None:
        self.stdout.write('\033[?25l')
        self.stdout.flush()
    def show_cursor(self) -> None:
        self.stdout.write('\033[?25h')
        self.stdout.flush()

    def __call__(self, arg:str) -> None:
        self.onecmd(arg, False)


def columnize(stdout, list:List[str], displaywidth:int=80) -> None:
    """
    Display a list of strings as a compact set of columns.

    Each column is only as wide as necessary.  Columns are
    separated by two spaces (one was not legible enough).
    """
    if not list:
        stdout.write("<empty>\n")
        return

    size = len(list)
    if size == 1:
        stdout.write('%s\n'%str(list[0]))
        return
    # Try every row count from 1 upwards
    for nrows in range(1, len(list)):
        ncols = (size+nrows-1) // nrows
        colwidths = []
        totwidth = -2
        for col in range(ncols):
            colwidth = 0
            for row in range(nrows):
                i = row + nrows*col
                if i >= size:
                    break
                x = list[i]
                colwidth = max(colwidth, len(x))
            colwidths.append(colwidth)
            totwidth += colwidth + 2
            if totwidth > displaywidth:
                break
        if totwidth <= displaywidth:
            break
    else:
        nrows = len(list)
        ncols = 1
        colwidths = [0]
    for row in range(nrows):
        texts = []
        for col in range(ncols):
            i = row + nrows*col
            if i >= size:
                x = ""
            else:
                x = list[i]
            texts.append(x)
        while texts and not texts[-1]:
            del texts[-1]
        for col in range(len(texts)):
            texts[col] = texts[col].ljust(colwidths[col])
        for n, t in enumerate(texts):
            texts[n] = str(t)
        stdout.write("%s\n"%str("  ".join(texts)))

@shellcommand
def mkdir(*args:str) -> None:
    if len(args) > 1:
        raise ValueError("Too many arguments to mkdir")
    elif len(args) == 0:
        raise ValueError("Too few arguments to mkdir")
    os.mkdir(args[0])

@shellcommand
def touch(*args:str) -> None:
    for f in args:
        if '*' in f:
            globbed = glob(f)
            for g in globbed:
                pathlib.Path(g).touch(exist_ok=True)
        else:
            pathlib.Path(f).touch(exist_ok=True)

@shellcommand
def ls(*args:str) -> None:
    files = sorted(os.listdir(*args))
    for n, f in enumerate(files):
        if os.path.isdir(f):
            files[n] = ColoredText(f, Colors.cyan) # type: ignore
    columnize(sys.stdout, files, shutil.get_terminal_size().columns)

@shellcommand
def realpath(*args:str) -> None:
    for arg in args:
        eu = os.path.expanduser(arg)
        rp = os.path.realpath(eu)
        np = os.path.realpath(rp)
        print(np)

if sys.platform == 'win32':
    vimexec  = r'C:\Program Files (x86)\Vim\vim82\gvim.exe'
    makeexec = r'C:\Program Files (x86)\GnuWin32\bin\make.exe'
    gitexec  = r'C:\Program Files\Git\bin\git.exe'
    openexec = r'Explorer.exe'
    native_commands = []

    @shellcommand
    def cp(*args:str) -> None:
        if len(args) != 2:
            raise Exception('Emulated copy only supports two args')
        shutil.copy(args[0], args[1])

elif sys.platform == 'darwin':
    vimexec  = 'vim'
    makeexec = 'make'
    gitexec  = 'git'
    openexec = 'open'
    native_commands = ['du', 'man', 'ssh', 'scp', 'locate', 'lldb', 'cat', 'less', 'cp', 'bash', 'sudo']

    @command
    def profile(profilee:str) -> None:
        profilee = os.path.join('Bin', profilee)
        make(profilee, '-j')
        command = ['instruments', '-t', 'Time Profile', profilee]
        subprocess.run(command, check=True)
        files = sorted(f for f in os.listdir() if f.endswith('.trace'))
        open(files[-1])

else:  # linux
    vimexec  = 'vim'
    makeexec = 'make'
    gitexec  = 'git'
    openexec = 'xdg-open'
    native_commands = ['du', 'man', 'ssh', 'scp', 'locate', 'lldb', 'cat', 'less', 'cp', 'bash', 'tmux', 'sudo']


if __name__ == '__main__':
    _git = Git()
    get_git_branch = _git.get_git_branch
    namedshellcommand('git', _git.gitcommand)
    enable_vt()
    _target = Target('docparser')
    namedcommand('target', _target.target)
    namedcommand('debug', _target.debug)
    namedcommand('play', _target.play)
    namedcommand('build', _target.build)
    RELAUNCH = False

    # I often forget the '!'
    @shellcommand
    def Bin(*args:str) ->None:
        args_ = list(args)
        args_[0] = os.path.normpath('Bin/{}'.format(args_[0]))
        return run(*args_)

    def relaunch(unused:str) -> bool:
        global RELAUNCH
        RELAUNCH = True
        return True
    cmds['relaunch'] = relaunch

    aliases = [
        ('p', 'play'),
        ('b', 'build'),
        ('op', 'open'),
        ('l', 'ls'),
        ]
    mycmd = MyCmd(cmds, aliases, native_commands)
    if sys.platform == 'darwin':
        myreadline.completer = mycmd.complete
    try:
        mycmd.cmdloop()
    finally:
        sys.stdout.write('\033[?25h') # show cursor
    if RELAUNCH:
        os.execv(sys.executable, ['python3', __file__])
