/*
 * linux_stub.c - Self-extracting loader for BennuGD2 games on Linux
 * 
 * Logic:
 * 1. Read payload from the end of the executable.
 * 2. Extract files to a temporary directory (/tmp/bgd_XXXXXX).
 * 3. Set executable permissions for bgdi and scripts.
 * 4. Launch bgdi with the game .dcb.
 * 5. Cleanup (optional/difficult due to running process).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

// Must match publisher.cpp
#define MAGIC_MARKER "BENNUGD2_PAYLOAD_V3"

typedef struct {
    char path[256];
    u_int32_t size;
} FileEntry;

typedef struct {
    char magic[32];
    u_int32_t numFiles;
} PayloadFooter;

// Recursively create parent directories
void create_parent_dirs(const char *baseDir, const char *relPath) {
    char fullPath[PATH_MAX];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", baseDir, relPath);
    
    char *p = strrchr(fullPath, '/');
    if (p) {
        *p = 0; // Truncate to parent dir
        char cmd[PATH_MAX + 32];
        // Linux mkdir -p is very convenient
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", fullPath);
        system(cmd);
    }
}

int extract_file(FILE *fp, const char *baseDir, const char *relPath, u_int32_t size) {
    create_parent_dirs(baseDir, relPath);
    
    char outPath[PATH_MAX];
    snprintf(outPath, sizeof(outPath), "%s/%s", baseDir, relPath);
    
    FILE *out = fopen(outPath, "wb");
    if (!out) {
        fprintf(stderr, "Failed to create file: %s\n", outPath);
        return 0;
    }
    
    char buffer[4096];
    u_int32_t bytesLeft = size;
    while (bytesLeft > 0) {
        u_int32_t toRead = (bytesLeft > sizeof(buffer)) ? sizeof(buffer) : bytesLeft;
        if (fread(buffer, 1, toRead, fp) != toRead) {
            fclose(out);
            return 0;
        }
        fwrite(buffer, 1, toRead, out);
        bytesLeft -= toRead;
    }
    
    fclose(out);
    
    // Set executable permissions for binaries or scripts (simple heuristic)
    // In Linux, bgdi needs +x. Libraries (.so) usually don't strictly need +x to be dlopen'ed but it's fine.
    // The launcher script needs +x.
    if (strstr(relPath, "bgdi") || strstr(relPath, ".sh") || strstr(relPath, "mod_") || strstr(relPath, ".so")) {
        chmod(outPath, 0755);
    }
    
    return 1;
}

int main(int argc, char *argv[]) {
    char exePath[PATH_MAX];
    
    // Get path to self
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath)-1);
    if (len == -1) {
        strcpy(exePath, argv[0]); // Fallback
    } else {
        exePath[len] = '\0';
    }
    
    FILE *fp = fopen(exePath, "rb");
    if (!fp) {
        perror("Error opening self");
        return 1;
    }
    
    // 1. Read Footer
    if (fseek(fp, -((long)sizeof(PayloadFooter)), SEEK_END) != 0) {
        fprintf(stderr, "Error seeking footer\n");
        return 1;
    }
    
    PayloadFooter footer;
    if (fread(&footer, sizeof(PayloadFooter), 1, fp) != 1) {
        fprintf(stderr, "Error reading footer\n");
        return 1;
    }
    
    if (strcmp(footer.magic, MAGIC_MARKER) != 0) {
        fprintf(stderr, "Error: Invalid payload magic. This is a stub without game data.\n");
        fprintf(stderr, "Magic found: %s\n", footer.magic);
        fclose(fp);
        return 1;
    }
    
    // 2. Read TOC
    u_int32_t tocSize = footer.numFiles * sizeof(FileEntry);
    FileEntry *entries = (FileEntry*)malloc(tocSize);
    
    fseek(fp, -((long)sizeof(PayloadFooter)) - tocSize, SEEK_END);
    fread(entries, sizeof(FileEntry), footer.numFiles, fp);
    
    // Calculate Data Start
    long totalFilesSize = 0;
    for(u_int32_t i=0; i<footer.numFiles; i++) {
        totalFilesSize += entries[i].size;
    }
    
    long dataStartPos = ftell(fp) - tocSize - totalFilesSize;
    
    // 3. Prepare Temp Dir
    char template[] = "/tmp/bgd_XXXXXX";
    char *workDir = mkdtemp(template);
    if (!workDir) {
        perror("Failed to create temp dir");
        return 1;
    }
    
    // 4. Extract Files
    fseek(fp, dataStartPos, SEEK_SET);
    
    char bgdiPath[PATH_MAX] = {0};
    char dcbPath[PATH_MAX] = {0};
    
    printf("Extracting %d files to %s...\n", footer.numFiles, workDir);
    
    for(u_int32_t i=0; i<footer.numFiles; i++) {
        extract_file(fp, workDir, entries[i].path, entries[i].size);
        
        if (strstr(entries[i].path, "bgdi") && !strstr(entries[i].path, ".s")) { // Avoid source files if mapped
            snprintf(bgdiPath, sizeof(bgdiPath), "%s/%s", workDir, entries[i].path);
        }
        if (strstr(entries[i].path, ".dcb")) {
            snprintf(dcbPath, sizeof(dcbPath), "%s/%s", workDir, entries[i].path);
        }
    }
    
    free(entries);
    fclose(fp);
    
    // 5. Launch Game
    if (bgdiPath[0] && dcbPath[0]) {
        printf("Launching: %s %s\n", bgdiPath, dcbPath);
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            // Set LD_LIBRARY_PATH so bgdi finds the extracted .so files
            char libPath[PATH_MAX * 2];
            snprintf(libPath, sizeof(libPath), "%s:%s/lib", workDir, workDir);
            
            // Append current LD_LIBRARY_PATH if needed
            char fullLdPath[PATH_MAX * 4];
            char *currentLd = getenv("LD_LIBRARY_PATH");
            if (currentLd)
                snprintf(fullLdPath, sizeof(fullLdPath), "%s:%s", libPath, currentLd);
            else
                strncpy(fullLdPath, libPath, sizeof(fullLdPath));

            setenv("LD_LIBRARY_PATH", fullLdPath, 1); 
            setenv("BENNU_LIB_PATH", libPath, 1); // CRITICAL: Tell Bennu where modules are
            
            // Set working directory to where we extracted everything
            if (chdir(workDir) != 0) {
                perror("Failed to change directory to temp folder");
            }
            
            // Execute bgdi
            // argv: bgdi, dcb, NULL
            char *args[] = {bgdiPath, dcbPath, NULL};
            execv(bgdiPath, args);
            
            // If we get here, exec failed
            perror("Exec failed");
            exit(1);
        } else if (pid > 0) {
            // Parent waits
            int status;
            waitpid(pid, &status, 0);
        } else {
            perror("Fork failed");
        }
    } else {
        fprintf(stderr, "Error: bgdi or .dcb not found in payload.\n");
    }
    
    // 6. Cleanup
    // rm -rf workDir
    char cmd[PATH_MAX + 32];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", workDir);
    system(cmd);
    
    return 0;
}
