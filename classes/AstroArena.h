#pragma once

struct ShipBase;

#include <vector>
#include <string>
#include <functional>

#include "AstroTypes.h"

struct AstroArena {
    struct ShipState {
        float x = 0, y = 0;
        float vx = 0, vy = 0;
        float angle = 0;        // degrees 0-360
        float targetAngle = 0;  // for smooth rotation
        int hp = ASTRO_START_HP;
        float fuel = ASTRO_START_FUEL;
        bool alive = true;
        ShipBase* ship = nullptr;

        // scan results
        float scan_dist = 0;      // 0 means nothing seen
        float scan_angle = 0;     // angle to scanned object
        bool scan_hit = false;

        // weapon cooldowns
        int phaser_cooldown = 0;
        int photon_cooldown = 0;

        // signal
        int signal = -1;

        ImU32 color; // ship color
    };

    std::vector<ShipState> ships;
    std::vector<PhotonTorpedo> torpedoes;
    std::vector<PhaserBeam> phaserBeams;
    std::vector<Particle> particles;
    std::vector<Asteroid> asteroids;
    std::vector<ShipDebrisSegment> shipDebris;
    std::vector<std::pair<float,float>> signals; // positions
    std::function<void(const std::string&)> log;

    // Rendering scale (screen pixels per world unit), set by renderer each frame
    float renderScale = 1.0f;
    // Broad-phase uniform grid (Phase 2)
    int gridCellSize = 128;
    int gridCols = 0;
    int gridRows = 0;
    std::vector<std::vector<int>> gridAsteroids; // per-cell asteroid indices
    std::vector<std::vector<int>> gridShips;     // per-cell ship indices
    void RebuildBroadphase();
    inline int CellIndex(int cx, int cy) const {
        if (gridCols <= 0 || gridRows <= 0) return -1;
        int x = ((cx % gridCols) + gridCols) % gridCols;
        int y = ((cy % gridRows) + gridRows) % gridRows;
        return y * gridCols + x;
    }
    inline void PosToCell(float x, float y, int& cx, int& cy) const {
        cx = (int)std::floor(x / (float)gridCellSize);
        cy = (int)std::floor(y / (float)gridCellSize);
    }
    void CollectNearCells(int cx, int cy, std::vector<int>& outCellIdx) const;
    // world queries & actions
    void UpdatePhysics();
    void WrapPosition(float& x, float& y);
    void Thrust(int self, float power);
    void TurnDeg(int self, int degrees);
    void FirePhaser(int self);
    void FirePhoton(int self);
    void Scan(int self);
    void Signal(int self, int value);
    void TurnToScan(int self);

    // collision detection
    bool CircleCollision(float x1, float y1, float r1, float x2, float y2, float r2);
    void HandleCollisions();
    void HandleTorpedoes();
    void KillShip(ShipState& s, const std::string& message);
    void BreakAsteroid(int asteroidIdx, float pushFromX = -1, float pushFromY = -1);

    void StartTurn();
    void SpawnAsteroids(int count);
    void SpawnAsteroidFromEdge(); // spawn a large asteroid just inside an edge moving inward
    void SpawnParticleBurst(float x, float y, int count, ImU32 baseColor, float speedScale = 1.0f, float lifeScale = 1.0f, float particleLength = PARTICLE_LENGTH);

    int edgeSpawnCooldown = 0; // turns until next edge spawn allowed
};


