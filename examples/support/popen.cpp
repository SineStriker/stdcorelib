#include <fstream>

#include <stdcorelib/console.h>
#include <stdcorelib/support/popen.h>

/*!
    Read all stdout and stderr output from a process and print it to the console.
*/
static int example_ReadProcessOutput() {
    stdc::Popen proc;
    proc.args({"git", "--version"})
        .text(true)
        .stdout_(stdc::Popen::PIPE)
        .stderr_(stdc::Popen::STDOUT);
    std::string err_msg;
    if (!proc.start(&err_msg)) {
        stdc::console::critical("Failed to start process: %1", err_msg);
        return -1;
    }

    FILE *out = proc.stdout_();
    std::filebuf buf(out);
    std::istream is(&buf);
    std::string line;
    while (std::getline(is, line)) {
        stdc::u8println(line);
    }
    proc.wait();

    int code = proc.returncode().value_or(-1);
    if (code == 0) {
        stdc::console::success("Process exited with code %1", code);
    } else {
        stdc::console::critical("Process exited with code %1", code);
    }
    return 0;
}

/*!
    Run a process and redirect its stdout and stderr to the console.
*/
static int example_System() {
    stdc::Popen proc;
    proc.args({"git", "--version"})
        .stdin_(stdc::Popen::DEVNULL)
        .stdout_(stdout)
        .stderr_(stdout);

    std::string err_msg;
    if (!proc.start(&err_msg)) {
        stdc::console::critical("Failed to start process: %1", err_msg);
        return -1;
    }
    proc.wait();

    int code = proc.returncode().value_or(-1);
    if (code == 0) {
        stdc::console::success("Process exited with code %1", code);
    } else {
        stdc::console::critical("Process exited with code %1", code);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    example_ReadProcessOutput();
    example_System();
    return 0;
}