#pragma once
#include <memory>

using Vec2 = std::pair<float, float>; 
using UVec2 = std::pair<unsigned, unsigned>; 

class Framebuffer final {
    std::unique_ptr<float[]> _buffer;
    UVec2 _dims;
public:
    Framebuffer(UVec2 dims);

    float *data();
    const float *data() const;

    UVec2 size() const;
    float lookup(const UVec2 &index) const;
    void set(const UVec2 &index, float brightness);
    void clear();
};

class Map final {
private:
    std::unique_ptr<uint8_t[]> _map;
    UVec2 _dims;

public:
    Map(UVec2 dims);

    void set(UVec2 pos, bool val);
    bool check(UVec2 pos) const;

    static Map from_string(std::string_view s);
};

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

    static Camera from_fovy(UVec2 dims, float fovy);
    static Camera from_fovx(UVec2 dims, float fovx);

    float fovy() const;
    float fovx() const;

    float column_height(float dist);
};

struct HitResult {
    bool hit;
    Vec2 pos;
};

class Renderer final {
private:
    Framebuffer _fb;
    Map _map;
    Camera _camera;

public:
    Renderer(Map &&map, UVec2 framebuffer_size);

    void draw_from(Vec2 pos, Vec2 look_dir);
    const Map &map() const;

private:
    HitResult ray(Vec2 pos, Vec2 dir) const;
    void render_fb();
};
