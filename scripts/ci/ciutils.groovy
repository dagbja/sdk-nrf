
def getGitCommitSha(String gitRepoPath) {
  shortCommit = sh(returnStdout: true, script: "git -C ${gitRepoPath} log -n 1 --pretty=format:'%h'").trim()
  return shortCommit;
}

OUT_LOG_FILE = "${env.WORKSPACE}/lwm2m-build.log"

def lwm2mLog(String log) {
  sh "echo \"> ${log}\" >> $OUT_LOG_FILE"
}

def lwm2mCreateLogFile() {
  writeFile file: OUT_LOG_FILE, text: "LWM2M build output log:\r\n"
  lwm2mLog("Build ID ${env.BUILD_ID}.")
}

return this;