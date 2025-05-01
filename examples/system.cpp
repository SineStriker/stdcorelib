#include <stdcorelib/system.h>
#include <stdcorelib/console.h>
#include <stdcorelib/path.h>

using namespace stdc;

/*!
    Prints the command line arguments.
*/
static void example_CommandLine() {
    auto args = stdc::system::command_line_arguments();
    for (int i = 0; i < args.size(); ++i) {
        u8printf("%d - %s\n", i, args[i].c_str());
    }
    u8println();
}

/*!
    Prints program information.
*/
static void example_ProgramInfo() {
    u8printf("File path: %s\n", stdc::path::to_utf8(stdc::system::application_path()).c_str());
    u8printf("Directory: %s\n", stdc::path::to_utf8(stdc::system::application_directory()).c_str());
    u8printf("File name: %s\n", stdc::path::to_utf8(stdc::system::application_filename()).c_str());
    u8printf("Name: %s\n", stdc::system::application_name().c_str());
    u8println();
}

int main(int argc, char *argv[]) {
    example_CommandLine();
    example_ProgramInfo();
    return 0;
}