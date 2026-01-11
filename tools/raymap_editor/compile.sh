#!/bin/bash
# Script de compilación del editor RayMap

echo "🔨 Compilando RayMap Editor..."

cd "$(dirname "$0")"

# Buscar qmake en ubicaciones comunes de Qt Creator
QMAKE=""
for path in \
    ~/Qt/*/gcc_64/bin/qmake \
    ~/Qt/*/bin/qmake \
    /opt/Qt/*/gcc_64/bin/qmake \
    /usr/lib/qt*/bin/qmake \
    /usr/bin/qmake-qt5 \
    /usr/bin/qmake6; do
    if [ -f "$path" ]; then
        QMAKE="$path"
        break
    fi
done

if [ -z "$QMAKE" ]; then
    echo "❌ No se encontró qmake"
    echo "Por favor, abre Qt Creator y compila desde ahí:"
    echo "  1. File → Open File or Project"
    echo "  2. Selecciona: raymap_editor.pro"
    echo "  3. Presiona Ctrl+B"
    exit 1
fi

echo "✓ Usando: $QMAKE"

# Compilar
cd build/Desktop-Debug
$QMAKE ../../raymap_editor.pro
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo "✅ Compilación exitosa!"
    echo "Ejecutable: $(pwd)/raymap_editor"
else
    echo "❌ Error en la compilación"
    exit 1
fi
