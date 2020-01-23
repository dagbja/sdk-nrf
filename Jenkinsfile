//
// Copyright (c) 2020 Nordic Semiconductor ASA. All Rights Reserved.
//
// The information contained herein is confidential property of Nordic Semiconductor ASA.
// The use, copying, transfer or disclosure of such information is prohibited except by
// express written agreement with Nordic Semiconductor ASA.
//

@Library("CI_LIB") _

HashMap CI_STATE = lib_State.getConfig(JOB_NAME)
def ciUtils = null

properties([
  pipelineTriggers([
    parameterizedCron( [
        ((JOB_NAME =~ /latest\/night\/.*\/master/).find() ? CI_STATE.CFG.CRON.NIGHTLY : ''),
        ((JOB_NAME =~ /latest\/week\/.*\/master/).find() ? CI_STATE.CFG.CRON.WEEKLY : '')
    ].join('    \n') )
  ]),
  ( JOB_NAME.contains('sub/') ? disableResume() :  disableConcurrentBuilds() )
])

pipeline
{
  parameters {
    booleanParam(name: 'RUN_TESTS', description: 'if false skip testing', defaultValue: true)
    booleanParam(name: 'RUN_BUILD', description: 'if false skip building', defaultValue: true)
    booleanParam(name: 'RUN_DOWNSTREAM', description: 'if false skip downstream jobs', defaultValue: true)
    booleanParam(name: 'GENERATE_OBFUSCATED_LIB', defaultValue: false, description: 'if true generate an obfuscated version of the library')
    booleanParam(name: 'COMPILE_NRF_CLIENT_APP', defaultValue: false, description: 'if true compile the nrf client application')
    string(      name: 'jsonstr_CI_STATE', description: 'Default State if no upstream job', defaultValue: CI_STATE.CFG.INPUT_STATE_STR )
    choice(      name: 'CRON', description: 'Cron Test Phase', choices: CI_STATE.CFG.CRON_CHOICES)
  }

  agent {
    docker {
      image CI_STATE.CFG.IMAGE_TAG
      label CI_STATE.CFG.AGENT_LABELS
    }
  }

  options {
    checkoutToSubdirectory('lwm2m')
    parallelsAlwaysFailFast()
    timeout(time: CI_STATE.CFG.TIMEOUT.time, unit: CI_STATE.CFG.TIMEOUT.unit)
  }

  environment {
    // Environment variables for building.
    // GNUARMEMB_TOOLCHAIN_PATH is set in the Docker image.
    ZEPHYR_TOOLCHAIN_VARIANT = 'gnuarmemb' // Or 'zephyr' to use Zephyr SDK.
    ZEPHYR_BASE = "${env.WORKSPACE}/zephyr"
  }

  stages {

    stage('Load') {
      steps { script {
        CI_STATE = lib_State.load('LWM2M', CI_STATE)
        ciUtils = load("lwm2m/scripts/ci/ciutils.groovy")
        ciUtils.lwm2mCreateLogFile()
      }}
    }

    stage('West') {
      when { expression { CI_STATE.SELF.RUN_BUILD } }
      steps { script {
        dir('lwm2m') {
          CI_STATE.SELF.REPORT_SHA = lib_Main.checkoutRepo(CI_STATE.SELF.GIT_URL,
                                                    "LWM2M", CI_STATE.SELF, false)
        }
        lib_West.InitUpdate('lwm2m')
        lib_West.ApplyManifestUpdates(CI_STATE)


        switch(lib_Main.getBuildType(CI_STATE.SELF).toLowerCase()) {
          case "pr":
            // If we're a pull request, compare the target branch against the current HEAD (the PR), and also report issues to the PR
            COMMIT_RANGE = "$CI_STATE.SELF.MERGE_BASE..$CI_STATE.SELF.REPORT_SHA"
            ciUtils.lwm2mLog("Building PR [$CHANGE_ID]: $COMMIT_RANGE")
            break

          case "tag":
            COMMIT_RANGE = "tags/${env.BRANCH_NAME}..tags/${env.BRANCH_NAME}"
            ciUtils.lwm2mLog("Building tag: $COMMIT_RANGE")
            break

          case "branch":
            // If not a PR, it's a non-PR-branch or master build. Compare against the origin.
            COMMIT_RANGE = "origin/${env.BRANCH_NAME}..HEAD"
            ciUtils.lwm2mLog("Building branch: $COMMIT_RANGE")
            break

          default:
            assert condition : "Build type is unknown."
            break
        }

        ciUtils.lwm2mLog("LWM2M git SHA '$CI_STATE.SELF.REPORT_SHA'.")

        dir('lwm2m') {
          WEST_LIST = sh (
            script: 'west list',
            returnStdout: true
          )
          ciUtils.lwm2mLog("West modules:\r\n${WEST_LIST}")
        }
      }}
    }

    stage('Build client app') {
      when { expression { CI_STATE.SELF.RUN_BUILD } }
      steps { script {

        /* Build the client application using library source files. */
        dir('lwm2m/client') {
          try {
            ciUtils.lwm2mLoadZephyrEnv()

            ciUtils.lwm2mLog("Building client application using library source files.")

            // Use West to built the application. West finds the ZEPHYR_BASE folder automatically.
            sh 'rm -rf build && west build -b nrf9160_pca10090ns .'

            /* Store compiled files as artifacts. */
            sh 'mkdir output/'
            sh 'find build/zephyr/ -type f \\( -iname \\*.hex -o -iname \\*.elf -o -iname \\*.map \\) -exec cp {} output/ \\;'
            sh 'cd output/ && tar -zcvf client_app.tar.gz *'

            archiveArtifacts artifacts: 'output/*.tar.gz'
          }
          catch (err) {
            /* Any error in the scripts mark this stage as failing. */
            ciUtils.lwm2mLog("Build client app failed: ${err}")
            error('Build client app failed !')
            currentBuild.result = 'FAILURE'
          }
        }
      }}
    }

    stage('Build carrier library') {
      when {
        expression {
          def runObf = (CI_STATE.SELF.RUN_BUILD && env.BRANCH_NAME ==~ /(master|release.*)/) || (params.GENERATE_OBFUSCATED_LIB == true)
          if (!runObf) {
            ciUtils.lwm2mLog("Skip library compilation step on branch '${env.BRANCH_NAME}'.")
          }
          else {
            ciUtils.lwm2mLog("Generate an obfuscated library version on branch '${env.BRANCH_NAME}' (force=${params.GENERATE_OBFUSCATED_LIB}).")
          }
          return runObf
        }
      }

      steps { script {
        dir('lwm2m/project') {
          /* Compile an obfuscated version of the LWM2M carrier library. */
          ciUtils.lwm2mCompileLib()
        }
      }}
    }

    stage('Build debug carrier library') {
      when { expression { CI_STATE.SELF.RUN_BUILD } }
      steps { script {
        dir('lwm2m/project') {
          /* Compile a debug version of the LWM2M carrier library. */
          ciUtils.lwm2mCompileLib(true)
        }
      }}
    }

    stage('Build nrf client app') {
      when {
        expression {

          if ( params.COMPILE_NRF_CLIENT_APP == false) {
            ciUtils.lwm2mLog("Skip nrf client application compilation.")
          }
          return params.COMPILE_NRF_CLIENT_APP && CI_STATE.SELF.RUN_BUILD
        }
      }

      steps { script {

        /* Build the client application using library source files. */
        dir('lwm2m/client_nrf') {
          try {
            ciUtils.lwm2mLoadZephyrEnv()

            ciUtils.lwm2mLog("Building nrf client application using library.")

            // Use West to built the application. West finds the ZEPHYR_BASE folder automatically.
            sh 'rm -rf build && west build -b nrf9160_pca10090ns .'

            /* Store compiled files as artifacts. */
            sh 'mkdir output/'
            sh 'find build/zephyr/ -type f \\( -iname \\*.hex -o -iname \\*.elf -o -iname \\*.map \\) -exec cp {} output/ \\;'
            sh 'cd output/ && tar -zcvf client_app.tar.gz *'

            archiveArtifacts artifacts: 'output/*.tar.gz'
          }
          catch (err) {
            /* Any error in the scripts mark this stage as failing. */
            ciUtils.lwm2mLog("Build nrf client app failed: ${err}")
            error('Build nrf client app failed !')
            currentBuild.result = 'FAILURE'
          }
        }
      }}
    }
    stage('Trigger Downstream Jobs') {
      when { expression { CI_STATE.SELF.RUN_DOWNSTREAM } }
      steps { script { lib_Stage.runDownstream(JOB_NAME, CI_STATE) } }
    }

  }

  post {
    // This is the order that the methods are run. {always->success/abort/failure/unstable->cleanup}
    always {
      /* Store custom output log file. */
      script { if ( CI_STATE.SELF.RUN_BUILD && CI_STATE.SELF.RUN_TESTS ) {
        archiveArtifacts artifacts: '*.log'
      } else {
        currentBuild.result = "UNSTABLE"
      }}
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
