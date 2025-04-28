#include "popen.h"
#include "popen_p.h"

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include "str.h"

namespace stdc {

    static std::string NSStringToString(NSString *nsString) {
        if (!string)
            return {};
        std::wstring wstr;
        wstr.resize([string length]);
        [string getCharacters:reinterpret_cast<unichar *>(wstr.data())
                        range:NSMakeRange(0, [string length])];
        return wstring_conv::to_utf8(wstr);
    }

    namespace system {

        std::map<std::string, std::string> environment() {
            __block std::map<std::string, std::string> env;
            [[[NSProcessInfo processInfo] environment]
                enumerateKeysAndObjectsUsingBlock:^(NSString *name, NSString *value,
                                                    BOOL *__unused stop) {
                    env.insert(std::make_pair(NSStringToString(name), NSStringToString(value)));
                }];
            return env;
        }

    }

}