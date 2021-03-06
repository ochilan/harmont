#include <shadow_pass.hpp>

#ifdef BUILD_DEFERRED_RENDERER

#include <chrono>
#include <list>
#include <random>

namespace harmont {


shadow_pass::shadow_pass(uint32_t resolution, uint32_t sample_count) : res_(resolution), sample_count_(sample_count) {
    tex_ = texture::texture_2d<float>(res_, res_, 1);
    dummy_tex_ = texture::depth_texture<float>(res_, res_, GL_DEPTH_COMPONENT32F);
    vertex_shader::ptr   vert = vertex_shader::from_file(std::string(GLSL_PREFIX)+"shadow.vert");
    fragment_shader::ptr frag = fragment_shader::from_file(std::string(GLSL_PREFIX)+"shadow.frag");
    vertex_shader::ptr vert_clear = vertex_shader::from_file(std::string(GLSL_PREFIX)+"full_quad.vert");
    fragment_shader::ptr frag_clear = fragment_shader::from_file(std::string(GLSL_PREFIX)+"clear_shadowmap.frag");
    pass_ = std::make_shared<render_pass>(vert, frag, render_pass::textures({tex_}), dummy_tex_);
    clear_pass_ = std::make_shared<render_pass_2d>(vert_clear, frag_clear, render_pass::textures({tex_}));
    disk_ = poisson_disk_(sample_count_, 1.f);
}

shadow_pass::~shadow_pass() {
}

uint32_t shadow_pass::resolution() const {
    return res_;
}

texture::ptr shadow_pass::shadow_texture() {
    return tex_;
}

texture::const_ptr shadow_pass::shadow_texture() const {
    return tex_;
}

shader_program::ptr shadow_pass::program() {
    return pass_->program();
}

shader_program::const_ptr shadow_pass::program() const {
    return pass_->program();
}

Eigen::Matrix4f shadow_pass::transform() {
    return mat_proj_ * mat_view_;
}

std::vector<float>& shadow_pass::poisson_disk() {
    return disk_;
}

const std::vector<float>& shadow_pass::poisson_disk() const {
    return disk_;
}

float shadow_pass::far() const {
    return far_;
}

void shadow_pass::render(const geometry_callback_t& render_callback, int width, int height, float vp_ratio) {
    pass_->set_uniform("shadow_view", mat_view_);
    pass_->set_uniform("shadow_proj", mat_proj_);
    pass_->set_uniform("vp_ratio", vp_ratio);
    glViewport(0, 0, res_, res_);
    glClearColor(1.0, 1.0, 1.0, 1.0);
    //glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    //glPolygonOffset(-1.1, 4.0);
    //glEnable(GL_POLYGON_OFFSET_FILL);
    clear_pass_->render([&] (shader_program::ptr) {});
    pass_->render([&] (shader_program::ptr program) { render_callback(program, SHADOW_GEOMETRY); });
    glDisable(GL_POLYGON_OFFSET_FILL);
    //glPolygonOffset(0, 0);
    //glDisable(GL_POLYGON_OFFSET_FILL);
    //glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, width, height);
    glClearColor(0.0, 0.0, 0.0, 1.0);
}

void shadow_pass::update(const bounding_box_t& bbox, const Eigen::Vector3f& light_dir) {
    const float margin = 0.01f;
    Eigen::Vector3f center = bbox.center();
    float radius = (bbox.max() - center).norm();
    Eigen::Vector3f forward = light_dir;
    Eigen::Vector3f up = Eigen::Vector3f::UnitZ();
    if (fabs(up.dot(forward)) + Eigen::NumTraits<float>::dummy_precision() > 1.f) {
        up = Eigen::Vector3f::UnitY();
    }
    up -= up.dot(forward) * forward;
    up.normalize();
    Eigen::Vector3f right = up.cross(forward);

    mat_view_.block<1,3>(0, 0) = right.transpose();
    mat_view_.block<1,3>(1, 0) = up.transpose();
    mat_view_.block<1,3>(2, 0) = forward.transpose();
    Eigen::Vector3f light_pos = center + (radius + margin) * light_dir;
    mat_view_.block<3,1>(0, 3) = mat_view_.block<3,3>(0, 0) * (-light_pos);
    mat_view_.row(3) = Eigen::RowVector4f(0.f, 0.f, 0.f, 1.f);
    mat_proj_ = ortho(-radius, radius, -radius, radius, margin, 2.f * radius + 2.f * margin);

    far_ = 2.f * radius + 2.f * margin;
}

void shadow_pass::render_(shader_program::ptr program, uint32_t num_indices) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, nullptr);
}

std::vector<float> shadow_pass::poisson_disk_(uint32_t n, float radius, uint32_t k) {
    if (n == 0) return std::vector<float>();

    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_real_distribution<float> float_dist;
    auto float_rng = std::bind(float_dist, generator);

    float r2 = radius * radius;
    Eigen::Vector2f p = gen_point_(float_rng, radius, 0.f);

    std::vector<Eigen::Vector2f> points(1, p);
    std::list<Eigen::Vector2f>   active(1, p);

    Eigen::Vector2f avg_point = p;
    uint32_t point_count = 0;
    while (points.size() < n && active.size()) {
        std::uniform_int_distribution<int> int_dist(0, active.size() - 1);
        std::list<Eigen::Vector2f>::iterator iter = active.begin();
        int random_int = int_dist(generator);
        std::advance(iter, random_int);
        const Eigen::Vector2f& s = *iter;

        bool valid = true;
        for (uint32_t j = 0; j < k; ++j) {
            Eigen::Vector2f np = gen_point_(float_rng, radius, radius, s);
            valid = true;
            for (uint32_t pt = 0; pt < points.size(); ++pt) {
                const Eigen::Vector2f& point = points[pt];
                float square_dist = (point - np).squaredNorm();
                if (square_dist < r2) {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                if (point_count > 1) avg_point *= static_cast<float>(point_count);
                avg_point += np;
                avg_point /= static_cast<float>(++point_count);
                points.push_back(np);
                active.push_back(np);
                break;
            }
        }

        if (!valid) {
            active.erase(iter);
        }
    }

    std::vector<float> result(points.size() * 2);
    for (uint32_t i = 0; i < points.size(); ++i) {
        Eigen::Vector2f local = points[i] - avg_point;
        result[2*i + 0] = local[0];
        result[2*i + 1] = local[1];
    }

    return result;
}

} // harmont

#endif // BUILD_DEFERRED_RENDERER
