#include "Connect4.h"
#include <limits>
#include <cmath>

Connect4::Connect4()
{
    _grid = new Grid(CONNECT4_COLS, CONNECT4_ROWS);
}

Connect4::~Connect4()
{
    delete _grid;
}

Bit* Connect4::PieceForPlayer(const int playerNumber)
{
    Bit *bit = new Bit();
    bit->LoadTextureFromFile(playerNumber == AI_PLAYER ? "yellow.png" : "red.png");
    bit->setOwner(getPlayerAt(playerNumber == AI_PLAYER ? 1 : 0));
    return bit;
}

void Connect4::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = CONNECT4_COLS;
    _gameOptions.rowY = CONNECT4_ROWS;

    _grid->initializeSquares(80, "square.png");

    startGame();
}

bool Connect4::actionForEmptyHolder(BitHolder &holder)
{
    ChessSquare* clickedSquare = dynamic_cast<ChessSquare*>(&holder);
    if (!clickedSquare) {
        return false;
    }

    int col = clickedSquare->getColumn();

    if (col == -1) {
        return false;
    }

    if (isColumnFull(col)) {
        return false;
    }

    int targetRow = getLowestEmptyRow(col);
    if (targetRow == -1) {
        return false;
    }

    Bit *bit = PieceForPlayer(getCurrentPlayer()->playerNumber() == 0 ? HUMAN_PLAYER : AI_PLAYER);
    if (bit) {
        ChessSquare* topSquare = _grid->getSquare(col, 0);
        ChessSquare* targetSquare = _grid->getSquare(col, targetRow);

        if (targetRow > 0) {
            bit->setPosition(topSquare->getPosition());
            bit->moveTo(targetSquare->getPosition());
            targetSquare->setBit(bit);
            endTurn();
        } else {
            bit->setPosition(targetSquare->getPosition());
            targetSquare->setBit(bit);
            endTurn();
        }
        return true;
    }
    return false;
}

int Connect4::getLowestEmptyRow(int col)
{
    for (int row = CONNECT4_ROWS - 1; row >= 0; row--) {
        ChessSquare* square = _grid->getSquare(col, row);
        if (square && !square->bit()) {
            return row;
        }
    }
    return -1;
}

bool Connect4::isColumnFull(int col)
{
    ChessSquare* topSquare = _grid->getSquare(col, 0);
    return topSquare && topSquare->bit() != nullptr;
}

bool Connect4::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    return false;
}

bool Connect4::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    return false;
}

void Connect4::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
}

Player* Connect4::ownerAt(int x, int y) const
{
    if (x < 0 || x >= CONNECT4_COLS || y < 0 || y >= CONNECT4_ROWS) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

// Searches in a specific direction (dx, dy) from a starting point (startX, startY)
// to check if there are 4 consecutive pieces belonging to the same player.
bool Connect4::checkDirection(int startX, int startY, int dx, int dy, Player* player)
{
    int count = 0;
    for (int i = 0; i < 4; i++) {
        int x = startX + i * dx;
        int y = startY + i * dy;

        if (ownerAt(x, y) == player) {
            count++;
        } else {
            break;
        }
    }
    return count >= 4;
}

// Checks for a Connect 4 winner by examining all possible 4-in-a-row combinations:
// horizontal, vertical, and both diagonal directions.

Player* Connect4::checkForWinner()
{
    for (int y = 0; y < CONNECT4_ROWS; y++) {
        for (int x = 0; x < CONNECT4_COLS; x++) {
            Player* player = ownerAt(x, y);
            if (player) {
                if (x <= CONNECT4_COLS - 4 && checkDirection(x, y, 1, 0, player)) {
                    return player;
                }

                if (y <= CONNECT4_ROWS - 4 && checkDirection(x, y, 0, 1, player)) {
                    return player;
                }

                if (x <= CONNECT4_COLS - 4 && y <= CONNECT4_ROWS - 4 &&
                    checkDirection(x, y, 1, 1, player)) {
                    return player;
                }

                if (x >= 3 && y <= CONNECT4_ROWS - 4 &&
                    checkDirection(x, y, -1, 1, player)) {
                    return player;
                }
            }
        }
    }
    return nullptr;
}

bool Connect4::checkForDraw()
{
    for (int x = 0; x < CONNECT4_COLS; x++) {
        if (!isColumnFull(x)) {
            return false;
        }
    }
    return true;
}

std::string Connect4::initialStateString()
{
    return std::string(CONNECT4_COLS * CONNECT4_ROWS, '0');
}

std::string Connect4::stateString()
{
    std::string s(CONNECT4_COLS * CONNECT4_ROWS, '0');
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        Bit *bit = square->bit();
        if (bit) {
            s[y * CONNECT4_COLS + x] = std::to_string(bit->getOwner()->playerNumber() + 1)[0];
        }
    });
    return s;
}

void Connect4::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * CONNECT4_COLS + x;
        int playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1));
        } else {
            square->setBit(nullptr);
        }
    });
}

