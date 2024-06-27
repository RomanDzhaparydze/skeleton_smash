#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    SmallShell& smallShell = SmallShell::getInstance();
    pid_t foreground_pid = smallShell.getForegroundPid();
    if (foreground_pid != -1) {
        // cout << "what is really killed" << foreground_pid << endl;
        if (kill(foreground_pid, SIGKILL) == -1) {
            perror("smash error: kill failed");
        } else {
            cout << "smash: process " << foreground_pid << " was killed" << endl;
        }
    }
}
