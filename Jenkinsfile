// from https://stackoverflow.com/a/48956042
@NonCPS
def cancelPreviousBuilds() {
  /* Needs the following methods in the Jenkins signature approval list
     (https://jenkins/scriptApproval):
       method hudson.model.Job getBuilds
       method hudson.model.Run getNumber
       method hudson.model.Run isBuilding
       method jenkins.model.Jenkins getItemByFullName java.lang.String
       method org.jenkinsci.plugins.workflow.job.WorkflowRun doStop
       staticMethod jenkins.model.Jenkins getInstance
       staticMethod org.codehaus.groovy.runtime.DefaultGroovyMethods toInteger java.lang.Number
    Note: `staticMethod jenkins.model.Jenkins getInstance` is marked as dangerous
  */
  print "Cancelling outdated running builds"
  try {
    def jobName = env.JOB_NAME
    def buildNumber = env.BUILD_NUMBER.toInteger()
    /* Get job name */
    def currentJob = Jenkins.instance.getItemByFullName(jobName)

    /* Iterating over the builds for specific job */
    for (def build : currentJob.builds) {
      /* If there is a build that is currently running and it's not current build */
      if (build.isBuilding() && build.number.toInteger() != buildNumber) {
        /* Than stopping it */
        build.doStop()
      }
    }
  } catch (Exception exc) {
    print "Couldn't stop outdated builds, continuing anyway"
  }
}

pipeline {
  agent none
  environment {
    JENKINS_RUN = '1'
    TEST_PARALLEL_LEVEL = '8'
  }
  stages {
    stage('cancel outdated builds') {
      steps {
        cancelPreviousBuilds()
      }
    }
    stage('test') {
      parallel {
        // run all tests with p11 and collect coverage
        stage('coverage') {
          agent {
            dockerfile {
              filename 'docker/Dockerfile.ubuntu.bionic'
            }
          }
          environment {
            TEST_BUILD_DIR = 'build-coverage'
            TEST_CMAKE_BUILD_TYPE = 'Valgrind'
            TEST_WITH_COVERAGE = '1'
            TEST_WITH_P11 = '1'
            TEST_WITH_FAULT_INJECTION = '1'
            // tests which requires credentials (build only)
            TEST_SOTA_PACKED_CREDENTIALS = credentials('garage-credentials')
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
              filename 'docker/Dockerfile.ubuntu.bionic'
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
        // run crypto and static tests on debian testing (with clang)
        stage('debian-testing') {
          agent {
            dockerfile {
              filename 'docker/Dockerfile.debian.testing'
            }
          }
          environment {
            TEST_BUILD_DIR = 'build-debian-testing'
            TEST_CC = 'clang'
            // should run with valgrind but some leaks are still unfixed
            // TEST_CMAKE_BUILD_TYPE = 'Valgrind'
            TEST_CMAKE_BUILD_TYPE = 'Debug'
            TEST_TESTSUITE_ONLY = 'crypto'
            TEST_WITH_STATICTESTS = '1'
            TEST_WITH_OPCUA = '1'
            TEST_WITH_LOAD_TESTS = '1'  // build only
          }
          steps {
            // FIXME: some failures left!
            sh 'scripts/test.sh'
          }
          post {
            always {
              step([$class: 'XUnitBuilder',
                  thresholds: [
                    [$class: 'SkippedThreshold', failureThreshold: '1'],
                    [$class: 'FailedThreshold', failureThreshold: '1']
                  ],
                  tools: [[$class: 'CTestType', pattern: 'build-debian-testing/**/Test.xml']]])
            }
          }
        }
        // build and test aktualizr.deb and garage_deploy.deb
        stage('bionic_pkg') {
          agent any
          environment {
            DOCKERFILE = 'docker/Dockerfile.ubuntu.bionic'
            INSTALL_DOCKERFILE = 'docker/Dockerfile-test-install.ubuntu.bionic'
            TEST_BUILD_DIR = 'build-bionic'
            TEST_INSTALL_RELEASE_NAME = '-ubuntu_18.04'
            TEST_INSTALL_DESTDIR = "${env.WORKSPACE}/build-bionic/pkg"
          }
          steps {
            // build package inside docker
            sh '''
               IMG_TAG=deb-$(cat /proc/sys/kernel/random/uuid)
               mkdir -p ${TEST_INSTALL_DESTDIR}
               docker build -t ${IMG_TAG} -f ${DOCKERFILE} .
               docker run -eTEST_BUILD_DIR=${TEST_BUILD_DIR} -eTEST_INSTALL_RELEASE_NAME=${TEST_INSTALL_RELEASE_NAME} -u $(id -u):$(id -g) -v $PWD:$PWD -v ${TEST_INSTALL_DESTDIR}:/persistent -w $PWD --rm ${IMG_TAG} $PWD/scripts/build_ubuntu.sh
               '''
            // test package installation in another docker
            sh 'scripts/test_garage_deploy_deb.sh ${TEST_INSTALL_DESTDIR} ${INSTALL_DOCKERFILE}'
            // FIXME: this can only run under xenial, because we use a static
            // .deb inside the repo
            // sh 'scripts/test_aktualizr_deb_ubuntu.sh ${TEST_INSTALL_DESTDIR} ${INSTALL_DOCKERFILE}'
          }
          post {
            always {
              archiveArtifacts artifacts: "build-bionic/pkg/*.deb", fingerprint: true
            }
          }
        }
        // build and test aktualizr.deb and garage_deploy.deb
        stage('xenial_pkg') {
          agent any
          environment {
            DOCKERFILE = 'docker/Dockerfile.ubuntu.xenial'
            INSTALL_DOCKERFILE = 'docker/Dockerfile-test-install.ubuntu.xenial'
            TEST_BUILD_DIR = 'build-xenial'
            TEST_INSTALL_RELEASE_NAME = '-ubuntu_16.04'
            TEST_INSTALL_DESTDIR = "${env.WORKSPACE}/build-xenial/pkg"
          }
          steps {
            // build package inside docker
            sh '''
               IMG_TAG=deb-$(cat /proc/sys/kernel/random/uuid)
               mkdir -p ${TEST_INSTALL_DESTDIR}
               docker build -t ${IMG_TAG} -f ${DOCKERFILE} .
               docker run -eTEST_BUILD_DIR=${TEST_BUILD_DIR} -eTEST_INSTALL_RELEASE_NAME=${TEST_INSTALL_RELEASE_NAME} -u $(id -u):$(id -g) -v $PWD:$PWD -v ${TEST_INSTALL_DESTDIR}:/persistent -w $PWD --rm ${IMG_TAG} $PWD/scripts/build_ubuntu.sh
               '''
            // test package installation in another docker
            sh 'scripts/test_garage_deploy_deb.sh ${TEST_INSTALL_DESTDIR} ${INSTALL_DOCKERFILE}'
            sh 'scripts/test_aktualizr_deb_ubuntu.sh ${TEST_INSTALL_DESTDIR} ${INSTALL_DOCKERFILE}'
          }
          post {
            always {
              archiveArtifacts artifacts: "build-xenial/pkg/*.deb", fingerprint: true
            }
          }
        }
      }
    }
  }
}
// vim: set ft=groovy tabstop=2 shiftwidth=2 expandtab:
