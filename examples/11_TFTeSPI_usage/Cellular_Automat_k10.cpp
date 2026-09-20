// The Game of Life, also known simply as Life, is a cellular automaton
// devised by the British mathematician John Horton Conway in 1970.
//  https://en.wikipedia.org/wiki/Conway's_Game_of_Life

#include <Arduino.h>
#include <ESP_IOExpander_Library.h>
#include <Wire.h>
// #include "pin_config.h"
// #include "XL95x5_Driver.h"

// XL95x5 initializes the configuration
// XL95x5 Class_XL95x5(XL95x5_IIC_ADDRESS, XL95x5_SDA, XL95x5_SCL);
/**
 * Create an ESP_IOExpander object, Currently supports:
 *      - TCA95xx_8bit
 *      - TCA95xx_16bit
 *      - HT8574
 *      - CH422G
 */
#define EXAMPLE_CHIP_NAME   TCA95xx_16bit
#define EXAMPLE_I2C_NUM     (0)
#define EXAMPLE_I2C_SDA_PIN (47)
#define EXAMPLE_I2C_SCL_PIN (48)

#define _EXAMPLE_CHIP_CLASS(name, ...) ESP_IOExpander_##name(__VA_ARGS__)
#define EXAMPLE_CHIP_CLASS(name, ...)  _EXAMPLE_CHIP_CLASS(name, ##__VA_ARGS__)

ESP_IOExpander *expander = NULL;
#include <SPI.h>

#include <TFT_eSPI.h>  // Hardware-specific library

// TFT_eSPI tft = TFT_eSPI();  // Invoke custom library
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 320;

// ESP_Panel *panel = NULL;
static TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */
// #define GRIDX 80


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

int getNumberOfNeighbors1(int x, int y);
int getNumberOfNeighbors(int x, int y);
void drawGrid(void);
void initGrid(void);
void computeCA(void);



void setup() {
    Serial.begin(115200);
    Serial.println("Test begin");
    pinMode(0, OUTPUT); // suppress the noise
    digitalWrite(0, LOW);
    Wire.begin(47, 48);
    expander =
        new EXAMPLE_CHIP_CLASS(EXAMPLE_CHIP_NAME, (i2c_port_t)EXAMPLE_I2C_NUM,
                               0x20, EXAMPLE_I2C_SCL_PIN, EXAMPLE_I2C_SDA_PIN);
    expander->begin();
    expander->reset();
    expander->pinMode(0, OUTPUT);
    expander->pinMode(1, OUTPUT);   
    delay(20);
    expander->digitalWrite(0, HIGH );
    expander->digitalWrite(1, HIGH);
    // Set up the display
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(5, 100);
    tft.print("hello world");
}

void loop() {
#if 1
    // Display a simple splash screen
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(40, 5);
    tft.println(F("Arduino"));
    tft.setCursor(35, 25);
    tft.println(F("Cellular"));
    tft.setCursor(35, 45);
    tft.println(F("Automata"));

    delay(1000);

    tft.fillScreen(TFT_BLACK);

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
            uint16_t color = (newgrid[x][y] == 1) ? 0XFFFF : 0;
            if (grid[x][y] != newgrid[x][y]) {
                tft.fillRect(CELLXY * x, CELLXY * y, CELLXY, CELLXY, color);
            }
        }
    }
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