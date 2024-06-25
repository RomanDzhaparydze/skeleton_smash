#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <utility>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <map>
#include <set>
#include <regex>

using namespace std;


#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)


class Command {
// TODO: Add your data members
protected:
std::string command_str;
std::string command_name;
std::vector <std::string> command_args;
bool isBackground;

public:
    Command(const char *cmd_line) : command_str(cmd_line), isBackground(false) {
        std::istringstream stream(cmd_line);
        std::string word;

        stream >> command_name;

        while (stream >> word) command_args.push_back(word);

        if (command_str.back() == '&') isBackground = true;
        {
            if (command_name.back() == '&') command_name = command_name.substr(0, command_name.size() - 1);
            else if (command_args.back() == "&") command_args.pop_back();
            else command_args.back() = command_args.back().substr(0, command_args.back().size() - 1);
        }

    };

    virtual ~Command() = default;

    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed

    bool background() const {
        return isBackground;
    }

    std::string getCommandStr() const {
        return command_str;
    }
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
        isBackground = false;
    }
    
    virtual ~BuiltInCommand() {}
};



class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() {}

    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {}

    void execute() override;
};

class WatchCommand : public Command {
    // TODO: Add your data members
public:
    WatchCommand(const char *cmd_line);

    virtual ~WatchCommand() {}

    void execute() override;
};

class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {}

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
private:
    char **plastPwd;

public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line), plastPwd(plastPwd) {};

    virtual ~ChangeDirCommand() {}

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

        char curr_dir[COMMAND_MAX_LENGTH];
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
    GetCurrDirCommand(const char *cmd_line): BuiltInCommand(cmd_line) {};

    virtual ~GetCurrDirCommand() {}

    void execute() override {
        char BUFFER[100000];
        if(getcwd(BUFFER, sizeof(BUFFER))!= NULL) {
            cout << BUFFER << endl;
        }
    }
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~ShowPidCommand() {}

    void execute() override {
        cout << getpid() << endl;
    }
};

class JobsList;

class QuitCommand : public BuiltInCommand {
// TODO: Add your data members
public:
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {}

    void execute() override;
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

    void addJob(Command *cmd, bool isStopped = false);

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

    virtual ~JobsCommand() {}

    void execute() override {
        jobs->printJobsList();
    
    }
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
    JobsList * jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line), jobs(jobs) {};

    virtual ~KillCommand() {}

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
        cout << curr_job->command->getCommandStr() << " " << curr_job->job_pid << endl;
        waitpid(curr_job->job_pid, nullptr, 0);
        jobs_list->removeJobById(id);
    }
};

class ListDirCommand : public BuiltInCommand {
public:
    ListDirCommand(const char *cmd_line);

    virtual ~ListDirCommand() {}

    void execute() override;
};

class GetUserCommand : public BuiltInCommand {
public:
    GetUserCommand(const char *cmd_line);

    virtual ~GetUserCommand() {}

    void execute() override;
};

static set<string> reserved_keywords = {
        "chprompt", "showpid", "pwd", "cd", "jobs", "fg", "quit", "kill", "alias", "unalias", "listdir", "getuser", "watch",
        "chprompt&", "showpid&", "pwd&", "cd&", "jobs&", "fg&", "quit&", "kill&", "alias&", "unalias&", "listdir&", "getuser&", "watch&"
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
    // TODO: Add your data members
    JobsList * job_list_of_shell;
    char* lastPwd;
    map<string, string> alias_map;
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

    void executeCommand(const char *cmd_line) {}
    // TODO: add extra methods as needed

    const map<string, string>& getAliasMap() const {
        return alias_map;
    }
};

class ChPromptCommand : public BuiltInCommand {
public:
    ChPromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    void execute() override {
        if(command_args.empty())
        {
            SmallShell::curr_prompt = "smash";
        } else {
            SmallShell::curr_prompt = command_args[0];
        }

    }
    virtual ~ChPromptCommand() {};
private:


};

#endif //SMASH_COMMAND_H_