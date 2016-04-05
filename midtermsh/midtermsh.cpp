#include <cstdio>
#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <set>
#include <queue>
#include <map>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <fcntl.h>
#define sc scanf
#define pr printf
#define pb push_back

typedef std::pair<int, int> pii;
typedef std::pair<double, double> pdd;

const int MN = 10010;
const long long MAX_LONG = std::numeric_limits<long long>::max();
const int MAX_INT = std::numeric_limits<int>::max();
const int MIN_INT = std::numeric_limits<int>::min();

using namespace std;

struct command {
    std::string name;
    std::vector<std::string> args;
};

std::vector<std::string> split(std::string const &s, char delim) {
    std::vector<std::string> elements;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elements.push_back(item);
    }
    return elements;
}

std::vector<command> parseLine(std::string line) {
    std::vector<command> command_container;
    auto str_commands = split(line, '|');
    for (std::string s : str_commands) {
        if (s.empty()) {
            continue;
        }
        auto command_content = split(s, ' ');
        command new_command;
        for (std::string element : command_content) {
            if (element.empty()) {
                continue;
            }
            if (new_command.name.empty()) {
                new_command.name = element;
            }
            else {
                new_command.args.push_back(element);
            }
        }
        std::string carry;
        for (int i = 0; i < new_command.args.size(); i++) {
            /*
            std::cout << "wowbegin_args{\n";
            for (int j = 0; j < new_command.args.size(); j++) {
                std::cout << new_command.args[j] << std::endl;
            } std::cout << "end_args}\n";
            */
            std::string part = new_command.args[i];
            //std::cout << part << " carry: " << carry << std::endl;
            if (part[0] == '\'') {
                if (carry.empty()) {
                    carry = part;
                    new_command.args.erase(new_command.args.begin() + i);
                    i--;
                    continue;
                }
            }

            if (part[ part.length() - 1 ] == '\'') {
                carry += " " + part;
                carry = carry.substr(1, carry.length() - 2);
                new_command.args[i] = carry;
                carry = "";
                continue;
            }

            if (!carry.empty()) {
                carry += " " + part;
                new_command.args.erase(new_command.args.begin() + i);
                i--;
                continue;
            }

        }
        if (!new_command.name.empty()) {
            command_container.push_back(new_command);
        }
    }
    return command_container;
}

char ** get_C_string(command const &c) {
    char ** res = new char *[c.args.size() + 2];
    res[0] = new char[c.name.size() + 1];
    res[0] = strcpy(res[0], c.name.c_str());
    for (int i = 0; i < c.args.size(); i++) {
        std::string arg = c.args[i];
        res[i + 1] = new char[arg.size() + 1];
        strcpy(res[i + 1], arg.c_str());
    }
    res[c.args.size() + 1] = NULL;
    return res;
}

void delete_C_string(char **res, command const &c) {
    //char ** res = new char *[c.args.size() + 2];
    //res[0] = new char[c.name.size() + 1];
    delete[] res[0];
    for (int i = 0; i < c.args.size(); i++) {
        //res[i + 1] = new char[arg.size() + 1];
        delete[] res[i + 1];
    }
    delete [] res;
}

std::vector< std::pair<char **, command> > resources;

int main() {
    while (1) {
		const char *dollar = "$\n";
		write(1, dollar, 2);
		char s[MN];
		int length = 0;

		while (read(0, s + length, 1) > 0) {
            if (s[length] == '\n') {
                s[length] = '\0';
                break;
            }
            length++;
		}

        auto commands = parseLine(std::string(s));
        /*for (command c : commands) {
            std::cout << c.name << " {";
            for (std::string arg : c.args) {
                std::cout << "[" << arg << "]";
            }
            std::cout << "}\n";
        }*/
        if (commands.empty()) {
            continue;
        }

        int fds[MN][2];
        int pids[MN];

        int last_in_fd = STDIN_FILENO;
        int last_out_fd = STDOUT_FILENO;
        for (int k = 0; k < commands.size(); k++) {
            command cur_command = commands[k];
            int * fd = fds[k];
            pipe2(fd, O_CLOEXEC);

            char ** args = get_C_string(cur_command);
            resources.pb({args, cur_command});

            int pid = fork();
            pids[k] = pid;
            if (pid == 0) {
                if (k != 0) {
                    dup2(last_in_fd, STDIN_FILENO);
                }
                if (k != commands.size() - 1) {
                    dup2(fd[1], STDOUT_FILENO);
                }
                int ret = execvp(args[0], args);
                if (ret == -1) {
                    write(1, "Error while executing", 21);
                }
                return 0;
            }
            else if (pid < 0){
                write(1, "Error while forking", 19);
            }
            else {
                //cout << "amazing" << endl;
            }
            last_in_fd = fd[0];
            last_out_fd = fd[1];
        }
        for (int k = 0; k < commands.size(); k++) {
            close(fds[k][0]);
            close(fds[k][1]);
        }
        for (int k = 0; k < commands.size(); k++) {
            int status;
            waitpid(pids[k], &status, WUNTRACED);
        }
        for (int i = 0; i < resources.size(); ++i) {
            delete_C_string(resources[i].first, resources[i].second);
        }
        resources.clear();
    }
    return 0;
}
