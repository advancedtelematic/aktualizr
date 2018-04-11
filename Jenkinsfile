pipeline {
    agent none
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
        }
    }
}
