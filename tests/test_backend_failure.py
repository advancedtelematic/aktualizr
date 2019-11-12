#!/usr/bin/env python3

import logging
import argparse

from os import getcwd, chdir

from test_fixtures import with_aktualizr, with_uptane_backend, KeyStore, with_secondary, with_path,\
    DownloadInterruptionHandler, MalformedJsonHandler, with_director, with_imagerepo, InstallManager,\
    with_install_manager, with_images, MalformedImageHandler, with_customrepo, SlowRetrievalHandler, \
    RedirectHandler, with_sysroot, with_treehub


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
@with_uptane_backend(start_generic_server=True)
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
    with director:
        with aktualizr:
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
@with_uptane_backend(start_generic_server=True)
@with_path(paths=['/1.root.json', '/root.json', '/timestamp.json', '/snapshot.json', '/targets.json'])
@with_imagerepo(handlers=[
                            DownloadInterruptionHandler(number_of_failures=1),
                            MalformedJsonHandler(number_of_failures=1),
                            DownloadInterruptionHandler(number_of_failures=3),
                        ])
@with_director(start=False)
@with_aktualizr(start=False, run_mode='full')
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
@with_uptane_backend(start_generic_server=True)
@with_images(images_to_install=[(('primary-hw-ID-001', 'primary-ecu-id'), 'primary-image.img')])
@with_imagerepo(handlers=[
                            DownloadInterruptionHandler(number_of_failures=1, url='/targets/primary-image.img'),
                            MalformedImageHandler(number_of_failures=1, url='/targets/primary-image.img'),
                        ])
@with_director(start=False)
@with_aktualizr(start=False, run_mode='full', id=('primary-hw-ID-001', 'primary-ecu-id'))
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
@with_uptane_backend(start_generic_server=True)
@with_customrepo(handlers=[
                            DownloadInterruptionHandler(number_of_failures=1, url='/primary-image.img'),
                            MalformedImageHandler(number_of_failures=1, url='/primary-image.img')
                            # TODO: this test fails too, although httpclient.cc sets
                            # CURLOPT_LOW_SPEED_TIME and CURLOPT_LOW_SPEED_TIME
                            # https://saeljira.it.here.com/browse/OTA-3737
                            #SlowRetrievalHandler(url='/primary-image.img')
                        ])
@with_imagerepo()
@with_director(start=False)
@with_aktualizr(start=False, run_mode='full')
def test_backend_failure_sanity_customrepo_update_after_image_download_failure(uptane_repo, custom_repo, director,
                                                                               aktualizr, **kwargs):
    update_hash = uptane_repo.add_image(aktualizr.id, 'primary-image.img',
                                        custom_url=custom_repo.base_url + '/' + 'primary-image.img')

    with aktualizr:
        with director:
            install_result = director.wait_for_install()

    return install_result and update_hash == aktualizr.get_current_image_info(aktualizr.id)


"""
    Verifies whether aktualizr is updatable after failure of object(s) download from Treehub/ostree repo
    with follow-up successful download.

    Currently, it's tested against two types of object download failure:
    - download interruption - object download is interrupted once, after that it's successful
    - malformed object - object download is successful but it's malformed. It happens once after that it's successful
"""
@with_uptane_backend(start_generic_server=True)
@with_director()
@with_treehub(handlers=[
                        DownloadInterruptionHandler(url='/objects/41/5ce9717fc7a5f4d743a4f911e11bd3ed83930e46756303fd13a3eb7ed35892.filez'),
                        MalformedImageHandler(url='/objects/41/5ce9717fc7a5f4d743a4f911e11bd3ed83930e46756303fd13a3eb7ed35892.filez'),

                        # TODO: ostree objects download is not resilient to `Slow Retrieval Attack`
                        # https://saeljira.it.here.com/browse/OTA-3737
                        #SlowRetrievalHandler(url='/objects/6b/1604b586fcbe052bbc0bd9e1c8040f62e085ca2e228f37df957ac939dff361.filez'),

                        # TODO: Limit a number of HTTP redirects within a single request processing
                        # https://saeljira.it.here.com/browse/OTA-3729
                        #RedirectHandler(number_of_redirects=1000, url='/objects/41/5ce9717fc7a5f4d743a4f911e11bd3ed83930e46756303fd13a3eb7ed35892.filez')
])
@with_sysroot()
@with_aktualizr(start=False, run_mode='once')
def test_backend_failure_sanity_treehub_update_after_image_download_failure(uptane_repo,
                                                                            aktualizr,
                                                                            director,
                                                                            uptane_server,
                                                                            sysroot, treehub):
    target_rev = treehub.revision
    uptane_repo.add_ostree_target(aktualizr.id, target_rev)
    with aktualizr:
        aktualizr.wait_for_completion()

    pending_rev = aktualizr.get_primary_pending_version()
    if pending_rev != target_rev:
        logger.error("Pending version {} != the target one {}".format(pending_rev, target_rev))
        return False

    sysroot.update_revision(pending_rev)
    aktualizr.emulate_reboot()

    with aktualizr:
        aktualizr.wait_for_completion()

    result = director.get_install_result() and (target_rev == aktualizr.get_current_primary_image_info())
    return result


"""
    Verifies that aktualizr does not install an image which contains files with wrong checksums
"""
@with_uptane_backend(start_generic_server=True)
@with_director()
@with_treehub(handlers=[
    MalformedImageHandler(url='/objects/41/5ce9717fc7a5f4d743a4f911e11bd3ed83930e46756303fd13a3eb7ed35892.filez',
                          number_of_failures=-1, fake_filez=True),

])
@with_sysroot()
@with_aktualizr(start=False, run_mode='once', output_logs=True)
def test_backend_failure_bad_ostree_checksum(uptane_repo,
                                             aktualizr,
                                             director,
                                             uptane_server,
                                             sysroot, treehub):
    target_rev = treehub.revision
    uptane_repo.add_ostree_target(aktualizr.id, target_rev)
    with aktualizr:
        aktualizr.wait_for_completion()

    pending_rev = aktualizr.get_primary_pending_version()
    if pending_rev == target_rev:
        logger.error("Pending version {} == the target one {}".format(pending_rev, target_rev))
        return False
    return True

"""
    Verifies if aktualizr supports redirects - update is successful after redirect
    Note: should aktualizr support unlimited number of redirects
"""
@with_uptane_backend(start_generic_server=True)
# TODO: Limit a number of HTTP redirects within a single request processing
# https://saeljira.it.here.com/browse/OTA-3729
@with_customrepo(handlers=[
                            RedirectHandler(number_of_redirects=10, url='/primary-image.img')
                        ])
@with_imagerepo()
@with_director()
@with_aktualizr(run_mode='once', output_logs=True)
def test_backend_failure_sanity_customrepo_update_redirect(aktualizr, uptane_repo,
                                                           custom_repo, director, **kwargs):
    update_hash = uptane_repo.add_image(aktualizr.id, 'primary-image.img',
                                        custom_url=custom_repo.base_url + '/' + 'primary-image.img')
    install_result = director.wait_for_install()
    return install_result and update_hash == aktualizr.get_current_image_info(aktualizr.id)

"""
    Verifies if aktualizr rejects redirects over 10 times - update fails after redirect
"""
@with_uptane_backend(start_generic_server=True)
@with_customrepo(handlers=[
                            RedirectHandler(number_of_redirects=(11 * 3 + 1), url='/primary-image.img')
                        ])
@with_imagerepo()
@with_director()
@with_aktualizr(start=False, run_mode='once', output_logs=True)
def test_backend_failure_sanity_customrepo_unsuccessful_update_redirect(aktualizr, uptane_repo,
                                                           custom_repo, director, **kwargs):
    update_hash = uptane_repo.add_image(aktualizr.id, 'primary-image.img',
                                        custom_url=custom_repo.base_url + '/' + 'primary-image.img')
    with aktualizr:
        aktualizr.wait_for_completion()

    return not director.get_install_result()

"""
  Verifies whether an update fails if director metadata download fails or they are malformed
  - download is interrupted three times
  - malformed json is received
"""
@with_uptane_backend(start_generic_server=True)
@with_path(paths=['/1.root.json', '/root.json', '/targets.json'])
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
@with_uptane_backend(start_generic_server=True)
@with_path(paths=['/1.root.json', '/root.json', '/timestamp.json', '/snapshot.json', '/targets.json'])
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
    parser.add_argument('-o', '--ostree', help='ostree support', default='OFF')
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
                    test_backend_failure_sanity_customrepo_unsuccessful_update_redirect,
    ]

    if input_params.ostree == 'ON':
        test_suite.append(test_backend_failure_sanity_treehub_update_after_image_download_failure)
        test_suite.append(test_backend_failure_bad_ostree_checksum)

    test_suite_run_result = True
    for test in test_suite:
        logger.info('>>> Running {}...'.format(test.__name__))
        test_run_result = test()
        logger.info('>>> {}: {}'.format('OK' if test_run_result else 'Failed', test.__name__))
        test_suite_run_result = test_suite_run_result and test_run_result

    chdir(initial_cwd)
    exit(0 if test_suite_run_result else 1)
