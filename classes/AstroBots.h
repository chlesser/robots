#pragma once

#include "Game.h"
#include <array>
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <cmath>

#include "AstroTypes.h"
#include "AstroArena.h"

// ===== ShipBase: tiny VM with space combat Domain-Specific Language =====
struct ShipBase {
    std::vector<int> code;
    std::vector<float> floatParams; // for storing float parameters like thrust power
    int script_cost = 0;
    std::string name = "Ship";

    // RAII helper for IF_{} blocks
    struct IfBlock {
        ShipBase* self;
        int patch_idx;
        IfBlock(ShipBase* s, AstroOpCode cond, int param): self(s) {
            self->code.push_back(cond);
            self->code.push_back(param);
            self->code.push_back(ASTRO_OP_JUMP_IF_FALSE);
            self->code.push_back(0); // placeholder
            patch_idx = (int)self->code.size()-1;
        }
        ~IfBlock() { self->code[patch_idx] = (int)self->code.size(); }
        explicit operator bool() const { return true; }
    };

    // DSL: bot coders will use these in SetupShip()
    #define THRUST(P)    do{ code.push_back(ASTRO_OP_THRUST); code.push_back((int)((P)*10)); script_cost += ASTRO_COST_THRUST; }while(0)
    #define TURN_DEG(D)  do{ code.push_back(ASTRO_OP_TURN_DEG); code.push_back((D)); script_cost += ASTRO_COST_TURN; }while(0)
    #define FIRE_PHASER() do{ code.push_back(ASTRO_OP_FIRE_PHASER); script_cost += ASTRO_COST_PHASER; }while(0)
    #define FIRE_PHOTON() do{ code.push_back(ASTRO_OP_FIRE_PHOTON); script_cost += ASTRO_COST_PHOTON; }while(0)
    #define SCAN()       do{ code.push_back(ASTRO_OP_SCAN); script_cost += ASTRO_COST_SCAN; }while(0)
    #define SIGNAL(V)    do{ code.push_back(ASTRO_OP_SIGNAL); code.push_back((V)); script_cost += ASTRO_COST_SIGNAL; }while(0)
    #define WAIT_()      do{ code.push_back(ASTRO_OP_WAIT); script_cost += ASTRO_COST_WAIT; }while(0)
    #define TURN_TO_SCAN() do{ code.push_back(ASTRO_OP_TURN_TO_SCAN); script_cost += ASTRO_COST_TURN; }while(0)

    #define IF_SEEN()      if (IfBlock _cb##__LINE__{this, ASTRO_OP_IF_SEEN, 0})
    #define IF_SCAN_LE(R)  if (IfBlock _cb##__LINE__{this, ASTRO_OP_IF_SCAN_LE, (R)})
    #define IF_SHIP_DAMAGED()   if (IfBlock _cb##__LINE__{this, ASTRO_OP_IF_DAMAGED, 0})
    #define IF_SHIP_HP_LE(N)    if (IfBlock _cb##__LINE__{this, ASTRO_OP_IF_HP_LE, (N)})
    #define IF_SHIP_FUEL_LE(N)  if (IfBlock _cb##__LINE__{this, ASTRO_OP_IF_FUEL_LE, (N)})
    #define IF_SHIP_CAN_FIRE_PHASER()  if (IfBlock _cb##__LINE__{this, ASTRO_OP_IF_CAN_FIRE_PHASER, 0})
    #define IF_SHIP_CAN_FIRE_PHOTON()  if (IfBlock _cb##__LINE__{this, ASTRO_OP_IF_CAN_FIRE_PHOTON, 0})

    int Finalize() { code.push_back(ASTRO_OP_END); return script_cost; }

    // hooks provided by Arena at runtime
    AstroArena* A = nullptr;
    int id = -1;
    virtual int SetupShip() = 0; // bot coders will implement this
    virtual ~ShipBase() = default;

    // interpreter
    void Run(int turn);
};

// ===== Sample ships =====
struct HunterShip : ShipBase {
    HunterShip() { name = "Hunter"; }
    int SetupShip() override;
};

struct DroneShip : ShipBase {
    DroneShip() { name = "Drone"; }
    int SetupShip() override;
};

struct MinerShip : ShipBase {
    MinerShip() { name = "Miner"; }
    int SetupShip() override;
};

struct GraemeShip : ShipBase {
    GraemeShip() { name = "Graeme"; }
    int SetupShip() override;
};
struct Crackhead2Ship : ShipBase {
    Crackhead2Ship() { name = "Crackhead 2"; }
    int SetupShip() override;
};

// ===== Main game class =====
class AstroBots : public Game
{
public:
    AstroBots();
    ~AstroBots();

    void setUpBoard() override;
    void drawFrame() override;
    void endTurn() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;

    void stopGame() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;

    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override { return nullptr; } // No grid in AstroBots

private:
    void DrawShip(ImDrawList* drawList, const AstroArena::ShipState& ship, ImVec2 offset);
    void DrawAsteroid(ImDrawList* drawList, const Asteroid& asteroid, ImVec2 offset);
    void DrawTorpedo(ImDrawList* drawList, const PhotonTorpedo& torpedo, ImVec2 offset);
    void DrawPhaserBeam(ImDrawList* drawList, const PhaserBeam& beam, ImVec2 offset);
    void DrawParticles(ImDrawList* drawList, const std::vector<Particle>& particles, ImVec2 offset);
    void DrawShipDebris(ImDrawList* drawList, const std::vector<ShipDebrisSegment>& debris, ImVec2 offset);
    void DrawHUD();
    void DrawDebugColliders(ImDrawList* drawList, ImVec2 offset);
    ImVec2 WorldToScreen(float x, float y);

    std::vector<std::unique_ptr<ShipBase>> makeShips();

    AstroArena _arena;
    std::vector<std::unique_ptr<ShipBase>> _ships;
    std::vector<std::string> _logLines;
    bool _logAutoScroll = true;
    bool _showColliders = false;
    int _currentTurn;
    bool _gameRunning;

    // Camera/viewport
    float _cameraX = ASTROBOTS_W / 2.0f;
    float _cameraY = ASTROBOTS_H / 2.0f;
    float _zoom = 1.0f;
};
