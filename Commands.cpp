#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << PRETTY_FUNCTION << " --> " << endl;

#define FUNC_EXIT()  \
  cout << PRETTY_FUNCTION << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell() {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

/
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/

    // For example:
/*
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if ...
  .....
  else {
    return new ExternalCommand(cmd_line);
  }
  */
Command *SmallShell::CreateCommand(const char *cmd_line) {


    string cmd_s = _trim(string(cmd_line)); 
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    if(firstWord.compare("chprompt")){
        return new ChPromptCommand(cmd_line);
    } else if (firstWord.compare("pwd") == 0){
        return new GetCurrDirCommand(cmd_line); 
    } else if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
    } else if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(cmd_line, // TODO);
    } else if (firstWord.compare("jobs") == 0) {
        return new JobsCommand(cmd_line, this->job_list_of_shell);
    } else if (firstWord.compare("fg") == 0) {
        return new ForegroundCommand(cmd_line, this->job_list_of_shell);
    } else if (firstWord.compare("quit") == 0) {
        return new QuitCommand(cmd_line, this->job_list_of_shell) //TODO;
    } else if (firstWord.compare("kill") == 0) {
        return new KillCommand(cmd_line, this->job_list_of_shell);
    } else if (firstWord.compare("alias") == 0) {
        return new aliasCommand(cmd_line);
    } else if (firstWord.compare("unalias") == 0) {
        return new unaliasCommand(cmd_line);
    } else {
        // TODO: call default shell to execute the command
        return new ExternalCommand(cmd_line);
    }

    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    //Command* cmd = CreateCommand(cmd_line);
    //cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
    cout << this->curr_prompt << ">" << endl;
}

void JobsList::addJob(Command *cmd, bool isStopped) {
    removeFinishedJobs();
    int job_id = 1;
    if (!jobs_list.empty()) job_id = jobs_list.back()->job_id + 1;

    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("smash error: fork failed");
        return;
    }
    if (child_pid == 0) {
        cmd->execute();
        exit(0);
    } else {
        if (cmd->background()) jobs_list.emplace_back(new JobEntry(job_id, child_pid, cmd, isStopped));
        else waitpid(child_pid, nullptr, 0);
    }
}

void JobsList::printJobsList() {
    removeFinishedJobs();
    for (JobEntry* job : jobs_list) {
        cout << "[" << job->job_id << "] " << job->command->getCommandStr() << endl;
    }
}

void JobsList::killAllJobs() {
    for (JobEntry* job : jobs_list) {
        if (kill(job->job_id, SIGKILL) != 0) perror("smash error: kill failed");
    }
    jobs_list.clear();
}

void JobsList::removeFinishedJobs() {
    auto it = jobs_list.begin();
    while (it != jobs_list.end()) {
        int end_status;
        pid_t result = waitpid((*it)->job_pid, &end_status, WNOHANG);
        if (result != 0) {
            delete *it;
            it = jobs_list.erase(it);
        }
        else it++;
    }

}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    for (JobEntry* job : jobs_list) {
        if (job->job_id == jobId) return job;
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId) {
    auto it = jobs_list.begin();
    while (it != jobs_list.end()) {
        if ((*it)->job_id == jobId) {
            delete *it;
            jobs_list.erase(it);
            return;
        }
    }
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    *lastJobId = jobs_list.back()->job_id;
    return jobs_list.back();
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    auto it = jobs_list.end() - 1;
    while (it != jobs_list.begin()) {
        if ((*it)->isStopped) {
            *jobId = (*it)->job_id;
            return *it;
        }
    }
    return nullptr;
}
