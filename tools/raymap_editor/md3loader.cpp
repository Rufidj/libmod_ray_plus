#include "md3loader.h"
#include <QtEndian>

MD3Loader::MD3Loader()
{
}

bool MD3Loader::load(const QString &filename)
{
    m_surfaces.clear();
    
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open MD3 file:" << filename;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    if (data.size() < (int)sizeof(MD3Header)) {
        qWarning() << "Invalid MD3 file size";
        return false;
    }
    
    const char *ptr = data.constData();
    const MD3Header *header = reinterpret_cast<const MD3Header*>(ptr);
    
    if (strncmp(header->ident, "IDP3", 4) != 0) {
        qWarning() << "Invalid MD3 header ident";
        return false;
    }
    
    if (header->version != 15) {
        qWarning() << "Invalid MD3 version:" << header->version;
        return false;
    }
    
    // Iterate over surfaces
    int surfaceOffset = header->ofsSurfaces;
    
    for (int i = 0; i < header->numSurfaces; i++) {
        const MD3Surface *surf = reinterpret_cast<const MD3Surface*>(ptr + surfaceOffset);
        
        RenderSurface renderSurf;
        renderSurf.name = QString::fromLatin1(surf->name);
        
        // Read triangles (indices)
        const MD3Triangle *tris = reinterpret_cast<const MD3Triangle*>(ptr + surfaceOffset + surf->ofsTriangles);
        for (int j = 0; j < surf->numTriangles; j++) {
            renderSurf.indices.append(tris[j].indexes[0]);
            renderSurf.indices.append(tris[j].indexes[2]); // Swap winding order for OpenGL? Usually 0,1,2 or 0,2,1
            renderSurf.indices.append(tris[j].indexes[1]);
        }
        
        // Read texture coordinates
        const MD3TexCoord *st = reinterpret_cast<const MD3TexCoord*>(ptr + surfaceOffset + surf->ofsSt);
        QVector<QVector2D> uvs;
        for (int j = 0; j < surf->numVerts; j++) {
            uvs.append(QVector2D(st[j].u, st[j].v));
        }
        
        // Read shaders (textures) to find texture name
        if (surf->numShaders > 0) {
            const MD3Shader *shader = reinterpret_cast<const MD3Shader*>(ptr + surfaceOffset + surf->ofsShaders);
            renderSurf.shaderName = QString::fromLatin1(shader->name);
        }
        
        // Read vertices (XYZ) - Only read frame 0
        const MD3Point *points = reinterpret_cast<const MD3Point*>(ptr + surfaceOffset + surf->ofsXyzNormals);
        
        // MD3 coordinates are scaled by 1/64
        float scale = 1.0f / 64.0f;
        
        for (int j = 0; j < surf->numVerts; j++) {
            // MD3 uses Z-up, Y-forward usually, but we need to adapt to our coordinate system
            // RayMap seems to use X/Z specific systems.
            // Standard loading:
            float x = points[j].x * scale;
            float y = points[j].y * scale;
            float z = points[j].z * scale;
            
            // In RayCasting engine, usually:
            // World Z is Height (Up)
            // But let's check basic orientation first.
            renderSurf.vertices.append(QVector3D(x, y, z));
            renderSurf.texCoords.append(uvs[j]);
        }
        
        m_surfaces.append(renderSurf);
        
        // Move to next surface
        surfaceOffset += surf->ofsEnd;
    }
    
    return true;
}
