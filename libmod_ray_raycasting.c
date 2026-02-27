/*
 * libmod_ray_raycasting.c - Raycasting for Geometric Sectors
 * Cleaned up for robust nested sectors support and consistent Y-coordinate
 * system
 */

#include "libmod_ray.h"
#include "libmod_ray_compat.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External engine instance */
extern RAY_Engine g_engine;

/* ============================================================================
   SECTOR RESOLVER
   ============================================================================
 */

static RAY_Sector *resolve_sector(RAY_Engine *engine, int sector_id) {
  if (sector_id < 0)
    return NULL;
  if (sector_id < engine->num_sectors &&
      engine->sectors[sector_id].sector_id == sector_id)
    return &engine->sectors[sector_id];
  for (int i = 0; i < engine->num_sectors; i++) {
    if (engine->sectors[i].sector_id == sector_id)
      return &engine->sectors[i];
  }
  return NULL;
}

/* ============================================================================
   RAY-WALL INTERSECTION
   Consistent with BennuGD/SDL Y-Down System (+sin for Y)
   ============================================================================
 */

void ray_find_wall_intersection(float ray_x, float ray_y, float ray_angle,
                                const RAY_Wall *wall, float *distance,
                                float *hit_x, float *hit_y) {
  if (!wall || !distance)
    return;

  *distance = FLT_MAX;

  float ray_dx = cosf(ray_angle);
  float ray_dy = sinf(ray_angle); // Changed to +sin to match movement/renderer

  float wall_dx = wall->x2 - wall->x1;
  float wall_dy = wall->y2 - wall->y1;

  float denom = ray_dx * wall_dy - ray_dy * wall_dx;
  if (fabsf(denom) < 1e-6f)
    return;

  float t =
      ((wall->x1 - ray_x) * wall_dy - (wall->y1 - ray_y) * wall_dx) / denom;
  float u = ((wall->x1 - ray_x) * ray_dy - (wall->y1 - ray_y) * ray_dx) / denom;

  if (t > 0 && u >= -0.001f && u <= 1.001f) {
    float ix = ray_x + t * ray_dx;
    float iy = ray_y + t * ray_dy;
    float dx = ix - ray_x;
    float dy = iy - ray_y;
    *distance = sqrtf(dx * dx + dy * dy);
    if (hit_x)
      *hit_x = ix;
    if (hit_y)
      *hit_y = iy;
  }
}

/* ============================================================================
   ROBUST GEOMETRY COLLECTION
   Collects all visible walls (own + descendants) for a sector
   ============================================================================
 */

static void collect_sector_geometry(RAY_Engine *engine, int sector_id,
                                    float ray_x, float ray_y, float ray_angle,
                                    float accum_dist, int strip_idx,
                                    RAY_RayHit *local_hits, int *num_local_hits,
                                    int max_local) {
  RAY_Sector *sector = resolve_sector(engine, sector_id);
  if (!sector)
    return;

  /* 1. Own Walls */
  for (int i = 0; i < sector->num_walls && *num_local_hits < max_local; i++) {
    RAY_Wall *wall = &sector->walls[i];
    float dist, hx, hy;
    ray_find_wall_intersection(ray_x, ray_y, ray_angle, wall, &dist, &hx, &hy);

    if (dist < FLT_MAX) {
      RAY_RayHit *hit = &local_hits[*num_local_hits];
      memset(hit, 0, sizeof(RAY_RayHit));
      hit->x = hx;
      hit->y = hy;
      hit->sector_id = sector_id;
      hit->wall_id = wall->wall_id;
      hit->wall = wall;
      hit->distance = dist + accum_dist;
      hit->rayAngle = ray_angle;
      hit->strip = strip_idx;
      hit->wallHeight = sector->ceiling_z - sector->floor_z;
      hit->wallZOffset = sector->floor_z;
      hit->is_child_sector = (ray_sector_get_parent(sector) >= 0);

      float dx = hx - wall->x1;
      float dy = hy - wall->y1;
      hit->tileX = sqrtf(dx * dx + dy * dy);

      float angle_diff = ray_angle - engine->camera.rot;
      hit->correctDistance = hit->distance * cosf(angle_diff);

      (*num_local_hits)++;
    }
  }

  /* 2. Recursive Children (Islands) */
  for (int i = 0; i < sector->num_children; i++) {
    int child_id = sector->child_sector_ids[i];
    collect_sector_geometry(engine, child_id, ray_x, ray_y, ray_angle,
                            accum_dist, strip_idx, local_hits, num_local_hits,
                            max_local);
  }

  /* 3. SAFETY FALLBACK: Find solid sectors that might be missing from the
   * hierarchy */
  /* If we are processing the main floor (typically sector_id 0), scan for solid
   * orphans */
  if (sector_id == 0) {
    for (int i = 0; i < engine->num_sectors; i++) {
      RAY_Sector *s = &engine->sectors[i];
      if (s->sector_id == 0 || !ray_sector_is_solid(s))
        continue;

      /* If its parent is 0 but it's not in the child list, it's an orphan we
       * need to catch */
      if (ray_sector_get_parent(s) == 0) {
        int is_already_child = 0;
        for (int c = 0; c < sector->num_children; c++) {
          if (sector->child_sector_ids[c] == s->sector_id) {
            is_already_child = 1;
            break;
          }
        }

        if (!is_already_child) {
          /* Force collection of this 'lost' building */
          collect_sector_geometry(engine, s->sector_id, ray_x, ray_y, ray_angle,
                                  accum_dist, strip_idx, local_hits,
                                  num_local_hits, max_local);
        }
      }
    }
  }
}

static int ray_local_hit_sorter(const void *a, const void *b) {
  const RAY_RayHit *ha = (const RAY_RayHit *)a;
  const RAY_RayHit *hb = (const RAY_RayHit *)b;
  if (ha->distance < hb->distance)
    return -1;
  if (ha->distance > hb->distance)
    return 1;
  return 0;
}

/* ============================================================================
   MAIN RAYCASTING FUNCTION
   ============================================================================
 */

void ray_cast_ray(RAY_Engine *engine, int sector_id, float x, float y,
                  float ray_angle, int strip_idx, RAY_RayHit *hits,
                  int *num_hits) {
  if (!engine || !hits || !num_hits)
    return;

  *num_hits = 0;
  int cur_sector_id = sector_id;
  if (cur_sector_id < 0) {
    RAY_Sector *s = ray_find_sector_at_point(engine, x, y);
    if (!s)
      return;
    cur_sector_id = s->sector_id;
  }

  float cur_x = x, cur_y = y;
  float accum_dist = 0.0f;
  int max_portal_depth = 128; // Increased for city maps

  for (int depth = 0; depth < max_portal_depth; depth++) {
    RAY_RayHit local_hits[256];
    int num_local_hits = 0;

    collect_sector_geometry(engine, cur_sector_id, cur_x, cur_y, ray_angle,
                            accum_dist, strip_idx, local_hits, &num_local_hits,
                            256);

    if (num_local_hits == 0)
      break;

    qsort(local_hits, num_local_hits, sizeof(RAY_RayHit), ray_local_hit_sorter);

    int exit_found = 0;
    for (int i = 0; i < num_local_hits; i++) {
      if (*num_hits >= RAY_MAX_RAYHITS)
        break;

      /* Add hit to global list */
      memcpy(&hits[*num_hits], &local_hits[i], sizeof(RAY_RayHit));
      (*num_hits)++;

      /* Check for portal traversal */
      if (local_hits[i].wall && local_hits[i].wall->portal_id >= 0) {
        int portal_id = local_hits[i].wall->portal_id;
        int next_id = -1;
        for (int p = 0; p < engine->num_portals; p++) {
          if (engine->portals[p].portal_id == portal_id) {
            next_id = (engine->portals[p].sector_a == local_hits[i].sector_id)
                          ? engine->portals[p].sector_b
                          : engine->portals[p].sector_a;
            break;
          }
        }

        if (next_id >= 0) {
          accum_dist = local_hits[i].distance;
          // Nudge in correct direction (+sin)
          cur_x = local_hits[i].x + cosf(ray_angle) * 0.01f;
          cur_y = local_hits[i].y + sinf(ray_angle) * 0.01f;
          cur_sector_id = next_id;
          exit_found = 1;
          break;
        }
      }
    }

    if (!exit_found)
      break;
  }
}

/* ============================================================================
   SPRITE RAYCASTING
   ============================================================================
 */

void ray_cast_sprites(RAY_Engine *engine, float ray_angle, int strip_idx,
                      RAY_RayHit *hits, int *num_hits) {
  if (!engine || !hits || !num_hits)
    return;

  for (int i = 0; i < engine->num_sprites; i++) {
    RAY_Sprite *sprite = &engine->sprites[i];
    if (sprite->hidden || sprite->cleanup)
      continue;

    float dx = sprite->x - engine->camera.x;
    float dy = sprite->y - engine->camera.y;
    float distance = sqrtf(dx * dx + dy * dy);
    if (distance < 0.1f)
      continue;

    // Unified Y system (+sin)
    float sprite_angle = atan2f(dy, dx);
    float angle_diff = sprite_angle - ray_angle;
    while (angle_diff > M_PI)
      angle_diff -= 2 * M_PI;
    while (angle_diff < -M_PI)
      angle_diff += 2 * M_PI;

    float sprite_angular_width = atanf((sprite->w / 2.0f) / distance);
    if (fabsf(angle_diff) < sprite_angular_width) {
      if (*num_hits < RAY_MAX_RAYHITS) {
        RAY_RayHit *hit = &hits[*num_hits];
        memset(hit, 0, sizeof(RAY_RayHit));
        hit->sprite = sprite;
        hit->distance = distance;
        hit->correctDistance = distance * cosf(ray_angle - engine->camera.rot);
        hit->rayAngle = ray_angle;
        hit->strip = strip_idx;
        hit->x = sprite->x;
        hit->y = sprite->y;
        hit->sector_id = -1;
        sprite->distance = distance;
        sprite->rayhit = 1;
        (*num_hits)++;
      }
    }
  }
}

/* ============================================================================
   COLLISION DETECTION
   ============================================================================
 */

int ray_check_collision(RAY_Engine *engine, float x, float y, float z,
                        float new_x, float new_y) {
  if (!engine)
    return 0;

  RAY_Sector *current_sector = ray_find_sector_at_position(engine, x, y, z);
  if (!current_sector)
    return 1;

  /* 1. Wall intersection check (current sector + descendants) */
  // We should check all local geometry to handle building collisions properly
  RAY_RayHit local_hits[128];
  int num_hits = 0;
  float dx = new_x - x;
  float dy = new_y - y;
  float dist = sqrtf(dx * dx + dy * dy);
  float angle = atan2f(dy, dx);

  collect_sector_geometry(engine, current_sector->sector_id, x, y, angle, 0.0f,
                          -1, local_hits, &num_hits, 128);

  for (int i = 0; i < num_hits; i++) {
    if (local_hits[i].distance < dist + 1.0f &&
        !ray_wall_is_portal(local_hits[i].wall)) {
      // Check height
      RAY_Sector *s = resolve_sector(engine, local_hits[i].sector_id);
      if (s) {
        float wall_top = s->ceiling_z;
        float wall_bot = s->floor_z;
        // If player Z is within wall Z range, or wall is higher than step
        // height
        if (z < wall_top && z + 32.0f > wall_bot) {
          return 1; // Collision
        }
      }
    }
  }

  /* 2. Check destination sector floor/ceil */
  RAY_Sector *new_sector = ray_find_sector_at_position(engine, new_x, new_y, z);
  if (!new_sector)
    return 1;

  /* 3. Prevent entering pits/pools: scan child sectors of the current sector.
     If any point within a collision margin of the destination falls inside a
     pit/pool child sector, block the movement. The margin prevents the
     object from visually entering the pool before collision is detected. */
  float step_height = 2.0f;
  float col_margin = 100.0f; /* Collision margin around center point */
  for (int c = 0; c < current_sector->num_children; c++) {
    int child_id = current_sector->child_sector_ids[c];
    if (child_id < 0 || child_id >= engine->num_sectors)
      continue;
    RAY_Sector *child = &engine->sectors[child_id];
    /* Only block if child floor is a significant drop (pit/pool) */
    float drop = current_sector->floor_z - child->floor_z;
    if (drop <= step_height)
      continue;
    /* Check center and 4 cardinal points around destination */
    if (ray_point_in_polygon(new_x, new_y, child->vertices,
                             child->num_vertices) ||
        ray_point_in_polygon(new_x + col_margin, new_y, child->vertices,
                             child->num_vertices) ||
        ray_point_in_polygon(new_x - col_margin, new_y, child->vertices,
                             child->num_vertices) ||
        ray_point_in_polygon(new_x, new_y + col_margin, child->vertices,
                             child->num_vertices) ||
        ray_point_in_polygon(new_x, new_y - col_margin, child->vertices,
                             child->num_vertices)) {
      return 1; /* Block: would fall into pit/pool */
    }
  }

  return 0;
}
