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
                sh 'echo $JENKINS_RUN'
                sh 'scripts/coverage.sh'
            }
        }
    }
}
