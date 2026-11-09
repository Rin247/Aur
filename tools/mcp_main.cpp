#include "aur/mcp.hpp"
#include "aur/project.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: aur_mcp <project-directory>\n";
        return 2;
    }

    try {
        aur::Project project = aur::Project::load(argv[1]);
        aur::run_mcp_server(project, std::cin, std::cout);
    } catch (const std::exception& error) {
        std::cerr << "aur_mcp: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
