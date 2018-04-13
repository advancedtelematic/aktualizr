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
                publishHTML (target: [
                        allowMissing: false,
                        alwaysLinkToLastBuild: false,
                        keepAll: true,
                        reportDir: 'build-coverage/coverage',
                        reportFiles: 'index.html',
                        reportName: 'Coverage Report'
                ])
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
