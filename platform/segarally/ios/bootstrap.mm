// SDL3 scene startup follows the working Daytona/Sega Rally iOS bootstrap.
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>
#include <unistd.h>

extern int segarally_host_main(int argc, char **argv);

static int app_main(int argc, char **argv)
{
    @autoreleasepool {
        NSFileManager *files = NSFileManager.defaultManager;
        NSURL *support = [files URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask
                              appropriateForURL:nil create:YES error:nil];
        NSURL *work = [support URLByAppendingPathComponent:@"SegaRallyNative" isDirectory:YES];
        NSError *error = nil;
        if (!work || ![files createDirectoryAtURL:work withIntermediateDirectories:YES attributes:nil error:&error]) {
            fprintf(stderr, "Could not create writable storage: %s\n", error.description.UTF8String);
            return 1;
        }
        [work setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];
        NSString *assets = [NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"assets"];
        setenv("SEGARALLY_DATA_DIR", assets.fileSystemRepresentation, 1);
        setenv("SEGARALLY_SAVE_DIR", work.path.fileSystemRepresentation, 1);
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
        UIApplication.sharedApplication.idleTimerDisabled = YES;
        fprintf(stderr, "Native scene callback: assets=%s saves=%s\n", assets.fileSystemRepresentation,
                work.path.fileSystemRepresentation);
        // iOS launches have no terminal arguments. Request only capabilities actually
        // compiled into this target; Mac and explicit diagnostic defaults stay separate.
        std::vector<char *> arguments;
        for (int i = 0; i < argc; ++i) arguments.push_back(argv[i]);
        arguments.push_back(const_cast<char *>("--player"));
#ifdef M2_SRALLY_SOUND
        arguments.push_back(const_cast<char *>("--sound"));
#endif
#ifdef M2_INSTRUCTION_BOUNDARY_HOOK
        arguments.push_back(const_cast<char *>("--instruction-boundary-hook"));
#endif
        const int argument_count = static_cast<int>(arguments.size());
        arguments.push_back(nullptr);
        try { return segarally_host_main(argument_count, arguments.data()); }
        catch (const std::exception &failure) {
            fprintf(stderr, "Native host startup failed: %s\n", failure.what());
            return 1;
        }
    }
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        NSString *documents = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
        [NSFileManager.defaultManager createDirectoryAtPath:documents withIntermediateDirectories:YES
                                                attributes:nil error:nil];
        NSString *log = [documents stringByAppendingPathComponent:@"segarally-native.log"];
        if (freopen(log.fileSystemRepresentation, "w", stdout)) {
            dup2(fileno(stdout), STDERR_FILENO);
            setvbuf(stdout, nullptr, _IOLBF, 0);
            setvbuf(stderr, nullptr, _IONBF, 0);
        }
        fprintf(stderr, "Sega Rally native entry, SDL3 3.4.16, argc=%d\n", argc);
        const int result = SDL_RunApp(argc, argv, app_main, nullptr);
        fprintf(stderr, "SDL_RunApp returned %d\n", result);
        return result;
    }
}
