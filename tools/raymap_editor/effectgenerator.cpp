#include "effectgenerator.h"
#include <QPainter>
#include <QtMath>
#include <QDateTime>

EffectGenerator::EffectGenerator()
    : m_type(EFFECT_EXPLOSION)
{
    m_random.seed(QDateTime::currentMSecsSinceEpoch());
}

void EffectGenerator::setType(EffectType type)
{
    m_type = type;
}

void EffectGenerator::setParams(const EffectParams &params)
{
    m_params = params;
    if (m_params.seed != 0) {
        m_random.seed(m_params.seed);
    }
}

QVector<QImage> EffectGenerator::generateAnimation()
{
    QVector<QImage> frames;
    initializeParticles();
    
    for (int i = 0; i < m_params.frames; i++) {
        float time = (float)i / m_params.frames;
        float deltaTime = 1.0f / m_params.frames;
        frames.append(renderFrame(i, time));
    }
    
    return frames;
}

QImage EffectGenerator::renderFrame(int frameIndex, float time)
{
    QImage frame(m_params.imageSize, m_params.imageSize, QImage::Format_ARGB32);
    frame.fill(Qt::transparent);
    
    QPainter painter(&frame);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    float deltaTime = 1.0f / m_params.frames;
    updateParticles(time, deltaTime);
    
    QPointF center(m_params.imageSize / 2.0f, m_params.imageSize / 2.0f);
    
    // Draw particles
    for (const Particle &p : m_particles) {
        if (p.life > 0 && p.alpha > 0) {
            QColor col = p.color;
            col.setAlphaF(p.alpha);
            painter.setBrush(col);
            painter.setPen(Qt::NoPen);
            
            QPointF pos = center + p.position;
            painter.drawEllipse(pos, p.size, p.size);
        }
    }
    
    return frame;
}

void EffectGenerator::initializeParticles()
{
    m_particles.clear();
    
    switch (m_type) {
        case EFFECT_EXPLOSION: initExplosion(); break;
        case EFFECT_SMOKE: initSmoke(); break;
        case EFFECT_FIRE: initFire(); break;
        case EFFECT_PARTICLES: initParticles(); break;
        case EFFECT_WATER: initWater(); break;
        case EFFECT_ENERGY: initEnergy(); break;
        case EFFECT_IMPACT: initImpact(); break;
    }
}

void EffectGenerator::updateParticles(float time, float deltaTime)
{
    switch (m_type) {
        case EFFECT_EXPLOSION: updateExplosion(time, deltaTime); break;
        case EFFECT_SMOKE: updateSmoke(time, deltaTime); break;
        case EFFECT_FIRE: updateFire(time, deltaTime); break;
        case EFFECT_PARTICLES: updateParticlesGeneric(time, deltaTime); break;
        case EFFECT_WATER: updateWater(time, deltaTime); break;
        case EFFECT_ENERGY: updateEnergy(time, deltaTime); break;
        case EFFECT_IMPACT: updateImpact(time, deltaTime); break;
    }
}

// EXPLOSION
void EffectGenerator::initExplosion()
{
    int count = m_params.particleCount;
    for (int i = 0; i < count; i++) {
        Particle p;
        float angle = randomFloat(0, 2 * M_PI);
        float speed = randomFloat(0.5f, 1.0f) * m_params.speed;
        
        p.position = QPointF(0, 0);
        p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
        p.size = randomFloat(2, 6) * (m_params.intensity / 50.0f);
        p.life = 1.0f;
        p.alpha = 1.0f;
        p.color = m_params.color1;
        
        m_particles.append(p);
    }
    
    // Add core particles
    for (int i = 0; i < 20; i++) {
        Particle p;
        p.position = QPointF(randomFloat(-5, 5), randomFloat(-5, 5));
        p.velocity = QPointF(0, 0);
        p.size = randomFloat(10, 20);
        p.life = 1.0f;
        p.alpha = 1.0f;
        p.color = QColor(255, 255, 200);
        m_particles.append(p);
    }
}

void EffectGenerator::updateExplosion(float time, float dt)
{
    for (Particle &p : m_particles) {
        p.position += p.velocity * m_params.radius * 0.1f;
        p.life = 1.0f - time;
        p.alpha = p.life;
        
        // Fade color from bright to dark
        float t = time;
        p.color = lerpColor(m_params.color1, m_params.color2, t);
        
        // Shrink core particles
        if (p.velocity.manhattanLength() < 0.1f) {
            p.size *= 0.95f;
        }
    }
}

// SMOKE
void EffectGenerator::initSmoke()
{
    int count = m_params.particleCount;
    for (int i = 0; i < count; i++) {
        Particle p;
        float offsetX = randomFloat(-20, 20);
        float offsetY = randomFloat(0, 10);
        
        p.position = QPointF(offsetX, offsetY);
        p.velocity = QPointF(randomFloat(-0.5f, 0.5f), -randomFloat(1, 3));
        p.size = randomFloat(5, 15);
        p.life = randomFloat(0.5f, 1.0f);
        p.alpha = randomFloat(0.3f, 0.7f);
        p.color = m_params.color1;
        
        m_particles.append(p);
    }
}

void EffectGenerator::updateSmoke(float time, float dt)
{
    for (Particle &p : m_particles) {
        // Rise and expand
        p.position += p.velocity * m_params.speed * 0.5f;
        p.size += 0.2f * m_params.dispersion;
        
        // Turbulence
        float noise = perlinNoise(p.position.x() * 0.1f, time * 5);
        p.position.rx() += noise * m_params.turbulence * 2;
        
        // Fade
        p.alpha -= m_params.fadeRate;
        p.life -= dt * 2;
    }
}

// FIRE
void EffectGenerator::initFire()
{
    int count = m_params.particleCount;
    for (int i = 0; i < count; i++) {
        Particle p;
        float offsetX = randomFloat(-15, 15);
        
        p.position = QPointF(offsetX, randomFloat(0, 20));
        p.velocity = QPointF(randomFloat(-0.3f, 0.3f), -randomFloat(2, 5));
        p.size = randomFloat(3, 8);
        p.life = randomFloat(0.5f, 1.0f);
        p.alpha = randomFloat(0.6f, 1.0f);
        p.color = QColor(255, 200, 0); // Yellow
        
        m_particles.append(p);
    }
}

void EffectGenerator::updateFire(float time, float dt)
{
    for (Particle &p : m_particles) {
        p.position += p.velocity * m_params.speed * 0.3f;
        
        // Flicker
        float flicker = perlinNoise(p.position.x() * 0.2f, time * 10);
        p.position.rx() += flicker * 2;
        
        // Color gradient: yellow -> orange -> red
        float lifeRatio = 1.0f - p.life;
        if (lifeRatio < 0.5f) {
            p.color = lerpColor(QColor(255, 255, 100), QColor(255, 150, 0), lifeRatio * 2);
        } else {
            p.color = lerpColor(QColor(255, 150, 0), QColor(200, 50, 0), (lifeRatio - 0.5f) * 2);
        }
        
        p.alpha -= 0.02f;
        p.life -= dt * 3;
        p.size *= 0.98f;
    }
    
    // Add sparks
    if (m_params.sparks && randomFloat(0, 1) > 0.7f) {
        Particle spark;
        spark.position = QPointF(randomFloat(-10, 10), 0);
        spark.velocity = QPointF(randomFloat(-2, 2), -randomFloat(5, 10));
        spark.size = randomFloat(1, 2);
        spark.life = 0.3f;
        spark.alpha = 1.0f;
        spark.color = QColor(255, 255, 150);
        m_particles.append(spark);
    }
}

// PARTICLES (Generic)
void EffectGenerator::initParticles()
{
    int count = m_params.particleCount;
    for (int i = 0; i < count; i++) {
        Particle p;
        float angle = randomFloat(0, 2 * M_PI);
        float speed = randomFloat(0.5f, 2.0f);
        
        p.position = QPointF(0, 0);
        p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
        p.size = randomFloat(2, 5);
        p.life = 1.0f;
        p.alpha = 1.0f;
        p.color = m_params.color1;
        
        m_particles.append(p);
    }
}

void EffectGenerator::updateParticlesGeneric(float time, float dt)
{
    for (Particle &p : m_particles) {
        p.position += p.velocity * m_params.speed * 0.5f;
        p.velocity.ry() += m_params.gravity * 0.1f; // Gravity
        p.life -= dt;
        p.alpha = p.life;
        
        // Color lerp
        p.color = lerpColor(m_params.color1, m_params.color2, 1.0f - p.life);
    }
}

// WATER
void EffectGenerator::initWater()
{
    int count = m_params.particleCount;
    for (int i = 0; i < count; i++) {
        Particle p;
        float angle = randomFloat(-M_PI/3, -2*M_PI/3); // Upward spray
        float speed = randomFloat(3, 8);
        
        p.position = QPointF(randomFloat(-5, 5), 0);
        p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
        p.size = randomFloat(2, 4);
        p.life = 1.0f;
        p.alpha = 0.8f;
        p.color = QColor(100, 150, 255, 200);
        
        m_particles.append(p);
    }
}

void EffectGenerator::updateWater(float time, float dt)
{
    for (Particle &p : m_particles) {
        p.position += p.velocity * m_params.speed * 0.3f;
        p.velocity.ry() += 0.3f; // Gravity
        p.life -= dt * 2;
        p.alpha = p.life * 0.8f;
    }
}

// ENERGY
void EffectGenerator::initEnergy()
{
    int count = m_params.particleCount;
    for (int i = 0; i < count; i++) {
        Particle p;
        float angle = randomFloat(0, 2 * M_PI);
        float radius = randomFloat(0, m_params.radius);
        
        p.position = QPointF(cos(angle) * radius, sin(angle) * radius);
        p.velocity = QPointF(0, 0);
        p.size = randomFloat(2, 6);
        p.life = 1.0f;
        p.alpha = randomFloat(0.5f, 1.0f);
        p.color = m_params.color1;
        p.rotation = angle;
        p.angularVel = randomFloat(-0.1f, 0.1f);
        
        m_particles.append(p);
    }
}

void EffectGenerator::updateEnergy(float time, float dt)
{
    for (Particle &p : m_particles) {
        // Spiral motion
        p.rotation += p.angularVel;
        float radius = m_params.radius * (1.0f - time * 0.5f);
        p.position = QPointF(cos(p.rotation) * radius, sin(p.rotation) * radius);
        
        // Pulsate
        float pulse = sin(time * 10 + p.rotation) * 0.3f + 0.7f;
        p.alpha = pulse;
        p.size = 3 + pulse * 3;
        
        // Color shift
        p.color = lerpColor(m_params.color1, m_params.color2, sin(time * 5) * 0.5f + 0.5f);
    }
}

// IMPACT
void EffectGenerator::initImpact()
{
    int count = m_params.particleCount;
    
    // Dust cloud
    for (int i = 0; i < count / 2; i++) {
        Particle p;
        float angle = randomFloat(-M_PI/4, -3*M_PI/4);
        float speed = randomFloat(1, 4);
        
        p.position = QPointF(randomFloat(-10, 10), 0);
        p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
        p.size = randomFloat(3, 8);
        p.life = 1.0f;
        p.alpha = 0.6f;
        p.color = QColor(150, 130, 100);
        
        m_particles.append(p);
    }
    
    // Debris
    if (m_params.debris) {
        for (int i = 0; i < count / 2; i++) {
            Particle p;
            float angle = randomFloat(0, 2 * M_PI);
            float speed = randomFloat(2, 6);
            
            p.position = QPointF(0, 0);
            p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
            p.size = randomFloat(1, 3);
            p.life = 1.0f;
            p.alpha = 1.0f;
            p.color = QColor(80, 70, 60);
            
            m_particles.append(p);
        }
    }
}

void EffectGenerator::updateImpact(float time, float dt)
{
    for (Particle &p : m_particles) {
        p.position += p.velocity * m_params.speed * 0.4f;
        
        // Gravity for debris
        if (p.size < 4) {
            p.velocity.ry() += 0.2f;
        }
        
        // Expand dust
        if (p.size > 4) {
            p.size += 0.3f;
        }
        
        p.alpha -= 0.015f;
        p.life -= dt * 1.5f;
    }
}

// UTILITIES
float EffectGenerator::perlinNoise(float x, float y)
{
    // Simple noise approximation
    int xi = (int)x;
    int yi = (int)y;
    float xf = x - xi;
    float yf = y - yi;
    
    float n00 = sin(xi * 12.9898f + yi * 78.233f) * 43758.5453f;
    float n10 = sin((xi+1) * 12.9898f + yi * 78.233f) * 43758.5453f;
    float n01 = sin(xi * 12.9898f + (yi+1) * 78.233f) * 43758.5453f;
    float n11 = sin((xi+1) * 12.9898f + (yi+1) * 78.233f) * 43758.5453f;
    
    n00 = n00 - floor(n00);
    n10 = n10 - floor(n10);
    n01 = n01 - floor(n01);
    n11 = n11 - floor(n11);
    
    float nx0 = n00 * (1 - xf) + n10 * xf;
    float nx1 = n01 * (1 - xf) + n11 * xf;
    
    return nx0 * (1 - yf) + nx1 * yf;
}

QColor EffectGenerator::lerpColor(const QColor &a, const QColor &b, float t)
{
    t = qBound(0.0f, t, 1.0f);
    return QColor(
        a.red() + (b.red() - a.red()) * t,
        a.green() + (b.green() - a.green()) * t,
        a.blue() + (b.blue() - a.blue()) * t,
        a.alpha() + (b.alpha() - a.alpha()) * t
    );
}

float EffectGenerator::randomFloat(float min, float max)
{
    return min + (max - min) * (m_random.generateDouble());
}
