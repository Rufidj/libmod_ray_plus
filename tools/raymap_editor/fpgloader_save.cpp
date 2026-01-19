#include "fpgloader.h"
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <zlib.h>

bool FPGLoader::saveFPG(const QString &filename, const QVector<TextureEntry> &textures, bool compress)
{
    if (textures.isEmpty()) {
        qWarning() << "No textures to save";
        return false;
    }

    QByteArray uncompressedData;
    QDataStream out(&uncompressedData, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);

    // Write header
    out.writeRawData("F32\0\0\0\0\0", 8);

    // Write each texture
    for (const TextureEntry &entry : textures) {
        int code = entry.id;
        QPixmap pixmap = entry.pixmap;
        QImage image = pixmap.toImage().convertToFormat(QImage::Format_RGBA8888);

        FPG_CHUNK chunk;
        chunk.code = code;
        chunk.regsize = 0;
        memset(chunk.name, 0, 32);
        memset(chunk.filename, 0, 12);
        snprintf(chunk.name, 32, "Texture_%d", code);
        chunk.width = image.width();
        chunk.height = image.height();
        chunk.flags = 0;

        // Write chunk header
        out << chunk.code << chunk.regsize;
        out.writeRawData(chunk.name, 32);
        out.writeRawData(chunk.filename, 12);
        out << chunk.width << chunk.height << chunk.flags;

        // Convert RGBA to BGRA and write pixel data
        int pixelDataSize = chunk.width * chunk.height * 4;
        char* pixelBuffer = new char[pixelDataSize];
        memcpy(pixelBuffer, image.constBits(), pixelDataSize);

        for (int i = 0; i < pixelDataSize; i += 4) {
            char temp = pixelBuffer[i];
            pixelBuffer[i] = pixelBuffer[i + 2];
            pixelBuffer[i + 2] = temp;
        }

        out.writeRawData(pixelBuffer, pixelDataSize);
        delete[] pixelBuffer;
    }

    // Write to file
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Could not open file for writing:" << filename;
        return false;
    }

    if (compress) {
        // Compress with gzip
        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;

        if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            qWarning() << "Failed to initialize gzip compression";
            file.close();
            return false;
        }

        strm.avail_in = uncompressedData.size();
        strm.next_in = (Bytef*)uncompressedData.data();

        QByteArray compressedData;
        compressedData.resize(deflateBound(&strm, uncompressedData.size()));

        strm.avail_out = compressedData.size();
        strm.next_out = (Bytef*)compressedData.data();

        deflate(&strm, Z_FINISH);
        compressedData.resize(compressedData.size() - strm.avail_out);
        deflateEnd(&strm);

        file.write(compressedData);
    } else {
        file.write(uncompressedData);
    }

    file.close();
    return true;
}
