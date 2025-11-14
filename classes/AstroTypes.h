#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include "../imgui/imgui.h"
#include "cute_c2.h"

// ===== Arena config =====
static constexpr float ASTROBOTS_W = 2048.0f;
static constexpr float ASTROBOTS_H = 2048.0f;
static constexpr int ASTRO_MAX_TURNS = 10000;
static constexpr int ASTRO_START_HP = 10;
static constexpr float ASTRO_START_FUEL = 100.0f;
static constexpr int ASTRO_MAX_SCRIPT_COST = 30;

// Ship physics
static constexpr float THRUST_POWER = 0.25f;         
static constexpr float THRUST_FUEL_COST = 0.05f;
static constexpr float MAX_VELOCITY = 4.0f;          
static constexpr float ROTATION_SPEED = 3.0f;        // degrees per turn, slightly slower
static constexpr float DRAG = 0.98f;                 // velocity damping

// Weapons
static constexpr float PHASER_RANGE = 500.0f;
static constexpr int PHASER_DAMAGE = 1;
static constexpr int PHASER_COOLDOWN = 30;
static constexpr float PHOTON_SPEED = 20.0f;          
static constexpr int PHOTON_DAMAGE = 3;
static constexpr int PHOTON_COOLDOWN = 60;           
static constexpr int PHOTON_LIFETIME = 100;           // turns
// Photon visual
static constexpr int PHOTON_SPOKES = 12;
static constexpr float PHOTON_BASE_SIZE = 4.0f;
static constexpr float PHOTON_PULSE_AMPLITUDE = 7.0f;
static constexpr float PHOTON_SPIN_SPEED = 0.11f;     // radians per frame
static constexpr float PHOTON_PULSE_SPEED = 0.20f;    // cycles per frame

// Scan
static constexpr float ASTRO_SCAN_RANGE = 600.0f;

// Asteroids
static constexpr int NUM_INITIAL_ASTEROIDS = 8;
static constexpr float LARGE_ASTEROID_SIZE = 75.0f;   
static constexpr float MEDIUM_ASTEROID_SIZE = 37.5f;  
static constexpr float SMALL_ASTEROID_SIZE = 18.0f;    
static constexpr float ASTEROID_MAX_SPEED = 2.0f;
static constexpr int LARGE_ASTEROID_HP = 3;
static constexpr int MEDIUM_ASTEROID_HP = 2;
static constexpr int SMALL_ASTEROID_HP = 1;
static constexpr float FUEL_PICKUP_AMOUNT = 30.0f;
static constexpr float FUEL_HIT_REWARD = 5.0f;        // fuel restored on any weapon hit on asteroids

// Particles
static constexpr int PARTICLE_DEFAULT_LIFETIME = 45;   // frames
static constexpr float PARTICLE_MIN_SPEED = 1.0f;
static constexpr float PARTICLE_MAX_SPEED = 6.0f;
static constexpr float PARTICLE_DRAG = 0.96f;
static constexpr float PARTICLE_LENGTH = 28.0f;        // line length scaling
static constexpr float PARTICLE_WRAP = 1;              // wrap particles? (1=true)

// Ship debris (Asteroids-style breakup)
static constexpr int SHIP_DEBRIS_LIFETIME = 60;        // frames
static constexpr float SHIP_DEBRIS_DRAG = 0.97f;
static constexpr int SHIP_DEBRIS_COUNT_PER_EDGE = 2;   // segments per triangle edge
static constexpr float SHIP_DRAW_SIZE = 55.0f;         // matches ship triangle size used in rendering

// ===== Opcodes / DSL =====
enum AstroOpCode {
    // actions
    ASTRO_OP_WAIT, ASTRO_OP_THRUST, ASTRO_OP_TURN_DEG, ASTRO_OP_FIRE_PHASER,
    ASTRO_OP_FIRE_PHOTON, ASTRO_OP_SCAN, ASTRO_OP_SIGNAL, ASTRO_OP_TURN_TO_SCAN,
    // conditions
    ASTRO_OP_IF_SEEN, ASTRO_OP_IF_SCAN_LE, ASTRO_OP_IF_DAMAGED, ASTRO_OP_IF_HP_LE,
    ASTRO_OP_IF_FUEL_LE, ASTRO_OP_IF_CAN_FIRE_PHASER, ASTRO_OP_IF_CAN_FIRE_PHOTON,
    // flow control
    ASTRO_OP_JUMP_IF_FALSE, ASTRO_OP_END
};

// energy costs (for compile-time budget)
enum AstroActionCost {
    ASTRO_COST_WAIT=0, ASTRO_COST_THRUST=2, ASTRO_COST_TURN=1,
    ASTRO_COST_PHASER=3, ASTRO_COST_PHOTON=4, ASTRO_COST_SCAN=1, ASTRO_COST_SIGNAL=1
};

// Forward declarations
struct AstroArena;

// ===== Photon Torpedo =====
struct PhotonTorpedo {
    float x, y;
    float vx, vy;
    int lifetime;
    int damage;
    int owner; // ship id that fired it
    bool alive;
    float anim = 0.0f; // animation time for spin/pulse
    float prevX = 0.0f, prevY = 0.0f; // previous position for swept collision
};

// ===== Phaser Beam (visual effect) =====
struct PhaserBeam {
    float x1, y1;  // start point
    float x2, y2;  // end point
    int lifetime;  // frames to display
    ImU32 color;
    bool alive;
};

// ===== Vector Particle =====
struct Particle {
    float x, y;
    float vx, vy;
    float length;    // visual line length scale
    int lifetime;    // frames remaining
    int startLifetime;
    ImU32 color;
    bool alive;
};

// ===== Ship Debris Segment =====
struct ShipDebrisSegment {
    float x1, y1;     // endpoint 1 (world space)
    float x2, y2;     // endpoint 2 (world space)
    float vx, vy;     // drift velocity (applied to both endpoints)
    float angVel;     // optional spin (radians/frame) about the segment midpoint
    int lifetime;     // frames remaining
    int startLifetime;
    ImU32 color;      // base color (used for core; glow computed at draw)
    bool alive;
};

// ===== Asteroid =====
struct Asteroid {
    float x, y;
    float vx, vy;
    float size;
    int hp;
    bool alive;
    std::vector<ImVec2> shape; // polygon vertices (relative to center)
    // cute_c2 cached convex polygon (local space)
    c2Poly poly;
    bool hasPoly = false;

    void GenerateShape(int sides, float radius);
};


