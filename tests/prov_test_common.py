import platform
import re
import subprocess

from time import sleep

def verify_provisioned(akt_info, conf):
    # Verify that device HAS provisioned.
    for delay in [5, 5, 5, 5, 10, 10, 10, 10]:
        sleep(delay)
        stdout, stderr, retcode = run_subprocess([str(akt_info), '--config', str(conf)])
        if retcode == 0 and stderr == b'' and 'Fetched metadata: yes' in stdout.decode():
            break
    machine = platform.node()
    if (b'Device ID: ' not in stdout or
            b'Primary ecu hardware ID: ' + machine.encode() not in stdout or
            b'Fetched metadata: yes' not in stdout):
        print('Provisioning failed: ' + stderr.decode() + stdout.decode())
        return 1
    p = re.compile(r'Device ID: ([a-z0-9-]*)\n')
    m = p.search(stdout.decode())
    if not m or m.lastindex <= 0:
        print('Device ID could not be read: ' + stderr.decode() + stdout.decode())
        return 1
    print('Device successfully provisioned with ID: ' + m.group(1))
    return 0


def run_subprocess(command, **kwargs):
    print('> Running {}'.format(' '.join(command)))
    s = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False, **kwargs)
    return s.stdout, s.stderr, s.returncode

def popen_subprocess(command, **kwargs):
    print('> Running {}'.format(' '.join(command)))
    return subprocess.Popen(command, **kwargs)
