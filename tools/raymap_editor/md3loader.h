#ifndef MD3LOADER_H
#define MD3LOADER_H

#include <QString>
#include <QVector>
#include <QVector3D>
#include <QVector2D>
#include <QFile>
#include <QDataStream>
#include <QDebug>

struct MD3Header {
    char ident[4];
    int version;
    char name[64];
    int flags;
    int numFrames;
    int numTags;
    int numSurfaces;
    int numSkins;
    int ofsFrames;
    int ofsTags;
    int ofsSurfaces;
    int ofsEnd;
};

struct MD3ServerHeader {
    char ident[4];
    char name[64];
    int flags;
    int numFrames;
    int numSurfaces;
    int numSkins;
    int ofsFrames;
    int ofsSurfaces;
    int ofsEnd;
};

struct MD3Surface {
    char ident[4];
    char name[64];
    int flags;
    int numFrames;
    int numShaders;
    int numVerts;
    int numTriangles;
    int ofsTriangles;
    int ofsShaders;
    int ofsSt;
    int ofsXyzNormals;
    int ofsEnd;
};

struct MD3Point {
    short x, y, z;
    unsigned char normal[2];
};

struct MD3Triangle {
    int indexes[3];
};

struct MD3TexCoord {
    float u, v;
};

struct MD3Shader {
    char name[64];
    int shaderIndex;
};

struct RenderSurface {
    QString name;
    QVector<QVector3D> vertices;
    QVector<QVector2D> texCoords;
    QVector<unsigned int> indices;
    QString shaderName;
};

class MD3Loader {
public:
    MD3Loader();
    bool load(const QString &filename);
    
    // Returns vertices and indices for rendering
    // Vertices are stored as flat arrays of floats (3 for position, 2 for uv, 3 for normal)
    // suitable for OpenGL rendering
    const QVector<RenderSurface>& getSurfaces() const { return m_surfaces; }
    
private:
    QVector<RenderSurface> m_surfaces;
};

#endif // MD3LOADER_H
