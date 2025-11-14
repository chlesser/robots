#include "../imgui/imgui.h"
#include <iostream>
#include "AstroTypes.h"
#include "AstroArena.h"
#include "AstroBots.h"
#include <random>
#include <algorithm>
#include <cmath> 

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// RNG for arena systems
static std::random_device rd_arena;
static std::mt19937 rng(rd_arena());

// ===== Helper functions (arena-local) =====
static float NormalizeAngle(float angle) {
    while (angle < 0) angle += 360.0f;
    while (angle >= 360.0f) angle -= 360.0f;
    return angle;
}
static float AngleDifference(float from, float to) {
    float diff = to - from;
    while (diff < -180.0f) diff += 360.0f;
    while (diff > 180.0f) diff -= 360.0f;
    return diff;
}
static float Distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx*dx + dy*dy);
}
static float AngleTo(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::atan2(dy, dx) * 180.0f / M_PI;
}

// ===== cute_c2 helpers for ship/torpedo shapes =====
static c2Capsule MakeShipCapsule(const AstroArena::ShipState& s) {
    const float halfLen = 15.0f;
    const float radius = 7.5f;
    float ang = s.angle * (float)M_PI / 180.0f;
    float dx = std::cos(ang), dy = std::sin(ang);
    c2Capsule cap;
    cap.a = c2V(s.x - dx * halfLen, s.y - dy * halfLen);
    cap.b = c2V(s.x + dx * halfLen, s.y + dy * halfLen);
    cap.r = radius;
    return cap;
}

static c2Circle MakeShipCircle(const AstroArena::ShipState& s) {
    c2Circle c;
    c.p = c2V(s.x, s.y);
    c.r = 15.0f;
    return c;
}

static c2Circle MakeTorpedoCircle(const PhotonTorpedo& t) {
    c2Circle c;
    c.p = c2V(t.x, t.y);
    c.r = 5.0f;
    return c;
}

static void BuildWrapTransforms(float x, float y, std::array<c2x, 9>& out_tr, int& out_count) {
    out_count = 0;
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            c2x tr = c2xIdentity();
            tr.p = c2V(x + ox * ASTROBOTS_W, y + oy * ASTROBOTS_H);
            out_tr[out_count++] = tr;
        }
    }
}

// ===== Broad-phase uniform grid =====
void AstroArena::RebuildBroadphase() {
    gridCols = (int)std::ceil(ASTROBOTS_W / (float)gridCellSize);
    gridRows = (int)std::ceil(ASTROBOTS_H / (float)gridCellSize);
    gridAsteroids.assign(gridCols * gridRows, {});
    gridShips.assign(gridCols * gridRows, {});
    // Bin asteroids
    for (size_t i = 0; i < asteroids.size(); ++i) {
        if (!asteroids[i].alive) continue;
        int cx, cy; PosToCell(asteroids[i].x, asteroids[i].y, cx, cy);
        int idx = CellIndex(cx, cy);
        if (idx >= 0) gridAsteroids[idx].push_back((int)i);
    }
    // Bin ships
    for (size_t i = 0; i < ships.size(); ++i) {
        if (!ships[i].alive) continue;
        int cx, cy; PosToCell(ships[i].x, ships[i].y, cx, cy);
        int idx = CellIndex(cx, cy);
        if (idx >= 0) gridShips[idx].push_back((int)i);
    }
}

void AstroArena::CollectNearCells(int cx, int cy, std::vector<int>& outCellIdx) const {
    outCellIdx.clear();
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int idx = CellIndex(cx + dx, cy + dy);
            if (idx >= 0) outCellIdx.push_back(idx);
        }
    }
}

// Collision helpers (legacy) removed in favor of cute_c2

// ===== Asteroid implementation =====
void Asteroid::GenerateShape(int sides, float radius) {
    shape.clear();
    std::uniform_real_distribution<float> radiusDist(radius * 0.7f, radius * 1.3f);
    for (int i = 0; i < sides; ++i) {
        float angle = (float)i / sides * 2.0f * M_PI;
        float r = radiusDist(rng);
        shape.push_back(ImVec2(std::cos(angle) * r, std::sin(angle) * r));
    }
    // Build cute_c2 convex poly (local space)
    int n = (int)shape.size();
    if (n > C2_MAX_POLYGON_VERTS) n = C2_MAX_POLYGON_VERTS;
    poly.count = n;
    for (int i = 0; i < n; ++i) {
        poly.verts[i] = c2V(shape[i].x, shape[i].y);
    }
    c2MakePoly(&poly);
    hasPoly = true;
}

// ===== Arena mechanics =====
void AstroArena::WrapPosition(float& x, float& y) {
    x = std::fmod(x, ASTROBOTS_W);
    if (x < 0) x += ASTROBOTS_W;
    y = std::fmod(y, ASTROBOTS_H);
    if (y < 0) y += ASTROBOTS_H;
    if (x >= ASTROBOTS_W) x = 0;
    if (y >= ASTROBOTS_H) y = 0;
}

void AstroArena::UpdatePhysics() {
    for (auto& s : ships) {
        if (!s.alive) continue;
        float angleDiff = AngleDifference(s.angle, s.targetAngle);
        if (std::abs(angleDiff) > ROTATION_SPEED) {
            s.angle += (angleDiff > 0 ? ROTATION_SPEED : -ROTATION_SPEED);
        } else {
            s.angle = s.targetAngle;
        }
        s.angle = NormalizeAngle(s.angle);
        s.x += s.vx;
        s.y += s.vy;
        WrapPosition(s.x, s.y);
        s.vx *= DRAG;
        s.vy *= DRAG;
        const float MIN_VELOCITY = 0.001f;
        if (std::abs(s.vx) < MIN_VELOCITY) s.vx = 0;
        if (std::abs(s.vy) < MIN_VELOCITY) s.vy = 0;
    }
    for (auto& a : asteroids) {
        if (!a.alive) continue;
        a.x += a.vx;
        a.y += a.vy;
        WrapPosition(a.x, a.y);
    }
    for (auto& t : torpedoes) {
        if (!t.alive) continue;
        t.prevX = t.x;
        t.prevY = t.y;
        t.x += t.vx;
        t.y += t.vy;
        t.anim += 1.0f;
        t.lifetime--;
        if (t.lifetime <= 0) t.alive = false;
    }
    for (auto& beam : phaserBeams) {
        if (!beam.alive) continue;
        beam.lifetime--;
        if (beam.lifetime <= 0) beam.alive = false;
    }
    for (auto& p : particles) {
        if (!p.alive) continue;
        p.x += p.vx;
        p.y += p.vy;
        if (PARTICLE_WRAP) {
            WrapPosition(p.x, p.y);
        }
        p.vx *= PARTICLE_DRAG;
        p.vy *= PARTICLE_DRAG;
        p.lifetime--;
        if (p.lifetime <= 0) p.alive = false;
    }
    // Update ship debris segments (no wrapping; let them drift off-screen)
    for (auto& d : shipDebris) {
        if (!d.alive) continue;
        // Advance by drift velocity
        d.x1 += d.vx; d.y1 += d.vy;
        d.x2 += d.vx; d.y2 += d.vy;
        // Optional spin about midpoint
        if (std::abs(d.angVel) > 1e-6f) {
            float mx = (d.x1 + d.x2) * 0.5f;
            float my = (d.y1 + d.y2) * 0.5f;
            float c = std::cos(d.angVel);
            float s = std::sin(d.angVel);
            float rx = d.x1 - mx, ry = d.y1 - my;
            float nx = rx * c - ry * s;
            float ny = rx * s + ry * c;
            d.x1 = mx + nx; d.y1 = my + ny;
            rx = d.x2 - mx; ry = d.y2 - my;
            nx = rx * c - ry * s;
            ny = rx * s + ry * c;
            d.x2 = mx + nx; d.y2 = my + ny;
        }
        // Drag
        d.vx *= SHIP_DEBRIS_DRAG;
        d.vy *= SHIP_DEBRIS_DRAG;
        // Lifetime
        d.lifetime--;
        if (d.lifetime <= 0) d.alive = false;
    }
    // Cleanup expired debris
    shipDebris.erase(
        std::remove_if(shipDebris.begin(), shipDebris.end(),
            [](const ShipDebrisSegment& d){ return !d.alive; }),
        shipDebris.end()
    );
}

void AstroArena::Thrust(int self, float power) {
    auto& s = ships[self];
    if (!s.alive) return;
    float fuelCost = power * THRUST_FUEL_COST;
    float effectivePower = power;
    if (s.fuel >= fuelCost) {
        s.fuel -= fuelCost;
    } else {
        if (s.fuel <= 0.0f) {
            effectivePower = power * 0.25f;
        } else {
            float fuelRatio = s.fuel / fuelCost; // 0..1
            effectivePower = power * (fuelRatio + (1.0f - fuelRatio) * 0.25f);
            s.fuel = 0.0f;
        }
    }
    float angleRad = s.angle * M_PI / 180.0f;
    float thrustX = std::cos(angleRad) * effectivePower * THRUST_POWER;
    float thrustY = std::sin(angleRad) * effectivePower * THRUST_POWER;
    s.vx += thrustX;
    s.vy += thrustY;
    float speed = std::sqrt(s.vx * s.vx + s.vy * s.vy);
    if (speed > MAX_VELOCITY) {
        s.vx = (s.vx / speed) * MAX_VELOCITY;
        s.vy = (s.vy / speed) * MAX_VELOCITY;
    }
}

void AstroArena::TurnDeg(int self, int degrees) {
    auto& s = ships[self];
    if (!s.alive) return;
    s.targetAngle = NormalizeAngle((float)degrees);
}

void AstroArena::FirePhaser(int self) {
    auto& s = ships[self];
    if (!s.alive || s.phaser_cooldown > 0) return;
    s.phaser_cooldown = PHASER_COOLDOWN;
    float angleRad = s.angle * M_PI / 180.0f;
    float dirX = std::cos(angleRad);
    float dirY = std::sin(angleRad);
    float closestDist = PHASER_RANGE;
    int hitShip = -1;
    int hitAsteroid = -1;
    float hitX = s.x + dirX * PHASER_RANGE;
    float hitY = s.y + dirY * PHASER_RANGE;
    c2Ray ray; ray.p = c2V(s.x, s.y); ray.d = c2V(dirX, dirY); ray.t = PHASER_RANGE;
    c2Raycast rc;
    // Ships
    for (size_t i = 0; i < ships.size(); ++i) {
        if (i == (size_t)self || !ships[i].alive) continue;
        c2Capsule cap = MakeShipCapsule(ships[i]);
        for (int oy = -1; oy <= 1; ++oy) {
            for (int ox = -1; ox <= 1; ++ox) {
                c2Capsule wcap = cap;
                wcap.a = c2Add(wcap.a, c2V(ox * ASTROBOTS_W, oy * ASTROBOTS_H));
                wcap.b = c2Add(wcap.b, c2V(ox * ASTROBOTS_W, oy * ASTROBOTS_H));
                c2Raycast out;
                if (c2RaytoCapsule(ray, wcap, &out)) {
                    if (out.t < closestDist) {
                        closestDist = out.t;
                        hitShip = (int)i;
                        hitAsteroid = -1;
                        c2v hp = c2Impact(ray, out.t);
                        hitX = hp.x; hitY = hp.y;
                    }
                }
            }
        }
    }
    // Asteroids
    for (size_t i = 0; i < asteroids.size(); ++i) {
        if (!asteroids[i].alive || !asteroids[i].hasPoly) continue;
        std::array<c2x, 9> tr;
        int trCount = 0;
        BuildWrapTransforms(asteroids[i].x, asteroids[i].y, tr, trCount);
        for (int ti = 0; ti < trCount; ++ti) {
            c2Raycast out;
            if (c2RaytoPoly(ray, &asteroids[i].poly, &tr[ti], &out)) {
                if (out.t < closestDist) {
                    closestDist = out.t;
                    hitAsteroid = (int)i;
                    hitShip = -1;
                    c2v hp = c2Impact(ray, out.t);
                    hitX = hp.x; hitY = hp.y;
                }
            }
        }
    }
    PhaserBeam beam;
    beam.x1 = s.x; beam.y1 = s.y;
    beam.x2 = hitX; beam.y2 = hitY;
    beam.lifetime = 3;
    beam.color = IM_COL32(255, 100, 100, 255);
    beam.alive = true;
    phaserBeams.push_back(beam);
    if (hitShip >= 0) {
        ships[hitShip].hp -= PHASER_DAMAGE;
        SpawnParticleBurst(hitX, hitY, 28, IM_COL32(255, 160, 120, 255), 0.8f, 0.7f);
        if (log) {
            std::string attacker = s.ship ? s.ship->name : "Ship";
            std::string target = ships[hitShip].ship ? ships[hitShip].ship->name : "Ship";
            log(attacker + " hits " + target + " with phaser for " + std::to_string(PHASER_DAMAGE) + " damage!");
        }
        if (ships[hitShip].hp <= 0) {
            std::string target = ships[hitShip].ship ? ships[hitShip].ship->name : "Ship";
            KillShip(ships[hitShip], target + " is destroyed!");
        }
    } else if (hitAsteroid >= 0) {
        SpawnParticleBurst(hitX, hitY, 36, IM_COL32(255, 120, 120, 255), 0.9f, 0.8f);
        BreakAsteroid(hitAsteroid, s.x, s.y);
        s.fuel += FUEL_HIT_REWARD;
        if (s.fuel > ASTRO_START_FUEL) s.fuel = ASTRO_START_FUEL;
    } else if (log) {
        std::string attacker = s.ship ? s.ship->name : "Ship";
        log(attacker + " fires phaser and misses.");
    }
}

void AstroArena::FirePhoton(int self) {
    auto& s = ships[self];
    if (!s.alive || s.photon_cooldown > 0) return;
    s.photon_cooldown = PHOTON_COOLDOWN;
    PhotonTorpedo t;
    t.x = s.x;
    t.y = s.y;
    t.prevX = t.x;
    t.prevY = t.y;
    float angleRad = s.angle * M_PI / 180.0f;
    t.vx = s.vx + std::cos(angleRad) * PHOTON_SPEED;
    t.vy = s.vy + std::sin(angleRad) * PHOTON_SPEED;
    t.lifetime = PHOTON_LIFETIME;
    t.damage = PHOTON_DAMAGE;
    t.owner = self;
    t.alive = true;
    std::uniform_real_distribution<float> phaseDist(0.0f, 2.0f * (float)M_PI);
    t.anim = phaseDist(rng);
    torpedoes.push_back(t);
    if (log) {
        std::string attacker = s.ship ? s.ship->name : "Ship";
        log(attacker + " fires photon torpedo!");
    }
}

void AstroArena::Scan(int self) {
    auto& s = ships[self];
    if (!s.alive) return;
    float closestDist = ASTRO_SCAN_RANGE;
    float closestAngle = 0;
    bool found = false;
    for (size_t i = 0; i < ships.size(); ++i) {
        if (i == (size_t)self || !ships[i].alive) continue;
        float dist = Distance(s.x, s.y, ships[i].x, ships[i].y);
        if (dist < closestDist) {
            closestDist = dist;
            closestAngle = AngleTo(s.x, s.y, ships[i].x, ships[i].y);
            found = true;
        }
    }
    for (auto& a : asteroids) {
        if (!a.alive) continue;
        float dist = Distance(s.x, s.y, a.x, a.y);
        if (dist < closestDist) {
            closestDist = dist;
            closestAngle = AngleTo(s.x, s.y, a.x, a.y);
            found = true;
        }
    }
    s.scan_hit = found;
    s.scan_dist = closestDist;
    s.scan_angle = NormalizeAngle(closestAngle);
}

void AstroArena::Signal(int self, int value) {
    auto& s = ships[self];
    if (!s.alive) return;
    s.signal = value;
    signals.emplace_back(s.x, s.y);
}

void AstroArena::TurnToScan(int self) {
    auto& s = ships[self];
    if (!s.alive || !s.scan_hit) return;
    s.targetAngle = s.scan_angle;
}

bool AstroArena::CircleCollision(float x1, float y1, float r1, float x2, float y2, float r2) {
    float dist = Distance(x1, y1, x2, y2);
    return dist < (r1 + r2);
}

void AstroArena::HandleCollisions() {
    RebuildBroadphase();
    for (size_t si = 0; si < ships.size(); ++si) {
        auto& s = ships[si];
        if (!s.alive) continue;
        int scx, scy; PosToCell(s.x, s.y, scx, scy);
        std::vector<int> cellIdx;
        CollectNearCells(scx, scy, cellIdx);
        for (int cell : cellIdx) {
            const auto& bucket = gridAsteroids[cell];
            for (int ai : bucket) {
                auto& a = asteroids[ai];
            if (!a.alive) continue;
            // Ship vs asteroid using cute_c2 (capsule vs poly with wrap)
            bool hit = false;
            c2Capsule shipCap = MakeShipCapsule(s);
            std::array<c2x, 9> tr;
            int trCount = 0;
            BuildWrapTransforms(a.x, a.y, tr, trCount);
            for (int ti = 0; ti < trCount && !hit; ++ti) {
                if (a.hasPoly && c2CapsuletoPoly(shipCap, &a.poly, &tr[ti])) {
                    hit = true;
                }
            }
            if (hit) {
                s.hp -= 1;
                a.hp--;
                SpawnParticleBurst(s.x, s.y, 24, IM_COL32(255, 150, 120, 255));
                if (s.hp <= 0) {
                    std::string name = s.ship ? s.ship->name : "Ship";
                    KillShip(s, name + " destroyed by asteroid collision!");
                }
                if (a.hp <= 0) {
                        BreakAsteroid((int)ai, s.x, s.y);
                    }
                }
            }
        }
    }
}

void AstroArena::HandleTorpedoes() {
    RebuildBroadphase();
    for (auto& t : torpedoes) {
        if (!t.alive) continue;
        // Prepare swept circle for torpedo using c2TOI
        c2Circle torpCircle;
        torpCircle.p = c2V(t.prevX, t.prevY);
        torpCircle.r = 5.0f;
        c2v vA = c2V(t.x - t.prevX, t.y - t.prevY);

        // Track earliest impact
        bool anyHit = false;
        float bestToi = 1.0f;
        enum { HIT_NONE, HIT_SHIP, HIT_AST } hitType = HIT_NONE;
        int hitIndex = -1;
        c2v hitPoint = c2V(t.x, t.y);

        // Candidate cells around swept path
        int c0x, c0y, c1x, c1y;
        PosToCell(t.prevX, t.prevY, c0x, c0y);
        PosToCell(t.x, t.y, c1x, c1y);
        std::vector<int> cells;
        CollectNearCells(c0x, c0y, cells);
        std::vector<int> cells2;
        CollectNearCells(c1x, c1y, cells2);
        cells.insert(cells.end(), cells2.begin(), cells2.end());

        // Against ships
        for (int cell : cells) {
            const auto& sbucket = gridShips[cell];
            for (int si : sbucket) {
                if (si == t.owner || !ships[si].alive) continue;
                c2Capsule shipCap = MakeShipCapsule(ships[si]);
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        c2Capsule wcap = shipCap;
                        wcap.a = c2Add(wcap.a, c2V(ox * ASTROBOTS_W, oy * ASTROBOTS_H));
                        wcap.b = c2Add(wcap.b, c2V(ox * ASTROBOTS_W, oy * ASTROBOTS_H));
                        c2TOIResult res = c2TOI(&torpCircle, C2_TYPE_CIRCLE, nullptr, vA, &wcap, C2_TYPE_CAPSULE, nullptr, c2V(0, 0), 1);
                        if (res.hit && res.toi >= 0.0f && res.toi <= bestToi) {
                            bestToi = res.toi;
                            hitType = HIT_SHIP;
                            hitIndex = si;
                            hitPoint = res.p;
                            anyHit = true;
                        }
                    }
                }
            }
        }

        // Against asteroids
        for (int cell : cells) {
            const auto& abucket = gridAsteroids[cell];
            for (int ai : abucket) {
                if (!asteroids[ai].alive || !asteroids[ai].hasPoly) continue;
                std::array<c2x, 9> tr;
                int trCount = 0;
                BuildWrapTransforms(asteroids[ai].x, asteroids[ai].y, tr, trCount);
                for (int ti = 0; ti < trCount; ++ti) {
                    c2TOIResult res = c2TOI(&torpCircle, C2_TYPE_CIRCLE, nullptr, vA, &asteroids[ai].poly, C2_TYPE_POLY, &tr[ti], c2V(0, 0), 1);
                    if (res.hit && res.toi >= 0.0f && res.toi <= bestToi) {
                        bestToi = res.toi;
                        hitType = HIT_AST;
                        hitIndex = ai;
                        hitPoint = res.p;
                        anyHit = true;
                    }
                }
            }
        }

        // Resolve earliest impact
        if (anyHit) {
            t.alive = false;
            if (hitType == HIT_SHIP && hitIndex >= 0) {
                ships[hitIndex].hp -= t.damage;
                SpawnParticleBurst(ships[hitIndex].x, ships[hitIndex].y, 42, IM_COL32(255, 200, 140, 255), 1.0f, 1.0f);
                SpawnParticleBurst(ships[hitIndex].x, ships[hitIndex].y, 20, IM_COL32(255, 255, 200, 255), 1.7f, 0.5f);
                if (log) {
                    std::string attacker = (t.owner >= 0 && ships[t.owner].ship) ? ships[t.owner].ship->name : "Ship";
                    std::string target = ships[hitIndex].ship ? ships[hitIndex].ship->name : "Ship";
                    log(attacker + "'s torpedo hits " + target + " for " + std::to_string(t.damage) + " damage!");
                }
                if (ships[hitIndex].hp <= 0) {
                    std::string target = ships[hitIndex].ship ? ships[hitIndex].ship->name : "Ship";
                    KillShip(ships[hitIndex], target + " is destroyed!");
                }
            } else if (hitType == HIT_AST && hitIndex >= 0) {
                SpawnParticleBurst(hitPoint.x, hitPoint.y, 48, IM_COL32(255, 180, 140, 255), 1.0f, 1.0f);
                SpawnParticleBurst(hitPoint.x, hitPoint.y, 25, IM_COL32(255, 255, 200, 255), 1.8f, 0.6f);
                BreakAsteroid(hitIndex, t.x, t.y);
                if (t.owner >= 0 && t.owner < (int)ships.size()) {
                    ships[t.owner].fuel += FUEL_HIT_REWARD;
                    if (ships[t.owner].fuel > ASTRO_START_FUEL) ships[t.owner].fuel = ASTRO_START_FUEL;
                }
            }
        }
    }
}

void AstroArena::KillShip(ShipState& s, const std::string& message) {
    if (!s.alive) return;
    s.alive = false;
    if (log) log(message);
    SpawnParticleBurst(s.x, s.y, 150, s.color, 1.2f, 1.5f);
    SpawnParticleBurst(s.x, s.y, 80, IM_COL32(255, 255, 220, 255), 2.2f, 0.8f);

    // Spawn Asteroids-style breakup debris from the triangle outline
    // Reconstruct ship triangle in world space
    float angleRad = s.angle * (float)M_PI / 180.0f;
    // Convert screen-size triangle length to world units using current renderScale
    float size = SHIP_DRAW_SIZE;
    if (renderScale > 0.00001f) {
        size = SHIP_DRAW_SIZE / renderScale;
    }
    ImVec2 nose(s.x + std::cos(angleRad) * size,
                s.y + std::sin(angleRad) * size);
    ImVec2 leftWing(s.x + std::cos(angleRad + 2.4f) * size * 0.6f,
                    s.y + std::sin(angleRad + 2.4f) * size * 0.6f);
    ImVec2 rightWing(s.x + std::cos(angleRad - 2.4f) * size * 0.6f,
                     s.y + std::sin(angleRad - 2.4f) * size * 0.6f);

    // Triangle centroid
    ImVec2 center((nose.x + leftWing.x + rightWing.x) / 3.0f,
                  (nose.y + leftWing.y + rightWing.y) / 3.0f);

    std::array<std::pair<ImVec2, ImVec2>, 3> edges = {{
        { nose, leftWing },
        { leftWing, rightWing },
        { rightWing, nose }
    }};

    std::uniform_real_distribution<float> jitter(-0.07f, 0.07f);
    std::uniform_real_distribution<float> speedDist(0.6f, 1.8f);
    std::uniform_real_distribution<float> angVelDist(-0.05f, 0.05f);
    std::uniform_int_distribution<int> lifeJitter(-10, 10);

    for (const auto& e : edges) {
        ImVec2 a = e.first;
        ImVec2 b = e.second;
        for (int i = 0; i < SHIP_DEBRIS_COUNT_PER_EDGE; ++i) {
            float t0 = (float)i / (float)SHIP_DEBRIS_COUNT_PER_EDGE;
            float t1 = (float)(i + 1) / (float)SHIP_DEBRIS_COUNT_PER_EDGE;
            t0 = std::max(0.0f, std::min(1.0f, t0 + jitter(rng)));
            t1 = std::max(0.0f, std::min(1.0f, t1 + jitter(rng)));
            if (t1 < t0) std::swap(t0, t1);
            ImVec2 p0(a.x + (b.x - a.x) * t0, a.y + (b.y - a.y) * t0);
            ImVec2 p1(a.x + (b.x - a.x) * t1, a.y + (b.y - a.y) * t1);

            // Midpoint and outward direction from centroid
            ImVec2 mid((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
            float dx = mid.x - center.x;
            float dy = mid.y - center.y;
            float len = std::sqrt(dx*dx + dy*dy);
            if (len < 1e-5f) { dx = 1.0f; dy = 0.0f; len = 1.0f; }
            dx /= len; dy /= len;

            float spd = speedDist(rng);

            ShipDebrisSegment seg;
            seg.x1 = p0.x; seg.y1 = p0.y;
            seg.x2 = p1.x; seg.y2 = p1.y;
            // Inherit some ship velocity, add outward impulse and slight downward bias
            seg.vx = s.vx + dx * spd;
            seg.vy = s.vy + dy * spd + 0.15f;
            seg.angVel = angVelDist(rng);
            seg.startLifetime = SHIP_DEBRIS_LIFETIME + lifeJitter(rng);
            if (seg.startLifetime < 20) seg.startLifetime = 20;
            seg.lifetime = seg.startLifetime;
            seg.color = s.color;
            seg.alive = true;
            shipDebris.push_back(seg);
        }
    }
}

void AstroArena::BreakAsteroid(int asteroidIdx, float pushFromX, float pushFromY) {
    if (asteroidIdx < 0 || asteroidIdx >= (int)asteroids.size()) return;
    auto& a = asteroids[asteroidIdx];
    if (!a.alive) return;
    a.alive = false;
    std::uniform_real_distribution<float> angleDist(0, 2.0f * M_PI);
    std::uniform_real_distribution<float> speedDist(0.5f, ASTEROID_MAX_SPEED);
    std::uniform_int_distribution<int> countDist(2, 3);
    float pushAngle = 0;
    float pushSpeed = 1.5f;
    bool hasPush = (pushFromX >= 0 && pushFromY >= 0);
    if (hasPush) pushAngle = AngleTo(pushFromX, pushFromY, a.x, a.y) * M_PI / 180.0f;
    if (a.size > MEDIUM_ASTEROID_SIZE) {
        int count = countDist(rng);
        for (int i = 0; i < count; ++i) {
            Asteroid newAst;
            newAst.x = a.x; newAst.y = a.y;
            float angle = angleDist(rng);
            float speed = speedDist(rng);
            if (hasPush) {
                float angleOffset = (float)(i - count / 2) * 0.8f;
                angle = pushAngle + angleOffset;
                speed += pushSpeed;
            }
            newAst.vx = a.vx + std::cos(angle) * speed;
            newAst.vy = a.vy + std::sin(angle) * speed;
            newAst.size = MEDIUM_ASTEROID_SIZE;
            newAst.hp = MEDIUM_ASTEROID_HP;
            newAst.alive = true;
            newAst.GenerateShape(7, MEDIUM_ASTEROID_SIZE);
            asteroids.push_back(newAst);
        }
    } else if (a.size > SMALL_ASTEROID_SIZE) {
        int count = countDist(rng);
        for (int i = 0; i < count; ++i) {
            Asteroid newAst;
            newAst.x = a.x; newAst.y = a.y;
            float angle = angleDist(rng);
            float speed = speedDist(rng);
            if (hasPush) {
                float angleOffset = (float)(i - count / 2) * 0.8f;
                angle = pushAngle + angleOffset;
                speed += pushSpeed;
            }
            newAst.vx = a.vx + std::cos(angle) * speed;
            newAst.vy = a.vy + std::sin(angle) * speed;
            newAst.size = SMALL_ASTEROID_SIZE;
            newAst.hp = SMALL_ASTEROID_HP;
            newAst.alive = true;
            newAst.GenerateShape(6, SMALL_ASTEROID_SIZE);
            asteroids.push_back(newAst);
        }
    } else {
        for (auto& s : ships) {
            if (!s.alive) continue;
            if (Distance(s.x, s.y, a.x, a.y) < 50.0f) {
                s.fuel += FUEL_PICKUP_AMOUNT;
                if (s.fuel > ASTRO_START_FUEL) s.fuel = ASTRO_START_FUEL;
                if (log) {
                    std::string name = s.ship ? s.ship->name : "Ship";
                    log(name + " collects fuel!");
                }
                break;
            }
        }
    }
}

void AstroArena::SpawnAsteroids(int count) {
    std::uniform_real_distribution<float> xDist(100.0f, ASTROBOTS_W - 100.0f);
    std::uniform_real_distribution<float> yDist(100.0f, ASTROBOTS_H - 100.0f);
    std::uniform_real_distribution<float> angleDist(0, 2.0f * M_PI);
    std::uniform_real_distribution<float> speedDist(0.3f, ASTEROID_MAX_SPEED);
    for (int i = 0; i < count; ++i) {
        Asteroid a;
        a.x = xDist(rng);
        a.y = yDist(rng);
        float angle = angleDist(rng);
        float speed = speedDist(rng);
        a.vx = std::cos(angle) * speed;
        a.vy = std::sin(angle) * speed;
        a.size = LARGE_ASTEROID_SIZE;
        a.hp = LARGE_ASTEROID_HP;
        a.alive = true;
        a.GenerateShape(8, LARGE_ASTEROID_SIZE);
        asteroids.push_back(a);
    }
}

void AstroArena::SpawnAsteroidFromEdge() {
    std::uniform_int_distribution<int> edgeDist(0, 3);
    std::uniform_real_distribution<float> alongX(0.0f, ASTROBOTS_W);
    std::uniform_real_distribution<float> alongY(0.0f, ASTROBOTS_H);
    std::uniform_real_distribution<float> angleJitter(-M_PI/12.0f, M_PI/12.0f);
    std::uniform_real_distribution<float> speedDist(0.4f, ASTEROID_MAX_SPEED);
    Asteroid a;
    int edge = edgeDist(rng);
    float inset = 8.0f;
    float cx = ASTROBOTS_W * 0.5f;
    float cy = ASTROBOTS_H * 0.5f;
    if (edge == 0) { a.x = alongX(rng); a.y = inset; }
    else if (edge == 1) { a.x = ASTROBOTS_W - inset; a.y = alongY(rng); }
    else if (edge == 2) { a.x = alongX(rng); a.y = ASTROBOTS_H - inset; }
    else { a.x = inset; a.y = alongY(rng); }
    float baseAngle = AngleTo(a.x, a.y, cx, cy) * (float)(M_PI / 180.0f);
    float angle = baseAngle + angleJitter(rng);
    float speed = speedDist(rng);
    a.vx = std::cos(angle) * speed;
    a.vy = std::sin(angle) * speed;
    a.size = LARGE_ASTEROID_SIZE;
    a.hp = LARGE_ASTEROID_HP;
    a.alive = true;
    a.GenerateShape(8, LARGE_ASTEROID_SIZE);
    asteroids.push_back(a);
}

void AstroArena::SpawnParticleBurst(float x, float y, int count, ImU32 baseColor, float speedScale, float lifeScale, float particleLength) {
    std::uniform_real_distribution<float> ang(0.0f, 2.0f * (float)M_PI);
    std::uniform_real_distribution<float> spd(PARTICLE_MIN_SPEED, PARTICLE_MAX_SPEED);
    std::uniform_int_distribution<int> life(PARTICLE_DEFAULT_LIFETIME - 15, PARTICLE_DEFAULT_LIFETIME + 15);
    std::uniform_real_distribution<float> lenDist(0.7f, 1.3f);
    std::uniform_int_distribution<int> colorJitter(-40, 40);
    for (int i = 0; i < count; ++i) {
        float a = ang(rng);
        float s = spd(rng) * speedScale;
        Particle p;
        p.x = x; p.y = y;
        p.vx = std::cos(a) * s;
        p.vy = std::sin(a) * s;
        p.lifetime = std::max(10, (int)(life(rng) * lifeScale));
        p.startLifetime = p.lifetime;
        p.length = particleLength * lenDist(rng);
        int r = (int)((baseColor >> IM_COL32_R_SHIFT) & 0xFF);
        int g = (int)((baseColor >> IM_COL32_G_SHIFT) & 0xFF);
        int b = (int)((baseColor >> IM_COL32_B_SHIFT) & 0xFF);
        r = std::min(255, std::max(0, r + colorJitter(rng)));
        g = std::min(255, std::max(0, g + colorJitter(rng)));
        b = std::min(255, std::max(0, b + colorJitter(rng)));
        p.color = IM_COL32(r, g, b, 255);
        p.alive = true;
        particles.push_back(p);
    }
}

void AstroArena::StartTurn() {
    signals.clear();
    for (auto& s : ships) {
        if (!s.alive) continue;
        if (s.phaser_cooldown > 0) --s.phaser_cooldown;
        if (s.photon_cooldown > 0) --s.photon_cooldown;
        s.signal = -1;
        s.scan_hit = false;
    }
}


