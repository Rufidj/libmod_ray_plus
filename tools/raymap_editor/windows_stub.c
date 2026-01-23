#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Marcador mágico V3
#define MAGIC_MARKER "BENNUGD2_PAYLOAD_V3"

typedef struct {
    char path[256]; // Ruta relativa (ej: "bgdi.exe", "assets/logo.png")
    DWORD size;     // Tamaño del archivo
} FileEntry;

typedef struct {
    char magic[32]; // MAGIC_MARKER
    DWORD numFiles; // Total de archivos empaquetados
} PayloadFooter;

// Crear directorios recursivamente para una ruta (ej: "a/b/c.txt" -> crea "a" y "a/b")
void create_parent_dirs(const char *baseDir, const char *relPath) {
    char fullPath[MAX_PATH];
    sprintf(fullPath, "%s\\%s", baseDir, relPath);
    
    // Cambiar / por \ para Windows
    for(int i=0; fullPath[i]; i++) {
        if(fullPath[i] == '/') fullPath[i] = '\\';
    }
    
    // Buscar el último separador para quitar el nombre del archivo
    char *p = strrchr(fullPath, '\\');
    if (p) {
        *p = 0; // Truncar para tener solo el directorio
        
        // Comando sucio pero efectivo para crear toda la ruta: "mkdir a\b\c"
        // Windows API CreateDirectory solo crea un nivel, así que usamos system o un bucle.
        // Un bucle es más limpio:
        char cmd[MAX_PATH + 32];
        sprintf(cmd, "mkdir \"%s\" 2>NUL", fullPath);
        system(cmd);
    }
}

// Helper para extraer un archivo
static int extractFile(FILE *fp, const char *baseDir, const char *relPath, DWORD size) {
    // Asegurar directorios
    create_parent_dirs(baseDir, relPath);
    
    char outPath[MAX_PATH];
    sprintf(outPath, "%s\\%s", baseDir, relPath);
     // Normalizar slashes
    for(int i=0; outPath[i]; i++) if(outPath[i] == '/') outPath[i] = '\\';

    FILE *out = fopen(outPath, "wb");
    if (!out) return 0;
    
    // Copiar por chunks para no saturar memoria con archivos grandes
    char buffer[4096];
    DWORD bytesLeft = size;
    while (bytesLeft > 0) {
        DWORD toRead = (bytesLeft > sizeof(buffer)) ? sizeof(buffer) : bytesLeft;
        fread(buffer, 1, toRead, fp);
        fwrite(buffer, 1, toRead, out);
        bytesLeft -= toRead;
    }
    
    fclose(out);
    return 1;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    char tempPath[MAX_PATH];
    char exePath[MAX_PATH];
    char workDir[MAX_PATH];
    
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    
    FILE *fp = fopen(exePath, "rb");
    if (!fp) {
        MessageBoxA(NULL, "Error reading executable", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // 1. Leer Footer Global
    fseek(fp, -((long)sizeof(PayloadFooter)), SEEK_END);
    PayloadFooter footer;
    fread(&footer, sizeof(PayloadFooter), 1, fp);
    
    if (strcmp(footer.magic, MAGIC_MARKER) != 0) {
        MessageBoxA(NULL, "Invalid or missing payload (V3 required).", "BennuGD2 Loader", MB_OK | MB_ICONERROR);
        fclose(fp);
        return 1;
    }
    
    // 2. Leer Tabla de Contenidos (TOC)
    // La TOC está justo antes del footer. Tamaño = numFiles * sizeof(FileEntry)
    DWORD tocSize = footer.numFiles * sizeof(FileEntry);
    FileEntry *entries = (FileEntry*)malloc(tocSize);
    
    fseek(fp, -((long)sizeof(PayloadFooter)) - tocSize, SEEK_END);
    fread(entries, sizeof(FileEntry), footer.numFiles, fp);
    
    // Calcular dónde empiezan los datos (files data start)
    // El payload total es: [FILES_DATA...] [TOC] [FOOTER]
    // Necesitamos saber el tamaño total de los archivos para ir al inicio.
    long totalFilesSize = 0;
    for(DWORD i=0; i<footer.numFiles; i++) totalFilesSize += entries[i].size;
    
    long dataStartPos = ftell(fp) - tocSize - totalFilesSize;
    
    // 3. Preparar directorio temporal
    GetTempPathA(MAX_PATH, tempPath);
    sprintf(workDir, "%sBGD_%lu", tempPath, GetTickCount());
    CreateDirectoryA(workDir, NULL);
    
    // 4. Extraer TODOS los archivos
    fseek(fp, dataStartPos, SEEK_SET);
    
    char bgdiPath[MAX_PATH] = {0};
    char dcbPath[MAX_PATH] = {0};
    
    for(DWORD i=0; i<footer.numFiles; i++) {
        extractFile(fp, workDir, entries[i].path, entries[i].size);
        
        // Detectar archivos clave
        if (strstr(entries[i].path, "bgdi.exe")) {
            sprintf(bgdiPath, "%s\\%s", workDir, entries[i].path);
        }
        if (strstr(entries[i].path, ".dcb")) {
            sprintf(dcbPath, "%s\\%s", workDir, entries[i].path);
        }
    }
    
    free(entries);
    fclose(fp);
    
    // 5. Ejecutar
    if (bgdiPath[0] && dcbPath[0]) {
        char cmdLine[MAX_PATH * 2];
        sprintf(cmdLine, "\"%s\" \"%s\"", bgdiPath, dcbPath);
        
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOW;
        
        if (CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, workDir, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    } else {
        MessageBoxA(NULL, "Missing bgdi.exe or .dcb in payload", "Launch Error", MB_OK);
    }
    
    // 6. Limpieza rápida (borrar directorio de trabajo y contenido)
    // Nota: Una limpieza recursiva completa en C puro y en Windows API es verbosa.
    // Para simplificar, lanzamos un comando de sistema en segundo plano que intente borrarlo después de un delay, o lo dejamos al sistema.
    // Por ahora, intento básico:
    char delCmd[MAX_PATH + 32];
    sprintf(delCmd, "rmdir /S /Q \"%s\"", workDir);
    // system(delCmd); // Cuidado: esto bloquearía o mostraría consola. Mejor dejar temp files si no es crítico.
    
    return 0;
}
