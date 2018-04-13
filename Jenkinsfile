pipeline {
    agent none
    environment {
        JENKINS_RUN = '1'
    }
    stages {
        stage('coverage') {
            agent {
                dockerfile {
                    filename 'Dockerfile'
                }
            }
            steps {
                sh 'scripts/coverage.sh'
            }
            post {
                always {
                    step([$class: 'XUnitBuilder',
                        thresholds: [
                            [$class: 'SkippedThreshold', failureThreshold: '0'],
                            [$class: 'FailedThreshold', failureThreshold: '0']],
                        tools: [[$class: 'CTestType', pattern: 'build-coverage/**/Test.xml']]])
                }
            }
        }
    }
}
