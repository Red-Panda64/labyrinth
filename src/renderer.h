#pragma once
#include <memory>
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

class Camera final {
private:
    float _fovx;
    float _fovy;

    Camera(float fovx, float fovy);

    static constexpr float REAL_COLUMN_HEIGHT = 1.0f;
public:
    Camera(const Camera&) = default;
    Camera(Camera&&) noexcept = default;
    Camera &operator=(const Camera&) = default;
    Camera &operator=(Camera&&) noexcept = default;

    static Camera from_fovy(uvec2_t dims, float fovy);
    static Camera from_fovx(uvec2_t dims, float fovx);

    float fovy() const;
    float fovx() const;

    float column_height(float dist);
};

struct HitResult {
    bool hit;
    vec2_t pos;
    float u;
};

class Renderer final {
private:
    texture_t _fb;
    map_t _map;
    Camera _camera;

public:
    Renderer(map_t *map, uvec2_t framebuffer_size);

    void draw_from(vec2_t pos, vec2_t look_dir);
    const map_t &map() const;

private:
    HitResult ray(vec2_t pos, vec2_t dir) const;
    void render_fb();
};
