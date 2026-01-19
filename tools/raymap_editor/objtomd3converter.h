#ifndef OBJTOMD3CONVERTER_H
#define OBJTOMD3CONVERTER_H

#include <QString>
#include <QVector>
#include <QVector3D>
#include <QVector2D>
#include <QMap>
#include <QImage>
#include <functional>

struct Md3Triangle {
    int indices[3];
};

struct ObjMaterial {
    QString name;
    QColor color;
    QString texturePath;
    QImage textureImage; // For embedded textures (GLB)
    bool hasTexture = false;
};

class ObjToMd3Converter
{
public:
    ObjToMd3Converter();
    
    bool loadObj(const QString &filename);
    bool loadGlb(const QString &filename); // GLB Support
    bool loadMtl(const QString &filename);
    bool saveMd3(const QString &filename, float scale = 1.0f);
    
    // Texture handling
    bool generateTextureAtlas(const QString &outputPath, int size = 512);
    QString debugInfo() const;

    // Processing
    void decimate(int targetTriangles);
    bool mergeTextures(const QString &atlasPath, int atlasSize = 2048);
    int triangleCount() const { return m_triangles.size(); }
    int vertexCount() const { return m_finalVertices.size(); }
    
    // Python Integration (Deprecated)
    static bool convertViaPython(const QString &input, const QString &output, double scale, QString &outputLog);

private:
    struct VertexData {
        QVector3D position;
        QVector2D texCoord;
        int originalIndex;
        // Key for map: position indices + uv indices
    };

    QVector<QVector3D> m_rawVertices;
    QVector<QVector2D> m_rawTexCoords;
    QVector<QVector3D> m_finalVertices;
    QVector<QVector2D> m_finalTexCoords;
    QVector<Md3Triangle> m_triangles;
    QVector<int> m_faceMaterialIndices;
    
    QMap<QString, ObjMaterial> m_materials;
    QVector<QString> m_materialNames; // Indexed list matching m_faceMaterialIndices
    QString m_baseTexturePath;
    
    // Helper to unify vertices
    void processFaceVertex(int vIdx, int vtIdx, int vnIdx, int matIdx, 
                          QMap<QString, int> &vertexMap, 
                          QVector<QVector3D> &tempVerts, 
                          QVector<QVector2D> &tempUVs);

public:
    // Progress callback: (percentage 0-100, status text)
    std::function<void(int, QString)> onProgress;
    void setProgress(int p, const QString &s) { if (onProgress) onProgress(p, s); }
};

#endif // OBJTOMD3CONVERTER_H
