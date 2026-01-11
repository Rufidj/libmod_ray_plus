/*
 * libmod_ray_map.c - Map Loading/Saving for Format v8
 * Complete rewrite - geometric sectors only
 */

#include "libmod_ray.h"
#include "libmod_ray_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* External engine instance */
extern RAY_Engine g_engine;

/* External geometry functions */
extern void ray_detect_portals(RAY_Engine *engine);

/* Forward declaration for nested sector portal detection */
static void ray_detect_shared_walls_with_children(void);
static void ray_detect_all_shared_walls(void);
static void ray_reconstruct_hierarchy(void);

/* ============================================================================
   MAP HEADER v8
   ============================================================================ */

typedef struct {
    char magic[8];              /* "RAYMAP\x1a" */
    uint32_t version;           /* 8 */
    uint32_t num_sectors;
    uint32_t num_portals;
    uint32_t num_sprites;
    uint32_t num_spawn_flags;
    float camera_x, camera_y, camera_z;
    float camera_rot, camera_pitch;
    int32_t skyTextureID;
    uint32_t num_decals;        /* v10+ */
} RAY_MapHeader_v8;

/* ============================================================================
   MAP LOADING
   ============================================================================ */


/* ============================================================================
   MAP HEADER v9
   ============================================================================ */

typedef struct {
    char magic[8];              /* "RAYMAP\x1a" */
    uint32_t version;           /* 9 or 10 */
    uint32_t num_sectors;
    uint32_t num_portals;
    uint32_t num_sprites;
    uint32_t num_spawn_flags;
    float camera_x, camera_y, camera_z;
    float camera_rot, camera_pitch;
    int32_t skyTextureID;
    uint32_t num_decals;        /* v10+ (0 for v9) - AFTER skyTextureID */
} RAY_MapHeader_v9;

/* ============================================================================
   MAP SAVING (V9)
   ============================================================================ */

int ray_save_map_v9(const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "RAY: Error creating file %s\n", filename);
        return 0;
    }
    
    printf("RAY: Saving map v9 to %s...\n", filename);

    /* 1. Header */
    RAY_MapHeader_v9 header;
    memcpy(header.magic, "RAYMAP\x1a", 8);
    header.version = 9;
    header.num_sectors = g_engine.num_sectors;
    header.num_portals = g_engine.num_portals;
    header.num_sprites = g_engine.num_sprites;
    header.num_spawn_flags = g_engine.num_spawn_flags;
    header.camera_x = g_engine.camera.x;
    header.camera_y = g_engine.camera.y;
    header.camera_z = g_engine.camera.z;
    header.camera_rot = g_engine.camera.rot;
    header.camera_pitch = g_engine.camera.pitch;
    header.skyTextureID = g_engine.skyTextureID;
    
    fwrite(&header, sizeof(RAY_MapHeader_v9), 1, file);
    
    /* 2. Sectors */
    for (int i = 0; i < g_engine.num_sectors; i++) {
        RAY_Sector *s = &g_engine.sectors[i];
        
        fwrite(&s->sector_id, sizeof(int), 1, file);
        fwrite(&s->floor_z, sizeof(float), 1, file);
        fwrite(&s->ceiling_z, sizeof(float), 1, file);
        fwrite(&s->floor_texture_id, sizeof(int), 1, file);
        fwrite(&s->ceiling_texture_id, sizeof(int), 1, file);
        fwrite(&s->light_level, sizeof(int), 1, file);
        
        /* Vertices */
        fwrite(&s->num_vertices, sizeof(int), 1, file);
        for (int v = 0; v < s->num_vertices; v++) {
            fwrite(&s->vertices[v].x, sizeof(float), 1, file);
            fwrite(&s->vertices[v].y, sizeof(float), 1, file);
        }
        
        /* Walls */
        fwrite(&s->num_walls, sizeof(int), 1, file);
        for (int w = 0; w < s->num_walls; w++) {
            RAY_Wall *wall = &s->walls[w];
            fwrite(&wall->wall_id, sizeof(int), 1, file);
            fwrite(&wall->x1, sizeof(float), 1, file);
            fwrite(&wall->y1, sizeof(float), 1, file);
            fwrite(&wall->x2, sizeof(float), 1, file);
            fwrite(&wall->y2, sizeof(float), 1, file);
            fwrite(&wall->texture_id_lower, sizeof(int), 1, file);
            fwrite(&wall->texture_id_middle, sizeof(int), 1, file);
            fwrite(&wall->texture_id_upper, sizeof(int), 1, file);
            fwrite(&wall->texture_split_z_lower, sizeof(float), 1, file);
            fwrite(&wall->texture_split_z_upper, sizeof(float), 1, file);
            fwrite(&wall->portal_id, sizeof(int), 1, file);
            fwrite(&wall->flags, sizeof(int), 1, file);
        }
        
        /* Removed: Hierarchy fields */
    }
    
    /* 3. Portals */
    for (int i = 0; i < g_engine.num_portals; i++) {
        RAY_Portal *p = &g_engine.portals[i];
        fwrite(&p->portal_id, sizeof(int), 1, file);
        fwrite(&p->sector_a, sizeof(int), 1, file);
        fwrite(&p->sector_b, sizeof(int), 1, file);
        fwrite(&p->wall_id_a, sizeof(int), 1, file);
        fwrite(&p->wall_id_b, sizeof(int), 1, file);
        fwrite(&p->x1, sizeof(float), 1, file);
        fwrite(&p->y1, sizeof(float), 1, file);
        fwrite(&p->x2, sizeof(float), 1, file);
        fwrite(&p->y2, sizeof(float), 1, file);
    }
    
    /* 4. Sprites */
    for (int i = 0; i < g_engine.num_sprites; i++) {
        RAY_Sprite *s = &g_engine.sprites[i];
        fwrite(&s->textureID, sizeof(int), 1, file);
        fwrite(&s->x, sizeof(float), 1, file);
        fwrite(&s->y, sizeof(float), 1, file);
        fwrite(&s->z, sizeof(float), 1, file);
        fwrite(&s->w, sizeof(int), 1, file);
        fwrite(&s->h, sizeof(int), 1, file);
        fwrite(&s->rot, sizeof(float), 1, file);
        // NOTE: runtime fields like 'visible' or animation state are NOT saved
    }
    
    /* 5. Spawn Flags */
    for (int i = 0; i < g_engine.num_spawn_flags; i++) {
        RAY_SpawnFlag *f = &g_engine.spawn_flags[i];
        fwrite(&f->flag_id, sizeof(int), 1, file);
        fwrite(&f->x, sizeof(float), 1, file);
        fwrite(&f->y, sizeof(float), 1, file);
        fwrite(&f->z, sizeof(float), 1, file);
    }
    
    fclose(file);
    printf("RAY: Saved map v9 (%d sectors, %d portals)\n", g_engine.num_sectors, g_engine.num_portals);
    return 1;
}

/* ============================================================================
   MAP LOADING (V9)
   ============================================================================ */

/* Forward declarations for portal detection */
static void ray_detect_all_shared_walls(void);
static void ray_detect_nested_sectors(void);

int ray_load_map_v9(FILE *file, RAY_MapHeader_v9 *header) {
    if (!file || !header) return 0;
    
    // Initialize num_decals to 0 for v9 maps (v10 will have it set from file)
    if (header->version == 9) {
        header->num_decals = 0;
    }
    
    printf("RAY: Loading Map v9 (%d sectors)...\n", header->num_sectors);
    
    /* 1. Allocate memory */
    g_engine.num_sectors = 0;
    if (header->num_sectors > 0) {
        // free existing if any? Assuming clean slate or handled by caller (like ray_unload_map)
        g_engine.sectors = (RAY_Sector*)calloc(header->num_sectors, sizeof(RAY_Sector));
        g_engine.sectors_capacity = header->num_sectors;
    }
    // Always allocate maximum portal capacity for auto-detected portals
    g_engine.num_portals = 0;
    g_engine.portals = (RAY_Portal*)calloc(RAY_MAX_PORTALS, sizeof(RAY_Portal));
    g_engine.portals_capacity = RAY_MAX_PORTALS;
    g_engine.num_sprites = 0;
    if (header->num_sprites > 0) {
        g_engine.sprites = (RAY_Sprite*)calloc(header->num_sprites, sizeof(RAY_Sprite));
        g_engine.sprites_capacity = header->num_sprites;
    }
    g_engine.num_spawn_flags = 0;
    if (header->num_spawn_flags > 0) {
        g_engine.spawn_flags = (RAY_SpawnFlag*)calloc(header->num_spawn_flags, sizeof(RAY_SpawnFlag));
        g_engine.spawn_flags_capacity = header->num_spawn_flags;
    }
    
    /* 2. Load basic state */
    g_engine.camera.x = header->camera_x;
    g_engine.camera.y = header->camera_y;
    g_engine.camera.z = header->camera_z;
    g_engine.camera.rot = header->camera_rot;
    g_engine.camera.pitch = header->camera_pitch;
    g_engine.skyTextureID = header->skyTextureID;
    
    /* 3. Sectors */
    for (int i = 0; i < header->num_sectors; i++) {
        RAY_Sector *s = &g_engine.sectors[i];
        
        if (fread(&s->sector_id, sizeof(int), 1, file) != 1) {
            fprintf(stderr, "RAY: Error reading sector %d header\n", i);
            break;
        }
        fread(&s->floor_z, sizeof(float), 1, file);
        fread(&s->ceiling_z, sizeof(float), 1, file);
        fread(&s->floor_texture_id, sizeof(int), 1, file);
        fread(&s->ceiling_texture_id, sizeof(int), 1, file);
        fread(&s->light_level, sizeof(int), 1, file);
        
        printf("RAY: Loading sector %d: floor_z=%.1f, ceiling_z=%.1f\n", 
               s->sector_id, s->floor_z, s->ceiling_z);
        
        /* Vertices */
        fread(&s->num_vertices, sizeof(int), 1, file);
        s->vertices_capacity = s->num_vertices > RAY_MAX_VERTICES_PER_SECTOR ? s->num_vertices : RAY_MAX_VERTICES_PER_SECTOR;
        s->vertices = (RAY_Point*)calloc(s->vertices_capacity, sizeof(RAY_Point));
        for (int v = 0; v < s->num_vertices; v++) {
            fread(&s->vertices[v].x, sizeof(float), 1, file);
            fread(&s->vertices[v].y, sizeof(float), 1, file);
        }
        printf("RAY:   %d vertices loaded\n", s->num_vertices);
        
        /* Walls */
        fread(&s->num_walls, sizeof(int), 1, file);
        s->walls_capacity = s->num_walls > RAY_MAX_WALLS_PER_SECTOR ? s->num_walls : RAY_MAX_WALLS_PER_SECTOR;
        s->walls = (RAY_Wall*)calloc(s->walls_capacity, sizeof(RAY_Wall));
        for (int w = 0; w < s->num_walls; w++) {
            RAY_Wall *wall = &s->walls[w];
            fread(&wall->wall_id, sizeof(int), 1, file);
            fread(&wall->x1, sizeof(float), 1, file);
            fread(&wall->y1, sizeof(float), 1, file);
            fread(&wall->x2, sizeof(float), 1, file);
            fread(&wall->y2, sizeof(float), 1, file);
            fread(&wall->texture_id_lower, sizeof(int), 1, file);
            fread(&wall->texture_id_middle, sizeof(int), 1, file);
            fread(&wall->texture_id_upper, sizeof(int), 1, file);
            fread(&wall->texture_split_z_lower, sizeof(float), 1, file);
            fread(&wall->texture_split_z_upper, sizeof(float), 1, file);
            fread(&wall->portal_id, sizeof(int), 1, file);
            fread(&wall->flags, sizeof(int), 1, file);
        }
        printf("RAY:   %d walls loaded\n", s->num_walls);
        
        // Try to load hierarchy fields (may not exist in old maps)
        // Check if there's more data to read
        long current_pos = ftell(file);
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, current_pos, SEEK_SET);
        
        // If there's enough data for hierarchy fields, read them
        if (current_pos + sizeof(int) * 2 <= file_size) {
            fread(&s->parent_sector_id, sizeof(int), 1, file);
            fread(&s->num_children, sizeof(int), 1, file);
            
            if (s->num_children > 0 && s->num_children < 100) {  // Sanity check
                s->children_capacity = s->num_children;
                s->child_sector_ids = (int*)malloc(s->num_children * sizeof(int));
                for (int c = 0; c < s->num_children; c++) {
                    fread(&s->child_sector_ids[c], sizeof(int), 1, file);
                }
                printf("RAY:   Sector %d: parent=%d, children=%d\n", 
                       s->sector_id, s->parent_sector_id, s->num_children);
            } else {
                // Invalid data, reset to defaults
                s->parent_sector_id = -1;
                s->child_sector_ids = NULL;
                s->num_children = 0;
                s->children_capacity = 0;
            }
        } else {
            // Old map format without hierarchy
            s->parent_sector_id = -1;
            s->child_sector_ids = NULL;
            s->num_children = 0;
            s->children_capacity = 0;
        }
        
        // Allocate portal_ids array for the sector (runtime lookup)
        // We'll populate this when we read the portal list later
        s->portals_capacity = RAY_MAX_WALLS_PER_SECTOR; // Enough for 1 portal per wall
        s->portal_ids = (int*)calloc(s->portals_capacity, sizeof(int));
        s->num_portals = 0;
        
        // Increment sector count as we successfully load each one
        g_engine.num_sectors++;
        
        // Calculate basic AABB
        // (Simplified logic here, skipped for brevity, should be done in a helper)
    }
    
    printf("RAY: Loaded %d sectors (expected %d)\n", g_engine.num_sectors, header->num_sectors);
    
    /* 4. Portals */
    for (int i = 0; i < header->num_portals; i++) {
        RAY_Portal *p = &g_engine.portals[i];
        fread(&p->portal_id, sizeof(int), 1, file);
        fread(&p->sector_a, sizeof(int), 1, file);
        fread(&p->sector_b, sizeof(int), 1, file);
        fread(&p->wall_id_a, sizeof(int), 1, file);
        fread(&p->wall_id_b, sizeof(int), 1, file);
        fread(&p->x1, sizeof(float), 1, file);
        fread(&p->y1, sizeof(float), 1, file);
        fread(&p->x2, sizeof(float), 1, file);
        fread(&p->y2, sizeof(float), 1, file);
        
        // Re-link sector's portal list
        if (p->sector_a >= 0 && p->sector_a < g_engine.num_sectors) {
            RAY_Sector *sa = &g_engine.sectors[p->sector_a];
            if (sa->num_portals < sa->portals_capacity) sa->portal_ids[sa->num_portals++] = p->portal_id;
        }
        if (p->sector_b >= 0 && p->sector_b < g_engine.num_sectors) {
            RAY_Sector *sb = &g_engine.sectors[p->sector_b];
            if (sb->num_portals < sb->portals_capacity) sb->portal_ids[sb->num_portals++] = p->portal_id;
        }
    }
    g_engine.num_portals = header->num_portals;

    /* 5. Sprites */
    for (int i = 0; i < header->num_sprites; i++) {
        RAY_Sprite *s = &g_engine.sprites[i];
        fread(&s->textureID, sizeof(int), 1, file);
        fread(&s->x, sizeof(float), 1, file);
        fread(&s->y, sizeof(float), 1, file);
        fread(&s->z, sizeof(float), 1, file);
        fread(&s->w, sizeof(int), 1, file);
        fread(&s->h, sizeof(int), 1, file);
        fread(&s->rot, sizeof(float), 1, file);
    }
    g_engine.num_sprites = header->num_sprites;

    /* 6. Spawn Flags */
    for (int i = 0; i < header->num_spawn_flags; i++) {
        RAY_SpawnFlag *f = &g_engine.spawn_flags[i];
        fread(&f->flag_id, sizeof(int), 1, file);
        fread(&f->x, sizeof(float), 1, file);
        fread(&f->y, sizeof(float), 1, file);
        fread(&f->z, sizeof(float), 1, file);
    }
    g_engine.num_spawn_flags = header->num_spawn_flags;
    
    /* 7. Decals (v10+) */
    printf("RAY: Checking for decals (num_decals=%d, version=%d)...\n", header->num_decals, header->version);
    g_engine.num_decals = 0;
    g_engine.decals = NULL;
    g_engine.decals_capacity = 0;
    
    // Safety check: only load decals if count is reasonable
    if (header->num_decals > 0 && header->num_decals < 10000) {
        printf("RAY: Allocating memory for %d decals...\n", header->num_decals);
        g_engine.decals = (RAY_Decal*)calloc(header->num_decals, sizeof(RAY_Decal));
        if (!g_engine.decals) {
            fprintf(stderr, "RAY: ERROR - Failed to allocate memory for decals\n");
            header->num_decals = 0;
        } else {
            g_engine.decals_capacity = header->num_decals;
        
            for (int i = 0; i < header->num_decals; i++) {
                RAY_Decal *d = &g_engine.decals[i];
                fread(&d->id, sizeof(int), 1, file);
                fread(&d->sector_id, sizeof(int), 1, file);
            
                uint8_t is_floor_byte;
                fread(&is_floor_byte, sizeof(uint8_t), 1, file);
                d->is_floor = (is_floor_byte != 0);
            
                fread(&d->x, sizeof(float), 1, file);
                fread(&d->y, sizeof(float), 1, file);
                fread(&d->width, sizeof(float), 1, file);
                fread(&d->height, sizeof(float), 1, file);
                fread(&d->rotation, sizeof(float), 1, file);
                fread(&d->texture_id, sizeof(int), 1, file);
                fread(&d->alpha, sizeof(float), 1, file);
                fread(&d->render_order, sizeof(int), 1, file);
            }
            g_engine.num_decals = header->num_decals;
            printf("RAY: Loaded %d decals\n", g_engine.num_decals);
        }
    } else if (header->num_decals >= 10000) {
        fprintf(stderr, "RAY: WARNING - Suspicious decal count %d, skipping\n", header->num_decals);
        header->num_decals = 0;
    }
    
    /* 8. Reconstruct parent-child relationships */
    printf("RAY: Reconstructing parent-child relationships...\n");
    for (int i = 0; i < g_engine.num_sectors; i++) {
        RAY_Sector *parent = &g_engine.sectors[i];
        
        for (int c = 0; c < parent->num_children; c++) {
            int child_id = parent->child_sector_ids[c];
            if (child_id >= 0 && child_id < g_engine.num_sectors) {
                RAY_Sector *child = &g_engine.sectors[child_id];
                child->parent_sector_id = parent->sector_id;
                printf("RAY:   Set S%d parent = S%d\n", child_id, parent->sector_id);
            }
        }
    }
    
    /* 8. Auto-detect portals (Build Engine style) */
    printf("RAY: Auto-detecting portals between sectors...\n");
    printf("RAY: Preserving %d manual portals from file\n", g_engine.num_portals);
    
    // NOTE: We do NOT clear portals here - we preserve manual portals from the file
    // and add auto-detected ones for shared walls
    
    ray_detect_all_shared_walls();
    // ray_detect_nested_sectors();  // DISABLED: Using automatic child rendering instead
    printf("RAY: Portal detection complete. Total portals: %d\n", g_engine.num_portals);
    
    /* 9. Bake decals into floor/ceiling textures */
    if (g_engine.num_decals > 0) {
        ray_bake_decals();
    }
    
    return 1;
}

int ray_load_map(const char *filename) {
    if (!filename) return 0;
    
    // Auto-detect v8 (deprecated) vs v9
    FILE *file = fopen(filename, "rb");
    if (!file) return 0;
    
    RAY_MapHeader_v9 header;
    if (fread(&header, sizeof(RAY_MapHeader_v9), 1, file) != 1) {
        fclose(file);
        return 0;
    }
    
    printf("RAY: Detected map version: %u\n", header.version);
    
    if (header.version < 9) {
        fclose(file);
        return ray_load_map_v8(filename); // Will print error/deprecation
    }
    
    int result = ray_load_map_v9(file, &header);
    fclose(file);
    return result;
}


/* ============================================================================
   AUTOMATIC HIERARCHY RECONSTRUCTION
   ============================================================================ */

/* Helper to calculate polygon area for sorting parents */
static float ray_sector_area(RAY_Sector *s) {
    float area = 0;
    for (int i = 0; i < s->num_vertices; i++) {
        int next = (i + 1) % s->num_vertices;
        area += (s->vertices[i].x * s->vertices[next].y);
        area -= (s->vertices[i].y * s->vertices[next].x);
    }
    return fabsf(area) * 0.5f;
}

/* Helper to check point inside sector (Simple PIP) */
/* Re-implementing simplified version to avoid dependency issues if external func not available */
static int ray_point_in_sector_local(RAY_Sector *sector, float x, float y) {
    int i, j, c = 0;
    for (i = 0, j = sector->num_vertices - 1; i < sector->num_vertices; j = i++) {
        if (((sector->vertices[i].y > y) != (sector->vertices[j].y > y)) &&
            (x < (sector->vertices[j].x - sector->vertices[i].x) * (y - sector->vertices[i].y) / 
            (sector->vertices[j].y - sector->vertices[i].y) + sector->vertices[i].x))
            c = !c;
    }
    return c;
}


/* ============================================================================
   AUTOMATIC PORTAL DETECTION FOR NESTED SECTORS
   ============================================================================ */

/* Helper to add a wall to a sector (dynamic array) */
static int add_wall_to_sector(RAY_Sector *sector, RAY_Wall *wall) {
    if (sector->num_walls >= sector->walls_capacity) {
        fprintf(stderr, "RAY: Cannot add wall - sector at capacity\n");
        return -1;
    }
    
    sector->walls[sector->num_walls] = *wall;
    return sector->num_walls++;
}

/* Helper function to calculate overlap region between two walls on the same line */
static int calculate_wall_overlap(RAY_Wall *w1, RAY_Wall *w2, 
                                   float *overlap_start_x, float *overlap_start_y,
                                   float *overlap_end_x, float *overlap_end_y) {
    // Increased tolerance to allow small gaps between sectors (Build Engine style)
    float epsilon = 10.0f;
    
    // Check if walls are on the same vertical line
    if (fabsf(w1->x1 - w1->x2) < epsilon && fabsf(w2->x1 - w2->x2) < epsilon) {
        if (fabsf(w1->x1 - w2->x1) < epsilon) {
            // Both vertical, same X - calculate Y overlap
            float w1_min_y = fminf(w1->y1, w1->y2);
            float w1_max_y = fmaxf(w1->y1, w1->y2);
            float w2_min_y = fminf(w2->y1, w2->y2);
            float w2_max_y = fmaxf(w2->y1, w2->y2);
            
            float overlap_min_y = fmaxf(w1_min_y, w2_min_y);
            float overlap_max_y = fminf(w1_max_y, w2_max_y);
            
            if (overlap_max_y >= overlap_min_y - epsilon) {
                *overlap_start_x = w1->x1;
                *overlap_start_y = overlap_min_y;
                *overlap_end_x = w1->x1;
                *overlap_end_y = overlap_max_y;
                return 1;
            }
        }
    }
    
    // Check if walls are on the same horizontal line
    if (fabsf(w1->y1 - w1->y2) < epsilon && fabsf(w2->y1 - w2->y2) < epsilon) {
        if (fabsf(w1->y1 - w2->y1) < epsilon) {
            // Both horizontal, same Y - calculate X overlap
            float w1_min_x = fminf(w1->x1, w1->x2);
            float w1_max_x = fmaxf(w1->x1, w1->x2);
            float w2_min_x = fminf(w2->x1, w2->x2);
            float w2_max_x = fmaxf(w2->x1, w2->x2);
            
            float overlap_min_x = fmaxf(w1_min_x, w2_min_x);
            float overlap_max_x = fminf(w1_max_x, w2_max_x);
            
            if (overlap_max_x >= overlap_min_x - epsilon) {
                *overlap_start_x = overlap_min_x;
                *overlap_start_y = w1->y1;
                *overlap_end_x = overlap_max_x;
                *overlap_end_y = w1->y1;
                return 1;
            }
        }
    }
    
    return 0;
}

/* Split a wall into segments based on overlap region */
static int split_wall_for_portal(RAY_Sector *sector, int wall_idx,
                                  float overlap_x1, float overlap_y1,
                                  float overlap_x2, float overlap_y2,
                                  int *portal_segment_idx) {
    RAY_Wall *original = &sector->walls[wall_idx];
    float epsilon = 2.0f;
    
    // Determine if wall is vertical or horizontal
    int is_vertical = fabsf(original->x1 - original->x2) < epsilon;
    
    if (is_vertical) {
        // Vertical wall - split by Y coordinates
        float wall_min_y = fminf(original->y1, original->y2);
        float wall_max_y = fmaxf(original->y1, original->y2);
        float overlap_min_y = fminf(overlap_y1, overlap_y2);
        float overlap_max_y = fmaxf(overlap_y1, overlap_y2);
        
        // Check if we need to split
        int need_before = fabsf(wall_min_y - overlap_min_y) > epsilon;
        int need_after = fabsf(wall_max_y - overlap_max_y) > epsilon;
        
        if (!need_before && !need_after) {
            // Entire wall is portal - no split needed
            *portal_segment_idx = wall_idx;
            return 1;
        }
        
        // We need to split - create new wall segments
        RAY_Wall temp_original = *original;
        int current_idx = wall_idx;
        
        // Segment 1: Before overlap (if needed)
        if (need_before) {
            original->x1 = temp_original.x1;
            original->y1 = wall_min_y;
            original->x2 = temp_original.x1;
            original->y2 = overlap_min_y;
            original->portal_id = -1;
            current_idx++;
        }
        
        // Segment 2: Overlap (portal)
        RAY_Wall portal_wall = temp_original;
        portal_wall.x1 = temp_original.x1;
        portal_wall.y1 = overlap_min_y;
        portal_wall.x2 = temp_original.x1;
        portal_wall.y2 = overlap_max_y;
        portal_wall.portal_id = -1;
        
        if (need_before) {
            *portal_segment_idx = add_wall_to_sector(sector, &portal_wall);
        } else {
            sector->walls[wall_idx] = portal_wall;
            *portal_segment_idx = wall_idx;
        }
        
        // Segment 3: After overlap (if needed)
        if (need_after) {
            RAY_Wall after_wall = temp_original;
            after_wall.x1 = temp_original.x1;
            after_wall.y1 = overlap_max_y;
            after_wall.x2 = temp_original.x1;
            after_wall.y2 = wall_max_y;
            after_wall.portal_id = -1;
            add_wall_to_sector(sector, &after_wall);
        }
        
        return 1;
    } else {
        // Horizontal wall - split by X coordinates
        float wall_min_x = fminf(original->x1, original->x2);
        float wall_max_x = fmaxf(original->x1, original->x2);
        float overlap_min_x = fminf(overlap_x1, overlap_x2);
        float overlap_max_x = fmaxf(overlap_x1, overlap_x2);
        
        int need_before = fabsf(wall_min_x - overlap_min_x) > epsilon;
        int need_after = fabsf(wall_max_x - overlap_max_x) > epsilon;
        
        if (!need_before && !need_after) {
            *portal_segment_idx = wall_idx;
            return 1;
        }
        
        RAY_Wall temp_original = *original;
        
        if (need_before) {
            original->x1 = wall_min_x;
            original->y1 = temp_original.y1;
            original->x2 = overlap_min_x;
            original->y2 = temp_original.y1;
            original->portal_id = -1;
        }
        
        RAY_Wall portal_wall = temp_original;
        portal_wall.x1 = overlap_min_x;
        portal_wall.y1 = temp_original.y1;
        portal_wall.x2 = overlap_max_x;
        portal_wall.y2 = temp_original.y1;
        portal_wall.portal_id = -1;
        
        if (need_before) {
            *portal_segment_idx = add_wall_to_sector(sector, &portal_wall);
        } else {
            sector->walls[wall_idx] = portal_wall;
            *portal_segment_idx = wall_idx;
        }
        
        if (need_after) {
            RAY_Wall after_wall = temp_original;
            after_wall.x1 = overlap_max_x;
            after_wall.y1 = temp_original.y1;
            after_wall.x2 = wall_max_x;
            after_wall.y2 = temp_original.y1;
            after_wall.portal_id = -1;
            add_wall_to_sector(sector, &after_wall);
        }
        
        return 1;
    }
}

/* Detect walls shared between ANY two sectors and create portals (Build Engine style) */
static void ray_detect_all_shared_walls(void) {
    int portals_created = 0;
    
    printf("RAY: Detecting shared walls between all sectors (Build Engine style)...\n");
    
    /* Compare all sector pairs */
    for (int i = 0; i < g_engine.num_sectors; i++) {
        RAY_Sector *sector_a = &g_engine.sectors[i];
        
        printf("RAY: Sector %d has %d walls\n", i, sector_a->num_walls);
        for (int wa = 0; wa < sector_a->num_walls; wa++) {
            RAY_Wall *wall_a = &sector_a->walls[wa];
            printf("RAY:   Wall %d: (%.1f,%.1f) -> (%.1f,%.1f) portal_id=%d\n", 
                   wa, wall_a->x1, wall_a->y1, wall_a->x2, wall_a->y2, wall_a->portal_id);
        }
        
        for (int j = i + 1; j < g_engine.num_sectors; j++) {
            RAY_Sector *sector_b = &g_engine.sectors[j];
            
            /* Compare all walls of sector_a with all walls of sector_b */
            for (int wa = 0; wa < sector_a->num_walls; wa++) {
                RAY_Wall *wall_a = &sector_a->walls[wa];
                
                /* Skip if already a portal */
                if (wall_a->portal_id != -1) {
                    continue;
                }
                
                for (int wb = 0; wb < sector_b->num_walls; wb++) {
                    RAY_Wall *wall_b = &sector_b->walls[wb];
                    
                    /* Skip if already a portal */
                    if (wall_b->portal_id != -1) {
                        continue;
                    }
                    
                    /* Calculate overlap region */
                    float overlap_x1, overlap_y1, overlap_x2, overlap_y2;
                    if (calculate_wall_overlap(wall_a, wall_b, &overlap_x1, &overlap_y1, &overlap_x2, &overlap_y2)) {
                        /* Check portal capacity */
                        if (g_engine.num_portals >= g_engine.portals_capacity) {
                            printf("RAY: WARNING - Portal capacity reached\n");
                            return;
                        }
                        
                        /* Split walls if needed and get portal segment indices */
                        int portal_wall_a_idx = wa;
                        int portal_wall_b_idx = wb;
                        
                        split_wall_for_portal(sector_a, wa, overlap_x1, overlap_y1, overlap_x2, overlap_y2, &portal_wall_a_idx);
                        split_wall_for_portal(sector_b, wb, overlap_x1, overlap_y1, overlap_x2, overlap_y2, &portal_wall_b_idx);
                        
                        /* Get the portal segment walls */
                        RAY_Wall *portal_wall_a = &sector_a->walls[portal_wall_a_idx];
                        RAY_Wall *portal_wall_b = &sector_b->walls[portal_wall_b_idx];
                        
                        /* Create bidirectional portal */
                        RAY_Portal *new_portal = &g_engine.portals[g_engine.num_portals];
                        memset(new_portal, 0, sizeof(RAY_Portal));
                        
                        new_portal->portal_id = g_engine.num_portals;
                        new_portal->sector_a = sector_a->sector_id;
                        new_portal->sector_b = sector_b->sector_id;
                        new_portal->wall_id_a = portal_wall_a_idx;
                        new_portal->wall_id_b = portal_wall_b_idx;
                        new_portal->x1 = overlap_x1;
                        new_portal->y1 = overlap_y1;
                        new_portal->x2 = overlap_x2;
                        new_portal->y2 = overlap_y2;
                        
                        /* BUILD ENGINE: Portal height uses min ceiling and max floor */
                        new_portal->floor_z = fmaxf(sector_a->floor_z, sector_b->floor_z);
                        new_portal->ceiling_z = fminf(sector_a->ceiling_z, sector_b->ceiling_z);
                        
                        /* Assign portal to both wall segments */
                        portal_wall_a->portal_id = new_portal->portal_id;
                        portal_wall_b->portal_id = new_portal->portal_id;
                        
                        /* Auto-assign step textures from main wall texture (Build Engine style) */
                        if (portal_wall_a->texture_id_upper == 0) {
                            portal_wall_a->texture_id_upper = portal_wall_a->texture_id_middle;
                        }
                        if (portal_wall_a->texture_id_lower == 0) {
                            portal_wall_a->texture_id_lower = portal_wall_a->texture_id_middle;
                        }
                        if (portal_wall_b->texture_id_upper == 0) {
                            portal_wall_b->texture_id_upper = portal_wall_b->texture_id_middle;
                        }
                        if (portal_wall_b->texture_id_lower == 0) {
                            portal_wall_b->texture_id_lower = portal_wall_b->texture_id_middle;
                        }
                        
                        /* Add to both sectors portal lists */
                        if (sector_a->num_portals < sector_a->portals_capacity) {
                            sector_a->portal_ids[sector_a->num_portals++] = new_portal->portal_id;
                        }
                        if (sector_b->num_portals < sector_b->portals_capacity) {
                            sector_b->portal_ids[sector_b->num_portals++] = new_portal->portal_id;
                        }
                        
                        g_engine.num_portals++;
                        portals_created++;
                        
                        printf("RAY: Created portal %d: Sector %d (wall %d) <-> Sector %d (wall %d)\n",
                               new_portal->portal_id, sector_a->sector_id, wa, sector_b->sector_id, wb);
                        
                        break; /* Found match for this wall_a */
                    }
                }
            }
        }
    }
    
    printf("RAY: Created %d automatic portals\n", portals_created);
}

/* Detect nested sectors using parent_sector_id and create invisible portals */
static void ray_detect_nested_sectors(void) {
    int portals_created = 0;
    
    printf("RAY: Creating invisible portals for parent-child sectors...\n");
    
    // For each sector that has a parent, create an invisible portal to that parent
    for (int i = 0; i < g_engine.num_sectors; i++) {
        RAY_Sector *child = &g_engine.sectors[i];
        
        // Skip if no parent
        if (child->parent_sector_id < 0 || child->parent_sector_id >= g_engine.num_sectors) {
            continue;
        }
        
        RAY_Sector *parent = &g_engine.sectors[child->parent_sector_id];
        
        printf("RAY:   Checking S%d (parent=S%d)...\n", child->sector_id, parent->sector_id);
        
        // Check if there's already a portal between these sectors (from shared walls)
        int portal_exists = 0;
        for (int p = 0; p < child->num_portals; p++) {
            int portal_id = child->portal_ids[p];
            if (portal_id >= 0 && portal_id < g_engine.num_portals) {
                RAY_Portal *portal = &g_engine.portals[portal_id];
                if ((portal->sector_a == parent->sector_id && portal->sector_b == child->sector_id) ||
                    (portal->sector_a == child->sector_id && portal->sector_b == parent->sector_id)) {
                    portal_exists = 1;
                    break;
                }
            }
        }
        
        if (portal_exists) {
            printf("RAY:   S%d <-> S%d already has portal (shared wall)\n", 
                   parent->sector_id, child->sector_id);
            continue;
        }
        
        // Find any wall from each sector (prefer first wall)
        if (parent->num_walls == 0 || child->num_walls == 0) {
            printf("RAY:   WARNING: S%d or S%d has no walls, skipping\n", 
                   parent->sector_id, child->sector_id);
            continue;
        }
        
        // Check portal capacity
        if (g_engine.num_portals >= g_engine.portals_capacity) {
            printf("RAY: WARNING - Portal capacity reached\n");
            return;
        }
        
        // Use first available wall from each sector
        int parent_wall_idx = 0;
        int child_wall_idx = 0;
        
        RAY_Wall *parent_wall = &parent->walls[parent_wall_idx];
        RAY_Wall *child_wall = &child->walls[child_wall_idx];
        
        // Create invisible portal
        RAY_Portal *new_portal = &g_engine.portals[g_engine.num_portals];
        memset(new_portal, 0, sizeof(RAY_Portal));
        
        new_portal->portal_id = g_engine.num_portals;
        new_portal->sector_a = parent->sector_id;
        new_portal->sector_b = child->sector_id;
        new_portal->wall_id_a = parent_wall_idx;
        new_portal->wall_id_b = child_wall_idx;
        
        // Use child wall coordinates for portal (doesn't matter for invisible portals)
        new_portal->x1 = child_wall->x1;
        new_portal->y1 = child_wall->y1;
        new_portal->x2 = child_wall->x2;
        new_portal->y2 = child_wall->y2;
        
        // ASSIGN portal_id to walls so the portal system can traverse them
        // The walls will still render (they're not transparent) but portals can be traversed
        parent_wall->portal_id = new_portal->portal_id;
        child_wall->portal_id = new_portal->portal_id;
        
        // Add to both sectors' portal lists
        if (parent->num_portals < parent->portals_capacity) {
            parent->portal_ids[parent->num_portals++] = new_portal->portal_id;
        }
        if (child->num_portals < child->portals_capacity) {
            child->portal_ids[child->num_portals++] = new_portal->portal_id;
        }
        
        g_engine.num_portals++;
        portals_created++;
        
        printf("RAY:   Created invisible portal %d: Parent S%d <-> Child S%d\n",
               new_portal->portal_id, parent->sector_id, child->sector_id);
    }
    
    printf("RAY: Created %d invisible portals for nested sectors\n", portals_created);
    
    // DEBUG: Print all portals
    printf("RAY: === PORTAL SUMMARY ===\n");
    for (int i = 0; i < g_engine.num_portals; i++) {
        RAY_Portal *p = &g_engine.portals[i];
        printf("RAY:   Portal %d: S%d <-> S%d\n", p->portal_id, p->sector_a, p->sector_b);
    }
    printf("RAY: === END PORTAL SUMMARY ===\n");
}

/* Detect walls shared between parent and child sectors and create portals */

/* ============================================================================
   MAP SAVING
   ============================================================================ */


/* ============================================================================
   V8 COMPATIBILITY STUBS (DEPRECATED)
   ============================================================================ */

int ray_load_map_v8(const char *filename) {
    fprintf(stderr, "RAY: Map format v8 is deprecated. Please re-export to v9.\n");
    return -1;
}

int ray_save_map_v8(const char *filename) {
    fprintf(stderr, "RAY: Map format v8 is deprecated. Please use v9.\n");
    return -1;
}
