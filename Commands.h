#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <utility>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <map>
#include <set>
#include <regex>
#include "dirent.h"
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>

using namespace std;


#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define MAX_BUFFER_SIZE (4096)


class Command {
protected:
string command_str;
string command_name;
vector <string> command_args;
bool isBackground;

public:
    explicit Command(const char *cmd_line);

    virtual ~Command() = default;

    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();

    bool background() const {
        return isBackground;
    }

    string getCommandStr() const {
        return command_str;
    }
};

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
        isBackground = false;
    }
    
    virtual ~BuiltInCommand() = default;
};











class ChangeDirCommand : public BuiltInCommand {
private:
    char **plastPwd;

public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line), plastPwd(plastPwd) {};

    ~ChangeDirCommand() override {
        free(*plastPwd);
    }

    void execute() override {
        if (command_args.size() != 1) {
            std::cerr << "smash error: cd: too many arguments" << std::endl;
            return;
        }

        if (command_args[0] == "-") {
            if (*plastPwd == nullptr) {
                std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
                return;
            }
            command_args[0] = *plastPwd;
        };

        char curr_dir[MAX_BUFFER_SIZE];
        getcwd(curr_dir, sizeof(curr_dir));

        if (chdir(command_args[0].c_str()) != 0) {
            perror("smash error: chdir failed");
            return;
        }
        if (*plastPwd != nullptr) free(*plastPwd);
        *plastPwd = strdup(curr_dir);
    }
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const char *cmd_line): BuiltInCommand(cmd_line) {};

    virtual ~GetCurrDirCommand() = default;

    void execute() override {
        char BUFFER[MAX_BUFFER_SIZE];
        if(getcwd(BUFFER, sizeof(BUFFER))!= nullptr) {
            cout << BUFFER << endl;
        }
    }
};

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~ShowPidCommand() = default;

    void execute() override {
        cout << getpid() << endl;
    }
};





class JobsList {
public:
    class JobEntry {
    public:
        int job_id;
        pid_t job_pid;
        Command* command;

        time_t start_time;
        bool isStopped;

        JobEntry(int job_id, pid_t job_pid, Command* command, bool isStopped)
                : job_id(job_id), job_pid(job_pid), command(command), isStopped(isStopped) {
            time(&start_time);
        }
    };

private:
    std::vector<JobEntry*> jobs_list;

public:
    JobsList() = default;

    ~JobsList() = default;

    void addJob(Command *cmd, int pid ,bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);

    std::vector<JobEntry*> getJobsList() const {
        return jobs_list;
    }
    // TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand {
    // TODO: Add your data members
    JobsList * jobs;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line), jobs(jobs) {};

    virtual ~JobsCommand() = default;

    void execute() override {
        jobs->printJobsList();
    
    }
};

class QuitCommand : public BuiltInCommand {
// TODO: Add your data members
public:
    JobsList * jobs;
    QuitCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line), jobs(jobs) {};

    virtual ~QuitCommand() = default;

    void execute() override {
        if (command_args.at(0).compare("kill") == 0){
            jobs->killAllJobs();
        }
        exit(0);
    }
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
    JobsList * jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line), jobs(jobs) {};

    virtual ~KillCommand() = default;

    void execute() override {
        if (command_args.size() != 2 || command_args[0][0] != '-' || (command_args.size() == 2 && !all_of(command_args[0].begin() + 1, command_args[0].end(), ::isdigit)
        && !all_of(command_args[1].begin(), command_args[1].end(), ::isdigit))) {
            cerr << "smash error: kill: invalid arguments" << endl;
            return;
        }
        int signal = stoi(command_args[0].substr(1));
        int id = stoi(command_args[0]);
        JobsList::JobEntry* curr_job = jobs->getJobById(id);
        if (curr_job == nullptr) {
            cerr << "smash error: kill: job-id "<< id <<" does not exist" << endl;
            return;
        }
        if (kill(curr_job->job_pid, signal) == -1) {
            perror("smash error: kill failed");
            return;
        }
        std::cout << "signal number " << signal << " was sent to pid " << curr_job->job_pid << std::endl;
    }
};



class ListDirCommand : public BuiltInCommand {
public:
    explicit ListDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

    virtual ~ListDirCommand() = default;

    void execute() override {
        if (command_args.size() > 1) {
            std::cerr << "smash error: listdir: too many arguments" << std::endl;
            return;
        }
        const char* path_of_dir;
        if (command_args.empty()) path_of_dir = ".";
        else path_of_dir = command_args[0].c_str();

        DIR* dir = opendir(path_of_dir);
        if (dir == nullptr) {
            perror("smash error: opendir failed");
            return;
        }
        vector<string> files;
        vector<string> not_files;

        struct dirent* dir_member;
        while ((dir_member = readdir(dir)) != nullptr) {
            string path_with_name = string (path_of_dir) + "/" + dir_member->d_name;
            struct stat dir_member_stat;
            if (lstat(path_with_name.c_str(), &dir_member_stat) == -1) {
                perror("smash error: lstat failed");
                closedir(dir);
                return;
            }

            if (S_ISREG(dir_member_stat.st_mode)) {
                files.push_back("file: " + string(dir_member->d_name));
            } else if (S_ISDIR(dir_member_stat.st_mode)) {
                not_files.push_back("directory: " + string(dir_member->d_name));
            } else if (S_ISLNK(dir_member_stat.st_mode)) {
                char where_link_points_to[MAX_BUFFER_SIZE];
                ssize_t len = readlink(path_with_name.c_str(), where_link_points_to, sizeof(where_link_points_to) - 1);
                if (len != -1) {
                    where_link_points_to[len] = '\0';
                    not_files.push_back("link: " + string(dir_member->d_name) + " -> " + string(where_link_points_to));
                } else {
                    perror("smash error: readlink failed");
                    closedir(dir);
                    return;
                }
            }
        }
        closedir(dir);

        sort(files.begin(), files.end());
        sort(not_files.begin(), not_files.end());

        for (const string& file : files) {
            cout << file << endl;
        }
        for (const string& not_file :not_files) {
            cout << not_file << endl;
        }
    }
};

class GetUserCommand : public BuiltInCommand {
public:
    explicit GetUserCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

    virtual ~GetUserCommand() = default;

    void execute() override {
        if (command_args.size() != 1) {
            cerr << "smash error: getuser: too many arguments" << endl;
            return;
        }
        if (!all_of(command_args[0].begin(), command_args[0].end(), ::isdigit)) {
            cerr << "smash error: getuser: process " << command_args[0] << " does not exist" << endl;
            return;
        }
        int pid = stoi(command_args[0]);

        string path_with_proc = "/proc/" + command_args[0];
        struct stat proc_stat;
        if (stat(path_with_proc.c_str(), &proc_stat) == -1) {
            perror("smash error: getuser: process does not exist");
            return;
        }

        struct passwd *username = getpwuid(proc_stat.st_uid);
        struct group *group_name = getgrgid(proc_stat.st_gid);

        if (username == nullptr || group_name == nullptr) {
            cerr << "smash error: getuser: process " << pid << " does not exist" << endl;
            return;
        }

        cout << "User: " << username->pw_name << endl;
        cout << "Group: " << group_name->gr_name << endl;
    }
};

static set<string> reserved_keywords = {
        "chprompt", "showpid", "pwd", "cd", "jobs", "fg", "quit", "kill", "alias", "unalias", "listdir", "getuser", "watch",
};

static regex regex_exp_for_name("^([a-zA-Z0-9_]+)='([^']*)'$");


class aliasCommand : public BuiltInCommand {
private:
    map<string, string>& alias_map;
public:
    aliasCommand(const char *cmd_line, map<string, string>& alias_map) : BuiltInCommand(cmd_line), alias_map(alias_map) {}

    virtual ~aliasCommand() {}

    void execute() override {
        if (command_args.empty()) {
            for (auto& alias: alias_map) {
                cout << alias.first << "='" << alias.second << "'" << endl;
            }
            return;
        }
        if (command_args.size() != 1 || !regex_match(command_args[0], regex_exp_for_name)) {
            std::cerr << "smash error: alias: invalid alias format" << std::endl;
            return;
        }

        size_t pos_of_equals = command_args[0].find('=');
        if (pos_of_equals == 0 || pos_of_equals == command_args[0].size() - 1 || pos_of_equals == string::npos) {
            std::cerr << "smash error: alias: invalid alias format" << std::endl;
            return;
        }

        string new_name = command_args[0].substr(0, pos_of_equals);
        string old_name = command_args[0].substr(pos_of_equals + 1);

        if (old_name.front() =='\'' && old_name.back() == '\'') {
            old_name = old_name.substr(1, old_name.size() - 2);
        }
        else {
            std::cerr << "smash error: alias: invalid alias format" << std::endl;
            return;
        }

        if (reserved_keywords.find(old_name) != reserved_keywords.end() ||
            alias_map.find(new_name) != alias_map.end()) {
            std::cerr << "smash error: alias: invalid alias format" << std::endl;
            return;
        }
        alias_map[new_name] = old_name;
    }
};

class unaliasCommand : public BuiltInCommand {
private:
    map<string, string>& alias_map;
public:
    unaliasCommand(const char *cmd_line, map<string, string>& alias_map) : BuiltInCommand(cmd_line), alias_map(alias_map) {}

    virtual ~unaliasCommand() {}

    void execute() override {
        if (command_args.empty()) {
            std::cerr << "smash error: unalias: not enough arguments" << std::endl;
            return;
        }
        for (const auto& new_name : command_args) {
            auto it = alias_map.find(new_name);
            if (it == alias_map.end()) {
                std::cerr << "smash error: unalias: " << new_name << " alias does not exist" << std::endl;
                return;
            }
            alias_map.erase(it);
        }
    }
};


class SmallShell {
private:
    JobsList * job_list_of_shell;
    char* lastPwd;
    map<string, string> alias_map;
    pid_t foreground_pid;
    SmallShell();

public:
    static std::string curr_prompt;
    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);
    // TODO: add extra methods as needed

    const map<string, string>& getAliasMap() const {
        return alias_map;
    }

    JobsList* getJobsList() {
        return job_list_of_shell;
    }

    void setForegroundPid(pid_t pid) {
        foreground_pid = pid;
    }

    pid_t getForegroundPid() const {
        return foreground_pid;
    }
};

class ChPromptCommand : public BuiltInCommand {
public:
    explicit ChPromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    void execute() override {
        if(command_args.empty())
        {
            SmallShell::curr_prompt = "smash";
        } else {
            SmallShell::curr_prompt = command_args[0];
        }

    }
    virtual ~ChPromptCommand() {}
private:


};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

    virtual ~ExternalCommand() = default;

    void execute() override {
        pid_t pid = fork();
        if (pid == -1) {
            perror("smash error: fork failed");
            return;
        }
        else if (pid == 0) {
            setpgrp();

            vector<const char*> argv;
            argv.push_back(command_name.c_str());
            for (const string& command_arg : command_args) {
                argv.push_back(command_arg.c_str());
            }
            argv.push_back(nullptr);

            if (command_name.find('*') == string::npos && command_name.find('?') == string::npos) {
                execvp(argv[0], const_cast<char* const*>(argv.data()));
            } else {
                execl("/bin/bash", "bash", "-c", command_str.c_str(), nullptr);
            }
            perror("smash error: exec failed");
            exit(1);
        }
        else {
            SmallShell& smallShell = SmallShell::getInstance();
            if (isBackground) {
                smallShell.getJobsList()->addJob(this, pid, false);
            }
            else {
                smallShell.setForegroundPid(pid);
                int status;
                if (waitpid(pid, &status, WUNTRACED) == -1) {
                    perror("smash error: waitpid failed");
                }
                smallShell.setForegroundPid(-1);
            }
        }


    }
};

class WatchCommand : public Command {
    int interval;
    string command_to_watch;
public:
    explicit WatchCommand(const char *cmd_line) : Command(cmd_line), interval(2) {}

    virtual ~WatchCommand() = default;

    void execute() override {
        if (command_args.empty()) {
            cerr << "smash error: watch: command not specified" << endl;
            return;
        }
        if (command_args.size() == 1 && all_of(command_args[0].begin(), command_args[0].end(), ::isdigit)) {
            std::cerr << "smash error: watch: command not specified" << std::endl;
            return;
        }

        if (command_args.size() >= 2 && all_of(command_args[0].begin(), command_args[0].end(), ::isdigit)) {
            interval = std::stoi(command_args[0]);
            if (interval <= 0) {
                std::cerr << "smash error: watch: invalid interval" << std::endl;
                return;
            }

            command_to_watch = command_str.substr(command_str.find(command_args[1]));
        } else {
            command_to_watch = command_str.substr(command_str.find(command_args[0]));
        }

        SmallShell& smash = SmallShell::getInstance();
        while (true) {
            std::cout << "\033[2J\033[H";

            Command *cmd = SmallShell::getInstance().CreateCommand(command_to_watch.c_str());
            smash.setForegroundPid(getpid());
            cmd->execute();
            smash.setForegroundPid(-1);
            delete cmd;

            sleep(interval);
        }
    }
};

class PipeCommand : public Command {
    bool isErr;
    string command_name_1;
    string command_name_2;
public:
    explicit PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() = default;

    void execute() override {
        int my_pipe[2];
        if (pipe(my_pipe) == -1) {
            perror("smash error: pipe failed");
            return;
        }
        pid_t pid1 = fork();
        if (pid1 == -1) {
            perror("smash error: fork failed");
            return;
        }
        if (pid1 == 0) {
            setpgrp();
            int closed_channel;
            if (isErr) closed_channel = 2;
            else closed_channel = 1;
            close(closed_channel);
            if (dup2(my_pipe[1], closed_channel) == -1) {
                perror("smash error: dup2 failed");
                exit(1);
            }
            close(my_pipe[0]);
            close(my_pipe[1]);

            istringstream stream(command_name_1);
            string word;
            vector<const char*> argv;
            while (stream >> word) {
                argv.push_back(word.c_str());
            }
            argv.push_back(nullptr);

            execvp(argv[0], const_cast<char* const*>(argv.data()));
            perror("smash error: exec failed");
            exit(1);
        }
        pid_t pid2 = fork();
        if (pid2 == -1) {
            perror("smash error: fork failed");
            return;
        }

        if (pid2 == 0) {
            setpgrp();
            close(0);
            dup2(my_pipe[0], 0);
            close(my_pipe[0]);
            close(my_pipe[1]);
            istringstream stream(command_name_2);
            string word;
            vector<const char*> argv;
            while (stream >> word) {
                argv.push_back(word.c_str());
            }
            argv.push_back(nullptr);

            execvp(argv[0], const_cast<char* const*>(argv.data()));
            perror("smash error: exec failed");
            exit(1);
        }
        else {
            SmallShell& smallShell = SmallShell::getInstance();
            smallShell.setForegroundPid(pid1);
            close(my_pipe[0]);
            close(my_pipe[1]);
            int status;
            if (waitpid(pid1, &status, WUNTRACED) == -1) {
                perror("smash error: waitpid failed");
            }
            if (waitpid(pid2, &status, WUNTRACED) == -1) {
                perror("smash error: waitpid failed");
            }
            smallShell.setForegroundPid(-1);
        }
    }
};

class ForegroundCommand : public BuiltInCommand {
private:
    JobsList* jobs_list;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {};

    virtual ~ForegroundCommand() {}

    void execute() override {
        if (command_args.empty() && jobs_list->getJobsList().empty()) {
            cerr << "smash error: fg: jobs list is empty" << endl;
            return;
        }
        if (command_args.size() > 1 || (command_args.size() == 1 && !all_of(command_args[0].begin(), command_args[0].end(), ::isdigit))) {
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        }
        bool isIdGiven = command_args.size() == 1;
        JobsList::JobEntry* curr_job;
        int id;
        if (isIdGiven) {
            id = stoi(command_args[0]);
            curr_job = jobs_list->getJobById(id);
            if (curr_job == nullptr) {
                cerr << "smash error: fg: job-id " << id << " does not exist" << endl;
                return;
            }
        }
        else {
            curr_job = jobs_list->getLastJob(&id);
        }
        SmallShell& smallShell = SmallShell::getInstance();
        smallShell.setForegroundPid(curr_job->job_pid);

        cout << curr_job->command->getCommandStr() << " " << curr_job->job_pid << endl;
        int status;
        if (waitpid(curr_job->job_pid, &status, WUNTRACED) == -1) {
            perror("smash error: waitpid failed");
        }

        smallShell.setForegroundPid(-1);

        jobs_list->removeJobById(id);
    }
};

class RedirectionCommand : public Command {
private:
    bool isAppend;
    string command_name;
    string file_name;
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() = default;

    void execute() override {

        pid_t pid = fork();
        if (pid == -1) {
            perror("smash error: fork failed");
            return;
        }
        else if (pid == 0) {
            setpgrp();
            close(1);
            int success;
            if (isAppend) success = open(file_name.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            else success = open(file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (success == -1) {
                perror("smash error: open failed");
                exit(1);
            }

            istringstream stream(command_name);
            string word;
            vector<const char*> argv;
            while (stream >> word) {
                argv.push_back(word.c_str());
            }
            argv.push_back(nullptr);

            execvp(argv[0], const_cast<char* const*>(argv.data()));
            perror("smash error: exec failed");
            exit(1);
        }
        else {
            SmallShell& smallShell = SmallShell::getInstance();
            smallShell.setForegroundPid(pid);

            int status;
            if (waitpid(pid, &status, WUNTRACED) == -1) {
                perror("smash error: waitpid failed");

                smallShell.setForegroundPid(-1);

            }
        }
    }
};
#endif //SMASH_COMMAND_H_