#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <utility>
#include <vector>
#include <algorithm>
#include <unistd.h>

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
    BuiltInCommand(const char *cmd_line) : Command(cmd_line) {};
    
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

        if (chdir(command_args[0]) != 0) {
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
    };
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
// TODO: Add your data members public:
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
    
    };
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
    JobsList * jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line), jobs(jobs) {};

    virtual ~KillCommand() {}

    void execute() override {

        if(command_args.at(0).compare("kill")==0) {
            jobs->killAllJobs();
        } 
        exit(0);
    };
};

class ForegroundCommand : public BuiltInCommand {
private:
    JobsList* jobs_list;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {};

    virtual ~ForegroundCommand() {}

    void execute() override {
        if (command_args.size() == 0 && (*jobs_list).getJobsList().empty()) {
            cerr << "smash error: fg: jobs list is empty" << endl;
            return;
        }
        if (command_args.size() > 1 || (command_args.size() == 1 && !all_of(command_args[0].begin(), command_args[0].end(), ::isdigit))) {
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        }
        bool isIdGiven = command_args.size() == 1;
        if (isIdGiven) {
            int id = stoi(command_args[0]);
            if (jobs_list->getJobById(id) == nullptr) {
                cerr << "smash error: fg: job-id " << id << " does not exist" << endl;
                return;
            }

        }

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

class aliasCommand : public BuiltInCommand {
public:
    aliasCommand(const char *cmd_line);

    virtual ~aliasCommand() {}

    void execute() override;
};

class unaliasCommand : public BuiltInCommand {
public:
    unaliasCommand(const char *cmd_line);

    virtual ~unaliasCommand() {}

    void execute() override;
};


class SmallShell {
private:
    // TODO: Add your data members
    JobsList * job_list_of_shell;
    char* lastPwd;

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

    };
    virtual ~ChPromptCommand() {};
private:


};

#endif //SMASH_COMMAND_H_