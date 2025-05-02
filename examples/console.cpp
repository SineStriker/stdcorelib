#include <stdcorelib/console.h>

using namespace stdc;

/*!
    Example of using console print APIs.
*/
static void example_ConsolePrint() {
    using namespace console;

    // use colors
    printf(nostyle, red, lightwhite, " Red in white ");
    u8println();
    printf(nostyle, yellow, blue, " Yellow in blue ");
    u8println();

    // use styles
    printf(bold, lightwhite, nocolor, "Bold");
    u8println();
    printf(italic, lightwhite, nocolor, "Italic");
    u8println();
    printf(underline, lightwhite, nocolor, "Underline");
    u8println();
    printf(strikethrough, lightwhite, nocolor, "Strikethrough");
    u8println();

    // use color variables
    cprintln("${yellow}yellow ${green}green ${blue}blue ${cyan}cyan ${magenta}magenta "
             "${white}white ${lightred}lightred ${strikethrough}strikethrough "
             "${underline}underline ${bold}bold ${italic}italic");
    cprintf("${%s}%s ${italic}italic ${red}red ${nocolor}nocolor\n", "bold", "bold");
    cprintln("${red}red ${italic}italic ${@blue}bg-blue ${nostyle}nostyle ${@nocolor}bg-nocolor");
    cprintln("${lightgreen}lightgreen $ $$ $$$ $$$$"); // two consecutive $ are regarded as one $
    u8println();
}

int main(int argc, char *argv[]) {
    example_ConsolePrint();
    return 0;
}