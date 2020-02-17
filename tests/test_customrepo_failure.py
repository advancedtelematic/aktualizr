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
                    test_backend_failure_sanity_customrepo_update_after_image_download_failure,
                    test_backend_failure_sanity_customrepo_update_redirect,
                    test_backend_failure_sanity_customrepo_unsuccessful_update_redirect,
    ]

    test_suite_run_result = True
    for test in test_suite:
        logger.info('>>> Running {}...'.format(test.__name__))
        test_run_result = test()
        logger.info('>>> {}: {}\n'.format('OK' if test_run_result else 'FAILED', test.__name__))
        test_suite_run_result = test_suite_run_result and test_run_result

    chdir(initial_cwd)
    exit(0 if test_suite_run_result else 1)
