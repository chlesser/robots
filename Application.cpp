#include "Application.h"
#include "imgui/imgui.h"
#include "classes/TicTacToe.h"
#include "classes/Checkers.h"
#include "classes/Othello.h"
#include "classes/Connect4.h"
#include "classes/Chess.h"
#include "classes/Robots.h"
#include <chrono>

// Undefine Robots macros before including AstroBots to avoid conflicts
#undef MOVE
#undef TURN
#undef ATTACK
#undef ATTACK_SCAN
#undef SIGNAL
#undef WAIT_
#undef SCAN
#undef TURN_SCAN
#undef TURN_AWAY
#undef IF_ENEMY
#undef IF_TURN_LT
#undef IF_SEEN
#undef IF_SCAN_LE
#undef IF_NEAR_SIGNAL

#include "classes/AstroBots.h"

namespace ClassGame {
        //
        // our global variables
        //
        Game *game = nullptr;
        bool gameOver = false;
        int gameWinner = -1;

        // Frame rate limiter for AstroBots (30Hz = 33.33ms per frame)
        static auto lastAstroBotsUpdate = std::chrono::steady_clock::now();
        static constexpr double ASTROBOTS_UPDATE_INTERVAL_MS = 1000.0 / 30.0;  // 30 Hz

        //
        // game starting point
        // this is called by the main render loop in main.cpp
        //
        void GameStartUp() 
        {
            game = nullptr;
        }

        //
        // game render loop
        // this is called by the main render loop in main.cpp
        //
        void RenderGame() 
        {
                ImGui::DockSpaceOverViewport();

                //ImGui::ShowDemoWindow();

                ImGui::Begin("Settings");

                if (gameOver) {
                    ImGui::Text("Game Over!");
                    ImGui::Text("Winner: %d", gameWinner);
                    if (ImGui::Button("Reset Game")) {
                        game->stopGame();
                        game->setUpBoard();
                        gameOver = false;
                        gameWinner = -1;
                    }
                }
                if (!game) {
                    if (ImGui::Button("Start Tic-Tac-Toe")) {
                        game = new TicTacToe();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("Start Checkers")) {
                        game = new Checkers();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("Start Othello")) {
                        game = new Othello();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("Start Connect 4")) {
                        game = new Connect4();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("Start Chess")) {
                        game = new Chess();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("Start Robots")) {
                        game = new Robots();
                        game->setUpBoard();
                    }

                    {
                        float t = (float)ImGui::GetTime();
                        float s = 0.5f + 0.5f * sinf(t * 4.2f);
                        s = s * s * (3.0f - 2.0f * s); // smoothstep

                        ImVec4 base     = ImGui::GetStyleColorVec4(ImGuiCol_Button);
                        ImVec4 hovered  = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
                        ImVec4 active   = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);

                        auto brighten = [](ImVec4 c, float amt) {
                            auto clampf = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
                            c.x = clampf(c.x + amt);
                            c.y = clampf(c.y + amt * 0.6f);
                            c.z = clampf(c.z + amt * 1.5f);
                            return c;
                        };

                        float glow = 0.20f + 0.25f * s;   // 0.20 -> 0.45
                        ImGui::PushStyleColor(ImGuiCol_Button,        brighten(base,    glow));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, brighten(hovered, glow * 1.1f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  brighten(active,  glow * 0.9f));
                    }
                    if (ImGui::Button("Start AstroBots")) {
                        ImGui::PopStyleColor(3);
                        game = new AstroBots();
                        game->setUpBoard();
                    } else {
                        ImGui::PopStyleColor(3);
                    }
                } else {
                    Robots *robotGame = dynamic_cast<Robots*>(game);
                    AstroBots *astroGame = dynamic_cast<AstroBots*>(game);
                    if (robotGame) {
                        if (ImGui::Button("Advance Turn")) {
                            robotGame->endTurn();
                        }
                    } else if (astroGame) {
                        auto now = std::chrono::steady_clock::now();
                        double elapsedMs = std::chrono::duration<double, std::milli>(now - lastAstroBotsUpdate).count();
                        if (elapsedMs >= ASTROBOTS_UPDATE_INTERVAL_MS) {
                            astroGame->endTurn();
                            lastAstroBotsUpdate = now;
                        }
                    } else {
                        ImGui::Text("Current Player Number: %d", game->getCurrentPlayer()->playerNumber());
                        std::string stateString = game->stateString();
                        int stride = game->_gameOptions.rowX;
                        int height = game->_gameOptions.rowY;

                        for(int y=0; y<height; y++) {
                            ImGui::Text("%s", stateString.substr(y*stride,stride).c_str());
                        }
                    }
                    ImGui::Text("Current Board State: %s", game->stateString().c_str());
                }
                ImGui::End();

                ImGui::Begin("GameWindow");
                if (game) {
                    if (game->gameHasAI() && (game->getCurrentPlayer()->isAIPlayer() || game->_gameOptions.AIvsAI))
                    {
                        game->updateAI();
                    }
                    game->drawFrame();
                }
                ImGui::End();
        }

        //
        // end turn is called by the game code at the end of each turn
        // this is where we check for a winner
        //
        void EndOfTurn() 
        {
            Player *winner = game->checkForWinner();
            if (winner)
            {
                gameOver = true;
                gameWinner = winner->playerNumber();
            }
            if (game->checkForDraw()) {
                gameOver = true;
                gameWinner = -1;
            }
        }
}
