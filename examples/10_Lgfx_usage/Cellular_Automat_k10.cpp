// The Game of Life, also known simply as Life, is a cellular automaton
// devised by the British mathematician John Horton Conway in 1970.
//  https://en.wikipedia.org/wiki/Conway's_Game_of_Life

#include <Arduino.h>
// #include <ESP_IOExpander_Library.h>
#include "initBoard.h"
#include "Lcd_handler.hpp"

// TFT_eSPI tft = TFT_eSPI();  // Invoke custom library
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 320;

// ESP_Panel *panel = NULL;
// static TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */
static K10_Lcd tft;
LGFX_Sprite sprite(&tft);
// #define GRIDX 80
// #define GRIDY 60
// #define CELLXY 4

#define GRIDX  120
#define GRIDY  160
#define CELLXY 2

#define GEN_DELAY 0

// Current grid
uint8_t grid[GRIDX][GRIDY];

// The new grid for the next generation
uint8_t newgrid[GRIDX][GRIDY];

// Number of generations
#define NUMGEN 600

uint16_t genCount = 0;



// Function declarations
int getNumberOfNeighbors1(int x, int y);
int getNumberOfNeighbors(int x, int y);
void drawGrid(void);
void initGrid(void);
void computeCA(void);
// Check the Moore neighbourhood

/*
   The MIT License (MIT)

   Copyright (c) 2016 RuntimeProjects.com

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

void setup() {
    Serial.begin(115200);
    Serial.println("Test begin");
    pinMode(0, OUTPUT); // suppress the noise
    digitalWrite(0, LOW);
    init_board();

    // Set up the display
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(5, 100);
    tft.print("hello world");
    sprite.setPsram(true);// 使用PSRAM,提升了绘图性能，值得推广。 
    sprite.createSprite(240, 320);
}

void loop() {
#if 1
    // Display a simple splash screen
    sprite.fillScreen(TFT_BLACK);
    sprite.setTextSize(2);
    sprite.setTextColor(TFT_WHITE);
    sprite.setCursor(40, 5);
    sprite.println(F("Arduino"));
    sprite.setCursor(35, 25);
    sprite.println(F("Cellular"));
    sprite.setCursor(35, 45);
    sprite.println(F("Automata"));
    sprite.pushSprite(0, 0);

    delay(1000);

    sprite.fillScreen(TFT_BLACK);

    initGrid();

    genCount = NUMGEN;

    drawGrid();

    // Compute generations
    for (int gen = 0; gen < genCount; gen++) {
        computeCA();
        drawGrid();
        delay(GEN_DELAY);
        memcpy(grid, newgrid, sizeof(grid));
    }

#endif
}


int getNumberOfNeighbors1(int x, int y) {
    int neighbors = 0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;  // Skip the cell itself
            int nx = x + dx;
            int ny = y + dy;
            if (nx >= 0 && nx < GRIDX && ny >= 0 && ny < GRIDY) {
                neighbors += grid[nx][ny];
            }
        }
    }
    return neighbors;
}
int getNumberOfNeighbors(int x, int y) {
    int neighbors = 0;
    int dx[]      = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[]      = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int i = 0; i < 8; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < GRIDX && ny >= 0 && ny < GRIDY) {
            neighbors += grid[nx][ny];
        }
    }
    return neighbors;
}
// Draws the grid on the display
void drawGrid(void) {
    for (int16_t x = 1; x < GRIDX - 1; x++) {
        for (int16_t y = 1; y < GRIDY - 1; y++) {
            // uint16_t color = random(0xFFFF); // 生成随机颜色
            // color = (newgrid[x][y] == 1) ? color : 0;
            // uint16_t color = (newgrid[x][y] == 1) ? 0XFFFF : 0;
            uint16_t color = (newgrid[x][y] == 1) ? TFT_MAGENTA : 0;
            if (grid[x][y] != newgrid[x][y]) {
                // tft.fillRect(CELLXY * x, CELLXY * y, CELLXY, CELLXY, color);
                sprite.fillRect(CELLXY * x, CELLXY * y, CELLXY, CELLXY, color);
            }
        }
    }
    sprite.pushSprite(0, 0);
}

// Initialise Grid
void initGrid(void) {
    // 遍历网格的每一行
    for (int16_t x = 0; x < GRIDX; x++) {
        // 遍历网格的每一列
        for (int16_t y = 0; y < GRIDY; y++) {
            // 将新网格的所有单元格初始化为 0
            newgrid[x][y] = 0;

            // 如果单元格位于网格的边缘（第一行、最后一行、第一列或最后一列）
            if (x == 0 || x == GRIDX - 1 || y == 0 || y == GRIDY - 1) {
                // 将当前网格的边缘单元格初始化为 0
                grid[x][y] = 0;
            } else {
                // 对于非边缘单元格，随机决定其初始状态
                grid[x][y] = (random(3) == 1) ? 1 : 0;
            }
        }
    }
}

// Compute the CA. Basically everything related to CA starts here
void computeCA() {
    for (int16_t x = 1; x < GRIDX - 1; x++) {
        for (int16_t y = 1; y < GRIDY - 1; y++) {
            int neighbors = getNumberOfNeighbors(x, y);
            newgrid[x][y] =
                (grid[x][y] == 1 && (neighbors == 2 || neighbors == 3)) ||
                        (grid[x][y] == 0 && neighbors == 3)
                    ? 1
                    : 0;
        }
    }
}
