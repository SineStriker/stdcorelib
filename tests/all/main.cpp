#include <string>
#include <vector>

#include <stdcorelib/system.h>
#include <stdcorelib/console.h>
#include <stdcorelib/path.h>
#include <stdcorelib/vla.h>

using stdc::u8printf;

static void tst_CommandLine() {
    auto args = stdc::system::command_line_arguments();
    for (int i = 0; i < args.size(); ++i) {
        u8printf("%d - %s\n", i, args[i].c_str());
    }
}

static void tst_AppInfo() {
    u8printf("File path: %s\n", stdc::path::to_utf8(stdc::system::application_path()).c_str());
    u8printf("Directory: %s\n", stdc::path::to_utf8(stdc::system::application_directory()).c_str());
    u8printf("File name: %s\n", stdc::path::to_utf8(stdc::system::application_filename()).c_str());
    u8printf("Name: %s\n", stdc::system::application_name().c_str());
}

static STDCORELIB_NOINLINE void tst_VLA() {
    auto args = stdc::system::command_line_arguments();
    VLA_NEW(std::string_view, vla_args_1, args.size());
    stdc::vlarray<std::string_view> vla_args_2(args.size());
    for (int i = 0; i < args.size(); ++i) {
        vla_args_1[i] = std::string_view(args[i].data(), args[i].size());
        vla_args_2[i] = std::string_view(args[i].data(), args[i].size());
    }
    u8printf("address of stack object: %p\n", &args);
    u8printf("address of VLA 1 object: %p\n", &vla_args_1[0]);
    u8printf("address of VLA 2 object: %p\n", &vla_args_2[0]);
    u8printf("address of heap object : %p\n", &args[0]);
}

static void tst_ConsoleColor() {
    using stdc::console::color;
    stdc::console::printf(color::red, color::white | color::intensified, " Red in white ");
    u8printf("\n");
    stdc::console::printf(color::yellow, color::blue, " Yellow in blue ");
    u8printf("\n");
}

int main(int /* argc */, char * /* argv */[]) {
    // 1 cmd line
    {
        u8printf("[Command line arguments]\n");
        tst_CommandLine();
        u8printf("\n");
    }

    // 2 app info
    {
        u8printf("[Application info]\n");
        tst_AppInfo();
        u8printf("\n");
    }

    // 3 vla
    {
        u8printf("[VLA]\n");
        tst_VLA();
        u8printf("\n");
    }

    // 4 console color
    {
        u8printf("[Console color]\n");
        tst_ConsoleColor();
        u8printf("\n");
    }

    u8printf("OK\n");
    return 0;
}
