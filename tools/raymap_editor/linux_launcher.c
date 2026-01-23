/*
 * linux_launcher.c - Simple wrapper to set LD_LIBRARY_PATH and launch bgdi
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>

int main(int argc, char *argv[]) {
    char exePath[PATH_MAX];
    char exeDir[PATH_MAX];
    
    // 1. Get executable path info
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath)-1);
    if (len != -1) {
        exePath[len] = '\0';
        strcpy(exeDir, exePath);
        dirname(exeDir);
    } else {
        strcpy(exeDir, ".");
        strcpy(exePath, argv[0]);
    }

    // 2. Configure Environment Variables
    char envLib[PATH_MAX * 2];
    const char *currentLd = getenv("LD_LIBRARY_PATH");
    
    // Add ./libs to LD_LIBRARY_PATH
    if (currentLd)
        snprintf(envLib, sizeof(envLib), "%s/libs:%s", exeDir, currentLd);
    else
        snprintf(envLib, sizeof(envLib), "%s/libs", exeDir);
        
    setenv("LD_LIBRARY_PATH", envLib, 1);
    setenv("BENNU_LIB_PATH", envLib, 1); // Also set BENNU_LIB_PATH
    
    // 3. Determine paths
    // Wrapper name: "GameName"
    // Engine name: "bgdi"
    // DCB name: "GameName.dcb"
    
    char baseName[PATH_MAX];
    char *name = basename(exePath);
    strcpy(baseName, name);
    
    // Strip extension if present (unlikely on Linux binaries but safe)
    char *ext = strrchr(baseName, '.');
    if (ext && strcmp(ext, ".bin") != 0) *ext = 0; // Don't strip if unknown, but usually binary has no ext
    
    char bgdiPath[PATH_MAX];
    snprintf(bgdiPath, sizeof(bgdiPath), "%s/bgdi", exeDir);
    
    char dcbPath[PATH_MAX];
    snprintf(dcbPath, sizeof(dcbPath), "%s/%s.dcb", exeDir, baseName);

    // 4. Exec
    // Construct new argv
    char **new_argv = malloc(sizeof(char*) * (argc + 2));
    new_argv[0] = bgdiPath;
    new_argv[1] = dcbPath;
    for(int i=1; i<argc; i++) {
        new_argv[i+1] = argv[i];
    }
    new_argv[argc+1] = NULL;
    
    execv(bgdiPath, new_argv);
    
    // Error handling
    perror("Error launching bgdi");
    fprintf(stderr, "Engine path: %s\n", bgdiPath);
    return 1;
}
