/**
   @file main.cpp
   @author Forairaaaaa
   @brief
   @version 0.1
   @date 2023-01-20

   @copyright Copyright (c) 2023

*/
// 频率最高可以超到240 ，这样玩起来会更流畅。 
#define LGFX_USE_V1
#include <LovyanGFX.hpp>  // 需要安装 Arduino LovyanGFX 库

#include "initBoard.h"
#include "Lcd_handler.hpp"
#include "Button_handler.h"
static K10_Lcd Lcd;
// static LGFX Lcd;
#include <vector>
static LGFX_Sprite Screen(&Lcd);
 


Button *buttonA = new Button(eP5_KeyA);

Button *buttonB = new Button(eP11_KeyB);
Button *buttonUp = new Button(eP14);
Button *buttonDw = new Button(eP15);

Button *buttonAB = new Button(eP5_KeyA, eP11_KeyB);



/* Game config */  // The color system only cast the shadow effect in the game ,as the reason you know(How the Oled produce the color except B&W)
#define SNAKE_BODY_WIDTH        16
#define GAME_UPDATE_INTERVAL    5
#define COLOR_SNAKE             0xff8153U
#define COLOR_SNAKE_SHADOW      0Xb55f3cU
#define COLOR_FOOD              0x88d35eU
#define COLOR_FOOD_SHADOW       0x5d8c3dU
#define COLOR_EXPLODE           0x88d35eU
#define COLOR_EXPLODE_SHADOW    0x5d8c3dU
#define COLOR_BG_GRID           0x61497eU
#define COLOR_BG_FRAME          0x61497eU
#define COLOR_BG_DIALOG         0x61497eU
#define COLOR_BG_DIALOG_SHADOW  0x443454U

struct Coordinate_t {
  int x;
  int y;
};

enum MoveDirection_t {
  MOVE_UP,
  MOVE_DOWN,
  MOVE_LEFT,
  MOVE_RIGHT
};





std::vector<Coordinate_t> Snake_BodyList;
static MoveDirection_t Snake_MoveDirection = MOVE_RIGHT;
static MoveDirection_t Snake_MoveDirection_Old = MOVE_RIGHT;
static Coordinate_t Food_Coor = {0, 0};
static uint8_t Food_Size = 0;
static uint32_t Food_UpdateTime_Old = 0;
static Coordinate_t Explode_Coor = {0, 0};
static uint8_t Explode_Size = 0;
static bool Explode_Exploding = false;
static uint32_t Explode_UpdateTime_Old = 0;
static uint32_t BackGround_UpdateTime_Old = 0;
static uint32_t Game_UpdateTime_LastFrame = 0;
static unsigned int Game_Score = 0;

void Game_Reset();
void Game_Over();
void Game_Input_Update();
void Game_Input_Update_Callback(MoveDirection_t &MoveDirection);
void Game_Over_Callback();
void Game_Snake_Move();
void Game_Snake_Grow();
void Game_Food_Update();
void Game_FoodExplode_Update();
void Game_Render_Snake();
void Game_Render_BackGround();
void Game_Render_Food();
void Game_Render_FoodExplode();
void Game_Check_EatFood();
void Game_Check_EatMyself();

// ------------------------------------------------------------------ //

void setup()
{
  // for pico_ssd1306
  Serial.begin(115200);
  pinMode(0, OUTPUT); // suppress the noise
  digitalWrite(0, LOW);
  init_board();
  Lcd.init();
  Lcd.print("Hello world");
  delay(1000);
  // Lcd.setRotation(0);
  Screen.createSprite(Lcd.width(), Lcd.height());
  /* Game init */
  Game_Reset();

}


void loop()
{
  /* Frame update */
  
  if (Game_UpdateTime_LastFrame == 0)
    Game_UpdateTime_LastFrame = millis();
  else if ((millis() - Game_UpdateTime_LastFrame) > GAME_UPDATE_INTERVAL)
  {
    Game_UpdateTime_LastFrame = millis();

    Game_Snake_Move();
    Game_Check_EatFood();
    Game_Check_EatMyself();

    Screen.clear();
    Game_Render_BackGround();
    Game_Render_Food();
    Game_Render_Snake();
    Game_Render_FoodExplode();
    Screen.pushSprite(0, 0);
  }

  Game_Input_Update();
}


void Game_Input_Update_Callback(MoveDirection_t &MoveDirection)
{
  /* Change moving direction, e.g. MoveDirection = MOVE_RIGHT */
  if (buttonA->isPressed()){
    if (Snake_MoveDirection_Old == MOVE_UP) MoveDirection = MOVE_LEFT;
    if (Snake_MoveDirection_Old == MOVE_DOWN) MoveDirection = MOVE_RIGHT;
    if (Snake_MoveDirection_Old == MOVE_RIGHT) MoveDirection = MOVE_UP;
    if (Snake_MoveDirection_Old == MOVE_LEFT)  MoveDirection = MOVE_DOWN;
  }
    if (buttonB->isPressed()){
    if (Snake_MoveDirection_Old == MOVE_UP) MoveDirection = MOVE_RIGHT;
    if (Snake_MoveDirection_Old == MOVE_DOWN) MoveDirection = MOVE_LEFT;
    if (Snake_MoveDirection_Old == MOVE_RIGHT) MoveDirection = MOVE_DOWN;
    if (Snake_MoveDirection_Old == MOVE_LEFT)  MoveDirection = MOVE_UP;
  }
}


void Game_Over_Callback()
{
  /* Call return to reset game */
  
  while (1)
  {
    
    if (buttonA->isPressed())
    { 
        return;
      }
  }
}

// ------------------------------------------------------------------ //

void Game_Reset()
{
  Snake_BodyList.clear();
  Snake_BodyList.push_back({(int)(SNAKE_BODY_WIDTH + SNAKE_BODY_WIDTH / 2), (int)(SNAKE_BODY_WIDTH + SNAKE_BODY_WIDTH / 2)});
  Snake_MoveDirection = MOVE_RIGHT;
  Snake_MoveDirection_Old = MOVE_RIGHT;
  Game_Score = 0;
  Explode_Exploding = false;

  Game_Food_Update();
}


void Game_Over()
{
  /* Render dialog window */
  for (int w = 0; w < ((Screen.width() * 5) / 6); w += 2)
  {
    Screen.fillRoundRect((Screen.width() / 12 + 3) + ((Screen.width() * 5) / 12) - (w / 2), Screen.height() / 10 + 3, w, (Screen.height() * 7) / 10, 8, COLOR_BG_DIALOG_SHADOW);
    Screen.fillRoundRect((Screen.width() / 12) + ((Screen.width() * 5) / 12) - (w / 2), Screen.height() / 10, w, (Screen.height() * 7) / 10, 8, COLOR_BG_DIALOG);
    Screen.pushSprite(0, 0);
  }
  delay(50);

  char TextBuff[10];
  int32_t FontHeight = 0;
  Screen.setFont(&fonts::Font8x8C64);
  Screen.setTextDatum(top_center);
  Screen.setTextColor(TFT_WHITE, COLOR_BG_GRID);

  Screen.setTextSize(Screen.width() / 128);
  snprintf(TextBuff, sizeof(TextBuff), "GAME OVER");
  Screen.drawCenterString(TextBuff, Screen.width() / 2 - 3, (Screen.height() / 2));

  Screen.setTextSize(Screen.width() / 42);
  FontHeight = Screen.fontHeight();
  snprintf(TextBuff, sizeof(TextBuff), "%d", Game_Score);
  for (float s = 1; s < (Screen.width() / 42); s += 0.1)
  {
    Screen.setTextSize(s);
    Screen.drawCenterString(TextBuff, Screen.width() / 2 - 3, (Screen.height() / 2) - (FontHeight / 2 *2));
    delay(2);
    Screen.pushSprite(0, 0);
  }

  Screen.pushSprite(0, 0);
  Game_Over_Callback();

  /* Close dialog window */
  for (int w = ((Screen.width() * 5) / 6); w > 0; w -= 2)
  {
    Screen.clear();
    Game_Render_BackGround();
    Screen.fillRoundRect((Screen.width() / 12 + 3) + ((Screen.width() * 5) / 12) - (w / 2), Screen.height() / 10 + 3, w, (Screen.height() * 7) / 10, 8, COLOR_BG_DIALOG_SHADOW);
    Screen.fillRoundRect((Screen.width() / 12) + ((Screen.width() * 5) / 12) - (w / 2), Screen.height() / 10, w, (Screen.height() * 7) / 10, 8, COLOR_BG_DIALOG);
    Screen.pushSprite(0, 0);
  }
  Game_Reset();
}


void Game_Input_Update()
{
  
  MoveDirection_t Snake_MoveDirection_New = Snake_MoveDirection;
  Game_Input_Update_Callback(Snake_MoveDirection_New);

  Snake_MoveDirection = Snake_MoveDirection_New;
}


void Game_Snake_Move()
{
  Coordinate_t Coor_New = *Snake_BodyList.begin();

  /* Turn when hit the right point */
  if (((Coor_New.x + (SNAKE_BODY_WIDTH / 2)) % SNAKE_BODY_WIDTH) == 0)
  {
    if (((Coor_New.y + (SNAKE_BODY_WIDTH / 2)) % SNAKE_BODY_WIDTH) == 0)
      Snake_MoveDirection_Old = Snake_MoveDirection;
  }

  switch (Snake_MoveDirection_Old)
  {
    case MOVE_UP:
      Coor_New.y -= 2;
      break;
    case MOVE_DOWN:
      Coor_New.y += 2;
      break;
    case MOVE_LEFT:
      Coor_New.x -= 2;
      break;
    case MOVE_RIGHT:
      Coor_New.x += 2;
      break;
    default:
      break;
  }

  /* If hit the wall */
  if (Coor_New.x < 0)
    Coor_New.x = Screen.width();
  if (Coor_New.y < 0)
    Coor_New.y = Screen.height();
  if (Coor_New.x > Screen.width())
    Coor_New.x = 0;
  if (Coor_New.y > Screen.height())
    Coor_New.y = 0;

  Snake_BodyList.insert(Snake_BodyList.begin(), Coor_New);
  Snake_BodyList.pop_back();
}


void Game_Snake_Grow()
{
  for (int i = 0; i < SNAKE_BODY_WIDTH/4; i++)
    Snake_BodyList.push_back(Snake_BodyList.back());
}


void Game_Render_Snake()
{
  for (auto i : Snake_BodyList)
    Screen.fillRoundRect(i.x - (SNAKE_BODY_WIDTH / 2) + 2, i.y - (SNAKE_BODY_WIDTH / 2) + 2, SNAKE_BODY_WIDTH, SNAKE_BODY_WIDTH, 1, COLOR_SNAKE_SHADOW);
  for (auto i : Snake_BodyList)
    Screen.fillRoundRect(i.x - (SNAKE_BODY_WIDTH / 2), i.y - (SNAKE_BODY_WIDTH / 2), SNAKE_BODY_WIDTH, SNAKE_BODY_WIDTH, 1, COLOR_SNAKE);
}


void Game_Food_Update()
{
  /* Get a random Y */
  while (1)
  {
    Food_Coor.x = random(0 + (SNAKE_BODY_WIDTH / 2), Screen.width() - (SNAKE_BODY_WIDTH / 2));
    /* Check if fit grid */
    if (((Food_Coor.x + (SNAKE_BODY_WIDTH / 2)) % SNAKE_BODY_WIDTH) != 0)
      continue;
    /* Check if hit snake */
    for (auto i : Snake_BodyList)
    {
      if (Food_Coor.x == i.x)
        continue;
    }
    break;
  }
  /* Get a random Y */
  while (1)
  {
    Food_Coor.y = random(0 + (SNAKE_BODY_WIDTH / 2), Screen.height() - (SNAKE_BODY_WIDTH / 2));
    /* Check if fit grid */
    if (((Food_Coor.y + (SNAKE_BODY_WIDTH / 2)) % SNAKE_BODY_WIDTH) != 0)
      continue;
    /* Check if hit snake */
    for (auto i : Snake_BodyList)
    {
      if (Food_Coor.y == i.y)
        continue;
    }
    break;
  }
  /* Resize food */
  Food_Size = 0;
}


void Game_FoodExplode_Update()
{
  Explode_Exploding = true;
  Explode_Coor.x = Snake_BodyList.begin()->x;
  Explode_Coor.y = Snake_BodyList.begin()->y;
  Explode_Size = 2;
}


void Game_Render_Food()
{
  Screen.fillRoundRect(Food_Coor.x - (Food_Size / 2) + 2, Food_Coor.y - (Food_Size / 2) + 2, Food_Size, Food_Size, 1, COLOR_FOOD_SHADOW);
  Screen.fillRoundRect(Food_Coor.x - (Food_Size / 2), Food_Coor.y - (Food_Size / 2), Food_Size, Food_Size, 1, COLOR_FOOD);

  /* Food grow up */
  if (Food_UpdateTime_Old == 0)
    Food_UpdateTime_Old = millis();
  else
  {
    if ((millis() - Food_UpdateTime_Old) > 10)
    {
      Food_UpdateTime_Old = millis();
      if (Food_Size < SNAKE_BODY_WIDTH)
        Food_Size++;
    }
  }
}


void Game_Render_FoodExplodeO()
{
  if (Explode_Exploding)
  {
    /* Render octagon explode */
    int32_t LittleFood_Size = SNAKE_BODY_WIDTH / 2;
    Screen.fillRoundRect(2 + Explode_Coor.x + Explode_Size, 2 + Explode_Coor.y, LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE_SHADOW);
    Screen.fillRoundRect(2 + Explode_Coor.x - Explode_Size, 2 + Explode_Coor.y, LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE_SHADOW);
    Screen.fillRoundRect(2 + Explode_Coor.x, 2 + Explode_Coor.y + Explode_Size, LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE_SHADOW);
    Screen.fillRoundRect(2 + Explode_Coor.x, 2 + Explode_Coor.y - Explode_Size, LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE_SHADOW);
    Screen.fillRoundRect(2 + Explode_Coor.x + (Explode_Size * 7 / 10), 2 + Explode_Coor.y - (Explode_Size * 7 / 10), LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE_SHADOW);
    Screen.fillRoundRect(2 + Explode_Coor.x + (Explode_Size * 7 / 10), 2 + Explode_Coor.y + (Explode_Size * 7 / 10), LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE_SHADOW);
    Screen.fillRoundRect(2 + Explode_Coor.x - (Explode_Size * 7 / 10), 2 + Explode_Coor.y + (Explode_Size * 7 / 10), LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE_SHADOW);
    Screen.fillRoundRect(2 + Explode_Coor.x - (Explode_Size * 7 / 10), 2 + Explode_Coor.y - (Explode_Size * 7 / 10), LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE_SHADOW);
    Screen.fillRoundRect(Explode_Coor.x + Explode_Size, Explode_Coor.y, LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE);
    Screen.fillRoundRect(Explode_Coor.x - Explode_Size, Explode_Coor.y, LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE);
    Screen.fillRoundRect(Explode_Coor.x, Explode_Coor.y + Explode_Size, LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE);
    Screen.fillRoundRect(Explode_Coor.x, Explode_Coor.y - Explode_Size, LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE);
    Screen.fillRoundRect(Explode_Coor.x + (Explode_Size * 7 / 10), Explode_Coor.y - (Explode_Size * 7 / 10), LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE);
    Screen.fillRoundRect(Explode_Coor.x + (Explode_Size * 7 / 10), Explode_Coor.y + (Explode_Size * 7 / 10), LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE);
    Screen.fillRoundRect(Explode_Coor.x - (Explode_Size * 7 / 10), Explode_Coor.y + (Explode_Size * 7 / 10), LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE);
    Screen.fillRoundRect(Explode_Coor.x - (Explode_Size * 7 / 10), Explode_Coor.y - (Explode_Size * 7 / 10), LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE);

    if (Explode_UpdateTime_Old == 0)
      Explode_UpdateTime_Old = millis();
    else if ((millis() - Explode_UpdateTime_Old) > 15)
    {
      Explode_UpdateTime_Old = millis();

      if (Explode_Size < (SNAKE_BODY_WIDTH * 2))
        Explode_Size++;
      else
        Explode_Exploding = false;
    }
  }
}

void Game_Render_FoodExplode()
{
    if (Explode_Exploding)
    {
        int32_t LittleFood_Size = SNAKE_BODY_WIDTH / 2;
        int32_t centerX = Explode_Coor.x;
        int32_t centerY = Explode_Coor.y;
        int32_t radius = Explode_Size;
        int32_t numPieces = 8; // 爆炸效果的碎片数量
        float angleIncrement = 2* M_PI / numPieces; // 每个碎片之间的角度增量

        for (int i = 0; i < numPieces; i++)
        {
            float angle = i * angleIncrement;
            int32_t x = centerX + radius * cosf(angle);
            int32_t y = centerY + radius * sinf(angle);

            // 渲染阴影
            Screen.fillRoundRect(x + 2, y + 2, LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE_SHADOW);
            // 渲染实际颜色
            Screen.fillRoundRect(x, y, LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE);
        }

        // 更新爆炸效果的大小
        if (Explode_UpdateTime_Old == 0)
            Explode_UpdateTime_Old = millis();
        else if ((millis() - Explode_UpdateTime_Old) > 15)
        {
            Explode_UpdateTime_Old = millis();

            if (Explode_Size < (SNAKE_BODY_WIDTH * 2))
                Explode_Size++;
            else
                Explode_Exploding = false;
        }
    }
}

void Game_Check_EatFood()
{
  for (auto i : Snake_BodyList)
  {
    if ((Food_Coor.x == i.x) && (Food_Coor.y == i.y))
    {
      Game_Score++;
      Game_Snake_Grow();
      Game_Food_Update();
      Game_FoodExplode_Update();
    }
  }
}


void Game_Check_EatMyself()
{
  /* ***Check bug, not yet figured out */
  if (Snake_BodyList.size() < (3 * (SNAKE_BODY_WIDTH / 2)))
    return;

  Coordinate_t Coor_Head = *Snake_BodyList.begin();
  for (auto i = Snake_BodyList.begin() + 1; i <= Snake_BodyList.end(); i++)
  {
    if ((Coor_Head.x == i->x) && (Coor_Head.y == i->y))
      Game_Over();
  }
}


void Game_Render_BackGround()
{
  /* Draw score */
  char TextBuff[10];
  Screen.setFont(&fonts::Font8x8C64);
  Screen.setTextDatum(top_center);
  Screen.setTextSize(Screen.width() / 20);
  snprintf(TextBuff, sizeof(TextBuff), "%d", Game_Score);
  Screen.setTextColor(COLOR_BG_DIALOG_SHADOW, TFT_BLACK);
  Screen.drawCenterString(TextBuff, Screen.width() / 2 - 4, (Screen.height() / 2) - Screen.getTextSizeY() * 4);
  Screen.setTextColor(COLOR_BG_GRID, TFT_BLACK);
  Screen.drawCenterString(TextBuff, Screen.width() / 2 - 8, (Screen.height() / 2) - Screen.getTextSizeY() * 4);

  /* Draw grid */
  for (int x = -(SNAKE_BODY_WIDTH / 2) - 1; x < Screen.width(); x += SNAKE_BODY_WIDTH)
    for (int y = -(SNAKE_BODY_WIDTH / 2); y < Screen.height(); y += SNAKE_BODY_WIDTH)
      Screen.drawPixel(x, y, COLOR_BG_GRID);
}
