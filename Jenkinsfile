//
// Copyright (c) 2019 Nordic Semiconductor ASA. All Rights Reserved.
//
// The information contained herein is confidential property of Nordic Semiconductor ASA.
// The use, copying, transfer or disclosure of such information is prohibited except by
// express written agreement with Nordic Semiconductor ASA.
//
@Library("CI_LIB") _

def AGENT_LABELS = lib_Main.getAgentLabels(JOB_NAME)
def IMAGE_TAG    = lib_Main.getDockerImage(JOB_NAME)
def TIMEOUT      = lib_Main.getTimeout(JOB_NAME)
def INPUT_STATE  = lib_Main.getInputState(JOB_NAME)
def CI_STATE     = new HashMap()

def ciUtils = null

pipeline {

  parameters {
    string(name: 'jsonstr_CI_STATE', defaultValue: INPUT_STATE, description: 'Default State if no upstream job')
    booleanParam(name: 'GENERATE_OBFUSCATED_LIB', defaultValue: false, description: 'if true generate an obfuscated version of the library')
  }

  agent {
    docker {
      image IMAGE_TAG
      label AGENT_LABELS
    }
  }

  options {
    checkoutToSubdirectory('lwm2m')
    parallelsAlwaysFailFast()
    timeout(time: TIMEOUT.time, unit: TIMEOUT.unit)
  }

  environment {
    // Environment variables for building.
    // GNUARMEMB_TOOLCHAIN_PATH is set in the Docker image.
    ZEPHYR_TOOLCHAIN_VARIANT = 'gnuarmemb' // Or 'zephyr' to use Zephyr SDK.
    ZEPHYR_BASE = "${env.WORKSPACE}/zephyr"
  }

  stages {

    stage('Load') {
      steps { script { CI_STATE = lib_Stage.load('LWM2M') }}
    }

    stage('West') {
      steps { script {
        lib_Main.cloneCItools(JOB_NAME)
        dir('lwm2m') {
          checkout scm
        }
        lib_West.InitUpdate('lwm2m')
        lib_West.ApplyManifestUpdates(CI_STATE)

        ciUtils = load("lwm2m/scripts/ci/ciutils.groovy")
        ciUtils.lwm2mCreateLogFile()

        def buildType = lib_Main.getBuildType(CI_STATE.NRF).toLowerCase();

        switch(buildType) {
          case "pr":
            // If we're a pull request, compare the target branch against the current HEAD (the PR), and also report issues to the PR
            COMMIT_RANGE = "$CI_STATE.NRF.MERGE_BASE..$CI_STATE.NRF.REPORT_SHA"
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

        def gitSha = ciUtils.getGitCommitSha('lwm2m/');
        ciUtils.lwm2mLog("LWM2M git SHA '${gitSha}'.")

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
      steps { script {

        /* Build the client application using library source files. */
        dir('lwm2m/client') {
          try {
            ciUtils.lwm2mLoadZephyrEnv()

            ciUtils.lwm2mLog("Building client application using library source files.")

            // Use West to built the application. West finds the ZEPHYR_BASE folder automatically.
            sh "rm -rf build && west build -b nrf9160_pca10090ns ."

            /* Store compiled files as artifacts. */
            sh 'mkdir output/'
            sh 'find build/zephyr/ -type f \\( -iname \\*.hex -o -iname \\*.elf -o -iname \\*.map \\) -exec cp {} output/ \\;'
            sh 'cd output/ && tar -zcvf client_app.tar.gz *'

            archiveArtifacts artifacts: 'output/*.tar.gz'
          }
          catch (err) {
            ciUtils.lwm2mLog("Build failed: ${err}")
          }
        }
      }}
    }

    stage('Build carrier library') {
      when {
        expression {
          def runObf = (env.BRANCH_NAME ==~ /(master|release.*)/) || (params.GENERATE_OBFUSCATED_LIB == true)
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
      steps { script {
        dir('lwm2m/project') {
          /* Compile a debug version of the LWM2M carrier library. */
          ciUtils.lwm2mCompileLib(true)
        }
      }}
    }
  }

  post {
    // This is the order that the methods are run. {always->success/abort/failure/unstable->cleanup}
    always {
      /* Store custom output log file. */
      archiveArtifacts artifacts: '*.log'
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
