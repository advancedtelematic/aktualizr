import platform
import re
import subprocess


def verify_provisioned(akt_info, conf):
    # Verify that device HAS provisioned.
    stdout, stderr, retcode = run_subprocess([str(akt_info), '--config', str(conf), '--wait-until-provisioned'])
    machine = platform.node()
    if (b'Device ID: ' not in stdout or
            b'Primary ECU hardware ID: ' + machine.encode() not in stdout or
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
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kwargs)
    try:
        stdout, stderr = proc.communicate(timeout=60)
    except subprocess.TimeoutExpired:
        proc.kill()
        stdout, stderr = proc.communicate()
    return stdout, stderr, proc.returncode
