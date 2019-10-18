
def getGitCommitSha(String gitRepoPath) {
  shortCommit = sh(returnStdout: true, script: "git -C ${gitRepoPath} log -n 1 --pretty=format:'%h'").trim()
  return shortCommit
}

OUT_LOG_FILE = "${env.WORKSPACE}/lwm2m-build.log"

def lwm2mLog(String log) {
  sh "echo \"> ${log}\" >> $OUT_LOG_FILE"
}

def lwm2mCreateLogFile() {
  writeFile file: OUT_LOG_FILE, text: "LWM2M build output log:\r\n"
  lwm2mLog("Build ID ${env.BUILD_ID}.")
}

def lwm2mLoadZephyrEnv() {
  /* Zephyr base must be set as environment variable. */
  lwm2mLog("Source Zephyr '${env.ZEPHYR_BASE}'.")
  sh "source ${env.ZEPHYR_BASE}/zephyr-env.sh"
}

/* Compile the LWM2M carrier library (debug or obfuscated) */
def lwm2mCompileLib(boolean debug = false) {
  try {

    lwm2mLoadZephyrEnv()

    /*
     * Make sure the toolchain is in PATH.
     * Prepare command for obfuscated or debug library generation.
     */
    lwm2mLog("Build ${debug ? "debug" : "obfuscated"} version of the LWM2M carrier library.")

    def statusCode = sh script:"""
    export PATH="${'$'}{GNUARMEMB_TOOLCHAIN_PATH}/bin/:${'$'}PATH" && \
    rm -rf output/ && \
    ./prepare.sh ${debug ? "-d" : ""}
    """, returnStatus:true

    if (statusCode != 0) {
      currentBuild.result = 'FAILURE'
    }

    lwm2mLog("Export generated files.")
    statusCode = sh script:"./export.sh", returnStatus:true

    if (statusCode != 0) {
      currentBuild.result = 'FAILURE'
    }

    lwm2mLog("Create archive to store artifacts.")
    def archiveName = "liblwm2m_carrier${debug ? "_debug" : ""}.tar.gz"
    sh "tar -zcvf ${archiveName} ../../nrf/lib/lwm2m_carrier/"

    archiveArtifacts artifacts: '*.tar.gz'
  }
  catch (err) {
    lwm2mLog("Build failed: ${err}")
  }
}

return this;