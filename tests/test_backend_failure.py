#!/usr/bin/env python3
import sys
import os

from test_fixtures import with_aktualizr, with_uptane_backend, KeyStore


@with_uptane_backend()
@with_aktualizr(start=True, output_logs=True)
def test_backend_failure_01(aktualizr, **kwargs):
    aktualizr.wait_for_completion()


if __name__ == "__main__":
    KeyStore.base_dir = os.getcwd()
    initial_cwd = os.getcwd()
    os.chdir(os.path.join(initial_cwd, "build"))
    res = test_backend_failure_01()
    os.chdir(initial_cwd)
    sys.exit(res)



