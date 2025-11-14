#pragma once

#include "Game.h"
#include "Grid.h"
#include <array>
#include <memory>
#include <string>
#include <functional>

// ===== Arena config =====
static constexpr int ROBOTS_W = 12, ROBOTS_H = 12;        // board size
static constexpr int MAX_TURNS = 40;        // length of the match
static constexpr int START_HP = 5;          // health per bot
static constexpr int MAX_SCRIPT_COST = 20;  // budget per bot (tune in class)
static constexpr int ATTACK_RANGE = 4;      // line-of-sight attack range
static constexpr int ATTACK_COOLDOWN = 2;   // turns to cool down after firing
static constexpr int SCAN_RANGE = 12;        // max scan distance

// ===== Opcodes / DSL =====
enum OpCode {
    // actions
    OP_WAIT, OP_MOVE, OP_TURN, OP_ATTACK, OP_SIGNAL, OP_ATTACK_SCAN, OP_SCAN, OP_TURN_SCAN, OP_TURN_AWAY, OP_TURN_RANDOM,
    // conditions
    OP_IF_ENEMY, OP_IF_TURN_LESS, OP_IF_SEEN, OP_IF_SCAN_LE, OP_IF_NEAR_SIGNAL,
    OP_IF_DAMAGED, OP_IF_HP_LE, OP_IF_CAN_ATTACK, OP_IF_NEAR_EDGE,
    // flow control
    OP_JUMP_IF_FALSE, OP_JUMP, OP_END
};

enum Direction { NORTH=0, EAST=1, SOUTH=2, WEST=3, NORTHEAST=4, SOUTHEAST=5, SOUTHWEST=6, NORTHWEST=7 };

// energy costs (only used for compile‑time budget)
enum ActionCost { COST_WAIT=0, COST_TURN=1, COST_SIGNAL=1, COST_MOVE=2, COST_ATTACK=3, COST_SCAN=1 };

// forward decl
struct Arena;

// ===== RobotBase: tiny VM with an educational macro language =====
struct RobotBase {
    std::vector<int> code;        // p‑code
    int script_cost = 0;     // compile‑time budget
    std::string name = "Bot";    // printed name
    struct IfContext { int jumpIfFalseIndex=-1; int jumpToEndIndex=-1; };
    std::vector<IfContext> _ifCtx;

    // RAII helper so IF_{} patches jump target automatically
    struct IfBlock {
        RobotBase* self; IfBlock(RobotBase* s, OpCode cond, int param): self(s){
            self->code.push_back(cond); self->code.push_back(param);
            self->code.push_back(OP_JUMP_IF_FALSE); self->code.push_back(0); // placeholder
            RobotBase::IfContext ctx; ctx.jumpIfFalseIndex = (int)self->code.size()-1; ctx.jumpToEndIndex = -1;
            self->_ifCtx.push_back(ctx);
        }
        ~IfBlock(){
            if (!self->_ifCtx.empty()){
                RobotBase::IfContext &ctx = self->_ifCtx.back();
                // If no ELSE() was emitted, patch false-jump to end of IF block
                if (ctx.jumpToEndIndex == -1){
                    self->code[ctx.jumpIfFalseIndex] = (int)self->code.size();
                }
                self->_ifCtx.pop_back();
            }
        }
        explicit operator bool() const { return true; }
    };
    struct ElseBlock {
        RobotBase* self;
        ElseBlock(RobotBase* s): self(s){
            // Begin ELSE: jump over else body, patch IF's false to here
            self->code.push_back(OP_JUMP); self->code.push_back(0); // placeholder to end of else
            int jumpToEndIdx = (int)self->code.size()-1;
            if (!self->_ifCtx.empty()){
                RobotBase::IfContext &ctx = self->_ifCtx.back();
                self->code[ctx.jumpIfFalseIndex] = (int)self->code.size(); // start of ELSE
                ctx.jumpToEndIndex = jumpToEndIdx;
            }
        }
        ~ElseBlock(){
            if (!self->_ifCtx.empty()){
                RobotBase::IfContext &ctx = self->_ifCtx.back();
                if (ctx.jumpToEndIndex != -1){
                    self->code[ctx.jumpToEndIndex] = (int)self->code.size(); // end of ELSE
                }
            }
        }
        explicit operator bool() const { return true; }
    };

    // DSL: bot coders will use these in SetupRobot()
    #define MOVE(N)        do{ code.push_back(OP_MOVE); code.push_back((N)); script_cost += COST_MOVE * (N); }while(0)
    #define TURN(D)        do{ code.push_back(OP_TURN); code.push_back((D)); script_cost += COST_TURN; }while(0)
    #define ATTACK(D)      do{ code.push_back(OP_ATTACK); code.push_back((D)); script_cost += COST_ATTACK; }while(0)
    #define ATTACK_SCAN()  do{ code.push_back(OP_ATTACK_SCAN); script_cost += COST_ATTACK; }while(0)
    #define SIGNAL(V)      do{ code.push_back(OP_SIGNAL); code.push_back((V)); script_cost += COST_SIGNAL; }while(0)
    #define WAIT_()        do{ code.push_back(OP_WAIT); script_cost += COST_WAIT; }while(0)
    #define SCAN()         do{ code.push_back(OP_SCAN); script_cost += COST_SCAN; }while(0)
    #define TURN_SCAN()    do{ code.push_back(OP_TURN_SCAN); script_cost += COST_TURN; }while(0)
    #define TURN_AWAY()    do{ code.push_back(OP_TURN_AWAY); script_cost += COST_TURN; }while(0)
    #define TURN_RANDOM()  do{ code.push_back(OP_TURN_RANDOM); script_cost += COST_TURN; }while(0)

    #define IF_ENEMY(D)    if (IfBlock _cb##__LINE__{this, OP_IF_ENEMY, (D)})
    #define IF_TURN_LT(T)  if (IfBlock _cb##__LINE__{this, OP_IF_TURN_LESS, (T)})
    #define IF_SEEN()      if (IfBlock _cb##__LINE__{this, OP_IF_SEEN, 0})
    #define IF_SCAN_LE(R)  if (IfBlock _cb##__LINE__{this, OP_IF_SCAN_LE, (R)})
    #define IF_NEAR_SIGNAL(R) if (IfBlock _cb##__LINE__{this, OP_IF_NEAR_SIGNAL, (R)})
    #define IF_DAMAGED()   if (IfBlock _cb##__LINE__{this, OP_IF_DAMAGED, 0})
    #define IF_HP_LE(N)    if (IfBlock _cb##__LINE__{this, OP_IF_HP_LE, (N)})
    #define IF_CAN_ATTACK() if (IfBlock _cb##__LINE__{this, OP_IF_CAN_ATTACK, 0})
    #define IF_NEAR_EDGE(R) if (IfBlock _cb##__LINE__{this, OP_IF_NEAR_EDGE, (R)})
    #define ELSE() else if (ElseBlock _eb##__LINE__{this})

    int Finalize(){ code.push_back(OP_END); return script_cost; }

    // hooks provided by Arena at runtime
    Arena* A = nullptr; int id = -1; // injected
    virtual int SetupRobot() = 0;    // bot coders will implement this

    // interpreter
    void Run(int turn);
};

// ===== Arena state & mechanics =====
struct Arena {
    struct BotState {
        int x=0,y=0;
        int dir=EAST;
        int hp=START_HP;
        int last_hp=START_HP;   // hp snapshot at start of previous turn
        bool damaged_last_turn=false;
        bool alive=true;
        RobotBase* r=nullptr;
        char glyph='?';
        int scan_dist=0;   // 0 means nothing seen
        int scan_dir=-1;   // -1 means none
        int cooldown=0;    // turns until next attack available
        int signal=-1;     // value signaled this turn, -1 if none
    };
    std::vector<BotState> bots;
    std::array<int,8> dx{0,1,0,-1, 1, 1,-1,-1};
    std::array<int,8> dy{-1,0,1,0, -1, 1, 1,-1};
    std::vector<std::pair<int,int>> signals; // positions that emitted a signal this turn
    std::function<void(const std::string&)> log; // optional logger callback

    // world queries used by VM
    bool EnemyAdjacent(int self, int dir);
    int BotAt(int x,int y);
    bool InBounds(int x,int y);
    void Move(int self,int dist);
    void Turn(int self,int d);
    void Attack(int self,int d);
    void AttackScan(int self);
    void Scan(int self);
    void Signal(int self, int value);
    bool HasSignalNearby(int self, int radius);
    void StartTurn();
private:
    std::string squareName(int x, int y) {
        char col = (char)('A' + x);
        int row = y + 1;
        return std::string(1, col) + std::to_string(row);
    }
};

// ===== Sample robots =====
struct Pusher : RobotBase {
    Pusher(){ name="Pusher"; }
    int SetupRobot() override;
};

struct Kamikaze : RobotBase {
    Kamikaze(){ name="Kamikaze"; }
    int SetupRobot() override;
};

struct Shy : RobotBase {
    Shy(){ name="Shy"; }
    int SetupRobot() override;
};

// ===== Main game class =====
class Robots : public Game
{
public:
    Robots();
    ~Robots();

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

    Grid* getGrid() override { return _grid; }

private:
    Bit* BotBit(int botIndex);
    void updateBotPositions();
    std::vector<std::unique_ptr<RobotBase>> makeClassBots();

    Grid* _grid;
    Arena _arena;
    std::vector<std::unique_ptr<RobotBase>> _bots;
	std::vector<Bit*> _botBits;
    std::vector<std::string> _logLines;
    bool _logAutoScroll = true;
    int _currentTurn;
    bool _gameRunning;
};
