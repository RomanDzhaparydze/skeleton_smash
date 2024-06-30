#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

string curr_prompt = "smash";

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

int _parseCommandLine(const char *cmd_line, char** args) {
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

//
//
SmallShell::SmallShell() : job_list_of_shell(new JobsList()), lastPwd(nullptr), foreground_pid(-1) {}


SmallShell::~SmallShell() {
    free(lastPwd);
}

Command *SmallShell::CreateCommand(const char *cmd_line) {


    string cmd_s = _trim(string(cmd_line)); 
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    const char* real_command = cmd_line;
    auto alias_it = alias_map.find(firstWord);
    if (alias_it != alias_map.end()) {
        string alias_cmd = alias_it->second + cmd_s.substr(firstWord.length());
        cmd_s = _trim(string(alias_cmd));
        firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
        real_command = cmd_s.c_str();

    }

    string cmd_line_str(real_command);


    if(firstWord.compare("chprompt") == 0){
        return new ChPromptCommand(real_command);
    } else if (firstWord.compare("pwd") == 0){
        return new GetCurrDirCommand(real_command); 
    } else if (firstWord.compare("alias") == 0) {
        return new aliasCommand(real_command, alias_map, keys);
    } else if (firstWord.compare("unalias") == 0) {
        return new unaliasCommand(real_command, alias_map, keys);
    } else if (cmd_line_str.find('>') != string::npos){
        return new RedirectionCommand(real_command);
    } else if (cmd_line_str.find('|') != string::npos){
        return new PipeCommand(real_command);
    }  else if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(real_command);
    } else if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(real_command, &lastPwd);
    } else if (firstWord.compare("jobs") == 0) {
        return new JobsCommand(real_command, this->job_list_of_shell);
    } else if (firstWord.compare("fg") == 0) {
        return new ForegroundCommand(real_command, this->job_list_of_shell);
    } else if (firstWord.compare("quit") == 0) {
        return new QuitCommand(real_command, this->job_list_of_shell);
    } else if (firstWord.compare("kill") == 0) {
        return new KillCommand(real_command, this->job_list_of_shell);
    } else if (firstWord.compare("listdir") == 0) {
        return new ListDirCommand(real_command);
    } else if (firstWord.compare("getuser") == 0) {
        return new GetUserCommand(real_command);
    } else if (firstWord.compare("watch") == 0) {
        return new WatchCommand(real_command);
    } else {
        return new ExternalCommand(real_command, string(cmd_line));
    }
}

void SmallShell::executeCommand(const char *cmd_line) {
    job_list_of_shell->removeFinishedJobs();
    Command* cmd = CreateCommand(cmd_line);
    setForegroundPid(-1);
    cmd->execute();
}

void JobsList::addJob(Command *cmd, int pid, bool isStopped) {
    removeFinishedJobs();
    int job_id = 1;
    if (!jobs_list.empty()) job_id = jobs_list.back()->job_id + 1;
    jobs_list.push_back(new JobEntry(job_id, pid, cmd, isStopped));
}

void JobsList::printJobsList() {
    removeFinishedJobs();

    for (JobEntry* job : jobs_list) {

        cout << "[" << job->job_id << "] " << job->command->aliased_command << endl;
    }
}

void JobsList::killAllJobs() {
    for (JobEntry* job : jobs_list) {
        if (kill(job->job_pid, SIGKILL) != 0) perror("smash error: kill failed");
    }
    jobs_list.clear();
}

void JobsList::removeFinishedJobs() {
    auto it = jobs_list.begin();
    while (it != jobs_list.end()) {
        int end_status;
        pid_t result = waitpid((*it)->job_pid, &end_status, WNOHANG);
        if (result > 0) {
            delete *it;
            it = jobs_list.erase(it);
        } 
        else ++it;
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
        ++it;
    }
}

void JobsList::removeJobByPid(int jobPid) {
    auto it = jobs_list.begin();
    while (it != jobs_list.end()) {
        if ((*it)->job_pid == jobPid) {
            delete *it;
            jobs_list.erase(it);
            return;
        }
        ++it;
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

Command::Command(const char *cmd_line) : command_str(cmd_line), aliased_command(cmd_line) {
    isBackground = _isBackgroundComamnd(cmd_line);
    char* new_cmd = strdup(cmd_line);
    if (isBackground) _removeBackgroundSign(new_cmd);

    istringstream stream(new_cmd);
    string word;

    if (stream >> word) {
        command_name = word;
    }
    while (stream >> word) {
        command_args.push_back(word);
    }
        
    free(new_cmd);

}

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {
    isBackground = false;
    char* copy = strdup(command_str.c_str());
    _removeBackgroundSign(copy);
    _trim(copy);
    command_str = string(copy);
    free(copy);

    size_t pos_of_big_redir = command_str.find(">>");
    if (pos_of_big_redir != string::npos) {
        isAppend = true;
        file_name = _trim(command_str.substr(pos_of_big_redir + 2));
        command_name_in_redir = _trim(command_str.substr(0, pos_of_big_redir));
    }
    else {
        pos_of_big_redir = command_str.find('>');
        isAppend = false;
        file_name = _trim(command_str.substr(pos_of_big_redir + 1));
        command_name_in_redir = _trim(command_str.substr(0, pos_of_big_redir));
    }
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {
    char* copy = strdup(command_str.c_str());
    _removeBackgroundSign(copy);
    _trim(copy);
    command_str = string(copy);
    free(copy);    size_t pos_of_pipe_err = command_str.find("|&");
    if (pos_of_pipe_err != string::npos) {
        isErr = true;
        command_name_1 = _trim(command_str.substr(0, pos_of_pipe_err));
        command_name_2 = _trim(command_str.substr(pos_of_pipe_err+2));
    }
    else {
        pos_of_pipe_err = command_str.find('|');
        isErr = false;
        command_name_1 = _trim(command_str.substr(0, pos_of_pipe_err));
        command_name_2 = _trim(command_str.substr(pos_of_pipe_err+1));
    }

}

aliasCommand::aliasCommand(const char *cmd_line, map<string, string>& alias_map, vector<string>& keys) : BuiltInCommand(cmd_line), alias_map(alias_map), keys(keys) {
     char* copy = strdup(command_str.c_str());
     _removeBackgroundSign(copy);
     command_str = string(copy);
     free(copy);
}