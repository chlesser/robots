#pragma once

#include "Game.h"
#include "Grid.h"

const int CONNECT4_COLS = 7;
const int CONNECT4_ROWS = 6;

class Connect4 : public Game
{
public:
    Connect4();
    ~Connect4();

    void setUpBoard() override;

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
    Bit* PieceForPlayer(const int playerNumber);
    int getLowestEmptyRow(int col);
    bool isColumnFull(int col);
    Player* ownerAt(int x, int y) const;
    bool checkDirection(int startX, int startY, int dx, int dy, Player* player);

    Grid* _grid;
};