#include<bits/stdc++.h>
#include "util_functions.h"
#include<signal.h>
#include<fcntl.h>

using namespace std;

int main()
{
    const char* home_env_variable = getenv("HOME");
    string home;
    if(home_env_variable)
    {
        home = home_env_variable;
    }
    else home = get_cwd();
    signal(SIGCHLD, SIG_IGN);
    while(true)
    {
        print_command_lhs(home);//print lhs every time

        string command;
        //take input, verify and remove unnessary whitespaces
        if(!getline(cin,command))
        {
            cout<<endl;
            break;
        }
        
        command = trim_space(command);
        if(command.empty())
        {
            continue;
        }
        // splitting commands into tokens so that we can use them
        vector<string> tokens = split_command(command);

        bool bg =false; // check for backgriund task
        if(!tokens.empty() && tokens.back() == "&")
        {
            bg = true;
            tokens.pop_back();
        }

        // io redirection is handled by changing the input and output fd
        string input_file, output_file;
        bool append = false;
        vector<string> io_processed_tokens;

        for(size_t i=0;i<tokens.size();i++)
        {
            if(tokens[i] == "<")
            {
                if(i+1 < tokens.size())
                {
                    input_file = tokens[i+1];
                    i++;
                }
            }

            else if(tokens[i] == ">")
            {
                if(i+1 < tokens.size())
                {
                    output_file = tokens[i+1];
                    append = false;
                    i++;
                }
            }

            else if(tokens[i] == ">>")
            {
                if(i+1 < tokens.size())
                {
                    output_file = tokens[i+1];
                    append = true;
                    i++;
                }
            }

            else
            {
                io_processed_tokens.push_back(tokens[i]);
            }
            
        }

        //creating a backup of standard input/output fd to use them later
        int stdin_copy = dup(STDIN_FILENO);
        int stdout_copy = dup(STDOUT_FILENO);

        //redirecting input /output when applicable
        if(!input_file.empty())
        {
            int fd = open(input_file.c_str(),O_RDONLY);
    
            if(fd<0)
            {
                perror("Input file error");
                dup2(stdin_copy, STDIN_FILENO);
                dup2(stdout_copy, STDOUT_FILENO);
                close(stdin_copy);
                close(stdout_copy);
                continue;
            }

            else
            {
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
        }

        if(!output_file.empty())
        {
            int fd;
            if(append)
            {
                fd = open(output_file.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
            }
            else
            {
                fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }

            if(fd < 0)
            {
                perror("Output file error\n");
                dup2(stdin_copy, STDIN_FILENO);
                dup2(stdout_copy, STDOUT_FILENO);
                close(stdin_copy);
                close(stdout_copy);
                continue;
            }

            else
            {
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }

        // //debug code to check this works

        // for (string s : tokens)
        // {
        //     cout<<s<<" ";
        // }
        // cout<<endl;
        const string starting_path = get_cwd();

        if(bg)
        {
            pid_t pid = fork();
            if(pid ==0)// child 
            {
                if(!implemented_commands(io_processed_tokens, home, starting_path))
                {
                    excecute_externally(io_processed_tokens);
                }
                exit(0); // exit child
            }
            else if(pid>0)
            {
                cout<<"["<<pid<<"]"<<endl;
            }

            else
            {
                perror("fork failed");
                continue;
            }
        }

        else
        {
            if(!implemented_commands(io_processed_tokens,home,starting_path))
            {
                excecute_externally(io_processed_tokens);
            }
        }

        //resetting standard input/output
        dup2(stdin_copy, STDIN_FILENO);
        dup2(stdout_copy, STDOUT_FILENO);
        close(stdin_copy);
        close(stdout_copy);

    }

    return 0;
}