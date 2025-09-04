#pragma once
#include<string>
#include<vector>


std::string get_cwd();

std::string return_modified_path(const std::string &abs_path, const std::string &home_path);

std::string trim_space(const std::string &path);

std::vector<std::string> split_command(const std::string &command);

std::string print_command_lhs(const std::string &home_path);

bool implemented_commands(const std::vector<std::string> &tokens, const std::string &home_path, const std::string &starting_dir);

void excecute_externally(std::vector<std::string> tokens);

std::string check_permissions(mode_t mode);

bool recursive_search(const std::string &path, const std::string &target);

