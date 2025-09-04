#include<bits/stdc++.h>
#include "util_functions.h"
#include<signal.h>
#include<fcntl.h>
#include<sys/wait.h>
#include<readline/readline.h>
#include<readline/history.h>
#include<dirent.h>

using namespace std;

pid_t fg_pgid = -1; //pid for foreground process group

struct job // used to store suspended background job
{
    pid_t pid;
    string command;
    bool stopped;
};

vector<job> jobs;

vector<string> built_in_funcs = {"cd", "ls", "pwd", "echo", "pinfo", "search", "history"}; // used to check in autocorrect

vector<string> find_matches(const char *text, bool is_command)
{
    vector<string> matches;
    string prefix(text);

    if(is_command)
    {
        for(const string &command:built_in_funcs)
        {
            if(command.compare(0,prefix.size(),prefix) == 0)
            {
                matches.push_back(command);
            }
        }
    

        //checking non built in commands
        static vector<string> not_built_in;// static so that system commands are not searched and loaded everytime one presses tab
        if(not_built_in.empty())
        {
            char *path = getenv("PATH"); //inbuilt commands stored in PATH
            if(path)
            {
                stringstream ss(path);//converts string to stream like cin so that we can use getline to split string using delimter like :, as PATH has multiple paths seperated by :
                string dir;
                while(getline(ss,dir,':'))
                {
                    DIR *d1 = opendir(dir.c_str());
                    if(!d1)
                    {
                        continue;//cannot open this dir, move to next
                    }

                    struct dirent* entry;
                    while((entry = readdir(d1))!=nullptr)
                    {
                        not_built_in.push_back(entry->d_name);
                    }
                    closedir(d1);
                }

                sort(not_built_in.begin(), not_built_in.end());

                //move all non unique elemtns to back and give pointer to end of unique list, then erase from there to end of list(all duplicate)
                not_built_in.erase(unique(not_built_in.begin(),not_built_in.end()), not_built_in.end());

            }
        }

        for(const string&command :not_built_in)
        {
            if(command.compare(0,prefix.size(),prefix) == 0)
            {
                matches.push_back(command);
            }
        }

    }
    else //not command , so file or directory
    {
        string dir = ".";//by defualt current directory is searched
        string base = prefix;

        size_t slash = prefix.find_last_of('/');//if path is given, search that instead, base becomes the incomplete last part of path
        if(slash != string::npos)//npos is used for not found
        {
            dir = prefix.substr(0,slash);// form start to last /
            if(dir.empty()) 
            {
                dir = "/";
            }

            base = prefix.substr(slash+1);//remaining part
        }

        DIR * d = opendir(dir.c_str());
        if(d)
        {
            struct dirent * entry;
            while((entry = readdir(d)) != nullptr)
            {
                string name = entry->d_name;
                if(name.compare(0, base.size(), base) == 0)
                {
                    string match = (dir == ".") ? name : (dir + "/" + name);
                    if(entry->d_type == DT_DIR)
                    {
                        match += "/";//directories autocomplete end with /
                    }
                    matches.push_back(match);
                }
            }
        }
    }

    return matches;
}

vector<string> light_tokenize(const string &line)
{
    vector<string> tokens;
    string token;

    for(size_t i=0; i<line.size(); i++)
    {
        char ch = line[i];

        //if blank, token complete, can add
        if(isspace((unsigned char)ch))
        {
            if(!token.empty())
            {
                tokens.push_back(token);
                token.clear();
            }
        }

        //if special char add token then add that char
        else if(ch =='|' || ch == ';' || ch == '&' || ch == '<' || ch == '>')
        {
            if(!token.empty())
            {
                tokens.push_back(token);
                token.clear();
            }

            if((ch == '>') && (i+1<line.size()) && (line[i+1] == '>'))
            {
                tokens.push_back(">>");
                i++;
            }
            else
            {
                tokens.push_back(string(1,ch));
            }
        }
        else
        {
            token.push_back(ch);
        }
    }

    if(!token.empty())
    {
        tokens.push_back(token);
    }

    return tokens;
}

char **list_matches(const char*text, int start, int end)
{
    string line(rl_line_buffer);//we need whole line, not just current word(text) to figure out if it is command or filename
    vector<string> tokens = light_tokenize(line);

    bool is_command = false;
    if(tokens.empty())
    {
        is_command = true;
    }

    else
    {
        string last = tokens.back();

        if(last == "|" || last == ";" || last == "&")
        {
            is_command = true;
        }

        else if(tokens.size() >=2)
        {
            string prev = tokens[tokens.size() -2];
            if(prev == "|" || prev==";" || prev=="&")
            {
                is_command = true;
            }
        }

        else if(tokens.size() == 1 && start ==0)
        {
            is_command = true;
        }
    }

    vector<string> matches = find_matches(text, is_command);

    if(matches.empty())
    {
        return nullptr;
    }

    // array for readline

    char ** completion_options = (char**)malloc((matches.size()+1) * sizeof(char*));
    for(size_t i=0;i<matches.size();i++)
    {
        completion_options[i] = strdup(matches[i].c_str());
    }

    completion_options[matches.size()] = nullptr;

    return completion_options;
    
}

vector<string> io_redirection(vector<string> &tokens)
{

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


    //redirecting input /output when applicable
    if(!input_file.empty())
    {
        int fd = open(input_file.c_str(),O_RDONLY);

        if(fd<0)
        {
            perror("Input file error");
            exit(1);
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
            exit(1);
        }

        else
        {
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
    }

    return io_processed_tokens;
}

vector<string> split_pipeline(const string &command)
{
    vector<string> parts;
    string part;

    for(char ch:command)
    {
        if(ch=='|')
        {
            if(!part.empty())
            {
                parts.push_back(trim_space(part));
                part.clear();
            }

        }
        else
        {
            part.push_back(ch);
        }
    }

    if(!part.empty()) //push last token into vector if it is availible
    {
        parts.push_back(trim_space(part));
    }

    return parts;
}

void pipeline(vector<string>parts, const string &home, const string &startingpath)
{
    int n = parts.size();
    // pipe() creates a pipe, each pipe has 2 ends :. number of pipeline 2*(n-1)
    vector<int> pipelines(2*(n-1));

    for(int i=0;i<n-1;i++)
    {
        if(pipe(pipelines.data()+2*i)<0)
        {
            perror("pipe failed");
            exit(1);
        } //pipelines.data() returns pointer to first element,pipe() fills i,i+1 element pipeline therefore 2*i 
        //i=0 fills 0,1 ,i=1 fills 2,3 , i=2 fills 4,5 , and so on
    }

    for(int i=0; i<n; i++)
    {
        //use fork for every command so that processes dont exit before reading from last pipe and writing to next pipe
        pid_t pid = fork();
        if(pid < 0)
        {
            perror("fork failed");
            exit(1);
        }
        if(pid==0)//child
        {
            //read from last pipe
            if(i>0)
            {
                dup2(pipelines[2*(i-1)], STDIN_FILENO);
            }

            //write to next pipe
            if(i<n-1)
            {
                dup2(pipelines[(2*i)+1], STDOUT_FILENO);
            }

            //closing pipes in child, as they are already duplicated in stdout,stdin
            for(int j=0;j<2*(n-1);j++)
            {
                close(pipelines[j]);
            }

            //run this
            vector<string> tokens = split_command(parts[i]);

            if(tokens.empty())
            {
                return;
            }

            vector<string> processed_tokens = io_redirection(tokens);


            if(!implemented_commands(processed_tokens, home, startingpath))
            {
                excecute_externally(processed_tokens);
            }
            exit(0);
        }
    }

    //close all parent pipes
    for(int j=0;j<2*(n-1);j++)
    {
        close(pipelines[j]);
    }

    // wait for all children to finish
    for(int i=0;i<n;i++)
    {
        wait(nullptr);
    }


}


void handle_sigint(int sig)
{
    if(fg_pgid > 0)
    {
        kill(-fg_pgid, SIGINT); // send SIGINT to whole fg group
    }
}

void handle_sigtstp(int sig)
{
    if(fg_pgid > 0)
    {
        kill(-fg_pgid, SIGTSTP); // send sigtstp to whole fg grp
    }
}

void handle_sigchld(int sig)
{
    //reap dead children
    int status;
    while(waitpid(-1,&status,WNOHANG) > 0); // wait to reap all children
}

int main()
{

    string home = get_cwd();
    signal(SIGCHLD, handle_sigchld);
    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    rl_attempted_completion_function = list_matches;
    using_history();
    read_history("shell_history.txt");
    stifle_history(20);

    while(true)
    {

        //take input, verify and remove unnessary whitespaces
        char *input = readline(print_command_lhs(home).c_str());
        if(!input)
        {
            cout<<endl; //ctrl d handling
            break;
        }

        if(*input) add_history(input);// add in history
        string command (input);
        free(input);
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

        const string starting_path = get_cwd();

        if(!tokens.empty() && tokens[0] == "cd")
        {
            implemented_commands(tokens,home,starting_path);
            continue;
        }

        if(!tokens.empty() && tokens[0] == "exit")
        {
            write_history("shell_history.txt");
            break;
        }

        // //debug code to check this works

        // for (string s : tokens)
        // {
        //     cout<<s<<" ";
        // }
        // cout<<endl;


        //checking if pipelines are there
        vector<string> pipeline_parts = split_pipeline(command);
        if(pipeline_parts.size()>1)
        {
            pipeline(pipeline_parts, home, starting_path);
            continue;
        }

        

        if(bg)
        {
            pid_t pid = fork(); //fork is done regardless of bg or not as we have to keep shell alive and make sure input/ output of last command or parent does not effect this command
            if(pid ==0)// child 
            {
                setpgid(0,0); //putting in its own process group

                //restore default signal handelling in bg child so that it reacts to terminal signals normally
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);


                vector<string> io_processed_tokens = io_redirection(tokens);
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
            pid_t pid = fork();
            if(pid == 0)
            {
                setpgid(0,0); // ctrl z pauses the whole fg group, not just current task. so we have to put child process in different group to avoid pausing all processes at once

                //restore default signal handelling in bg child so that it reacts to terminal signals normally
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);

                vector<string> io_processed_tokens = io_redirection(tokens);
                if(!implemented_commands(io_processed_tokens,home,starting_path))
                {
                    excecute_externally(io_processed_tokens);
                }
                exit(0);
            }

            else if(pid>0)
            {
                setpgid(pid,pid);// can fail sometimes doe to race condition as child also calls setpgid

                fg_pgid = pid; //mark as foreground
                tcsetpgrp(STDIN_FILENO, fg_pgid); //giving terminal control to child

                int status;
                waitpid(-fg_pgid, &status, WUNTRACED|WCONTINUED); //WUNTRACED detects when process stopped
                
                tcsetpgrp(STDIN_FILENO, getpgrp()); // child finsihed, give terminal back to shell
                
                if(WIFSTOPPED(status)) // stopped by ctrl z
                {
                    cout<<"\n["<<pid<<"] stopped \n";
                    jobs.push_back({pid, command, true});
                }

                fg_pgid = -1; //done, reset
                

            }

            else
            {
                perror("fork failed");
                continue;
            }

        }


    }
    write_history("shell_history.txt");
    return 0;
}