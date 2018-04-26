pipeline {
  agent none
  environment {
    JENKINS_RUN = '1'
  }
  stages {
    stage('test') {
      parallel {
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
          }
          steps {
            sh 'scripts/test.sh'
          }
          post {
            always {
              step([$class: 'XUnitBuilder',
                  thresholds: [
                  [$class: 'SkippedThreshold', failureThreshold: '0'],
                  [$class: 'FailedThreshold', failureThreshold: '0']],
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
        stage('openssl11') {
          agent {
            dockerfile {
              filename 'Dockerfile.deb-testing'
            }
          }
          environment {
            TEST_BUILD_DIR = 'build-openssl11'
            TEST_WITH_TESTSUITE = '0'
            TEST_WITH_STATICTESTS = '1'
          }
          steps {
            sh 'scripts/test.sh'
          }
        }
        stage('debian_pkg') {
          agent any
          steps {
            // build package inside docker
            sh '''
               IMG_TAG=deb-$(cat /proc/sys/kernel/random/uuid)
               mkdir -p $PWD/build-ubuntu/pkg
               docker build -t ${IMG_TAG} -f Dockerfile.noostree .
               docker run -u $(id -u):$(id -g) -v $PWD:$PWD -v $PWD/build-ubuntu/pkg:/persistent -w $PWD --rm ${IMG_TAG} $PWD/scripts/build-ubuntu.sh
               '''
            // test package installation in another docker
            sh 'scripts/test_aktualizr_deb_ubuntu.sh $PWD/build-ubuntu/pkg'
          }
          post {
            always {
              archiveArtifacts artifacts: 'build-ubuntu/pkg/*aktualizr.deb', fingerprint: true
            }
          }
        }
      }
    }
  }
}
// vim: set ft=groovy tabstop=2 shiftwidth=2 expandtab:
