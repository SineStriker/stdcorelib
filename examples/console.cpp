#include <stdcorelib/console.h>

using namespace stdc;

/*!
    Example of using console print APIs.
*/
static void example_ConsolePrint() {
    using namespace console;

    console::printf(nostyle, red, lightwhite, " Red in white ");
    u8println();
    console::printf(nostyle, yellow, blue, " Yellow in blue ");
    u8println();


    console::printf(bold, lightwhite, nocolor, "Bold");
    u8println();
    console::printf(italic, lightwhite, nocolor, "Italic");
    u8println();
    console::printf(underline, lightwhite, nocolor, "Underline");
    u8println();
    console::printf(strikethrough, lightwhite, nocolor, "Strikethrough");
    u8println();

    cprintln("${yellow}yellow ${green}green ${blue}blue ${cyan}cyan ${magenta}magenta "
             "${white}white ${lightred}lightred ${strikethrough}strikethrough "
             "${underline}underline ${bold}bold ${italic}italic $$");
    u8println();
}

int main(int argc, char *argv[]) {
    example_ConsolePrint();
    return 0;
}