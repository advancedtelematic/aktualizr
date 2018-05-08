pipeline {
  agent none
  environment {
    JENKINS_RUN = '1'
  }
  stages {
    stage('test') {
      parallel {
        // run all tests with p11 and collect coverage
        stage('coverage') {
          agent {
            dockerfile {
              filename 'Dockerfile'
            }
          }
          environment {
            TEST_BUILD_DIR = 'build-coverage'
            TEST_CMAKE_BUILD_TYPE = 'Valgrind'
            TEST_WITH_COVERAGE = '1'
            TEST_WITH_P11 = '1'
            // tests which requires credentials (build only)
            TEST_SOTA_PACKED_CREDENTIALS = 'dummy-credentials'
            TEST_TESTSUITE_EXCLUDE = 'credentials'
          }
          steps {
            sh 'scripts/test.sh'
          }
          post {
            always {
              step([$class: 'XUnitBuilder',
                  thresholds: [
                    [$class: 'SkippedThreshold', failureThreshold: '1'],
                    [$class: 'FailedThreshold', failureThreshold: '1']
                  ],
                  tools: [[$class: 'CTestType', pattern: 'build-coverage/**/Test.xml']]])
              publishHTML (target: [
                  allowMissing: false,
                  alwaysLinkToLastBuild: false,
                  keepAll: true,
                  reportDir: 'build-coverage/coverage',
                  reportFiles: 'index.html',
                  reportName: 'Coverage Report'
              ])
            }
          }
        }
        // run crypto tests without p11
        stage('nop11') {
          agent {
            dockerfile {
              filename 'Dockerfile.nop11'
            }
          }
          environment {
            TEST_BUILD_DIR = 'build-nop11'
            TEST_CMAKE_BUILD_TYPE = 'Valgrind'
            TEST_TESTSUITE_ONLY = 'crypto'
          }
          steps {
            sh 'scripts/test.sh'
          }
        }
        // build garage_deploy.deb
        stage('garage_deploy') {
          agent any
          environment {
            TEST_INSTALL_DESTDIR = "${env.WORKSPACE}/build-debstable/pkg"
          }
          steps {
            // build package inside docker
            sh '''
               IMG_TAG=deb-$(cat /proc/sys/kernel/random/uuid)
               mkdir -p ${TEST_INSTALL_DESTDIR}
               docker build -t ${IMG_TAG} -f Dockerfile.deb-stable .
               docker run -u $(id -u):$(id -g) -v $PWD:$PWD -v ${TEST_INSTALL_DESTDIR}:/persistent -w $PWD --rm ${IMG_TAG} $PWD/scripts/build_garage_deploy.sh
               '''
            // test package installation in another docker
            sh 'scripts/test_garage_deploy_deb.sh ${TEST_INSTALL_DESTDIR}'
          }
          post {
            always {
              archiveArtifacts artifacts: "build-debstable/pkg/*garage_deploy.deb", fingerprint: true
            }
          }
        }
        // run crypto tests with Openssl 1.1
        stage('openssl11') {
          agent {
            dockerfile {
              filename 'Dockerfile.deb-testing'
            }
          }
          environment {
            TEST_BUILD_DIR = 'build-openssl11'
            TEST_CMAKE_BUILD_TYPE = 'Valgrind'
            TEST_TESTSUITE_ONLY = 'crypto'
            TEST_WITH_STATICTESTS = '1'
            TEST_WITH_LOAD_TESTS = '1'  // build only
          }
          steps {
            // FIXME: some failures left!
            sh 'scripts/test.sh || true'
          }
          post {
            always {
              step([$class: 'XUnitBuilder',
                  thresholds: [
                    [$class: 'SkippedThreshold', failureThreshold: '1'],
                    [$class: 'FailedThreshold', failureThreshold: '1']
                  ],
                  tools: [[$class: 'CTestType', pattern: 'build-openssl11/**/Test.xml']]])
            }
          }
        }
        // build and test aktualizr.deb
        stage('debian_pkg') {
          agent any
          environment {
            TEST_INSTALL_DESTDIR = "${env.WORKSPACE}/build-ubuntu/pkg"
          }
          steps {
            // build package inside docker
            sh '''
               IMG_TAG=deb-$(cat /proc/sys/kernel/random/uuid)
               mkdir -p ${TEST_INSTALL_DESTDIR}
               docker build -t ${IMG_TAG} -f Dockerfile.noostree .
               docker run -u $(id -u):$(id -g) -v $PWD:$PWD -v ${TEST_INSTALL_DESTDIR}:/persistent -w $PWD --rm ${IMG_TAG} $PWD/scripts/build_ubuntu.sh
               '''
            // test package installation in another docker
            sh 'scripts/test_aktualizr_deb_ubuntu.sh ${TEST_INSTALL_DESTDIR}'
          }
          post {
            always {
              archiveArtifacts artifacts: "build-ubuntu/pkg/*aktualizr.deb", fingerprint: true
            }
          }
        }
      }
    }
  }
}
// vim: set ft=groovy tabstop=2 shiftwidth=2 expandtab:
