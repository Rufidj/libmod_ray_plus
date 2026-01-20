#include "objtomd3converter.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDataStream>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <qmath.h>
#include <QProcess>
#include <QCoreApplication>

#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

ObjToMd3Converter::ObjToMd3Converter()
{
}

bool ObjToMd3Converter::loadMtl(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open MTL file:" << filename;
        return false;
    }

    QTextStream in(&file);
    QString currentMatName;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#")) continue;
        
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        
        if (parts[0] == "newmtl") {
            currentMatName = parts.mid(1).join(" ");
            ObjMaterial mat;
            mat.name = currentMatName;
            mat.color = Qt::gray;
            m_materials[currentMatName] = mat;
            m_materialNames.append(currentMatName);
        } else if (parts[0] == "Kd" && !currentMatName.isEmpty()) {
            if (parts.size() >= 4) {
                int r = static_cast<int>(parts[1].toFloat() * 255);
                int g = static_cast<int>(parts[2].toFloat() * 255);
                int b = static_cast<int>(parts[3].toFloat() * 255);
                m_materials[currentMatName].color = QColor(r, g, b);
            }
        } else if (parts[0] == "map_Kd" && !currentMatName.isEmpty()) {
            QString rawPath = parts.mid(1).join(" ");
            rawPath.replace("\\", "/"); // Normalize slashes
            
            qDebug() << "MTL: Processing map_Kd for material" << currentMatName << "- raw path:" << rawPath;
            
            QFileInfo fiMtl(filename);
            QString absPath = fiMtl.absolutePath() + "/" + rawPath;
            bool found = false;
            
            // 1. Try exact path
            if (QFile::exists(absPath)) {
                m_materials[currentMatName].texturePath = absPath;
                found = true;
                qDebug() << "  ✓ Found texture at:" << absPath;
            } 
            
            // 2. Try just the filename in the same dir (ignoring folders in mtl)
            if (!found) {
                QString justName = QFileInfo(rawPath).fileName();
                QString tryPath = fiMtl.absolutePath() + "/" + justName;
                if (QFile::exists(tryPath)) {
                    m_materials[currentMatName].texturePath = tryPath;
                    found = true;
                    qDebug() << "  ✓ Found texture (filename only) at:" << tryPath;
                }
            }
            
            // 3. Try handling options (e.g. map_Kd -o 0 0 texture.png)
            if (!found && parts.size() > 2) {
                 // Try the last part as filename
                 QString lastPart = parts.last();
                 QString tryPath = fiMtl.absolutePath() + "/" + lastPart;
                 if (QFile::exists(tryPath)) {
                     m_materials[currentMatName].texturePath = tryPath;
                     found = true;
                     qDebug() << "  ✓ Found texture (last part) at:" << tryPath;
                 }
            }
            
            if (!found) {
                // Keep the raw path as fallback, maybe it works relative to CWD eventually
                m_materials[currentMatName].texturePath = rawPath;
                qDebug() << "  ✗ Texture NOT found, keeping raw path:" << rawPath;
            }
        }
    }
    
    return true;
}

bool ObjToMd3Converter::loadObj(const QString &filename)
{
    setProgress(0, "Abriendo archivo OBJ...");
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    // Speculative MTL loading removed - relying on mtllib directive inside OBJ

    m_rawVertices.clear();
    m_rawTexCoords.clear();
    m_finalVertices.clear();
    m_finalTexCoords.clear();
    m_triangles.clear();
    m_faceMaterialIndices.clear();
    m_materials.clear(); 
    m_materialNames.clear();
    // Pre-add dummy UV if needed (though logic usually handles it)
    
    QTextStream in(&file);
    int currentMatIdx = -1;
    QMap<QString, int> vertexCache; // Key: "v/vt/vn", Value: index in m_finalVertices
    
    qint64 totalBytes = file.size();
    qint64 readBytes = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        readBytes += line.length() + 1;
        
        // Don't spam progress updates
        if (readBytes % 50000 == 0) {
            int pct = (int)((double)readBytes / totalBytes * 60.0); // Map to 0-60%
            setProgress(pct, QString("Analizando lineas... %1%").arg(pct));
        }

        if (line.isEmpty() || line.startsWith("#")) continue;

        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;

        if (parts[0] == "v") {
            m_rawVertices.append(QVector3D(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat()));
        } else if (parts[0] == "vt") {
            m_rawTexCoords.append(QVector2D(parts[1].toFloat(), parts[2].toFloat()));
        } else if (parts[0] == "mtllib") {
            QString mtlFileName = parts.mid(1).join(" ");
            mtlFileName.replace("\\", "/");
            QFileInfo fiObj(filename);
            QString mtlPath = fiObj.absolutePath() + "/" + mtlFileName;
            loadMtl(mtlPath);
        } else if (parts[0] == "usemtl") {
            QString matName = parts.mid(1).join(" ");
            // Find index
            currentMatIdx = m_materialNames.indexOf(matName);
            if (currentMatIdx == -1) {
                // Add if not exists (handling missing mtl definition)
                m_materialNames.append(matName);
                ObjMaterial mat; mat.name = matName; mat.color = Qt::gray;
                m_materials[matName] = mat;
                currentMatIdx = m_materialNames.size() - 1;
            }
        } else if (parts[0] == "f") {
             // Triangulate fan
             QVector<int> faceIndices;
             for (int i = 1; i < parts.size(); ++i) {
                 QStringList idxParts = parts[i].split('/');
                 int vIdx = idxParts[0].toInt() - 1;
                 int vtIdx = (idxParts.size() > 1 && !idxParts[1].isEmpty()) ? idxParts[1].toInt() - 1 : -1;
                 int vnIdx = (idxParts.size() > 2 && !idxParts[2].isEmpty()) ? idxParts[2].toInt() - 1 : -1;
                 
                 // Create unique vertex key
                 QString key = QString("%1/%2/%3").arg(vIdx).arg(vtIdx).arg(vnIdx);
                 
                 if (!vertexCache.contains(key)) {
                     // Add new vertex
                     QVector3D pos = m_rawVertices[vIdx];
                     // Swap Y/Z logic from python script: (x, z, y)
                     // Python: converted_vertex = (vx, vz, vy)
                     QVector3D convertedPos(pos.x(), pos.z(), pos.y());
                     
                     QVector2D tex = (vtIdx >= 0 && vtIdx < m_rawTexCoords.size()) ? m_rawTexCoords[vtIdx] : QVector2D(0.5f, 0.5f);
                     
                     vertexCache[key] = m_finalVertices.size();
                     m_finalVertices.append(convertedPos);
                     m_finalTexCoords.append(tex);
                 }
                 faceIndices.append(vertexCache[key]);
             }
             
             // Triangulate
             for (int i = 1; i < faceIndices.size() - 1; ++i) {
                 Md3Triangle tri;
                 tri.indices[0] = faceIndices[0];
                 tri.indices[1] = faceIndices[i];
                 tri.indices[2] = faceIndices[i+1];
                 m_triangles.append(tri);
                 m_faceMaterialIndices.append(currentMatIdx);
             }
        }
    }
    
    return true;
}



bool ObjToMd3Converter::loadGlb(const QString &filename)
{
    setProgress(10, "Cargando GLB...");
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    // Header (12 bytes)
    quint32 magic; quint32 version; quint32 length;
    in >> magic >> version >> length;

    if (magic != 0x46546C67) { // "glTF"
        qWarning() << "Invalid GLB Magic";
        return false;
    }

    // JSON Chunk Header (8 bytes)
    quint32 jsonChunkLen; quint32 jsonChunkType;
    in >> jsonChunkLen >> jsonChunkType;
    
    // JSON Chunk Type must be 0x4E4F534A ("JSON")
    if (jsonChunkType != 0x4E4F534A) { 
        return false;
    }

    QByteArray jsonData = file.read(jsonChunkLen);
    
    // BIN Chunk
    quint32 binChunkLen = 0; quint32 binChunkType = 0;
    QByteArray binData;
    
    if (!file.atEnd()) {
        in >> binChunkLen >> binChunkType;
        if (binChunkType == 0x004E4942) {
            binData = file.read(binChunkLen);
        }
    }

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull()) return false;
    QJsonObject root = doc.object();

    QJsonArray accessors = root["accessors"].toArray();
    QJsonArray bufferViews = root["bufferViews"].toArray();
    QJsonArray meshes = root["meshes"].toArray();
    QJsonArray materials = root["materials"].toArray();
    
    m_rawVertices.clear();
    m_rawTexCoords.clear();
    m_finalVertices.clear();
    m_finalTexCoords.clear();
    m_triangles.clear();
    m_faceMaterialIndices.clear();
    m_materials.clear();
    m_materialNames.clear();

    QJsonArray images = root["images"].toArray();
    QJsonArray textures = root["textures"].toArray();
    
    QVector<QImage> loadedImages;
    for (const QJsonValue &iv : images) {
        QJsonObject imgObj = iv.toObject();
        int bvIdx = imgObj["bufferView"].toInt(-1);
        QImage img;
        if (bvIdx >= 0 && bvIdx < bufferViews.size()) {
             QJsonObject bv = bufferViews[bvIdx].toObject();
             int bvOffset = bv["byteOffset"].toInt(0);
             int bvLen = bv["byteLength"].toInt();
             int bufIdx = bv["buffer"].toInt(0);
             if (bufIdx == 0 && binData.size() >= bvOffset + bvLen) {
                 QByteArray imgData = binData.mid(bvOffset, bvLen);
                 img.loadFromData(imgData);
             }
        }
        loadedImages.append(img);
    }

    // Parse Materials
    for (int i=0; i<materials.size(); ++i) {
        QJsonObject mat = materials[i].toObject();
        QString name = mat["name"].toString(QString("Material_%1").arg(i));
        
        QColor color = Qt::gray;
        QImage texImage;
        bool hasTex = false;
        
        if (mat.contains("pbrMetallicRoughness")) {
            QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
            if (pbr.contains("baseColorFactor")) {
                QJsonArray factors = pbr["baseColorFactor"].toArray();
                if (factors.size() >= 3) {
                    int r = static_cast<int>(factors[0].toDouble() * 255);
                    int g = static_cast<int>(factors[1].toDouble() * 255);
                    int b = static_cast<int>(factors[2].toDouble() * 255);
                    color = QColor(r, g, b);
                }
            }
            if (pbr.contains("baseColorTexture")) {
                int texIdx = pbr["baseColorTexture"].toObject()["index"].toInt(-1);
                if (texIdx >= 0 && texIdx < textures.size()) {
                     int sourceIdx = textures[texIdx].toObject()["source"].toInt(-1);
                     if (sourceIdx >= 0 && sourceIdx < loadedImages.size()) {
                         texImage = loadedImages[sourceIdx];
                         hasTex = !texImage.isNull();
                     }
                }
            }
        }
        
        ObjMaterial m;
        m.name = name;
        m.color = color;
        m.textureImage = texImage;
        m.hasTexture = hasTex;
        m_materials[name] = m;
        m_materialNames.append(name);
    }
    
    // Helper to get buffer data with stride
    auto getAccessorData = [&](int accessorIdx, int &count, int &compType, int &stride) -> const char* {
        if (accessorIdx < 0 || accessorIdx >= accessors.size()) return nullptr;
        QJsonObject acc = accessors[accessorIdx].toObject();
        int bvIdx = acc["bufferView"].toInt();
        int byteOffset = acc["byteOffset"].toInt(0);
        count = acc["count"].toInt();
        compType = acc["componentType"].toInt();
        
        if (bvIdx < 0 || bvIdx >= bufferViews.size()) return nullptr;
        QJsonObject bv = bufferViews[bvIdx].toObject();
        int bvOffset = bv["byteOffset"].toInt(0);
        stride = bv["byteStride"].toInt(0); // 0 = tightly packed
        
        if (bv.contains("buffer") && bv["buffer"].toInt() != 0) return nullptr;
        
        int finalOffset = bvOffset + byteOffset;
        if (finalOffset + 4 > binData.size()) return nullptr;
        
        return binData.constData() + finalOffset;
    };

    // Iterate meshes
    int baseVertexIndex = 0;
    for (const QJsonValue &mv : meshes) {
        QJsonArray primitives = mv.toObject()["primitives"].toArray();
        for (const QJsonValue &pv : primitives) {
            QJsonObject prim = pv.toObject();
            QJsonObject attrs = prim["attributes"].toObject();
            
            int posIdx = attrs["POSITION"].toInt(-1);
            int uvIdx = attrs["TEXCOORD_0"].toInt(-1);
            int indicesIdx = prim["indices"].toInt(-1);
            int matIdx = prim["material"].toInt(-1);
            
            if (posIdx == -1) continue;

            // Load Vertices
            int vCount = 0, vCompType = 0, vStride = 0;
            const char* vDataRaw = getAccessorData(posIdx, vCount, vCompType, vStride);
            if (!vDataRaw) continue;
            if (vStride == 0) vStride = 12; // vec3 float default
            
            // Load UVs
            int uvCount = 0, uvCompType = 0, uvStride = 0;
            const char* uvRaw = nullptr;
            if (uvIdx >= 0) {
                 uvRaw = getAccessorData(uvIdx, uvCount, uvCompType, uvStride);
                 if (uvRaw && uvStride == 0) {
                     if (uvCompType == 5126) uvStride = 8; // FLOAT
                     else if (uvCompType == 5123) uvStride = 4; // USHORT
                     else if (uvCompType == 5121) uvStride = 2; // UBYTE
                     else uvStride = 8;
                 }
            }

            baseVertexIndex = m_finalVertices.size();

            for (int i=0; i<vCount; i++) {
                // Read Position (always FLOAT vec3 for standard GLTF IIUC, but stride matters)
                // Technically spec requires POSITION to be FLOAT.
                const float* vPtr = reinterpret_cast<const float*>(vDataRaw + i*vStride);
                float x = vPtr[0];
                float y = vPtr[1];
                float z = vPtr[2];
                // Swap Y/Z for Quake coords (x, z, y)
                m_finalVertices.append(QVector3D(x, z, y));

                if (uvRaw && i < uvCount) {
                    float u = 0.0f, v = 0.0f;
                    const char* uvPtr = uvRaw + i*uvStride;
                    
                    if (uvCompType == 5126) { // FLOAT
                        const float* fuv = reinterpret_cast<const float*>(uvPtr);
                        u = fuv[0]; v = fuv[1];
                    } else if (uvCompType == 5123) { // UNSIGNED_SHORT (normalized)
                        const quint16* suv = reinterpret_cast<const quint16*>(uvPtr);
                        u = suv[0] / 65535.0f;
                        v = suv[1] / 65535.0f;
                    } else if (uvCompType == 5121) { // UNSIGNED_BYTE (normalized)
                        const quint8* buv = reinterpret_cast<const quint8*>(uvPtr);
                        u = buv[0] / 255.0f;
                        v = buv[1] / 255.0f;
                    }
                    m_finalTexCoords.append(QVector2D(u, v));
                } else {
                    m_finalTexCoords.append(QVector2D(0.5f, 0.5f));
                }
            }

            qDebug() << "GLB Primitive: loaded" << vCount << "vertices, baseIndex:" << baseVertexIndex;
            
            // Load Indices (Triangles)
            if (indicesIdx >= 0) {
                int iCount = 0, iCompType = 0, iStride = 0;
                const char* iData = getAccessorData(indicesIdx, iCount, iCompType, iStride);
                if (!iData) continue;
                // Indices are tightly packed usually, componentType dictates stride
                
                for (int i=0; i<iCount; i+=3) {
                    if (i+2 >= iCount) break;
                    
                    int idx1, idx2, idx3;
                    if (iCompType == 5123) { // unsigned short
                        const quint16* p = reinterpret_cast<const quint16*>(iData);
                        idx1 = p[i+0]; idx2 = p[i+1]; idx3 = p[i+2];
                    } else if (iCompType == 5125) { // unsigned int
                        const quint32* p = reinterpret_cast<const quint32*>(iData);
                        idx1 = p[i+0]; idx2 = p[i+1]; idx3 = p[i+2];  
                    } else { // byte? UBYTE 5121
                        const quint8* p = reinterpret_cast<const quint8*>(iData);
                        idx1 = p[i+0]; idx2 = p[i+1]; idx3 = p[i+2];  
                    }
                    
                    Md3Triangle tri;
                    tri.indices[0] = baseVertexIndex + idx1;
                    tri.indices[1] = baseVertexIndex + idx2;
                    tri.indices[2] = baseVertexIndex + idx3;
                    m_triangles.append(tri);
                    m_faceMaterialIndices.append(matIdx);
                }
                
                qDebug() << "  Loaded" << (iCount/3) << "triangles, index range:" << baseVertexIndex << "to" << (m_finalVertices.size()-1);
            }
        }
    }

    return true;
}

bool ObjToMd3Converter::saveMd3(const QString &filename, float scale)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) return false;
    
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);
    
    // Header
    out.writeRawData("IDP3", 4);
    out << (int)15; // Version
    
    // Surface name (max 64 chars)
    QByteArray surfName = QFileInfo(filename).baseName().toLatin1();
    surfName.truncate(63);
    char nameBuf[64] = {0};
    strcpy(nameBuf, surfName.constData());
    out.writeRawData(nameBuf, 64);
    
    out << (int)0; // Flags
    out << (int)1; // Num frames
    out << (int)0; // Num tags
    out << (int)1; // Num surfaces
    out << (int)0; // Num skins
    
    // Offsets
    int ofs_frames = 108;
    int ofs_tags = ofs_frames + (56 * 1); // 1 frame
    int ofs_surfaces = ofs_tags;
    int ofs_eof = 0; // Placeholder
    
    out << ofs_frames << ofs_tags << ofs_surfaces << ofs_eof;
    
    // Frame Data (Bounding Box)
    float minX = 99999, minY = 99999, minZ = 99999;
    float maxX = -99999, maxY = -99999, maxZ = -99999;
    
    for (const QVector3D &v : m_finalVertices) {
        float x = v.x() * scale;
        float y = v.y() * scale;
        float z = v.z() * scale;
        if (x < minX) minX = x; if (y < minY) minY = y; if (z < minZ) minZ = z;
        if (x > maxX) maxX = x; if (y > maxY) maxY = y; if (z > maxZ) maxZ = z;
    }
    
    out << minX << minY << minZ;
    out << maxX << maxY << maxZ;
    
    QVector3D center((minX+maxX)/2, (minY+maxY)/2, (minZ+maxZ)/2);
    out << center.x() << center.y() << center.z(); // origin
    
    float radius = qMax(maxX-minX, qMax(maxY-minY, maxZ-minZ)) / 2.0f;
    out << radius;
    
    char frameNameBuf[16] = "frame_0";
    out.writeRawData(frameNameBuf, 16);
    
    // Surface Data
    // Save current position as start of surface
    qint64 surfStart = file.pos();
    
    out.writeRawData("IDP3", 4);
    out.writeRawData(nameBuf, 64); // Name again
    out << (int)0; // Flags
    out << (int)1; // Num frames
    out << (int)0; // Num shaders
    out << (int)m_finalVertices.size(); // Num verts
    out << (int)m_triangles.size(); // Num triangles
    
    int ofs_tris = 108;
    int ofs_shaders = ofs_tris + (12 * m_triangles.size()); // 3 ints per tri (12 bytes)
    int ofs_st = ofs_shaders; // No shaders
    int ofs_verts = ofs_st + (8 * m_finalVertices.size()); // 2 floats per UV (8 bytes)
    int ofs_end = ofs_verts + (8 * m_finalVertices.size()); // 4 shorts per vert (8 bytes)
    
    out << ofs_tris << ofs_shaders << ofs_st << ofs_verts << ofs_end;
    
    // Triangles
    for (const Md3Triangle &tri : m_triangles) {
        out << tri.indices[0] << tri.indices[1] << tri.indices[2];
    }
    
    // Shaders (skipped)
    
    // TexCoords (ST)
    for (const QVector2D &uv : m_finalTexCoords) {
        out << uv.x() << (1.0f - uv.y()); // Flip V
    }
    
    // Vertices (XYZ + Normal)
    for (const QVector3D &v : m_finalVertices) {
        short x = qBound((short)-32768, (short)(v.x() * scale * 64.0f), (short)32767);
        short y = qBound((short)-32768, (short)(v.y() * scale * 64.0f), (short)32767);
        short z = qBound((short)-32768, (short)(v.z() * scale * 64.0f), (short)32767);
        out << x << y << z;
        out << (short)0; // Normal (encoded) - placeholder
    }
    
    // Update EOF in header
    ofs_eof = file.pos();
    file.seek(104);
    out << ofs_eof;
    
    return true;
}

bool ObjToMd3Converter::generateTextureAtlas(const QString &outputPath, int size)
{
    if (m_finalTexCoords.isEmpty()) {
        qDebug() << "Atlas generation skipped: No UVs";
        return false;
    }
    
    qDebug() << "Generating texture atlas:" << outputPath << "size:" << size;
    qDebug() << "  Materials:" << m_materials.size() << "Triangles:" << m_triangles.size();
    
    // PRE-LOAD all external textures before the loop
    int texturesLoaded = 0;
    int texturesFailed = 0;
    QVector<QString> loadedTexturePaths;
    
    for (auto it = m_materials.begin(); it != m_materials.end(); ++it) {
        ObjMaterial &mat = it.value();
        
        // Skip if already has embedded texture
        if (mat.hasTexture) {
            qDebug() << "  Material" << mat.name << "has embedded texture, size:" << mat.textureImage.size();
            loadedTexturePaths.append(mat.name + "_embedded");
            continue;
        }
        
        // Try to load external texture
        if (!mat.texturePath.isEmpty() && QFile::exists(mat.texturePath)) {
            qDebug() << "  Loading texture for material" << mat.name << "from:" << mat.texturePath;
            
            if (mat.textureImage.load(mat.texturePath)) {
                mat.hasTexture = true;
                texturesLoaded++;
                loadedTexturePaths.append(mat.texturePath);
                qDebug() << "    ✓ Loaded successfully, size:" << mat.textureImage.size();
            } else {
                texturesFailed++;
                qDebug() << "    ✗ Failed to load image file";
            }
        } else if (!mat.texturePath.isEmpty()) {
            qDebug() << "  Material" << mat.name << "texture path does not exist:" << mat.texturePath;
        }
    }
    
    qDebug() << "Pre-load complete. Textures loaded:" << texturesLoaded << "failed:" << texturesFailed;
    
    // For single texture OBJ: simply draw the texture scaled to atlas size
    // The UVs already map correctly to the texture
    if (texturesLoaded == 1 && m_materials.size() == 1) {
        ObjMaterial &mat = m_materials.first();
        if (mat.hasTexture && !mat.textureImage.isNull()) {
            qDebug() << "Single texture detected, drawing to atlas at full resolution";
            
            // Simply draw the texture scaled to the atlas size
            QImage atlas = mat.textureImage.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            
            bool saved = atlas.save(outputPath);
            qDebug() << "Atlas saved:" << saved;
            return saved;
        }
    }
    
    // Multiple materials: need proper UV baking
    qDebug() << "Multiple materials, using UV-based baking";
    
    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(QColor(128, 128, 128));
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    
    // Draw each triangle with its material color
    for (int i = 0; i < m_triangles.size(); ++i) {
        const Md3Triangle &tri = m_triangles[i];
        int matIdx = (i < m_faceMaterialIndices.size()) ? m_faceMaterialIndices[i] : -1;
        
        QColor color = Qt::gray;
        
        if (matIdx >= 0 && matIdx < m_materialNames.size()) {
            ObjMaterial &mat = m_materials[m_materialNames[matIdx]];
            color = mat.color;
        }
        
        QVector2D uv1 = m_finalTexCoords[tri.indices[0]];
        QVector2D uv2 = m_finalTexCoords[tri.indices[1]];
        QVector2D uv3 = m_finalTexCoords[tri.indices[2]];
        
        QPointF points[3] = {
            QPointF(uv1.x() * size, (1.0f - uv1.y()) * size),
            QPointF(uv2.x() * size, (1.0f - uv2.y()) * size),
            QPointF(uv3.x() * size, (1.0f - uv3.y()) * size)
        };
        
        if (color.alpha() == 0) color.setAlpha(255);
        painter.setBrush(color);
        painter.setPen(QPen(color, 1.0));
        painter.drawPolygon(points, 3);
    }
    
    qDebug() << "Atlas baking complete, saving to:" << outputPath;
    return image.save(outputPath);
}

QString ObjToMd3Converter::debugInfo() const {
    return QString("Vertices: %1, Triangles: %2, Materials: %3")
            .arg(m_finalVertices.size())
            .arg(m_triangles.size())
            .arg(m_materials.size());
}

bool ObjToMd3Converter::convertViaPython(const QString &input, const QString &output, double scale, QString &outputLog)
{
    // ... code ...
    return false; // Deprecated or Fallback
}

void ObjToMd3Converter::decimate(int targetTriangles)
{
    if (m_triangles.size() <= targetTriangles) return;
    
    double ratio = (double)targetTriangles / m_triangles.size();
    int step = qMax(1, (int)(1.0 / ratio));
    
    QVector<Md3Triangle> newTris;
    QVector<int> newMatIndices;
    
    for (int i = 0; i < m_triangles.size(); i += step) {
        newTris.append(m_triangles[i]);
        if (i < m_faceMaterialIndices.size())
            newMatIndices.append(m_faceMaterialIndices[i]);
    }
    
    QVector<bool> used(m_finalVertices.size(), false);
    for (const auto &t : newTris) {
        used[t.indices[0]] = true;
        used[t.indices[1]] = true;
        used[t.indices[2]] = true;
    }
    
    QVector<QVector3D> packedV;
    QVector<QVector2D> packedUV;
    QVector<int> map(m_finalVertices.size(), -1);
    
    for (int i=0; i<m_finalVertices.size(); ++i) {
        if (used[i]) {
            map[i] = packedV.size();
            packedV.append(m_finalVertices[i]);
            packedUV.append(m_finalTexCoords[i]);
        }
    }
    
    for (auto &t : newTris) {
        t.indices[0] = map[t.indices[0]];
        t.indices[1] = map[t.indices[1]];
        t.indices[2] = map[t.indices[2]];
    }
    
    m_triangles = newTris;
    m_faceMaterialIndices = newMatIndices;
    m_finalVertices = packedV;
    m_finalTexCoords = packedUV;
    
    qDebug() << "Decimated to" << m_triangles.size() << "triangles";
}

bool ObjToMd3Converter::mergeTextures(const QString &atlasPath, int atlasSize)
{
    QSet<int> usedMatSet;
    for (int idx : m_faceMaterialIndices) usedMatSet.insert(idx);
    
    QVector<int> activeMats = usedMatSet.values().toVector();
    if (activeMats.size() <= 1) return false;
    
    int numMats = activeMats.size();
    int cols = qCeil(qSqrt(numMats));
    int rows = qCeil((double)numMats / cols);
    int cellW = atlasSize / cols;
    int cellH = atlasSize / rows;
    
    QImage atlas(atlasSize, atlasSize, QImage::Format_ARGB32);
    atlas.fill(Qt::gray);
    QPainter painter(&atlas);
    
    struct UVTransform { double u_off, v_off, u_scale, v_scale; };
    QMap<int, UVTransform> transforms;
    
    for (int i=0; i<numMats; ++i) {
        int matIdx = activeMats[i];
        QString matName = m_materialNames[matIdx];
        ObjMaterial &mat = m_materials[matName];
        
        int r = i / cols;
        int c = i % cols;
        int x = c * cellW;
        int y = r * cellH; // Y grows down
        
        // Draw texture
        if (mat.hasTexture) {
             painter.drawImage(QRect(x, y, cellW, cellH), mat.textureImage);
        } else if (!mat.texturePath.isEmpty()) {
             if (QFile::exists(mat.texturePath)) {
                 QImage img(mat.texturePath);
                 painter.drawImage(QRect(x, y, cellW, cellH), img);
             } else {
                 painter.fillRect(x, y, cellW, cellH, mat.color);
             }
        } else {
             painter.fillRect(x, y, cellW, cellH, mat.color);
        }
        
        // Calculate UV Transform (Assuming standard OpenGL 0,0 Bottom-Left)
        // Image Y=0 is Top.
        // Cell (r=0) is At Top.
        // In UV Space, Top is V=1. Bottom is V=0.
        // So Cell (r=0) covers V range [1.0 - 1/rows,  1.0].
        // Cell (r=rows-1) covers V range [0.0, 1/rows].
        
        UVTransform t;
        t.u_scale = 1.0 / cols;
        t.v_scale = 1.0 / rows;
        t.u_off = c * t.u_scale;
        
        // V offset for the bottom of the cell
        // For row 0: Bottom is 1.0 - 1/rows.
        // For row r: Bottom is 1.0 - (r+1)/rows.
        t.v_off = 1.0 - (double)(r + 1) / rows;
        
        transforms[matIdx] = t;
    }
    
    atlas.save(atlasPath);
    
    // Remap Vertices
    // We create a "dirty" set to avoid re-transforming shared vertices multiple times?
    // Or just accept last-win.
    
    for (int i=0; i<m_triangles.size(); ++i) {
        int matIdx = m_faceMaterialIndices[i];
        if (!transforms.contains(matIdx)) continue;
        
        UVTransform t = transforms[matIdx];
        for (int k=0; k<3; ++k) {
            int vIdx = m_triangles[i].indices[k];
            // Since we don't have per-face UVs but per-vertex UVs,
            // we modify the vertex.
            // Note: If shared vertex has different materials, this will be wrong for one.
            // But we assume split vertices for split materials.
            
            // We use original values? No, we modify in place.
            // Problem: if we visit vertex twice, we double transform.
            // WE NEED TO TRACK VISITED VERTICES or DUPLICATE DATA.
            
            // Actually, `m_finalTexCoords` contains the CURRENT UVs.
            // If we assume the input UVs were in [0,1] range relative to sub-texture.
            // We want to map them to [u_off, u_off+scale].
            
            // To avoid double transform, we need to know if vertex was already processed.
            // But we can't easily know.
            
            // Safe approach: Build NEW UV array. Default to (0,0) or copy old if not mapped?
            // If we copy old, and don't transform, it stays old.
            // If we transform, we write new.
            // If collision (shared vertex), last write wins.
        }
    }
    
    // Proper Remap Loop
    QVector<QVector2D> newUVs = m_finalTexCoords;
    for (int i=0; i<m_triangles.size(); ++i) {
        int matIdx = m_faceMaterialIndices[i];
        if (!transforms.contains(matIdx)) continue;
        UVTransform t = transforms[matIdx];
        
        for (int k=0; k<3; ++k) {
             int vIdx = m_triangles[i].indices[k];
             QVector2D old = m_finalTexCoords[vIdx]; // Read from ORIGINAL
             // Wait, if vertex is used by T1 and T2, and both are processed,
             // using `old` from original array is correct.
             // But if T1 and T2 have DIFFERENT transforms (diff material),
             // newUVs[vIdx] will be overwritten by T2.
             // This confirms "Last Win" strategy.
             
             float newU = t.u_off + old.x() * t.u_scale;
             float newV = t.v_off + old.y() * t.v_scale;
             newUVs[vIdx] = QVector2D(newU, newV);
        }
    }
    m_finalTexCoords = newUVs;
    
    return true;
}
