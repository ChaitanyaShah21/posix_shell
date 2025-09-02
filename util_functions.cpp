#include "util_functions.h"
#include <bits/stdc++.h>
#include<unistd.h>
#include<pwd.h>
#include<limits.h>
#include<sys/wait.h>
#include<dirent.h>
#include<sys/stat.h>
#include<grp.h>
#include<ctime>

bool before_start = false;
using namespace std;

string get_cwd() //just adding error detection to normal getcwd 
{
    char buffer[PATH_MAX];
    if(getcwd(buffer,sizeof(buffer)) == nullptr)
    {
        return string(".");
    }    
    return string(buffer);
}

// if user is in home or in a subdirectory of home, replace absolute home path with ~
string return_modified_path(const string &abs_path, const string &home_path)
{
    if(before_start)
    {
        return abs_path;
    }
    if(!home_path.empty() && (abs_path.rfind(home_path,0) == 0))
    {
        if(abs_path.size() == home_path.size())
        {
            return "~";
        }
        if(abs_path[home_path.size()] == '/')
        {
            return "~" + abs_path.substr(home_path.size());
        }
    }
    return abs_path;
}

string trim_space(const string &path)//remove space from start and end of path
{
    size_t i=0, j=path.size();
    while(i<j && isspace((unsigned char)path[i]))
    {
        i++;
    }

    while(j>i && isspace((unsigned char)path[j-1]))
    {
        j--;
    }
    return path.substr(i,j-i);
}

//split input command into tokens, so that we can analyse the commands

vector<string> split_command(const string &command)
{
    vector<string> token_list;
    string token;

    for(char ch :command)
    {
        if(isspace((unsigned char)ch))
        {
            if(!token.empty())
            {
                token_list.push_back(token);
                token.clear();
            }
        }
        else
        {
            token.push_back(ch);
        }
    }

    if(!token.empty())
    {
        token_list.push_back(token);
    }
    return token_list;
}

//finally print linux like prompt(lhs of command)
void print_command_lhs(const string &home_path)
{
    passwd *pw = getpwuid(getuid());
    const char* user = pw? pw->pw_name:"user";

    char host_name[256] = {0};

    gethostname(host_name,sizeof(host_name)-1);

    string cwd = get_cwd();
    cerr<<user<<"@"<<host_name<<":"<<return_modified_path(cwd,home_path)<<"> ";
    cerr.flush();
}

void excecute_externally( vector<string> tokens)
{
    if(tokens.empty())
    {
        return;
    }

    //check if command is background task
    bool bg_task = false;
    if(tokens.back() == "&")
    {
        bg_task=true;
        tokens.pop_back();
    }

    //execvp needs char* array as arguments, so we covert token to that

    vector<char*>argv;
    for(const string &token:tokens)
    {
        argv.push_back(const_cast<char*>(token.c_str()));//c_str needs char* not const char*,so we need const_cast to remove const
    }
    argv.push_back(nullptr);//execvp requires null terminated char*

    pid_t process_id = fork();//pid_t is used to store process id
    //fork returns 0 for child process, positive number for parent as parent id and -1 for failure

    if(process_id == 0)
    {
        if(execvp(argv[0],argv.data()) == -1)
        //argv[0] is program name, argv.data() returns pointer to first first element of argv
        {
            perror("execvp failed to execute");
            exit(1);
        }
    }

    else if(process_id>0)//if parent, wait for child to finish
    {
        if(bg_task)
        {
            cout<<"["<<process_id<<"]"<<endl;
        }
        else
        {
            int child_status;
            waitpid(process_id,&child_status,0);
        }

    }
    else
    {
        perror("Fork failed to execute");
    }
}

string check_permissions(mode_t mode)
{
    string permissions;//letter if particular permission availible else -
    permissions += (S_ISDIR(mode)) ?'d':'-';
    permissions += (mode & S_IRUSR) ?'r':'-';
    permissions += (mode & S_IWUSR) ?'w':'-';
    permissions += (mode & S_IXUSR) ?'x':'-';
    permissions += (mode & S_IRGRP) ?'r':'-';
    permissions += (mode & S_IWGRP) ?'w':'-';
    permissions += (mode & S_IXGRP) ?'x':'-';
    permissions += (mode & S_IROTH) ?'r':'-';
    permissions += (mode & S_IWOTH) ?'w':'-';
    permissions += (mode & S_IXOTH) ?'x':'-';

    return permissions;
}

// recursive file search
bool recursive_search(const string &path, const string &target)
{
    DIR *dir = opendir(path.c_str());
    if(!dir)
    {
        return false;
    }

    struct dirent * directory_entry;
    while((directory_entry = readdir(dir)) != nullptr)
    {
        string file_name = directory_entry->d_name;
        
        //skip . and .. as they are not vested directories
        if(file_name == "." || file_name == "..")
        {
            continue;
        }

        string file_path = path+"/"+file_name;

        if(file_name == target) //file found
        {
            closedir(dir);
            return true;
        }

        //directory, recursively search it to find file
        struct stat st;
        if(stat(file_path.c_str(), &st)==-1)
        {
            cerr<<"Stat failed for file/directory\n";
            continue;
        }
        if(S_ISDIR(st.st_mode))
        {
            if(recursive_search(file_path, target))
            {
                closedir(dir);
                return true;
            }
        }

    }

    closedir(dir);
    return false;

}

//if a command impe=lementation is done here, use that and return true else return false and excecute externally
bool implemented_commands(const vector<string> &tokens, const string &home_path, const string &starting_dir)
{
    if(tokens.empty())
    {
        return false;
    }

    const string &command = tokens[0];

    //check command and excecute

    if(command == "cd")
    {
        if(tokens.size()>2)
        {
            cerr<<"Invalid number of arguments\n";//cerr because perror is used for system error(adds correspoing error details provided by system)
            // we dont use cout as output of cout can beredirected directly to a file which we dont want in case of error message
            return true;
        }

        static string last_dir = get_cwd();//used in cd -, static so that it doesnt reset everytime cd is called
        string current_dir = get_cwd();
        string target_dir;

        if(tokens.size()==1 || tokens[1]=="~")// just cd therefor move to home
        {
            target_dir = home_path;
        }

        else if(tokens[1]=="-")
        {
            target_dir = last_dir;
        }
        else if(tokens[1] == ".")
        {
            target_dir = get_cwd();
        }
        else if(tokens[1] == "..")
        {
            int count=0;
            string curr_path = get_cwd();
            if(curr_path.size()>1&&curr_path.back()=='/')
            {
                curr_path.pop_back();// remove / at last , edge case
            }
            for( ssize_t i=curr_path.size()-1;i>=0;i--)
            {

                if(curr_path[i]=='/')
                {
                    break;
                }
                count++;
            }
            target_dir=curr_path.substr(0,curr_path.size()-count);//get parent directory
        }
        else
        {
            target_dir = tokens[1];
        }

        if(chdir(target_dir.c_str())!=0)
        {
            perror("cd failed to excecute");
        }
        else
        {
            last_dir = current_dir;
            current_dir=get_cwd();
            if(tokens[1] == "-")
            {
                cout<<target_dir<<endl;
            }
            //checking if what current directory is wrt to starting directory so that we can decide wether to display abs path or not
            if(starting_dir == current_dir.substr(0,starting_dir.size()))
            {
                if(starting_dir.size() < current_dir.size())
                {
                    if(current_dir[starting_dir.size()] == '/')
                    {
                        before_start = false;
                    }
                    else
                    {
                        before_start=true;
                    }
                }
                else if(starting_dir.size()==current_dir.size())
                {
                    before_start=false;
                }
                else
                {
                    before_start=true;
                }
            }
            else
            {
                before_start = true;
            }
        }

        return true;
    }

    
    if(command == "pwd")//easy, just print cwd
    {
        cout<<get_cwd()<<endl;
        return true;
    }

    
    if(command == "echo")//as string woulld be tokenized, manually add spaces which solves multiple space issue
    {
        for(size_t i=1;i<tokens.size();i++)
        {
            if(i!=1)//space before every word except first
            {
                cout<<" ";
            }
            cout<<tokens[i];
        }   
        cout<<endl;
        return true;
    }


    if(command=="ls")
    {
        bool all=false;
        bool long_version=false;
        bool path_set=false;
        string ls_path = get_cwd();//default oath is current path

        for(size_t i=1;i<tokens.size();i++)// read command to find flags and path
        {
            if(tokens[i][0]=='-')
            {
                if(tokens[i]=="-a" || tokens[i]=="-al" ||tokens[i]=="la")
                {
                    all = true;
                }
                if(tokens[i]=="-l" || tokens[i]=="-al" ||tokens[i]=="la")
                {
                    long_version = true;
                }
            }
            else
            {
                if(path_set==true)
                {
                    cerr<<"Only one path argument required\n";
                    return true;
                }
                ls_path=tokens[i];
                path_set=true;
            }
        }
        if(ls_path[ls_path.size()-1] == '/')
        {
            ls_path.pop_back();
        }

        struct  stat st;// to check if it is file or directory
        if(stat(ls_path.c_str(), &st)==-1)
        {
            perror("Cannot access path");
            return true;
        }
        if(S_ISDIR(st.st_mode))//if directory
        {
            DIR *dir = opendir(ls_path.c_str());//open path directory
            if(!dir)
            {
                perror("Could not open path");
                return true;
            }
            struct dirent *entry;
            vector<string> entries;
            while(((entry=readdir(dir)) != nullptr))
            {
                string name = entry->d_name;
                if(!all && name[0]=='.') //if -a, add hidden files also
                {
                    continue;
                }
                entries.push_back(name);
            }
            closedir(dir);

            sort(entries.begin(),entries.end());//ls in sorted order

            for(const string &name:entries)
            {
                string filepath = ls_path+"/"+name;

                struct stat st2;
                if(stat(filepath.c_str(),&st2) == -1)
                {
                    perror("Stat error");
                    continue;
                }

                if(long_version)
                {
                    cout<<check_permissions(st2.st_mode)<<" "
                    <<st2.st_nlink<<" "
                    <<getpwuid(st2.st_uid)->pw_name<<" "
                    <<getgrgid(st2.st_gid)->gr_name<<" "
                    <<st2.st_size<<" "
                    <<string(ctime(&st2.st_mtime)).substr(0,16)<<" "
                    <<name<<endl;
                }

                else
                {
                    cout<<name<<" ";
                }

            }
            if(!long_version)
            {
                cout<<endl;
            }

        }

        else
        {
            if(long_version)
            {

                cout<<check_permissions(st.st_mode)<<" "
                <<st.st_nlink<<" "
                <<getpwuid(st.st_uid)->pw_name<<" "
                <<getgrgid(st.st_gid)->gr_name<<" "
                <<st.st_size<<" "
                <<string(ctime(&st.st_mtime)).substr(0,16)<<" "
                <<ls_path<<endl;
            }    

            else
            {
                cout<<ls_path<<endl;
            }
            
        }
        return true;

    }

    if(command == "pinfo")
    {
        pid_t process_id;
        if( tokens.size() == 1)
        {
            process_id = getpid();// no extra argument, use current process id
        }
        else if(tokens.size() ==2)
        {
            process_id = stoi(tokens[1]);//process id argument given, use that
        }
        else
        {
            cerr<<"Invalid Arguments\n";
        }

        string stat_location = "/proc/"+to_string(process_id)+"/stat";
        string exe_location = "/proc/"+to_string(process_id)+"/exe";
        string statm_location = "/proc/"+to_string(process_id)+"/statm";

        ifstream stat_in(stat_location);
        if(!stat_in)
        {
            perror("ifstream stat error");
            return true;
        }

        int pid1;
        char com_name[256], state;//only state is needed, rest we have to stat_in to get to state
        stat_in>> pid1>>com_name>>state;
        stat_in.close();

        //detect foreground

        pid_t fg_process_group = tcgetpgrp(STDIN_FILENO);//get foreground process group
        pid_t id_process_group = getpgid(process_id);
        string state_extended(1,state);
        if(fg_process_group==id_process_group)
        {
            state_extended+="+";
        }

        cout<<"pid -- "<<process_id<<"\n";
        cout<<"Processs Status -- "<<state_extended<<"\n";

        ifstream statm_in(statm_location);
        if(!statm_in)
        {
            perror("ifstream statm error");
            return true;
        }
        long file_size;
        statm_in>>file_size;
        statm_in.close();
        cout<<"memory -- "<<file_size<<"\n";

        char exe_path[PATH_MAX];
        ssize_t l = readlink(exe_location.c_str(),exe_path,sizeof(exe_path));
        if(l == -1)
        {
            cerr<<"Cannot find exe path\n";
            return true;
        }

        exe_path[l] = '\0';
        cout<<"Executable Path -- "<<exe_path<<"\n";

        return true;
    }

    if(command == "search")
    {
        if(tokens.size()!=2)
        {
            cerr<<"Invalid arguments\n";
            return true;
        }

        string file_name = tokens[1];
        string current_dir = get_cwd();

        bool file_exists = recursive_search(current_dir, file_name);

        if(file_exists)
        {
            cout<<"True\n";
        }
        else
        {
            cout<<"False\n";
        }
        return true;
    }

    return false;
}

