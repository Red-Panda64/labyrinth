#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float first, second;
} vec2_t;

typedef struct {
    unsigned first, second;
} uvec2_t;

typedef struct {
    float *_buffer;
    uvec2_t _dims;
} texture_t;

void texture_create(texture_t *tex, uvec2_t dims);
void texture_destroy(texture_t *tex);
uvec2_t texture_size(const texture_t *self);
float texture_lookup(const texture_t *self, uvec2_t index);
float texture_sample(const texture_t *self, vec2_t pos);
void texture_set(texture_t *self, uvec2_t index, float brightness);
void texture_clear(texture_t *self);

typedef struct {
    uint8_t *_map;
    uvec2_t _dims;
} map_t;

void map_create(map_t *map, uvec2_t dims);
void map_set(map_t *self, uvec2_t pos, bool val);
bool map_check(const map_t *self, uvec2_t pos);
int map_from_string(map_t *map, const char *s);

typedef struct {
    float _fovx, _fovy;
} camera_t;

void camera_from_fovy(camera_t *camera, uvec2_t dims, float fovy);
void camera_from_fovx(camera_t *camera, uvec2_t dims, float fovx);
float column_height(const camera_t *camera, float dist);

struct HitResult {
    bool hit;
    vec2_t pos;
    float u;
};

typedef struct {
    texture_t _fb;
    map_t _map;
    camera_t _camera;
} renderer_t;

void renderer_create(renderer_t *renderer, map_t *map, uvec2_t framebuffer_size);
void renderer_draw_from(renderer_t *self, vec2_t pos, vec2_t look_dir);
HitResult renderer_ray(const renderer_t *self, vec2_t pos, vec2_t dir);
void renderer_show_fb(const renderer_t *self);
