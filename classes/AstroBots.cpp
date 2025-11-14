#include "AstroBots.h"
#include "../imgui/imgui.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <random>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ===== VM implementation =====
void ShipBase::Run(int turn) {
    int pc = 0;
    bool flag = false;
    while (pc < (int)code.size()) {
        int op = code[pc++];
        switch(op) {
            case ASTRO_OP_WAIT:
                break;
            case ASTRO_OP_THRUST: {
                int powerInt = code[pc++];
                float power = powerInt / 10.0f;
                A->Thrust(id, power);
                break;
            }
            case ASTRO_OP_TURN_DEG: {
                int degrees = code[pc++];
                A->TurnDeg(id, degrees);
                break;
            }
            case ASTRO_OP_FIRE_PHASER:
                A->FirePhaser(id);
                break;
            case ASTRO_OP_FIRE_PHOTON:
                A->FirePhoton(id);
                break;
            case ASTRO_OP_SCAN:
                A->Scan(id);
                break;
            case ASTRO_OP_SIGNAL: {
                int value = code[pc++];
                A->Signal(id, value);
                break;
            }
            case ASTRO_OP_TURN_TO_SCAN:
                A->TurnToScan(id);
                break;
            case ASTRO_OP_IF_SEEN:
                pc++; // skip param
                flag = A->ships[id].scan_hit;
                break;
            case ASTRO_OP_IF_SCAN_LE: {
                int range = code[pc++];
                flag = (A->ships[id].scan_hit && A->ships[id].scan_dist <= range);
                break;
            }
            case ASTRO_OP_IF_DAMAGED:
                pc++; // skip param
                flag = (A->ships[id].hp < ASTRO_START_HP);
                break;
            case ASTRO_OP_IF_HP_LE: {
                int hp = code[pc++];
                flag = (A->ships[id].hp <= hp);
                break;
            }
            case ASTRO_OP_IF_FUEL_LE: {
                int fuel = code[pc++];
                flag = (A->ships[id].fuel <= fuel);
                break;
            }
            case ASTRO_OP_IF_CAN_FIRE_PHASER:
                pc++; // skip param
                flag = (A->ships[id].phaser_cooldown == 0);
                break;
            case ASTRO_OP_IF_CAN_FIRE_PHOTON:
                pc++; // skip param
                flag = (A->ships[id].photon_cooldown == 0);
                break;
            case ASTRO_OP_JUMP_IF_FALSE: {
                int target = code[pc++];
                if (!flag) pc = target;
                break;
            }
            case ASTRO_OP_END:
                return;
            default:
                return;
        }
    }
}

// ===== Sample ship implementations =====
int HunterShip::SetupShip() {
    SCAN();
    IF_SEEN() {
        // Always turn toward and pursue what we see
        TURN_TO_SCAN();
        IF_SCAN_LE(500) {  // within phaser range: shoot
            IF_SHIP_CAN_FIRE_PHASER() {
                FIRE_PHASER();
            }
            IF_SHIP_CAN_FIRE_PHOTON() {
                FIRE_PHOTON();
            }
        }
        THRUST(2);  // close distance if not in range
    }
    IF_SHIP_FUEL_LE(40) {
        SCAN();
        IF_SCAN_LE(300) {  // Increased from 100 - look for fuel further away
            TURN_TO_SCAN();
            THRUST(2);
        }
    }
    return Finalize();
}

int DroneShip::SetupShip() {
    SCAN();
    IF_SEEN() {
        IF_SCAN_LE(450) {  // Increased from 400 - be more cautious
            // Thrust away from threat
            THRUST(3);
            IF_SHIP_CAN_FIRE_PHOTON() {
                FIRE_PHOTON();  // Fire while retreating
            }
        }
    }
    IF_SHIP_HP_LE(6) {  // Emergency threshold
        THRUST(2);
    }
    IF_SHIP_FUEL_LE(35) {
        SCAN();
        IF_SCAN_LE(200) {  // Increased from 100
            TURN_TO_SCAN();
            THRUST(2);
        }
    }
    return Finalize();
}

int MinerShip::SetupShip() {
    // Move toward scanned objects and fire when close
    SCAN();
    IF_SEEN() {
        TURN_TO_SCAN();
        THRUST(2);
        IF_SCAN_LE(150) {  // close range work
            IF_SHIP_CAN_FIRE_PHASER() {
                FIRE_PHASER();
            }
        }
    }
    IF_SHIP_DAMAGED() {
        SCAN();
        IF_SEEN() {
            IF_SCAN_LE(350) {  // Increased from 200 - flee earlier
                THRUST(3);
            }
        }
    }
    return Finalize();
}
int GraemeShip::SetupShip() {
    SCAN();
    IF_SEEN() {
        IF_SCAN_LE(450) {  // Increased from 400 - be more cautious
            // Thrust away from threat
            THRUST(3);
            IF_SHIP_CAN_FIRE_PHOTON() {
                FIRE_PHOTON();  // Fire while retreating
            }
        }
    }
    IF_SHIP_HP_LE(6) {  // Emergency threshold
        THRUST(2);
    }
    IF_SHIP_FUEL_LE(35) {
        SCAN();
        IF_SCAN_LE(200) {  // Increased from 100
            TURN_TO_SCAN();
            THRUST(2);
        }
    }
    return Finalize();
}
int Crackhead2Ship::SetupShip() {
    SCAN();
    IF_SEEN() {
        IF_SCAN_LE(450) {  // Increased from 400 - be more cautious
            // Thrust away from threat
            THRUST(3);
            IF_SHIP_CAN_FIRE_PHOTON() {
                FIRE_PHOTON();  // Fire while retreating
            }
        }
    }
    IF_SHIP_HP_LE(6) {  // Emergency threshold
        THRUST(2);
    }
    IF_SHIP_FUEL_LE(35) {
        SCAN();
        IF_SCAN_LE(200) {  // Increased from 100
            TURN_TO_SCAN();
            THRUST(2);
        }
    }
    return Finalize();
}

// ===== AstroBots game implementation =====
AstroBots::AstroBots() {
    _currentTurn = 0;
    _gameRunning = false;
}

AstroBots::~AstroBots() {
}

std::vector<std::unique_ptr<ShipBase>> AstroBots::makeShips() {
    std::vector<std::unique_ptr<ShipBase>> v;
    v.emplace_back(std::make_unique<HunterShip>());
    v.emplace_back(std::make_unique<DroneShip>());
    v.emplace_back(std::make_unique<MinerShip>());
    v.emplace_back(std::make_unique<GraemeShip>());
    v.emplace_back(std::make_unique<Crackhead2Ship>());
    return v;
}

void AstroBots::setUpBoard() {
    setNumberOfPlayers(1);
    _gameOptions.rowX = (int)ASTROBOTS_W;
    _gameOptions.rowY = (int)ASTROBOTS_H;

    _ships = makeShips();
    _arena.ships.resize(_ships.size());
    _logLines.clear();

    // Validate scripts & inject arena refs
    ImU32 shipColors[] = {
        IM_COL32(255, 80, 80, 255),   // Red
        IM_COL32(80, 255, 80, 255),   // Green
        IM_COL32(80, 180, 255, 255),  // Blue
        IM_COL32(255, 255, 80, 255),  // Yellow
        IM_COL32(255, 80, 255, 255),  // Magenta
        IM_COL32(80, 255, 255, 255),   // Cyan
        IM_COL32(255, 160, 0, 255),    // Orange
        IM_COL32(128, 0, 128, 255)     // Purple
    };

    // Hook up logger
    _arena.log = [this](const std::string& line) {
        _logLines.push_back(line);
        if (_logLines.size() > 500) {
            _logLines.erase(_logLines.begin(), _logLines.begin() + (_logLines.size() - 500));
        }
    };

    for (size_t i = 0; i < _ships.size(); ++i) {
        int cost = _ships[i]->SetupShip();
        // log the ship setup cost and show the name, highlight if it exceeds the limit
        std::string nm = _ships[i] ? _ships[i]->name : std::string("Ship");
        std::string line = nm + " script cost " + std::to_string(cost) + "/" + std::to_string(ASTRO_MAX_SCRIPT_COST);
        if (cost > ASTRO_MAX_SCRIPT_COST) line += " (EXCEEDS LIMIT)";
        _logLines.push_back(line);
        if (_logLines.size() > 500) {
            _logLines.erase(_logLines.begin(), _logLines.begin() + (_logLines.size() - 500));
        }
        _arena.ships[i].ship = _ships[i].get();
        _arena.ships[i].color = shipColors[i % 6];
        _ships[i]->A = &_arena;
        _ships[i]->id = (int)i;
    }

    // Spawn ships in a circle around the center
    float centerX = ASTROBOTS_W / 2.0f;
    float centerY = ASTROBOTS_H / 2.0f;
    float spawnRadius = 300.0f;

    for (size_t i = 0; i < _arena.ships.size(); ++i) {
        float angle = (float)i / _ships.size() * 2.0f * M_PI;
        _arena.ships[i].x = centerX + std::cos(angle) * spawnRadius;
        _arena.ships[i].y = centerY + std::sin(angle) * spawnRadius;
        _arena.ships[i].angle = angle * 180.0f / M_PI;
        _arena.ships[i].targetAngle = _arena.ships[i].angle;
        _arena.ships[i].vx = 0;
        _arena.ships[i].vy = 0;
    }

    // Spawn asteroids
    _arena.SpawnAsteroids(NUM_INITIAL_ASTEROIDS);

    _currentTurn = 0;
    _gameRunning = true;

    startGame();
}

ImVec2 AstroBots::WorldToScreen(float x, float y) {
    // Properly scaled viewport
    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
    ImVec2 size = ImVec2(contentMax.x - contentMin.x, contentMax.y - contentMin.y);

    // Calculate scale to fit world in window
    float scaleX = size.x / ASTROBOTS_W;
    float scaleY = size.y / ASTROBOTS_H;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;

    // Center the world in the window
    float screenX = x * scale;
    float screenY = y * scale;

    return ImVec2(screenX, screenY);
}

void AstroBots::DrawShip(ImDrawList* drawList, const AstroArena::ShipState& ship, ImVec2 offset) {
    if (!ship.alive) return;

    ImVec2 pos = WorldToScreen(ship.x, ship.y);
    pos.x += offset.x;
    pos.y += offset.y;

    // Draw ship as triangle pointing in facing direction
    float angleRad = ship.angle * M_PI / 180.0f;
    float size = 15.0f;

    // Triangle vertices (nose, left wing, right wing)
    ImVec2 nose(pos.x + std::cos(angleRad) * size,
                pos.y + std::sin(angleRad) * size);
    ImVec2 leftWing(pos.x + std::cos(angleRad + 2.4f) * size * 0.6f,
                    pos.y + std::sin(angleRad + 2.4f) * size * 0.6f);
    ImVec2 rightWing(pos.x + std::cos(angleRad - 2.4f) * size * 0.6f,
                     pos.y + std::sin(angleRad - 2.4f) * size * 0.6f);

    // Draw filled triangle
    drawList->AddTriangleFilled(nose, leftWing, rightWing, ship.color);
    // Draw outline
    drawList->AddTriangle(nose, leftWing, rightWing, IM_COL32(255, 255, 255, 255), 2.0f);

    // Health bar
    const float barWidth = 40.0f;
    const float barHeight = 4.0f;
    const float barYOffset = 25.0f;
    ImVec2 barTL(pos.x - barWidth / 2, pos.y - barYOffset);
    ImVec2 barBR(pos.x + barWidth / 2, pos.y - barYOffset + barHeight);

    drawList->AddRectFilled(barTL, barBR, IM_COL32(40, 40, 40, 200));
    float hpRatio = (float)ship.hp / (float)ASTRO_START_HP;
    if (hpRatio < 0.0f) hpRatio = 0.0f;
    if (hpRatio > 1.0f) hpRatio = 1.0f;
    ImVec2 hpBR(barTL.x + barWidth * hpRatio, barBR.y);
    int r = (int)((1.0f - hpRatio) * 220);
    int g = (int)(hpRatio * 220);
    drawList->AddRectFilled(barTL, hpBR, IM_COL32(r, g, 64, 230));

    // Fuel bar (below health bar)
    ImVec2 fuelTL(pos.x - barWidth / 2, pos.y - barYOffset + barHeight + 2);
    ImVec2 fuelBR(pos.x + barWidth / 2, pos.y - barYOffset + barHeight * 2 + 2);
    drawList->AddRectFilled(fuelTL, fuelBR, IM_COL32(40, 40, 40, 200));
    float fuelRatio = ship.fuel / ASTRO_START_FUEL;
    if (fuelRatio < 0.0f) fuelRatio = 0.0f;
    if (fuelRatio > 1.0f) fuelRatio = 1.0f;
    ImVec2 fuelFillBR(fuelTL.x + barWidth * fuelRatio, fuelBR.y);
    drawList->AddRectFilled(fuelTL, fuelFillBR, IM_COL32(255, 200, 64, 230));

    // Name label
    const char* label = ship.ship ? ship.ship->name.c_str() : "Ship";
    ImVec2 textSize = ImGui::CalcTextSize(label);
    ImVec2 textPos(pos.x - textSize.x / 2, pos.y + 20);
    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), label);
}

void AstroBots::DrawAsteroid(ImDrawList* drawList, const Asteroid& asteroid, ImVec2 offset) {
    if (!asteroid.alive) return;

    ImVec2 pos = WorldToScreen(asteroid.x, asteroid.y);
    pos.x += offset.x;
    pos.y += offset.y;

    // Draw asteroid as polygon
    if (asteroid.shape.size() < 3) return;

    std::vector<ImVec2> points;
    for (const auto& v : asteroid.shape) {
        ImVec2 p = WorldToScreen(asteroid.x + v.x, asteroid.y + v.y);
        p.x += offset.x; p.y += offset.y;
        points.push_back(p);
    }

    // Draw filled polygon (using triangles)
    ImVec2 center = pos;
    for (size_t i = 0; i < points.size(); ++i) {
        size_t next = (i + 1) % points.size();
        drawList->AddTriangleFilled(center, points[i], points[next], IM_COL32(100, 80, 70, 180));
    }

    // Draw outline
    for (size_t i = 0; i < points.size(); ++i) {
        size_t next = (i + 1) % points.size();
        drawList->AddLine(points[i], points[next], IM_COL32(150, 130, 120, 255), 2.0f);
    }
}

void AstroBots::DrawTorpedo(ImDrawList* drawList, const PhotonTorpedo& torpedo, ImVec2 offset) {
    if (!torpedo.alive) return;

    ImVec2 pos = WorldToScreen(torpedo.x, torpedo.y);
    pos.x += offset.x;
    pos.y += offset.y;

    // Animated rotating/pulsing asterisk photon
    const int spokes = PHOTON_SPOKES;
    const float angle = (float)(torpedo.anim * PHOTON_SPIN_SPEED);
    const float pulseT = (float)(torpedo.anim * PHOTON_PULSE_SPEED);
    const float pulse = 0.65f + 0.35f * (0.5f * (std::sin(pulseT) + 1.0f));
    const float base = PHOTON_BASE_SIZE * pulse;
    const float amp = PHOTON_PULSE_AMPLITUDE * (0.6f + 0.4f * (0.5f * (std::sin(pulseT * 0.8f + 1.3f) + 1.0f)));

    ImU32 coreColor = IM_COL32(255, 180, 120, 255);
    ImU32 spokeColor = IM_COL32(255, 80, 80, 255);
    ImU32 glowColor = IM_COL32(255, 100, 100, 110);

    // Spokes
    for (int i = 0; i < spokes; ++i) {
        float a = angle + (float)i * (float)M_PI * 2.0f / spokes;
        // Stagger length for adjacent spokes and animate length
        float phase = (i % 2 == 0) ? 0.0f : (float)M_PI * 0.5f;
        float len = base + amp * (0.5f * (std::sin(pulseT + phase) + 1.0f));
        float dx = std::cos(a);
        float dy = std::sin(a);
        ImVec2 p1(pos.x - dx * len * 0.25f, pos.y - dy * len * 0.25f);
        ImVec2 p2(pos.x + dx * len,        pos.y + dy * len);
        // outer glow
        drawList->AddLine(p1, p2, glowColor, 6.0f);
        // main spoke
        drawList->AddLine(p1, p2, spokeColor, 2.5f);
    }

    // Core and halo
    drawList->AddCircleFilled(pos, 3.0f, IM_COL32(255, 240, 180, 230));
    drawList->AddCircle(pos, (base + amp) * 0.35f, coreColor, 0, 2.0f);
}

void AstroBots::DrawPhaserBeam(ImDrawList* drawList, const PhaserBeam& beam, ImVec2 offset) {
    if (!beam.alive) return;

    ImVec2 p1 = WorldToScreen(beam.x1, beam.y1);
    ImVec2 p2 = WorldToScreen(beam.x2, beam.y2);
    p1.x += offset.x; p1.y += offset.y;
    p2.x += offset.x; p2.y += offset.y;

    // Draw bright beam with glow effect
    drawList->AddLine(p1, p2, beam.color, 3.0f);
    // Add outer glow
    ImU32 glowColor = IM_COL32(255, 150, 150, 100);
    drawList->AddLine(p1, p2, glowColor, 6.0f);
}

void AstroBots::DrawParticles(ImDrawList* drawList, const std::vector<Particle>& particles, ImVec2 offset) {
    for (const auto& p : particles) {
        if (!p.alive) continue;
        ImVec2 pos = WorldToScreen(p.x, p.y);
        pos.x += offset.x; pos.y += offset.y;

        // Calculate normalized lifetime (1.0 at spawn, 0.0 at death)
        float lifeT = 0.0f;
        if (p.startLifetime > 0) {
            lifeT = std::max(0.0f, (float)p.lifetime / (float)p.startLifetime);
        }

        // Fade length to zero
        float len = p.length * lifeT;

        // Get velocity direction
        float vx = p.vx, vy = p.vy;
        float vlen = std::sqrt(vx*vx + vy*vy);
        if (vlen < 1e-4f) vlen = 1.0f;
        vx /= vlen; vy /= vlen;

        // *** "Hot" Particles ***
        // Interpolate color from white-hot to its base color as it "cools"
        int r_base = (p.color >> IM_COL32_R_SHIFT) & 0xFF;
        int g_base = (p.color >> IM_COL32_G_SHIFT) & 0xFF;
        int b_base = (p.color >> IM_COL32_B_SHIFT) & 0xFF;

        // As lifeT goes from 1.0 -> 0.0, we fade from 255 -> base_color
        int r = (int)(r_base + (255 - r_base) * lifeT);
        int g = (int)(g_base + (255 - g_base) * lifeT);
        int b = (int)(b_base + (255 - b_base) * lifeT);

        // Fade alpha based on (lifeT^2) so it fades faster at the end
        int a_main = (int)(220 * lifeT * lifeT + 35);
        int a_glow = (int)(120 * lifeT * lifeT);

        ImU32 colorMain = IM_COL32(r, g, b, a_main);
        ImU32 glow = IM_COL32(r, g, b, a_glow);

        ImVec2 tail(pos.x - vx * len, pos.y - vy * len);
        
        // Layered lines: a thick, dim glow and a thin, bright core
        // Both shrink and fade with lifeT
        drawList->AddLine(tail, pos, glow, 7.0f * lifeT + 2.0f); 
        drawList->AddLine(tail, pos, colorMain, 3.0f * lifeT + 1.0f);
    }
}

void AstroBots::DrawShipDebris(ImDrawList* drawList, const std::vector<ShipDebrisSegment>& debris, ImVec2 offset) {
    for (const auto& d : debris) {
        if (!d.alive) continue;
        ImVec2 p1 = WorldToScreen(d.x1, d.y1);
        ImVec2 p2 = WorldToScreen(d.x2, d.y2);
        p1.x += offset.x; p1.y += offset.y;
        p2.x += offset.x; p2.y += offset.y;

        float lifeT = 0.0f;
        if (d.startLifetime > 0) {
            lifeT = std::max(0.0f, (float)d.lifetime / (float)d.startLifetime);
        }

        // Glow color derived from ship color, core is white
        int r = (int)((d.color >> IM_COL32_R_SHIFT) & 0xFF);
        int g = (int)((d.color >> IM_COL32_G_SHIFT) & 0xFF);
        int b = (int)((d.color >> IM_COL32_B_SHIFT) & 0xFF);
        int aGlow = (int)(120 * lifeT * lifeT);
        ImU32 glow = IM_COL32(r, g, b, aGlow);
        int aCore = (int)(230 * lifeT * lifeT + 20);
        ImU32 coreWhite = IM_COL32(255, 255, 255, aCore);

        float glowWidth = 6.0f * lifeT + 1.5f;
        float coreWidth = 2.0f * lifeT + 1.0f;

        drawList->AddLine(p1, p2, glow, glowWidth);
        drawList->AddLine(p1, p2, coreWhite, coreWidth);
    }
}

void AstroBots::drawFrame() {
    Game::drawFrame();

    //ImGui::Begin("AstroBotsView");

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
    ImVec2 origin = ImVec2(windowPos.x + contentMin.x, windowPos.y + contentMin.y);
    ImVec2 size = ImVec2(contentMax.x - contentMin.x, contentMax.y - contentMin.y);
    // Update arena render scale for effects that need screen-size awareness
    float scaleX = size.x / ASTROBOTS_W;
    float scaleY = size.y / ASTROBOTS_H;
    _arena.renderScale = (scaleX < scaleY) ? scaleX : scaleY;

    // Draw space background in content region
    drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                           IM_COL32(5, 5, 15, 255));

    // Draw grid (optional, for reference)
    ImU32 gridColor = IM_COL32(20, 20, 30, 100);
    for (float x = 0; x < ASTROBOTS_W; x += 200) {
        ImVec2 p1 = WorldToScreen(x, 0);
        ImVec2 p2 = WorldToScreen(x, ASTROBOTS_H);
        p1.x += origin.x; p1.y += origin.y;
        p2.x += origin.x; p2.y += origin.y;
        drawList->AddLine(p1, p2, gridColor);
    }
    for (float y = 0; y < ASTROBOTS_H; y += 200) {
        ImVec2 p1 = WorldToScreen(0, y);
        ImVec2 p2 = WorldToScreen(ASTROBOTS_W, y);
        p1.x += origin.x; p1.y += origin.y;
        p2.x += origin.x; p2.y += origin.y;
        drawList->AddLine(p1, p2, gridColor);
    }

    // Draw border around play area
    ImVec2 borderTL = WorldToScreen(0, 0);
    ImVec2 borderBR = WorldToScreen(ASTROBOTS_W, ASTROBOTS_H);
    borderTL.x += origin.x; borderTL.y += origin.y;
    borderBR.x += origin.x; borderBR.y += origin.y;
    drawList->AddRect(borderTL, borderBR, IM_COL32(100, 100, 150, 255), 0.0f, 0, 3.0f);

    // Draw asteroids
    for (const auto& a : _arena.asteroids) {
        DrawAsteroid(drawList, a, origin);
    }

    // Draw phaser beams (drawn first so they appear behind torpedoes and ships)
    for (const auto& beam : _arena.phaserBeams) {
        DrawPhaserBeam(drawList, beam, origin);
    }

    // Draw particles (hits, sparks)
    DrawParticles(drawList, _arena.particles, origin);

    // Draw ship debris segments
    DrawShipDebris(drawList, _arena.shipDebris, origin);

    // Draw torpedoes
    for (const auto& t : _arena.torpedoes) {
        DrawTorpedo(drawList, t, origin);
    }

    // Draw ships
    for (const auto& s : _arena.ships) {
        DrawShip(drawList, s, origin);
    }

    // Debug: collision boundaries
    if (_showColliders) {
        DrawDebugColliders(drawList, origin);
    }

    // Draw HUD
    DrawHUD();

    ImGui::End();

    // Logging window
    ImGui::Begin("AstroBots Log");
    if (ImGui::Button("Clear")) {
        _logLines.clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &_logAutoScroll);
    ImGui::Separator();
    ImGui::Text("Turn: %d / %d", _currentTurn, ASTRO_MAX_TURNS);
    ImGui::Separator();
    ImGui::BeginChild("scroll_region", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& line : _logLines) {
        ImGui::TextUnformatted(line.c_str());
    }
    if (_logAutoScroll) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    //ImGui::End();
}

void AstroBots::DrawHUD() {
    ImGui::SetCursorPos(ImVec2(10, 30));
    ImGui::BeginGroup();
    ImGui::Text("Ship Status:");
    ImGui::Separator();
    ImGui::Checkbox("Show Colliders", &_showColliders);
    ImGui::Separator();
    for (size_t i = 0; i < _arena.ships.size(); ++i) {
        const auto& s = _arena.ships[i];
        const char* name = s.ship ? s.ship->name.c_str() : "Ship";
        if (s.alive) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
                             "%s: HP=%d Fuel=%.0f", name, s.hp, s.fuel);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                             "%s: DESTROYED", name);
        }
    }
    ImGui::Separator();
    ImGui::Text("Asteroids: %d", (int)_arena.asteroids.size());
    ImGui::Text("Torpedoes: %d", (int)_arena.torpedoes.size());
    ImGui::EndGroup();
}

void AstroBots::DrawDebugColliders(ImDrawList* drawList, ImVec2 offset) {
    // Colors
    ImU32 shipColor = IM_COL32(80, 255, 120, 180);
    ImU32 shipOutline = IM_COL32(30, 200, 90, 220);
    ImU32 asteroidColor = IM_COL32(255, 180, 60, 160);
    ImU32 asteroidOutline = IM_COL32(255, 210, 120, 220);
    ImU32 torpColor = IM_COL32(80, 180, 255, 200);
    ImU32 sweepColor = IM_COL32(80, 80, 255, 140);

    const float scale = _arena.renderScale;

    // Ships as capsules
    for (const auto& s : _arena.ships) {
        if (!s.alive) continue;
        // Match capsule used in collisions
        const float halfLen = 15.0f;
        const float radius = 7.5f;
        float ang = s.angle * (float)M_PI / 180.0f;
        float dx = std::cos(ang), dy = std::sin(ang);
        ImVec2 a = WorldToScreen(s.x - dx * halfLen, s.y - dy * halfLen);
        ImVec2 b = WorldToScreen(s.x + dx * halfLen, s.y + dy * halfLen);
        a.x += offset.x; a.y += offset.y;
        b.x += offset.x; b.y += offset.y;
        float rpx = radius * scale;
        // Thick line body approximating capsule hull
        drawList->AddLine(a, b, shipColor, rpx * 2.0f);
        // Endcaps
        drawList->AddCircle(a, rpx, shipOutline, 24, 2.0f);
        drawList->AddCircle(b, rpx, shipOutline, 24, 2.0f);
        // Outline along body
        drawList->AddLine(a, b, shipOutline, 2.0f);
    }

    // Asteroids as collision polys (local verts translated to world)
    for (const auto& a : _arena.asteroids) {
        if (!a.alive || a.shape.size() < 3) continue;
        // Build points
        std::vector<ImVec2> pts;
        pts.reserve(a.shape.size());
        for (const auto& v : a.shape) {
            ImVec2 p = WorldToScreen(a.x + v.x, a.y + v.y);
            p.x += offset.x; p.y += offset.y;
            pts.push_back(p);
        }
        // Triangulated fill to show region lightly
        ImVec2 center = WorldToScreen(a.x, a.y);
        center.x += offset.x; center.y += offset.y;
        for (size_t i = 0; i < pts.size(); ++i) {
            size_t j = (i + 1) % pts.size();
            drawList->AddTriangleFilled(center, pts[i], pts[j], IM_COL32(255, 180, 60, 40));
        }
        // Outline
        for (size_t i = 0; i < pts.size(); ++i) {
            size_t j = (i + 1) % pts.size();
            drawList->AddLine(pts[i], pts[j], asteroidOutline, 2.0f);
        }
    }

    // Torpedoes as circles + sweep segment (prev->curr)
    for (const auto& t : _arena.torpedoes) {
        if (!t.alive) continue;
        float rad = 5.0f * scale;
        ImVec2 p = WorldToScreen(t.x, t.y);
        p.x += offset.x; p.y += offset.y;
        drawList->AddCircle(p, rad, torpColor, 24, 2.0f);
        // Sweep (for debugging TOI)
        ImVec2 p0 = WorldToScreen(t.prevX, t.prevY);
        p0.x += offset.x; p0.y += offset.y;
        drawList->AddLine(p0, p, sweepColor, 1.5f);
    }
}

void AstroBots::endTurn() {
    if (!_gameRunning) return;

    _currentTurn++;

    if (_currentTurn > ASTRO_MAX_TURNS) {
        _gameRunning = false;
        return;
    }

    // Start turn (reset cooldowns, etc.)
    _arena.StartTurn();

    // Each alive ship takes a turn
    for (size_t i = 0; i < _arena.ships.size(); ++i) {
        if (!_arena.ships[i].alive) continue;
        _ships[i]->Run(_currentTurn);
    }

    // Update physics
    _arena.UpdatePhysics();

    // Handle collisions
    _arena.HandleCollisions();
    _arena.HandleTorpedoes();

    // After handling torpedo collisions based on unwrapped motion, wrap torpedoes
    for (auto& t : _arena.torpedoes) {
        if (t.alive) {
            _arena.WrapPosition(t.x, t.y);
        }
    }

    // Clean up dead torpedoes, asteroids, and phaser beams
    _arena.torpedoes.erase(
        std::remove_if(_arena.torpedoes.begin(), _arena.torpedoes.end(),
                      [](const PhotonTorpedo& t) { return !t.alive; }),
        _arena.torpedoes.end()
    );
    _arena.asteroids.erase(
        std::remove_if(_arena.asteroids.begin(), _arena.asteroids.end(),
                      [](const Asteroid& a) { return !a.alive; }),
        _arena.asteroids.end()
    );
    _arena.phaserBeams.erase(
        std::remove_if(_arena.phaserBeams.begin(), _arena.phaserBeams.end(),
                      [](const PhaserBeam& b) { return !b.alive; }),
        _arena.phaserBeams.end()
    );

    // Maintain asteroid population by spawning from edges with a cooldown
    if (_arena.edgeSpawnCooldown > 0) {
        _arena.edgeSpawnCooldown--;
    }
    if (_arena.asteroids.size() < NUM_INITIAL_ASTEROIDS && _arena.edgeSpawnCooldown == 0) {
        _arena.SpawnAsteroidFromEdge();
        _arena.edgeSpawnCooldown = 60; // spawn at most every ~2 seconds (at 30Hz)
    }

    // Update camera to follow action (center on average ship position)
    float avgX = 0, avgY = 0;
    int aliveCount = 0;
    for (const auto& s : _arena.ships) {
        if (s.alive) {
            avgX += s.x;
            avgY += s.y;
            aliveCount++;
        }
    }
    if (aliveCount > 0) {
        _cameraX = avgX / aliveCount;
        _cameraY = avgY / aliveCount;
    }

    // Check if game over
    int alive = 0;
    for (const auto& s : _arena.ships) {
        if (s.alive) alive++;
    }
    if (alive <= 1) {
        _gameRunning = false;
    }

    Game::endTurn();
}

bool AstroBots::actionForEmptyHolder(BitHolder &holder) {
    return false;
}

bool AstroBots::canBitMoveFrom(Bit &bit, BitHolder &src) {
    return false;
}

bool AstroBots::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) {
    return false;
}

void AstroBots::stopGame() {
    _gameRunning = false;

    // Clear all arena state
    _arena.torpedoes.clear();
    _arena.phaserBeams.clear();
    _arena.asteroids.clear();
    _arena.signals.clear();
    _arena.ships.clear();
    _arena.particles.clear();

    // Clear ship scripts
    _ships.clear();
    _logLines.clear();
}

Player* AstroBots::checkForWinner() {
    if (!_gameRunning) {
        int alive = 0;
        for (size_t i = 0; i < _arena.ships.size(); ++i) {
            if (_arena.ships[i].alive) {
                alive++;
            }
        }
        if (alive == 1) {
            return getPlayerAt(0);
        }
    }
    return nullptr;
}

bool AstroBots::checkForDraw() {
    if (!_gameRunning && _currentTurn >= ASTRO_MAX_TURNS) {
        int alive = 0;
        for (const auto& s : _arena.ships) {
            if (s.alive) alive++;
        }
        return alive > 1;
    }
    return false;
}

std::string AstroBots::initialStateString() {
    return stateString();
}

std::string AstroBots::stateString() {
    std::stringstream ss;
    ss << _currentTurn << ";";
    for (const auto& s : _arena.ships) {
        ss << s.x << "," << s.y << "," << s.vx << "," << s.vy << ","
           << s.angle << "," << s.hp << "," << s.fuel << "," << s.alive << ";";
    }
    return ss.str();
}

void AstroBots::setStateString(const std::string &s) {
    // Not fully implemented - would need to restore full state
}
