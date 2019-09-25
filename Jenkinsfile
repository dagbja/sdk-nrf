
@Library("CI_LIB") _

def AGENT_LABELS = lib_Main.getAgentLabels(JOB_NAME)
def IMAGE_TAG    = lib_Main.getDockerImage(JOB_NAME)
def TIMEOUT      = lib_Main.getTimeout(JOB_NAME)
def INPUT_STATE  = lib_Main.getInputState(JOB_NAME)
def CI_STATE     = new HashMap()

pipeline {

  parameters {
    booleanParam(name: 'RUN_DOWNSTREAM', description: 'if false skip downstream jobs', defaultValue: true)
    booleanParam(name: 'RUN_TESTS', description: 'if false skip testing', defaultValue: true)
    booleanParam(name: 'RUN_BUILD', description: 'if false skip building', defaultValue: true)
    string(name: 'jsonstr_CI_STATE', description: 'Default State if no upstream job', defaultValue: INPUT_STATE)
  }
  agent { label AGENT_LABELS }

  options {
    checkoutToSubdirectory('lwm2m')
    parallelsAlwaysFailFast()
    timeout(time: TIMEOUT.time, unit: TIMEOUT.unit)
  }

  // environment {
  // }

  stages {
    stage('Load') { steps { script { CI_STATE = lib_Stage.load('LWM2M') }}}
    // stage('Specification') { steps { script {
    // }}}

    stage('Execution') { steps { script {
      docker.image("$DOCKER_REG/$IMAGE_TAG").inside {
        lib_Main.cloneCItools(JOB_NAME)
        dir('lwm2m') {
          checkout scm
          CI_STATE.LWM2M.REPORT_SHA = lib_Main.checkoutRepo(
                CI_STATE.LWM2M.GIT_URL, "LWM2M", CI_STATE.LWM2M, false)
          lib_West.AddManifestUpdate("LWM2M", 'nrf',
                CI_STATE.LWM2M.GIT_URL, CI_STATE.LWM2M.GIT_REF, CI_STATE)
        }
        lib_West.InitUpdate('lwm2m')
        lib_West.ApplyManifestUpdates(CI_STATE)
        sh "call build.sh"
      }
    }}}

    stage('Trigger Downstream Jobs') {
      when { expression { CI_STATE.LWM2M.RUN_DOWNSTREAM } }
      steps { script {
          CI_STATE.LWM2M.WAITING = true
          def DOWNSTREAM_JOBS = lib_Main.getDownStreamJobs(JOB_NAME)
          if (DOWNSTREAM_JOBS.size() == 1){
            DOWNSTREAM_JOBS.add("thst/test-ci-nrfconnect-cfg-null/lib")
          }
          def jobs = [:]
          DOWNSTREAM_JOBS.each {
            jobs["${it}"] = {
              build job: "${it}", propagate: CI_STATE.LWM2M.WAITING, wait: CI_STATE.LWM2M.WAITING,
                  parameters: [string(name: 'jsonstr_CI_STATE', value: lib_Util.HashMap2Str(CI_STATE))]
            }
          }
          parallel jobs
      } }
    }
    stage('Report') {
      when { expression { CI_STATE.LWM2M.RUN_TESTS } }
      steps { script {
          println 'no report generation yet'
      } }
    }
  }
  post {
    // This is the order that the methods are run. {always->success/abort/failure/unstable->cleanup}
    always {
      echo "Pipeline Post: always"
    }
    success {
      echo "Pipeline Post: success"
      script { lib_Status.set("SUCCESS", 'LWM2M', CI_STATE) }
    }
    aborted {
      echo "Pipeline Post: aborted"
      script { lib_Status.set("ABORTED", 'LWM2M', CI_STATE) }
    }
    unstable {
      echo "Pipeline Post: unstable"
    }
    failure {
      echo "Pipeline Post: failure"
      script { lib_Status.set("FAILURE", 'LWM2M', CI_STATE) }
    }
    cleanup {
        echo "Pipeline Post: cleanup"
        cleanWs()
    }
  }
}
