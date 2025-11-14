#include "Robots.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <random>

constexpr int botSize = 60;
static int randomIntInclusive(int lo, int hi){
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

// ===== VM implementation =====
void RobotBase::Run(int turn){
    int pc = 0; bool flag=false; // last condition
    while(pc < (int)code.size()){
        int op = code[pc++];
        switch(op){
            case OP_WAIT: break;
            case OP_MOVE: { int n=code[pc++]; A->Move(id,n); break; }
            case OP_TURN: { int d=code[pc++]; A->Turn(id,d); break; }
            case OP_ATTACK: { int d=code[pc++]; A->Attack(id,d); break; }
            case OP_ATTACK_SCAN: { A->AttackScan(id); break; }
            case OP_SIGNAL: { int v=code[pc++]; A->Signal(id,v); break; }
            case OP_SCAN: { A->Scan(id); break; }
            case OP_TURN_SCAN: { int d = A->bots[id].scan_dir; if(d>=0) A->Turn(id,d); break; }
            case OP_TURN_AWAY: { int d = A->bots[id].scan_dir; if(d>=0) A->Turn(id,(d+4)%8); break; }
            case OP_TURN_RANDOM: { A->Turn(id, randomIntInclusive(0,7)); break; }
            case OP_IF_ENEMY: { int d=code[pc++]; flag = A->EnemyAdjacent(id,d); break; }
            case OP_IF_TURN_LESS: { int t=code[pc++]; flag = (turn < t); break; }
            case OP_IF_SEEN: { /* param placeholder (unused) */ pc++; flag = (A->bots[id].scan_dist > 0); break; }
            case OP_IF_SCAN_LE: { int r=code[pc++]; flag = (A->bots[id].scan_dist > 0 && A->bots[id].scan_dist <= r); break; }
            case OP_IF_NEAR_SIGNAL: { int r=code[pc++]; flag = A->HasSignalNearby(id, r); break; }
            case OP_IF_DAMAGED: { pc++; flag = A->bots[id].damaged_last_turn; break; }
            case OP_IF_HP_LE: { int n=code[pc++]; flag = (A->bots[id].hp <= n); break; }
            case OP_IF_CAN_ATTACK: { pc++; flag = (A->bots[id].cooldown == 0); break; }
            case OP_IF_NEAR_EDGE: {
                int r=code[pc++];
                auto &b=A->bots[id];
                int to_left   = b.x;
                int to_right  = ROBOTS_W - 1 - b.x;
                int to_top    = b.y;
                int to_bottom = ROBOTS_H - 1 - b.y;
                int m = std::min(std::min(to_left,to_right), std::min(to_top,to_bottom));
                flag = (m <= r);
                break;
            }
            case OP_JUMP_IF_FALSE: { int tgt=code[pc++]; if(!flag) pc=tgt; break; }
            case OP_JUMP: { int tgt=code[pc++]; pc=tgt; break; }
            case OP_END: return;
            default: return;
        }
    }
}

// ===== Arena mechanics =====
bool Arena::EnemyAdjacent(int self, int dir){
    auto &b = bots[self]; int nx=b.x+dx[dir], ny=b.y+dy[dir];
    for (int i=0;i<(int)bots.size();++i){ if(i==self) continue; auto &o=bots[i]; if(o.alive && o.x==nx && o.y==ny) return true; }
    return false;
}

int Arena::BotAt(int x,int y){
    for (int i=0;i<(int)bots.size();++i){
        auto&o=bots[i];
        if(o.alive && o.x==x && o.y==y) return i;
    }
    return -1;
}

bool Arena::InBounds(int x,int y){
    return !(x<0||y<0||x>=ROBOTS_W||y>=ROBOTS_H);
}

void Arena::Move(int self,int dist){
    auto &b=bots[self];
    int startX = b.x, startY = b.y;
    while(dist-- && b.alive){
        int nx=b.x+dx[b.dir], ny=b.y+dy[b.dir];
        if(nx<0||ny<0||nx>=ROBOTS_W||ny>=ROBOTS_H) break; // wall
        if(BotAt(nx,ny)!=-1) break;        // blocked by bot
        b.x=nx; b.y=ny;
    }
    if ((b.x != startX || b.y != startY) && log) {
        std::string who = b.r ? b.r->name : std::string("Bot");
        log(who + " moves to " + squareName(b.x, b.y));
    }
}

void Arena::Turn(int self,int d){
    bots[self].dir = d;
}

void Arena::Attack(int self,int d){
    auto &b=bots[self];
    if(b.cooldown>0) return;
    int x=b.x, y=b.y;
    for(int step=1; step<=ATTACK_RANGE; ++step){
        x += dx[d]; y += dy[d];
        if(!InBounds(x,y)) break;
        int t = BotAt(x,y);
        if(t!=-1){
            std::string attacker = b.r ? b.r->name : std::string("Bot");
            std::string target = bots[t].r ? bots[t].r->name : std::string("Bot");
            bots[t].hp--;
            if (log) {
                log(attacker + " attacks " + target + " for 1 point!");
            }
            if(bots[t].hp<=0){ bots[t].alive=false; }
            if (!bots[t].alive && log) {
                log(target + " is destroyed!");
            }
            b.cooldown = ATTACK_COOLDOWN;
            return;
        }
    }
    // even a miss incurs cooldown
    b.cooldown = ATTACK_COOLDOWN;
    if (log) {
        std::string attacker = b.r ? b.r->name : std::string("Bot");
        log(attacker + " fires and misses.");
    }
}

void Arena::AttackScan(int self){
    auto &b=bots[self];
    if(b.scan_dir>=0) Attack(self, b.scan_dir);
}

void Arena::Scan(int self){
    auto &b=bots[self];
    int best_dist = 0;
    int best_dir  = -1;
    // Radial scan: find nearest alive enemy within Chebyshev distance
    for (int i = 0; i < (int)bots.size(); ++i) {
        if (i == self) continue;
        auto &o = bots[i];
        if (!o.alive) continue;
        int dxv = o.x - b.x;
        int dyv = o.y - b.y;
        int dist = std::max(std::abs(dxv), std::abs(dyv)); // Chebyshev distance
        if (dist == 0 || dist > SCAN_RANGE) continue;
        if (best_dist == 0 || dist < best_dist) {
            best_dist = dist;
            // Quantize vector to 8-way compass direction
            int sx = (dxv > 0) ? 1 : (dxv < 0 ? -1 : 0);
            int sy = (dyv > 0) ? 1 : (dyv < 0 ? -1 : 0);
            if (sx == 0 && sy == -1) best_dir = 0;           // NORTH
            else if (sx == 1 && sy == 0) best_dir = 1;       // EAST
            else if (sx == 0 && sy == 1) best_dir = 2;       // SOUTH
            else if (sx == -1 && sy == 0) best_dir = 3;      // WEST
            else if (sx == 1 && sy == -1) best_dir = 4;      // NORTHEAST
            else if (sx == 1 && sy == 1) best_dir = 5;       // SOUTHEAST
            else if (sx == -1 && sy == 1) best_dir = 6;      // SOUTHWEST
            else if (sx == -1 && sy == -1) best_dir = 7;     // NORTHWEST
        }
    }
    b.scan_dist = best_dist;
    b.scan_dir  = best_dir;
}

void Arena::Signal(int self, int value){
    auto &b=bots[self];
    b.signal = value;
    signals.emplace_back(b.x, b.y);
}

bool Arena::HasSignalNearby(int self, int radius){
    auto &b=bots[self];
    for(auto &p: signals){
        int dxv = abs(p.first - b.x);
        int dyv = abs(p.second - b.y);
        int dist = std::max(dxv, dyv);
        if(dist <= radius) return true;
    }
    return false;
}

void Arena::StartTurn(){
    signals.clear();
    for(auto &bs : bots){
        if(!bs.alive) continue;
        // track whether bot was damaged since previous turn
        bs.damaged_last_turn = (bs.hp < bs.last_hp);
        bs.last_hp = bs.hp;
        if(bs.cooldown>0) --bs.cooldown;
        bs.signal = -1;
    }
}

// ===== Sample robots implementation =====
int Pusher::SetupRobot() {
    SCAN();                 // Sense nearest enemy (radial). Sets scan_dist/scan_dir
    IF_SEEN() {             // If something was detected this turn...
        TURN_SCAN();        //   Face toward the scanned target
        ATTACK_SCAN();      //   Fire along line-of-sight in that direction
    }
    MOVE(1);                // Advance to apply pressure and close distance
    SIGNAL(1);              // Emit a signal (can be used by others or for logs)
    return Finalize();      // Seal program and return total script budget used
}

int Kamikaze::SetupRobot() {
    SCAN();                 // Look for enemies first
    IF_SEEN(){              // If a target is visible...
        TURN_SCAN();        //   Snap to face the target
        ATTACK_SCAN();      //   Try to land a shot immediately
        MOVE(1);            //   Keep momentum after firing
    }
    MOVE(2);                // Always surge forward (fast, aggressive style)
    SIGNAL(1);              // Mark presence/pressure zone
    return Finalize();      // Done
}

int Shy::SetupRobot() {
    SCAN();                 // Gather info before deciding
    IF_SCAN_LE(5){          // If an enemy is within 5 tiles (Chebyshev)...
        TURN_AWAY();        //   Face away from the threat
        MOVE(2);            //   Create distance quickly
        SIGNAL(2);          //   Drop a 'danger' signal (useful for others)
    } ELSE(){               // Otherwise (no nearby threat)...
        MOVE(1);            //   Drift slowly to reposition over time
    }
    return Finalize();      // Done
}

struct Hunter : RobotBase {
    Hunter(){ name = "Hunter"; }
    int SetupRobot() override {
        SCAN();                                 // Always gather info first
        IF_DAMAGED(){                           // If we took damage last turn...
            TURN_AWAY();                        //   Face away from the likely attacker
            MOVE(1);                            //   Create a bit of space
            SIGNAL(2);                          //   Mark danger zone
        }
        IF_SEEN(){                              // If we have a target in memory...
            IF_CAN_ATTACK(){                    //   If weapon is ready...
                TURN_SCAN();                    //     Face target
                ATTACK_SCAN();                  //     Shoot!
            } ELSE(){                           //   Weapon cooling down
                IF_SCAN_LE(3){                  //     Too close? back off a little
                    TURN_AWAY();
                    MOVE(1);
                } ELSE(){
                    TURN_SCAN();                //     Otherwise close the distance
                    MOVE(1);
                }
            }
        }
        IF_NEAR_EDGE(1){                        // Avoid hugging walls
            TURN_RANDOM();                      //   Nudge direction randomly
            MOVE(1);
        }
        return Finalize();
    }
};

// ===== Robots game implementation =====
Robots::Robots()
{
    _grid = new Grid(ROBOTS_W, ROBOTS_H);
    _currentTurn = 0;
    _gameRunning = false;
}

Robots::~Robots()
{
    delete _grid;
}

std::vector<std::unique_ptr<RobotBase>> Robots::makeClassBots(){
    std::vector<std::unique_ptr<RobotBase>> v;
    v.emplace_back(std::make_unique<Pusher>());
    v.emplace_back(std::make_unique<Kamikaze>());
    v.emplace_back(std::make_unique<Shy>());
    v.emplace_back(std::make_unique<Hunter>());
    return v;
}

Bit* Robots::BotBit(int botIndex)
{
    if (botIndex < 0 || botIndex >= (int)_bots.size()) {
        return nullptr;
    }

    Bit* bit = new Bit();
    // Use different sprites or fall back to a default
    if (botIndex < 6) {
        int index = botIndex + 1;
        // filename format is robot_01.png etc... so we need to pad the index with a 0 if it's less than 10
        std::string filename = std::string("robot_") + std::string(index < 10 ? "0" : "") + std::to_string(index) + ".png";
        bit->LoadTextureFromFile(filename.c_str());
    } else {
        // Fallback for more than 6 robots
        bit->LoadTextureFromFile("robot_01.png");
    }
    bit->setOwner(getPlayerAt(0)); // All bots belong to player 0 for now
    bit->setSize(botSize, botSize);
    bit->setGameTag(botIndex);

    return bit;
}

void Robots::setUpBoard()
{
    setNumberOfPlayers(1); // Single player watching the battle
    _gameOptions.rowX = ROBOTS_W;
    _gameOptions.rowY = ROBOTS_H;

    _grid->initializeSquares(botSize, "ground.png");

    // Initialize bots
    _bots = makeClassBots();
    _arena.bots.resize(_bots.size());
	_botBits.clear();
	_botBits.resize(_bots.size(), nullptr);
	_logLines.clear();

    // Validate scripts & inject arena refs
    for(size_t i=0; i<_bots.size(); ++i){
        int cost = _bots[i]->SetupRobot();
        if(cost > MAX_SCRIPT_COST){
            // Log error but continue
        }
        // Log script budget usage
        {
            std::string nm = _bots[i] ? _bots[i]->name : std::string("Bot");
            std::string line = nm + " script cost " + std::to_string(cost) + "/" + std::to_string(MAX_SCRIPT_COST);
            if (cost > MAX_SCRIPT_COST) line += " (EXCEEDS LIMIT)";
            _logLines.push_back(line);
            if (_logLines.size() > 500) {
                _logLines.erase(_logLines.begin(), _logLines.begin() + (_logLines.size() - 500));
            }
        }
        _arena.bots[i].r = _bots[i].get();
        _arena.bots[i].glyph = char('A'+(int)i);
        _bots[i]->A = &_arena;
        _bots[i]->id = (int)i;
    }
	// Hook up logger
	_arena.log = [this](const std::string& line){
		_logLines.push_back(line);
		// keep the log from growing unbounded
		if (_logLines.size() > 500) {
			_logLines.erase(_logLines.begin(), _logLines.begin() + (_logLines.size() - 500));
		}
	};

    // Interior lattice spawn pattern (spaced), randomized order each game (no edges)
    std::vector<std::pair<int,int>> spawns;
    for(int y=1; y<ROBOTS_H-1; y+=2) {
        for(int x=1; x<ROBOTS_W-1; x+=3) {
            spawns.push_back({x,y});
        }
    }
    // Shuffle to avoid lining up on the same rows/columns each match
    {
        std::mt19937 rng(std::random_device{}());
        std::shuffle(spawns.begin(), spawns.end(), rng);
    }

    for(size_t i=0; i<_arena.bots.size(); ++i){
        _arena.bots[i].x = spawns[i % spawns.size()].first;
        _arena.bots[i].y = spawns[i % spawns.size()].second;

        // Create and place bit on grid
        Bit* bit = BotBit(i);
        ChessSquare* square = _grid->getSquare(_arena.bots[i].x, _arena.bots[i].y);
        bit->setPosition(square->getPosition());
        bit->setParent(square);
        square->setBit(bit);
		_botBits[i] = bit;
    }

    _currentTurn = 0;
    _gameRunning = true;

    startGame();
}

void Robots::drawFrame()
{
    Game::drawFrame();

    // Update bot positions on the grid if game is running
    if (_gameRunning && _currentTurn < MAX_TURNS) {
        updateBotPositions();
    }

	// Draw overlays: health bars and names
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 win_pos = ImGui::GetWindowPos();

	for (size_t i = 0; i < _arena.bots.size(); ++i) {
		if (i >= _botBits.size()) break;
		auto &bs = _arena.bots[i];
		if (!bs.alive) continue;
		Bit* bit = _botBits[i];
		if (!bit) continue;

		ImVec2 bitPos = bit->getPosition(); // window-local
		ImVec2 p = ImVec2(win_pos.x + bitPos.x, win_pos.y + bitPos.y); // screen space

		// Health bar geometry (above the sprite)
		const float barWidth = (float)botSize;
		const float barHeight = 6.0f;
		const float barYOffset = 8.0f;
		ImVec2 barTL = ImVec2(p.x, p.y - barYOffset - barHeight);
		ImVec2 barBR = ImVec2(p.x + barWidth, p.y - barYOffset);

		// Background
		draw_list->AddRectFilled(barTL, barBR, IM_COL32(40, 40, 40, 200), 2.0f);
		// Health fill
		float ratio = (float)bs.hp / (float)START_HP;
		if (ratio < 0.0f) ratio = 0.0f;
		if (ratio > 1.0f) ratio = 1.0f;
		ImVec2 fillBR = ImVec2(barTL.x + barWidth * ratio, barBR.y);
		// Gradient-ish color from red to green
		int r = (int)((1.0f - ratio) * 220.0f);
		int g = (int)(ratio * 220.0f);
		draw_list->AddRectFilled(barTL, fillBR, IM_COL32(r, g, 64, 230), 2.0f);
		draw_list->AddRect(barTL, barBR, IM_COL32(0, 0, 0, 200), 2.0f, 0, 1.0f);

		// Name label below the health bar (avoid clipping at top of board)
		const char* label = _bots[i] ? _bots[i]->name.c_str() : "?";
		ImVec2 textPos = ImVec2(barTL.x, barBR.y + 2.0f);
		draw_list->AddText(textPos, IM_COL32(255, 255, 255, 255), label);

		// Facing direction overlay (arrow)
		{
			ImVec2 center = ImVec2(p.x + botSize * 0.5f, p.y + botSize * 0.5f);
			int dir = bs.dir;
			if (dir >= 0 && dir < 8) {
				float vx = (float)_arena.dx[dir];
				float vy = (float)_arena.dy[dir];
				float mag = std::sqrt(vx*vx + vy*vy);
				if (mag > 0.0f) {
					float len = 18.0f;
					ImVec2 v = ImVec2(vx / mag * len, vy / mag * len);
					ImVec2 tip = ImVec2(center.x + v.x, center.y + v.y);
					ImU32 col = IM_COL32(80, 220, 255, 220);
					// main shaft
					draw_list->AddLine(center, tip, col, 2.0f);
					// arrow head
					ImVec2 back = ImVec2(tip.x - v.x * 0.35f, tip.y - v.y * 0.35f);
					ImVec2 perp = ImVec2(-v.y / len, v.x / len); // normalized perpendicular
					float headW = 5.0f;
					ImVec2 left = ImVec2(back.x + perp.x * headW, back.y + perp.y * headW);
					ImVec2 right = ImVec2(back.x - perp.x * headW, back.y - perp.y * headW);
					draw_list->AddTriangleFilled(left, tip, right, col);
				}
			}
		}
	}

	// Logging window
	ImGui::Begin("Robots Log");
	if (ImGui::Button("Clear")) {
		_logLines.clear();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &_logAutoScroll);
	ImGui::Separator();
	ImGui::BeginChild("scroll_region", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
	for (const auto& line : _logLines) {
		ImGui::TextUnformatted(line.c_str());
	}
	if (_logAutoScroll) {
		ImGui::SetScrollHereY(1.0f);
	}
	ImGui::EndChild();
	ImGui::End();
}

void Robots::endTurn()
{
    if (!_gameRunning) {
        return;
    }

    _currentTurn++;

    if (_currentTurn > MAX_TURNS) {
        _gameRunning = false;
        // Inform the log/UI that the match ended in a draw due to turn limit
        if (_arena.log) {
            _arena.log("Draw: maximum turns reached.");
        }
        Game::endTurn();
        return;
    }

    // Run one turn of the arena
    _arena.StartTurn();

    // Each alive bot takes a turn in id order (deterministic)
    for(size_t i=0; i<_arena.bots.size(); ++i){
        if(!_arena.bots[i].alive) continue;
        _bots[i]->Run(_currentTurn);
    }

    updateBotPositions();

    // Check if one bot remains
    int alive = 0;
    for(size_t i=0; i<_arena.bots.size(); ++i){
        if(_arena.bots[i].alive){
            alive++;
        }
    }

    if(alive <= 1){
        _gameRunning = false;
    }

    Game::endTurn();
}

void Robots::updateBotPositions()
{
	// Pass 1: Clean up any dead bots first to avoid deleting their Bit later via setBit on another square
	for(size_t i=0; i<_arena.bots.size(); ++i){
		if (_arena.bots[i].alive) continue;
		if (i < _botBits.size() && _botBits[i]) {
			Bit* bit = _botBits[i];
			BitHolder* holder = bit->getHolder();
			if (holder) {
				holder->destroyBit();
			} else {
				delete bit;
			}
			_botBits[i] = nullptr;
		}
	}

	// Pass 2: Move or create bits to match current arena positions (animated)
	for(size_t i=0; i<_arena.bots.size(); ++i){
		if (!_arena.bots[i].alive) continue;
        int x = _arena.bots[i].x;
        int y = _arena.bots[i].y;

		// Ensure bit exists
		if (i >= _botBits.size()) {
			_botBits.resize(i + 1, nullptr);
		}
		Bit* bit = _botBits[i];
		ChessSquare* target = _grid->getSquare(x, y);

		if (!bit) {
			// Create a new bit for this bot
			bit = BotBit((int)i);
			// Place immediately (no animation on first placement)
			bit->setPosition(target->getPosition());
			target->setBit(bit); // also sets parent
			_botBits[i] = bit;
			continue;
		}

		// If already at the target holder, nothing to do
		BitHolder* currentHolder = bit->getHolder();
		if (currentHolder == target) {
			continue;
		}

		// Move the bit to the new holder and animate
		// Ensure target doesn't hold a stray bit (shouldn't in normal movement)
		if (target->bit() && target->bit() != bit) {
			target->destroyBit();
		}
		target->setBit(bit);              // this also sets the parent
		bit->moveTo(target->getPosition());
		// Do NOT manually clear the old holder; it will self-clear via bit() accessor on next use
    }
}

bool Robots::actionForEmptyHolder(BitHolder &holder)
{
    // No user interaction - bots move automatically
    return false;
}

bool Robots::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    // User cannot move bots manually
    return false;
}

bool Robots::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    // User cannot move bots manually
    return false;
}

void Robots::stopGame()
{
    _gameRunning = false;
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
    // Clear local references to deleted Bits
    for (auto &ptr : _botBits) {
        ptr = nullptr;
    }
    _botBits.clear();
    // Reset arena and bots
    _arena = Arena();          // clears bots, signals, cooldowns, logger, etc.
    _bots.clear();             // release any VM instances
    // Reset turn/log state
    _currentTurn = 0;
    _logLines.clear();
}

Player* Robots::checkForWinner()
{
    if (!_gameRunning) {
        int alive = 0;
        int lastBot = -1;
        for(size_t i=0; i<_arena.bots.size(); ++i){
            if(_arena.bots[i].alive){
                alive++;
                lastBot = i;
            }
        }

        if (alive == 1) {
            return getPlayerAt(0); // Winner found
        }
    }
    return nullptr;
}

bool Robots::checkForDraw()
{
    if (!_gameRunning && _currentTurn >= MAX_TURNS) {
        int alive = 0;
        for(size_t i=0; i<_arena.bots.size(); ++i){
            if(_arena.bots[i].alive){
                alive++;
            }
        }
        return alive > 1; // Draw if multiple bots still alive after max turns
    }
    return false;
}

std::string Robots::initialStateString()
{
    return stateString();
}

std::string Robots::stateString()
{
    std::stringstream ss;
    ss << _currentTurn << ";";

    for(size_t i=0; i<_arena.bots.size(); ++i){
        auto& bot = _arena.bots[i];
        ss << bot.x << "," << bot.y << "," << bot.hp << "," << bot.alive << "," << bot.dir << ";";
    }

    return ss.str();
}

void Robots::setStateString(const std::string &s)
{
    // Parse state string and restore game state
    // Format: turn;x,y,hp,alive,dir;x,y,hp,alive,dir;...
    std::istringstream ss(s);
    std::string token;

    // Read turn
    std::getline(ss, token, ';');
    _currentTurn = std::stoi(token);

    // Read bot states
    size_t botIndex = 0;
    while(std::getline(ss, token, ';') && botIndex < _arena.bots.size()){
        std::istringstream botStream(token);
        std::string val;

        std::getline(botStream, val, ',');
        _arena.bots[botIndex].x = std::stoi(val);

        std::getline(botStream, val, ',');
        _arena.bots[botIndex].y = std::stoi(val);

        std::getline(botStream, val, ',');
        _arena.bots[botIndex].hp = std::stoi(val);

        std::getline(botStream, val, ',');
        _arena.bots[botIndex].alive = (std::stoi(val) != 0);

        std::getline(botStream, val, ',');
        _arena.bots[botIndex].dir = std::stoi(val);

        botIndex++;
    }

    updateBotPositions();
}
