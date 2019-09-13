#!/usr/bin/env python3

import logging
import argparse

from os import getcwd, chdir

from test_fixtures import with_aktualizr, with_uptane_backend, KeyStore, with_secondary, with_path,\
    DownloadInterruptionHandler, MalformedJsonHandler, with_director, with_imagerepo, InstallManager,\
    with_install_manager, with_images, MalformedImageHandler, with_customrepo, SlowRetrievalHandler, \
    RedirectHandler


logger = logging.getLogger(__file__)

"""
Verifies whether aktualizr is updatable after director metadata download failure
with follow-up successful metadata download.

Currently, it's tested against two types of metadata download/parsing failure:
    - download interruption - metadata file download is interrupted once|three times, after that it's successful
    - malformed json - aktualizr receives malformed json/metadata as a response to the first request for metadata,
      a response to subsequent request is successful

Note: Aktualizr doesn't send any installation report in manifest in case of metadata download failure
https://saeljira.it.here.com/browse/OTA-3730
"""
@with_uptane_backend(start_generic_server=True, port=8888)
@with_path(paths=['/1.root.json', '/root.json', '/targets.json'])
@with_director(handlers=[
                            DownloadInterruptionHandler(number_of_failures=1),
                            MalformedJsonHandler(number_of_failures=1),
                            DownloadInterruptionHandler(number_of_failures=3),
                        ], start=False)
@with_aktualizr(start=False, run_mode='full')
@with_install_manager()
def test_backend_failure_sanity_director_update_after_metadata_download_failure(install_mngr, director,
                                                                                aktualizr, **kwargs):
    with aktualizr:
        with director:
            # we have to stop director before terminating aktualizr since the later doesn't support graceful shutdown
            # (doesn't handle any signal (SIGTERM, SIGKILL, etc) what leads to receiving broken requests at director
            # https://saeljira.it.here.com/browse/OTA-3744
            install_result = director.wait_for_install()
            install_result = install_result and install_mngr.are_images_installed()
    return install_result


"""
Verifies whether aktualizr is updatable after image metadata download failure
with follow-up successful metadata download.

Currently, it's tested against two types of metadata download/parsing failure:
    - download interruption - metadata file download is interrupted once|three times, after that it's successful
    - malformed json - aktualizr receives malformed json/metadata as a response to the first request for metadata,
      a response to subsequent request is successful

Note: Aktualizr doesn't send any installation report in manifest in case of metadata download failure
"""
@with_uptane_backend(start_generic_server=True, port=8888)
@with_path(paths=['/1.root.json', '/root.json', '/timestamp.json', '/snapshot.json', '/targets.json'])
@with_imagerepo(handlers=[
                            DownloadInterruptionHandler(number_of_failures=1),
                            MalformedJsonHandler(number_of_failures=1),
                            DownloadInterruptionHandler(number_of_failures=3),
                        ])
@with_director(start=False)
@with_aktualizr(run_mode='full')
@with_install_manager()
def test_backend_failure_sanity_imagerepo_update_after_metadata_download_failure(install_mngr, director,
                                                                                 aktualizr, **kwargs):
    with aktualizr:
        with director:
            install_result = director.wait_for_install()
            logger.info('Director install result: {}'.format(install_result))
            install_result = install_result and install_mngr.are_images_installed()
            logger.info('Are images installed: {}'.format(install_result))
    return install_result


"""
Verifies whether aktualizr is updatable after image download failure
with follow-up successful download.

Currently, it's tested against two types of image download failure:
    - download interruption - file download is interrupted once, after that it's successful
    - malformed image - image download is successful but it's malformed. It happens once after that it's successful
"""
@with_uptane_backend(start_generic_server=True, port=8888)
@with_images(images_to_install=[(('primary-hw-ID-001', 'primary-ecu-id'), 'primary-image.img')])
@with_imagerepo(handlers=[
                            # TODO: test fails because aktualizr issues byte range request
                            # that are not supported by server
                            # https://saeljira.it.here.com/browse/OTA-3716
                            #DownloadInterruptionHandler(number_of_failures=1, url='/targets/primary-image.img'),
                            MalformedImageHandler(number_of_failures=1, url='/targets/primary-image.img'),
                        ])
@with_director(start=False)
@with_aktualizr(run_mode='full', id=('primary-hw-ID-001', 'primary-ecu-id'))
@with_install_manager()
def test_backend_failure_sanity_imagerepo_update_after_image_download_failure(install_mngr, director,
                                                                              aktualizr, **kwargs):
    with aktualizr:
        with director:
            install_result = director.wait_for_install()
            install_result = install_result and install_mngr.are_images_installed()
    return install_result


"""
    Verifies whether aktualizr is updatable after malformed image is downloaded
    from a custom image server with follow-up successful download.
"""
@with_uptane_backend(start_generic_server=True, port=8888)
@with_images(images_to_install=[(('primary-hw-ID-001', 'primary-ecu-id'), 'primary-image.img',
                                 'http://localhost:8891/primary-image.img')])
@with_customrepo(handlers=[
                            # TODO: This test fails because the issue with image download
                            #  from a server that doesn't support byte range requests
                            # DownloadInterruptionHandler(number_of_failures=1, url='/primary-image.img'),
                            # https://saeljira.it.here.com/browse/OTA-3716
                            MalformedImageHandler(number_of_failures=1, url='/primary-image.img')
                            # TODO: this test fails too, although httpclient.cc sets
                            # CURLOPT_LOW_SPEED_TIME and CURLOPT_LOW_SPEED_TIME
                            # https://saeljira.it.here.com/browse/OTA-3737
                            #SlowRetrievalHandler(url='/primary-image.img')
                        ])
@with_imagerepo()
@with_director(start=False)
@with_aktualizr(run_mode='full', id=('primary-hw-ID-001', 'primary-ecu-id'))
@with_install_manager()
def test_backend_failure_sanity_customrepo_update_after_image_download_failure(install_mngr, director,
                                                                               aktualizr, **kwargs):
    with aktualizr:
        with director:
            install_result = director.wait_for_install()
            install_result = install_result and install_mngr.are_images_installed()
    return install_result


"""
    Verifies if aktualizr supports redirects - update is successful after redirect
    Note: should aktualizr support unlimited number of redirects
"""
@with_uptane_backend(start_generic_server=True, port=8888)
@with_images(images_to_install=[(('primary-hw-ID-001', 'primary-ecu-id'), 'primary-image.img',
                                 'http://localhost:8891/primary-image.img')])

# TODO: Limit a number of HTTP redirects within a single request processing
# https://saeljira.it.here.com/browse/OTA-3729
@with_customrepo(handlers=[
                            RedirectHandler(number_of_redirects=10, url='/primary-image.img')
                        ])
@with_imagerepo()
@with_director()
@with_aktualizr(run_mode='once', id=('primary-hw-ID-001', 'primary-ecu-id'))
@with_install_manager()
def test_backend_failure_sanity_customrepo_update_redirect(install_mngr, director, **kwargs):
    install_result = director.wait_for_install()
    return install_result and install_mngr.are_images_installed()


"""
  Verifies whether an update fails if director metadata download fails or they are malformed
  - download is interrupted three times
  - malformed json is received
"""
@with_uptane_backend(start_generic_server=True, port=8888)
# TODO: if root.json is malformed aktualizr ignores it and proceed with an update
# https://saeljira.it.here.com/browse/OTA-3717
# @with_path(paths=['/root.json'])

# TODO: if 1.root.json download from director fails then aktualizr just exits
# https://saeljira.it.here.com/browse/OTA-3728
#@with_path(paths=['/1.root.json'])
@with_path(paths=['/targets.json'])
@with_imagerepo()
@with_director(handlers=[
                            DownloadInterruptionHandler(number_of_failures=3),
                            MalformedJsonHandler(number_of_failures=1),
                        ])
@with_aktualizr(run_mode='once')
@with_install_manager()
def test_backend_failure_sanity_director_unsuccessful_download(install_mngr, aktualizr,
                                                               director, **kwargs):
    aktualizr.wait_for_completion()
    return not (director.get_install_result() or install_mngr.are_images_installed())


"""
  Verifies whether an update fails if repo metadata download fails or they are malformed
  - download is interrupted three times
  - malformed json is received
"""
@with_uptane_backend(start_generic_server=True, port=8888)
#@with_path(paths=['/root.json']) # TODO: if root.json is malformed aktualizr ignores it and proceed with an update
@with_path(paths=['/1.root.json', '/timestamp.json', '/snapshot.json', '/targets.json'])
@with_imagerepo(handlers=[
                            DownloadInterruptionHandler(number_of_failures=3),
                            MalformedJsonHandler(number_of_failures=1),
                        ])
@with_director()
@with_aktualizr(run_mode='once')
@with_install_manager()
def test_backend_failure_sanity_imagerepo_unsuccessful_download(install_mngr, aktualizr,
                                                                director, **kwargs):
    aktualizr.wait_for_completion()
    return not (director.get_install_result() or install_mngr.are_images_installed())


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)

    parser = argparse.ArgumentParser(description='Test backend failure')
    parser.add_argument('-b', '--build-dir', help='build directory', default='build')
    parser.add_argument('-s', '--src-dir', help='source directory', default='.')
    input_params = parser.parse_args()

    KeyStore.base_dir = input_params.src_dir
    initial_cwd = getcwd()
    chdir(input_params.build_dir)

    test_suite = [
                    test_backend_failure_sanity_director_update_after_metadata_download_failure,
                    test_backend_failure_sanity_imagerepo_update_after_metadata_download_failure,
                    test_backend_failure_sanity_imagerepo_update_after_image_download_failure,
                    test_backend_failure_sanity_customrepo_update_after_image_download_failure,
                    test_backend_failure_sanity_director_unsuccessful_download,
                    test_backend_failure_sanity_imagerepo_unsuccessful_download,
                    test_backend_failure_sanity_customrepo_update_redirect,
    ]

    test_suite_run_result = True
    for test in test_suite:
        logger.info('>>> Running {}...'.format(test.__name__))
        test_run_result = test()
        logger.info('>>> {}: {}'.format('OK' if test_run_result else 'Failed', test.__name__))
        test_suite_run_result = test_suite_run_result and test_run_result

    chdir(initial_cwd)
    exit(0 if test_suite_run_result else 1)
