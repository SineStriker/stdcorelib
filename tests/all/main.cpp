#include <string>
#include <vector>

#include <stdcorelib/system.h>
#include <stdcorelib/console.h>
#include <stdcorelib/path.h>
#include <stdcorelib/vla.h>

using namespace stdc;

int main(int /* argc */, char * /* argv */[]) {

    // 1 cmd line
    {
        u8printf("[Command line arguments]\n");
        auto args = System::commandLineArguments();
        for (int i = 0; i < args.size(); ++i) {
            u8printf("%d - %s\n", i, args[i].c_str());
        }
        u8printf("\n");
    }

    // 2 app info
    {
        u8printf("[Application info]\n");
        u8printf("File path: %s\n", pathToUtf8(System::applicationPath()).c_str());
        u8printf("Directory: %s\n", pathToUtf8(System::applicationDirectory()).c_str());
        u8printf("File name: %s\n", pathToUtf8(System::applicationFileName()).c_str());
        u8printf("Name: %s\n", System::applicationName().c_str());
        u8printf("\n");
    }

    // 3 vla
    {
        u8printf("[VLA]\n");
        auto args = System::commandLineArguments();
        VLA_NEW(std::string_view, stack_args, args.size());
        for (int i = 0; i < args.size(); ++i) {
            stack_args[i] = std::string_view(args[i].data(), args[i].size());
        }

        u8printf("address of stack object: %p\n", &args);
        u8printf("address of VLA object  : %p\n", &stack_args[0]);
        u8printf("address of heap object : %p\n", &args[0]);
        u8printf("\n");
    }

    // 4 console color
    {
        u8printf("[Console color]\n");
        Console::printf(Console::Red, Console::White | Console::Intensified, " Red in white ");
        u8printf("\n");
        Console::printf(Console::Yellow, Console::Blue, " Yellow in blue ");
        u8printf("\n");
        u8printf("\n");
    }

    u8printf("OK\n");
    return 0;
}
