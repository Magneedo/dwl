#!/usr/bin/env python3
"""Test the popup with real foot/tmux and a busy, disposable Codex stand-in."""
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import tempfile
import time

REPO = Path(__file__).resolve().parent.parent


def eventually(check, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = check()
        if result:
            return result
        time.sleep(0.02)
    raise AssertionError('Timed out: ' + str(check))


def alive(pid):
    stat = Path('/proc') / str(pid) / 'stat'
    try:
        return pid > 0 and stat.read_text().split(')')[-1].split()[0] != 'Z'
    except FileNotFoundError:
        return False


with tempfile.TemporaryDirectory(prefix='dwl-codex-test-') as temporary:
    root = Path(temporary)
    (root / 'Vault').mkdir()
    runtime = root / 'runtime'
    runtime.mkdir(mode=0o700)
    program = root / 'codex'
    program.write_text('''#!/usr/bin/env python3
import os, select, sys, time, tty
from pathlib import Path
root = Path(os.environ['HOME'])
assert Path.cwd() == root / 'Vault'
assert sys.argv[1:] == ['--no-alt-screen']
tty.setraw(0)
with (root / 'starts').open('a') as f:
    f.write(str(os.getpid()) + '\\n')
i = 0
while True:
    if not (root / 'pause').exists():
        os.write(1, ''.join('working %d line %d\\r\\n' % (i, j) for j in range(200)).encode())
    if select.select([0], [], [], 0)[0]:
        with (root / 'input').open('ab') as f:
            f.write(os.read(0, 4096))
    (root / 'progress').write_text(str(i))
    i += 1
    time.sleep(.02)
''')
    program.chmod(0o755)
    (root / 'tofi').mkdir()
    (root / 'tofi/emoji').write_text('font = Noto Sans\n')
    (root / 'tofi/characters.txt').write_text('👩‍💻\ttechnologist\nα\talpha\n')
    profile_menu = root / 'tofi-power-profile'
    profile_menu.write_text('#!/bin/sh\nprintf "opened\\n" >> "$HOME/profile-menu"\n')
    profile_menu.chmod(0o755)
    env = dict(os.environ, HOME=temporary, XDG_CONFIG_HOME=temporary,
               XDG_DATA_HOME=temporary,
               XDG_RUNTIME_DIR=str(runtime), WLR_BACKENDS='headless',
               WLR_HEADLESS_OUTPUTS='2', WLR_RENDERER='pixman',
               PATH=temporary + ':' + os.environ['PATH'])
    env.pop('WAYLAND_DISPLAY', None)
    env.pop('DISPLAY', None)
    env.pop('TMUX', None)
    # Personal tmux configuration must never affect the dedicated server.
    (root / '.tmux.conf').write_text('set -g mouse off\nset -g status on\n')

    def tmux(*args, socket=None):
        return subprocess.check_output(
            ['tmux', '-N', '-S', socket or state['socket'], *args], text=True).strip()

    def launches():
        path = root / 'starts'
        return path.read_text().splitlines() if path.exists() else []

    def progress():
        try:
            return int((root / 'progress').read_text())
        except (ValueError, FileNotFoundError):
            return -1

    def input_bytes():
        path = root / 'input'
        return path.read_bytes() if path.exists() else b''

    def menus():
        found = []
        for proc in Path('/proc').glob('[0-9]*'):
            try:
                if (proc / 'comm').read_text().strip() == 'tofi' and (
                        b'XDG_RUNTIME_DIR=' + os.fsencode(runtime)) in (proc / 'environ').read_bytes().split(b'\0'):
                    found.append(int(proc.name))
            except (FileNotFoundError, PermissionError, ProcessLookupError):
                pass
        return found

    def geometry(s):
        x, y, width, height = s['geometry']
        mx, my, mw, mh = s['usable']
        pw, ph = (max(1, min(100, p)) for p in s['size'])
        assert width == max(1 + 2 * s['border'], mw * pw // 100), s
        assert height == max(1 + 2 * s['border'], mh * ph // 100), s
        assert (x, y) == (mx + (mw - width) // 2, my + (mh - height) // 2), s

    with (root / 'log').open('w+') as log:
        process = subprocess.Popen([str(REPO / 'tests/codex'), str(REPO / 'dwl-codex')],
                                   env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=log, bufsize=0)
        output = bytearray()

        def send(keys):
            process.stdin.write(keys.encode())

        def snapshot():
            send('s')
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                while b'\n' in output:
                    line, _, rest = output.partition(b'\n')
                    output[:] = rest
                    if line.startswith(b'TEST '):
                        return json.loads(line[5:])
                if select.select([process.stdout], [], [], 0.1)[0]:
                    chunk = os.read(process.stdout.fileno(), 65536)
                    if not chunk:
                        raise AssertionError('Headless compositor exited')
                    output.extend(chunk)
            raise AssertionError('No compositor response')

        def waitstate(**expected):
            def matches():
                s = snapshot()
                return s if all(s[k] == v for k, v in expected.items()) else None
            return eventually(matches)

        try:
            # Race startup with many presses before the first surface maps.
            send('t' * 100)
            state = waitstate(count=1, shown=0, visible=0, enabled=0)
            eventually(lambda: len(launches()) == 1)
            server, terminal = state['server'], state['terminal']
            pane = tmux('display-message', '-p', '#{pane_pid}')
            assert tmux('display-message', '-p', '#{pid}') == str(server)
            assert state['floating'] and state['noswallow']
            assert tmux('show-options', '-gv', 'mouse') == 'on'
            assert tmux('show-options', '-gv', 'status') == 'off'
            clipboard_env = dict(env, WAYLAND_DISPLAY=state['display'])

            def clipboard():
                return subprocess.check_output(
                    ['wl-paste', '--no-newline'], env=clipboard_env, timeout=2)

            send('m')
            waitstate(selected=0)
            send('M')
            waitstate(selected=1)

            send('P')
            eventually(lambda: (root / 'profile-menu').exists())
            assert (root / 'profile-menu').read_text() == 'opened\n'

            # Exercise the configured binding and actual tofi, including its
            # startup race and lock release. Never invoke a power action.
            for _ in range(2):
                send('w' * 100)
                eventually(lambda: len(menus()) == 1)
                for _ in range(20):
                    send('wwwww')
                    assert len(menus()) == 1
                    time.sleep(.01)
                os.kill(menus()[0], signal.SIGTERM)
                eventually(lambda: not menus())
                eventually(lambda: subprocess.run(
                    ['flock', '-n', str(runtime / 'tofi-power.lock'), 'true']).returncode == 0)

            # A ZWJ sequence must reach the focused terminal and clipboard intact,
            # without its search label, using the actual picker and wtype.
            send('t')
            waitstate(visible=1, focused=1)
            before_input = input_bytes()
            subprocess.run(['wl-copy', '--type', 'text/plain;charset=utf-8'],
                           input=b'unchanged', env=clipboard_env, check=True)
            send('.' * 100)
            waitstate(menufocused=1)
            assert len(menus()) == 1
            send('.' * 20)
            waitstate(menufocused=1)
            assert len(menus()) == 1
            send('r')
            waitstate(menufocused=0)
            eventually(lambda: clipboard() == '👩‍💻'.encode())
            eventually(lambda: input_bytes() == before_input + '👩‍💻'.encode())
            eventually(lambda: subprocess.run(
                ['flock', '-n', str(runtime / 'tofi-emoji.lock'), 'true']).returncode == 0)
            send('.')
            waitstate(menufocused=1)
            send('ir')
            waitstate(menufocused=0)
            eventually(lambda: clipboard() == 'α'.encode())
            eventually(lambda: input_bytes() == before_input + '👩‍💻α'.encode())
            before_input = input_bytes()
            send('.')
            waitstate(menufocused=1)
            send('e')
            waitstate(menufocused=0)
            eventually(lambda: not menus())
            assert clipboard() == 'α'.encode()
            time.sleep(.2)
            assert input_bytes() == before_input
            send('t')
            waitstate(visible=0)

            send('n')
            waitstate(normal=1, normalfocused=1, visible=0)
            send('fa')
            waitstate(normalfull=1, normalfocused=1, visible=0)

            # The hidden terminal must keep draining output so work never stalls.
            before = progress()
            eventually(lambda: progress() > before + 50)
            send('t')
            state = waitstate(count=1, visible=1, focused=1, selected=1, layer=1)
            assert state['normalfull'] == 1
            geometry(state)
            send('b')
            state = waitstate(visible=1, focused=1)
            geometry(state)

            # Reproduce foot's alternate-scroll arrow translation, then prove
            # the fix scrolls tmux's displayed history without typing arrows.
            (root / 'pause').touch()
            time.sleep(.1)
            tmux('set-option', '-g', 'mouse', 'off')
            time.sleep(.1)
            send('+')
            eventually(lambda: b'\x1b[A' in input_bytes() or b'\x1bOA' in input_bytes())
            tmux('set-option', '-g', 'mouse', 'on')
            time.sleep(.1)
            before_input = input_bytes()
            assert tmux('display-message', '-p', '#{pane_in_mode}') == '0'
            send('++++')
            eventually(lambda: tmux('display-message', '-p', '#{pane_in_mode}') == '1')
            eventually(lambda: int(tmux('display-message', '-p', '#{scroll_position}')) > 0)
            assert input_bytes() == before_input
            before = progress()
            eventually(lambda: progress() > before + 5)
            send('-' * 30)
            eventually(lambda: tmux('display-message', '-p', '#{pane_in_mode}') == '0')
            assert input_bytes() == before_input
            send('ip')
            eventually(lambda: input_bytes().startswith(before_input + b'a\x1b'))
            before_input = input_bytes()
            send('++++')
            eventually(lambda: tmux('display-message', '-p', '#{pane_in_mode}') == '1')
            send('e')
            eventually(lambda: tmux('display-message', '-p', '#{pane_in_mode}') == '0')
            assert input_bytes() == before_input
            (root / 'pause').unlink()
            send('t')
            waitstate(normalfull=1, normalfocused=1, visible=0)
            send('t')
            waitstate(visible=1, focused=1)
            send('t' * 100)
            state = waitstate(count=1, visible=1, focused=1)
            assert (state['server'], state['terminal']) == (server, terminal)
            assert tmux('display-message', '-p', '#{pane_pid}') == pane
            assert len(launches()) == 1

            # Summon from another monitor and another tag without duplicating.
            send('mt')
            state = waitstate(count=1, visible=1, focused=1, selected=1)
            geometry(state)
            send('v')
            waitstate(visible=0, enabled=0)
            send('t')
            state = waitstate(visible=1, focused=1)
            send('tvu')
            state = waitstate(shown=0, visible=0, enabled=0)
            send('t')
            state = waitstate(visible=1, focused=1, selected=1)
            geometry(state)
            assert tmux('display-message', '-p', '#{pane_pid}') == pane

            # Closing the popup and detaching the tmux client preserve Codex.
            send('k')
            waitstate(count=0, terminal=0)
            send('t' * 101)
            state = waitstate(count=1, visible=1, focused=1)
            assert tmux('display-message', '-p', '#{pane_pid}') == pane
            tmux('detach-client', '-s', 'codex')
            waitstate(count=0, terminal=0)
            send('t')
            state = waitstate(count=1, visible=1, focused=1)
            assert len(launches()) == 1

            # An actual killed footclient also leaves its pane running.
            os.kill(state['terminal'], signal.SIGKILL)
            waitstate(count=0, terminal=0)
            send('t')
            state = waitstate(count=1, visible=1, focused=1)
            assert tmux('display-message', '-p', '#{pane_pid}') == pane

            # A crashed Codex is retained, and Enter starts exactly one new chat.
            os.kill(int(launches()[-1]), signal.SIGKILL)
            eventually(lambda: tmux('display-message', '-p', '#{pane_dead}') == '1')
            client = tmux('list-clients', '-F', '#{client_name}')
            tmux('send-keys', '-K', '-c', client, 'Enter')
            eventually(lambda: len(launches()) == 2)
            assert tmux('display-message', '-p', '#{pane_dead}') == '0'

            tmux('kill-session', '-t', 'codex')
            waitstate(count=0, terminal=0)
            send('t')
            state = waitstate(count=1, visible=1, focused=1)
            eventually(lambda: len(launches()) == 3)
            assert state['server'] == server

            os.kill(server, signal.SIGKILL)
            waitstate(count=0, terminal=0, server=0)
            send('t')
            state = waitstate(count=1, visible=1, focused=1)
            eventually(lambda: len(launches()) == 4)
            assert state['server'] != server

            # Losing the shared foot server falls back to standalone foot.
            os.kill(state['foot'], signal.SIGTERM)
            waitstate(count=0, terminal=0)
            send('t')
            state = waitstate(count=1, visible=1, focused=1)
            assert Path('/proc', str(state['terminal']), 'comm').read_text().strip() == 'foot'
            assert len(launches()) == 4
            server, terminal = state['server'], state['terminal']
            last = int(launches()[-1])
            send('q')
            assert process.wait(timeout=5) == 0
            eventually(lambda: not alive(server) and not alive(terminal) and not alive(last))
            assert not Path(state['socket']).parent.exists()

            previous_socket = state['socket']
            process = subprocess.Popen([str(REPO / 'tests/codex'), str(REPO / 'dwl-codex')],
                                       env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=log, bufsize=0)
            output.clear()
            state = waitstate(count=1, shown=0, visible=0)
            eventually(lambda: len(launches()) == 5)
            assert state['socket'] != previous_socket
            server, terminal, last = state['server'], state['terminal'], int(launches()[-1])
            os.kill(process.pid, signal.SIGKILL)
            process.wait(timeout=5)
            eventually(lambda: not alive(server) and not alive(terminal) and not alive(last))

            # A new compositor ignores a stale socket left by the killed one.
            previous_socket = state['socket']
            assert Path(previous_socket).is_socket()
            process = subprocess.Popen([str(REPO / 'tests/codex'), str(REPO / 'dwl-codex')],
                                       env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=log, bufsize=0)
            output.clear()
            state = waitstate(count=1, shown=0, visible=0)
            eventually(lambda: len(launches()) == 6)
            assert state['socket'] != previous_socket
            send('q')
            assert process.wait(timeout=5) == 0
            assert not Path(state['socket']).parent.exists()
            print('Codex popup lifecycle, scrolling, geometry and tofi menu/clipboard integration tests passed')
        except BaseException:
            log.seek(0)
            print(log.read())
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
