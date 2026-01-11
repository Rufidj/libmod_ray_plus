/*
 * raymapformat.cpp - Map Loading/Saving for Editor (Format v10)
 * v10: Added decals support
 * v9: Added nested sectors support
 * v8: Geometric sectors only
 */

#include "raymapformat.h"
#include "mapdata.h"
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <cstring>
#include <QMap>

RayMapFormat::RayMapFormat()
{
}

/* ============================================================================
   MAP LOADING
   ============================================================================ */

bool RayMapFormat::loadMap(const QString &filename, MapData &mapData,
                           std::function<void(const QString&)> progressCallback)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "No se pudo abrir el archivo:" << filename;
        return false;
    }
    
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    
    /* Read header */
    char magic[8];
    uint32_t version, num_sectors, num_portals, num_sprites, num_spawn_flags, num_decals;
    float camera_x, camera_y, camera_z, camera_rot, camera_pitch;
    int32_t skyTextureID;
    
    in.readRawData(magic, 8);
    in.readRawData(reinterpret_cast<char*>(&version), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&num_sectors), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&num_portals), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&num_sprites), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&num_spawn_flags), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&camera_x), sizeof(float));
    in.readRawData(reinterpret_cast<char*>(&camera_y), sizeof(float));
    in.readRawData(reinterpret_cast<char*>(&camera_z), sizeof(float));
    in.readRawData(reinterpret_cast<char*>(&camera_rot), sizeof(float));
    in.readRawData(reinterpret_cast<char*>(&camera_pitch), sizeof(float));
    in.readRawData(reinterpret_cast<char*>(&skyTextureID), sizeof(int32_t));
    
    /* Verify magic number */
    if (memcmp(magic, "RAYMAP\x1a", 7) != 0) {
        qWarning() << "Formato de archivo inválido";
        file.close();
        return false;
    }
    
    /* Verify version */
    if (version < 8 || version > 10) {
        qWarning() << "Versión no soportada:" << version << "(solo v8-v10)";
        file.close();
        return false;
    }
    
    /* Read num_decals if v10+ */
    if (version >= 10) {
        in.readRawData(reinterpret_cast<char*>(&num_decals), sizeof(uint32_t));
    } else {
        num_decals = 0;
    }
    
    qDebug() << "Cargando mapa v" << version << ":" << num_sectors << "sectores," << num_portals << "portales";
    
    if (progressCallback) progressCallback("Cargando sectores...");
    
    /* Set camera */
    mapData.camera.x = camera_x;
    mapData.camera.y = camera_y;
    mapData.camera.z = camera_z;
    mapData.camera.rotation = camera_rot;
    mapData.camera.pitch = camera_pitch;
    mapData.camera.enabled = true;
    mapData.skyTextureID = skyTextureID;
    
    /* Load sectors */
    mapData.sectors.clear();
    for (uint32_t i = 0; i < num_sectors; i++) {
        Sector sector;
        
        in.readRawData(reinterpret_cast<char*>(&sector.sector_id), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&sector.floor_z), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&sector.ceiling_z), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&sector.floor_texture_id), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&sector.ceiling_texture_id), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&sector.light_level), sizeof(int));
        
        /* Read vertices */
        uint32_t num_vertices;
        in.readRawData(reinterpret_cast<char*>(&num_vertices), sizeof(uint32_t));
        
        for (uint32_t v = 0; v < num_vertices; v++) {
            float x, y;
            in.readRawData(reinterpret_cast<char*>(&x), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&y), sizeof(float));
            sector.vertices.append(QPointF(x, y));
        }
        
        /* Read walls */
        uint32_t num_walls;
        in.readRawData(reinterpret_cast<char*>(&num_walls), sizeof(uint32_t));
        
        for (uint32_t w = 0; w < num_walls; w++) {
            Wall wall;
            
            in.readRawData(reinterpret_cast<char*>(&wall.wall_id), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&wall.x1), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&wall.y1), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&wall.x2), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&wall.y2), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&wall.texture_id_lower), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&wall.texture_id_middle), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&wall.texture_id_upper), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&wall.texture_split_z_lower), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&wall.texture_split_z_upper), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&wall.portal_id), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&wall.flags), sizeof(int));
            
            sector.walls.append(wall);
        }
        
        // Load hierarchy fields (parent and children)
        in.readRawData(reinterpret_cast<char*>(&sector.parent_sector_id), sizeof(int));
        int numChildren;
        in.readRawData(reinterpret_cast<char*>(&numChildren), sizeof(int));
        for (int c = 0; c < numChildren; c++) {
            int childId;
            in.readRawData(reinterpret_cast<char*>(&childId), sizeof(int));
            sector.child_sector_ids.append(childId);
        }
        
        mapData.sectors.append(sector);
    }
    
    if (progressCallback) progressCallback("Cargando portales...");
    
    /* Load portals */
    mapData.portals.clear();
    for (uint32_t i = 0; i < num_portals; i++) {
        Portal portal;
        
        in.readRawData(reinterpret_cast<char*>(&portal.portal_id), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&portal.sector_a), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&portal.sector_b), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&portal.wall_id_a), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&portal.wall_id_b), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&portal.x1), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&portal.y1), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&portal.x2), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&portal.y2), sizeof(float));
        
        /* Add portal IDs to sectors */
        for (Sector &sector : mapData.sectors) {
            if (sector.sector_id == portal.sector_a || sector.sector_id == portal.sector_b) {
                if (!sector.portal_ids.contains(portal.portal_id)) {
                    sector.portal_ids.append(portal.portal_id);
                }
            }
        }
        
        mapData.portals.append(portal);
    }
    
    if (progressCallback) progressCallback("Cargando sprites...");
    
    /* Load sprites */
    mapData.sprites.clear();
    for (uint32_t i = 0; i < num_sprites; i++) {
        SpriteData sprite;
        
        in.readRawData(reinterpret_cast<char*>(&sprite.texture_id), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&sprite.x), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&sprite.y), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&sprite.z), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&sprite.w), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&sprite.h), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&sprite.rot), sizeof(float));
        
        mapData.sprites.append(sprite);
    }
    
    if (progressCallback) progressCallback("Cargando spawn flags...");
    
    /* Load spawn flags */
    mapData.spawnFlags.clear();
    for (uint32_t i = 0; i < num_spawn_flags; i++) {
        SpawnFlag flag;
        
        in.readRawData(reinterpret_cast<char*>(&flag.flagId), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&flag.x), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&flag.y), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&flag.z), sizeof(float));
        
        mapData.spawnFlags.append(flag);
    }
    
    if (progressCallback) progressCallback("Cargando decals...");
    
    /* Load decals (v10+) */
    mapData.decals.clear();
    for (uint32_t i = 0; i < num_decals; i++) {
        Decal decal;
        
        in.readRawData(reinterpret_cast<char*>(&decal.id), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&decal.sector_id), sizeof(int));
        
        uint8_t is_floor_byte;
        in.readRawData(reinterpret_cast<char*>(&is_floor_byte), sizeof(uint8_t));
        decal.is_floor = (is_floor_byte != 0);
        
        in.readRawData(reinterpret_cast<char*>(&decal.x), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&decal.y), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&decal.width), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&decal.height), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&decal.rotation), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&decal.texture_id), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&decal.alpha), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&decal.render_order), sizeof(int));
        
        mapData.decals.append(decal);
    }
    
    file.close();
    
    qDebug() << "Mapa cargado:" << mapData.sectors.size() << "sectores,"
             << mapData.portals.size() << "portales,"
             << mapData.sprites.size() << "sprites,"
             << mapData.spawnFlags.size() << "spawn flags,"
             << mapData.decals.size() << "decals";
    
    return true;
}

/* ============================================================================
   MAP SAVING
   ============================================================================ */

bool RayMapFormat::saveMap(const QString &filename, const MapData &mapData,
                           std::function<void(const QString&)> progressCallback)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "No se pudo crear el archivo:" << filename;
        return false;
    }
    
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    
    /* Prepare header */
    char magic[8];
    memcpy(magic, "RAYMAP\x1a", 7);
    magic[7] = 0;
    
    // --- PORTAL RENUMBERING (Defragmentation) ---
    // Engine expects portals to be contiguous 0..N-1 matching array index.
    // Editor IDs might have gaps due to deletions. We must map them.
    QMap<int, int> portalIdMap;
    int nextPortalId = 0;
    for (const Portal &p : mapData.portals) {
        portalIdMap[p.portal_id] = nextPortalId++;
    }
    // --------------------------------------------
    
    uint32_t version = 10;  // Updated to v10 for decals
    uint32_t num_sectors = mapData.sectors.size();
    uint32_t num_portals = mapData.portals.size();
    uint32_t num_sprites = mapData.sprites.size();
    uint32_t num_spawn_flags = mapData.spawnFlags.size();
    uint32_t num_decals = mapData.decals.size();
    
    float camera_x = mapData.camera.x;
    float camera_y = mapData.camera.y;
    float camera_z = mapData.camera.z;
    float camera_rot = mapData.camera.rotation;
    float camera_pitch = mapData.camera.pitch;
    int32_t skyTextureID = mapData.skyTextureID;
    
    qDebug() << "DEBUG SAVE: Writing camera position:" << camera_x << "," << camera_y << "," << camera_z;
    
    /* Write header */
    out.writeRawData(magic, 8);
    out.writeRawData(reinterpret_cast<const char*>(&version), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&num_sectors), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&num_portals), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&num_sprites), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&num_spawn_flags), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&camera_x), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&camera_y), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&camera_z), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&camera_rot), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&camera_pitch), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&skyTextureID), sizeof(int32_t));
    out.writeRawData(reinterpret_cast<const char*>(&num_decals), sizeof(uint32_t));
    
    if (progressCallback) progressCallback("Guardando sectores...");
    
    /* Write sectors */
    for (const Sector &sector : mapData.sectors) {
        out.writeRawData(reinterpret_cast<const char*>(&sector.sector_id), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&sector.floor_z), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&sector.ceiling_z), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&sector.floor_texture_id), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&sector.ceiling_texture_id), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&sector.light_level), sizeof(int));
        
        /* Write vertices */
        uint32_t num_vertices = sector.vertices.size();
        out.writeRawData(reinterpret_cast<const char*>(&num_vertices), sizeof(uint32_t));
        
        for (const QPointF &vertex : sector.vertices) {
            float x = vertex.x();
            float y = vertex.y();
            out.writeRawData(reinterpret_cast<const char*>(&x), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&y), sizeof(float));
        }
        
        /* Write walls */
        uint32_t num_walls = sector.walls.size();
        out.writeRawData(reinterpret_cast<const char*>(&num_walls), sizeof(uint32_t));
        
        for (const Wall &wall : sector.walls) {
            out.writeRawData(reinterpret_cast<const char*>(&wall.wall_id), sizeof(int));
            out.writeRawData(reinterpret_cast<const char*>(&wall.x1), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&wall.y1), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&wall.x2), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&wall.y2), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&wall.texture_id_lower), sizeof(int));
            out.writeRawData(reinterpret_cast<const char*>(&wall.texture_id_middle), sizeof(int));
            out.writeRawData(reinterpret_cast<const char*>(&wall.texture_id_upper), sizeof(int));
            out.writeRawData(reinterpret_cast<const char*>(&wall.texture_split_z_lower), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&wall.texture_split_z_upper), sizeof(float));
            
            // USE MAPPED ID
            int savedPortalId = -1;
            if (wall.portal_id >= 0 && portalIdMap.contains(wall.portal_id)) {
                savedPortalId = portalIdMap[wall.portal_id];
            } else if (wall.portal_id >= 0) {
                 qWarning() << "Warning: Wall points to non-existent portal ID:" << wall.portal_id;
            }
            out.writeRawData(reinterpret_cast<const char*>(&savedPortalId), sizeof(int));
            
            out.writeRawData(reinterpret_cast<const char*>(&wall.flags), sizeof(int));
        }
        
        // Save hierarchy (parent and children)
        out.writeRawData(reinterpret_cast<const char*>(&sector.parent_sector_id), sizeof(int));
        int numChildren = sector.child_sector_ids.size();
        out.writeRawData(reinterpret_cast<const char*>(&numChildren), sizeof(int));
        for (int childId : sector.child_sector_ids) {
            out.writeRawData(reinterpret_cast<const char*>(&childId), sizeof(int));
        }
    }
    
    if (progressCallback) progressCallback("Guardando portales...");
    
    /* Write portals */
    for (const Portal &portal : mapData.portals) {
        // USE MAPPED ID (which is sequential 0..N-1)
        int savedPortalId = portalIdMap[portal.portal_id];
        
        out.writeRawData(reinterpret_cast<const char*>(&savedPortalId), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&portal.sector_a), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&portal.sector_b), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&portal.wall_id_a), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&portal.wall_id_b), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&portal.x1), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&portal.y1), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&portal.x2), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&portal.y2), sizeof(float));
    }
    
    if (progressCallback) progressCallback("Guardando sprites...");
    
    /* Write sprites */
    for (const SpriteData &sprite : mapData.sprites) {
        out.writeRawData(reinterpret_cast<const char*>(&sprite.texture_id), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&sprite.x), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&sprite.y), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&sprite.z), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&sprite.w), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&sprite.h), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&sprite.rot), sizeof(float));
    }
    
    if (progressCallback) progressCallback("Guardando spawn flags...");
    
    /* Write spawn flags */
    for (const SpawnFlag &flag : mapData.spawnFlags) {
        out.writeRawData(reinterpret_cast<const char*>(&flag.flagId), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&flag.x), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&flag.y), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&flag.z), sizeof(float));
    }
    
    if (progressCallback) progressCallback("Guardando decals...");
    
    /* Write decals */
    for (const Decal &decal : mapData.decals) {
        out.writeRawData(reinterpret_cast<const char*>(&decal.id), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&decal.sector_id), sizeof(int));
        
        uint8_t is_floor_byte = decal.is_floor ? 1 : 0;
        out.writeRawData(reinterpret_cast<const char*>(&is_floor_byte), sizeof(uint8_t));
        
        out.writeRawData(reinterpret_cast<const char*>(&decal.x), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&decal.y), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&decal.width), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&decal.height), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&decal.rotation), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&decal.texture_id), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&decal.alpha), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&decal.render_order), sizeof(int));
    }
    
    /* Flush */
    out.device()->waitForBytesWritten(-1);
    file.flush();
    file.close();
    
    qDebug() << "Mapa guardado:" << mapData.sectors.size() << "sectores,"
             << mapData.portals.size() << "portales,"
             << mapData.decals.size() << "decals";
    
    return true;
}
