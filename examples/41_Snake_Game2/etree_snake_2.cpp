
#include "Button.h"
#include "initBoard.h"
#include "Lcd_handler.hpp"
// #include "Button_handler.h"
static K10_Lcd Lcd;
// static LGFX Lcd;
#include <vector>
static LGFX_Sprite Screen(&Lcd);


#define up_pin 1
#define down_pin 2

#include <vector>
// Button API
#define DEBOUNCE_MS 10

Button BtnUp = Button(up_pin, true, DEBOUNCE_MS);  //实例化按键，起到消抖作用，代码更优雅。
Button BtnDown = Button(down_pin, true, DEBOUNCE_MS);


/* Game config */
#define SNAKE_BODY_WIDTH 24
#define GAME_UPDATE_INTERVAL 1
//#define COLOR_SNAKE             0xff8153U
#define COLOR_SNAKE_SHADOW 0Xb55f3cU
#define COLOR_FOOD 0x88d35eU
#define COLOR_FOOD_SHADOW 0x5d8c3dU
//#define COLOR_EXPLODE           0x88d35eU
//#define COLOR_EXPLODE_SHADOW    0x5d8c3dU
#define COLOR_BG_GRID 0x61497eU
#define COLOR_BG_FRAME 0x61497eU
#define COLOR_BG_DIALOG 0x61497eU
#define COLOR_BG_DIALOG_SHADOW 0x443454U
#define Rad 0.017453292519943295
struct Coordinate_t {
  int x;
  int y;
  uint8_t fsize;
};
struct FoodInfo_t {
  int x;
  int y;
  uint8_t fsize;
  uint8_t color;
};
struct ExplodeInfo_t { 
  int x;
  int y;
  bool is_Exploding;
  uint8_t _Size;
  uint8_t color;
};

enum MoveDirection_t {
  MOVE_UP,
  MOVE_DOWN,
  MOVE_LEFT,
  MOVE_RIGHT
};

std::vector<Coordinate_t> Snake_BodyList;
std::vector<FoodInfo_t> Food_List;
std::vector<ExplodeInfo_t> Explode_List;

static MoveDirection_t Snake_MoveDirection = MOVE_RIGHT;
static MoveDirection_t Snake_MoveDirection_Old = MOVE_RIGHT;
static FoodInfo_t Food_Coor = { 0, 0, 0 };
static uint8_t Food_Size = 0;
static uint32_t Food_UpdateTime_Old = 0;
static Coordinate_t Explode_Coor = { 0, 0 };
static uint8_t Explode_Size = 0;
static bool Explode_Exploding = false;
static uint32_t Explode_UpdateTime_Old = 0;
static uint32_t BackGround_UpdateTime_Old = 0;
static uint32_t Game_UpdateTime_LastFrame = 0;
static unsigned int Game_Score = 0;
static uint16_t COLOR_EXPLODE;
static uint16_t COLOR_SNAKE = 0xff8153U;
void Game_Reset();
void Game_Over();
void Game_Input_Update();
void Game_Input_Update_Callback(MoveDirection_t &MoveDirection);
void Game_Over_Callback();
void Game_Snake_Move();
void Game_Snake_Grow();
void Game_Food_Update();
void Game_FoodExplode_Update();
void Game_FoodExplode_Update2();
void Game_Render_Snake();
void Game_Render_BackGround();
void Game_Render_Food();
void Game_Render_FoodExplode();
void Game_Render_FoodExplode2();
void Game_Check_EatFood();
void Game_Check_EatItself();

// ------------------------------------------------------------------ //

void setup() {

  Serial.begin(115200);
  pinMode(0, OUTPUT); // suppress the noise
  digitalWrite(0, LOW);
  
  init_board();
  Lcd.init();
  Lcd.print("Hello world");
  Screen.createSprite(240, 320);
  Screen.setColorDepth(24);
//  Screen.println("Hello world");
//  Screen.pushSprite(0,0);
  delay(1000);
  /* Game init */

  Game_Reset();
  //  xTaskCreate(Game_Food_Update, "Game_Food_Update", 4096*3, NULL, 3, NULL);
}


void loop() {


  BtnUp.read();  //读取按键变化数据。
  BtnDown.read();

  static long Game_Foodupdate_LastTime;
  if ((millis() - Game_Foodupdate_LastTime) > (random(500) + 1000)) {
    Game_Foodupdate_LastTime = millis();
    Game_Food_Update();
  }
  /* Frame update */
  if ((millis() - Game_UpdateTime_LastFrame) > GAME_UPDATE_INTERVAL) {
    Game_UpdateTime_LastFrame = millis();

    Game_Snake_Move();
    Game_Check_EatFood();
    Game_Check_EatItself();
Lcd.startWrite();
    Screen.clear();
    Game_Render_BackGround();
    Game_Render_Food();
    Game_Render_Snake();
    Game_Render_FoodExplode2();
    Screen.pushSprite(0, 0);
    Lcd.endWrite();  
  }


  Game_Input_Update();
}


void Game_Input_Update_Callback(MoveDirection_t &MoveDirection) {
  /* Change moving direction, e.g. MoveDirection = MOVE_RIGHT */
  
    if (BtnUp.wasPressed()){
    if (Snake_MoveDirection_Old == MOVE_UP) MoveDirection = MOVE_LEFT;
    if (Snake_MoveDirection_Old == MOVE_DOWN) MoveDirection = MOVE_RIGHT;
    if (Snake_MoveDirection_Old == MOVE_RIGHT) MoveDirection = MOVE_UP;
    if (Snake_MoveDirection_Old == MOVE_LEFT)  MoveDirection = MOVE_DOWN;
  }
    if (BtnDown.wasPressed()){
    if (Snake_MoveDirection_Old == MOVE_UP) MoveDirection = MOVE_RIGHT;
    if (Snake_MoveDirection_Old == MOVE_DOWN) MoveDirection = MOVE_LEFT;
    if (Snake_MoveDirection_Old == MOVE_RIGHT) MoveDirection = MOVE_DOWN;
    if (Snake_MoveDirection_Old == MOVE_LEFT)  MoveDirection = MOVE_UP;
  }
}


void Game_Over_Callback() {
  /* Call return to reset game */
  //  pinMode(39, INPUT);
  while (!BtnUp.wasPressed()) {
    Serial.println("");  //it must put something here, else it dont work.
  }
}

// ------------------------------------------------------------------ //

void Game_Reset() {
  Snake_BodyList.clear();
  Food_List.clear();
  Snake_BodyList.push_back({ (int)(SNAKE_BODY_WIDTH + SNAKE_BODY_WIDTH / 2), (int)(SNAKE_BODY_WIDTH + SNAKE_BODY_WIDTH / 2) });
  Snake_MoveDirection = MOVE_RIGHT;
  Snake_MoveDirection_Old = MOVE_RIGHT;
  Game_Score = 0;
  Explode_Exploding = false;
}


void Game_Over() {
  /* Render dialog window */
  for (int w = 0; w < ((Screen.width() * 5) / 6); w += 2) {
    Screen.fillRoundRect((Screen.width() / 12 + 3) + ((Screen.width() * 5) / 12) - (w / 2), Screen.height() / 10 + 3, w, (Screen.height() * 7) / 10, 8, COLOR_BG_DIALOG_SHADOW);
    Screen.fillRoundRect((Screen.width() / 12) + ((Screen.width() * 5) / 12) - (w / 2), Screen.height() / 10, w, (Screen.height() * 7) / 10, 8, COLOR_BG_DIALOG);
    Screen.pushSprite(0, 0);
  }
  delay(5);

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
  for (float s = 1; s < (Screen.width() / 42); s += 0.1) {
    Screen.setTextSize(s);
    Screen.drawCenterString(String(Game_Score) /*TextBuff*/, Screen.width() / 2 - 3, (Screen.height() / 2) - (FontHeight / 2 * 3));
    delay(2);
    Screen.pushSprite(0, 0);
  }

  Screen.pushSprite(0, 0);
  Game_Over_Callback();

  /* Close dialog window */
  for (int w = ((Screen.width() * 5) / 6); w > 0; w -= 2) {
    Screen.clear();
    Game_Render_BackGround();
    Screen.fillRoundRect((Screen.width() / 12 + 3) + ((Screen.width() * 5) / 12) - (w / 2), Screen.height() / 10 + 3, w, (Screen.height() * 7) / 10, 8, COLOR_BG_DIALOG_SHADOW);
    Screen.fillRoundRect((Screen.width() / 12) + ((Screen.width() * 5) / 12) - (w / 2), Screen.height() / 10, w, (Screen.height() * 7) / 10, 8, COLOR_BG_DIALOG);
    Screen.pushSprite(0, 0);
  }
  Game_Reset();
}


void Game_Input_Update() {
  MoveDirection_t Snake_MoveDirection_New = Snake_MoveDirection;
  Game_Input_Update_Callback(Snake_MoveDirection_New);

  if ((Snake_MoveDirection_New + Snake_MoveDirection_Old == 1) || (Snake_MoveDirection_New + Snake_MoveDirection_Old == 5)) return;
  /* No backward */
  //  switch (Snake_MoveDirection_New)
  //  {
  //    case MOVE_UP:
  //      if (Snake_MoveDirection_Old == MOVE_DOWN)
  //        return;
  //      break;
  //    case MOVE_DOWN:
  //      if (Snake_MoveDirection_Old == MOVE_UP)
  //        return;
  //      break;
  //    case MOVE_LEFT:
  //      if (Snake_MoveDirection_Old == MOVE_RIGHT)
  //        return;
  //      break;
  //    case MOVE_RIGHT:
  //      if (Snake_MoveDirection_Old == MOVE_LEFT)
  //        return;
  //      break;
  //    default:
  //      return;
  //  }
  Snake_MoveDirection = Snake_MoveDirection_New;
}


void Game_Snake_Move() {
  Coordinate_t Coor_New = *Snake_BodyList.begin();

  /* Turn when hit the right point */
  if (((Coor_New.x + (SNAKE_BODY_WIDTH / 2)) % SNAKE_BODY_WIDTH) == 0)  // make it on the food track
  {
    if (((Coor_New.y + (SNAKE_BODY_WIDTH / 2)) % SNAKE_BODY_WIDTH) == 0)
      Snake_MoveDirection_Old = Snake_MoveDirection;  // onley on the fit condition can turn the direction.
  }

  switch (Snake_MoveDirection_Old)  //keep moving a body_width size before change the new direction.
  {
    case MOVE_UP:
      Coor_New.y -= 2;  // move one pixel
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
  // move forward pixel by pixel,insert a_body_width items into the list before turn any direction.
  Snake_BodyList.insert(Snake_BodyList.begin(), Coor_New);  // it helps to show the moving forward details.
  Snake_BodyList.pop_back();
}


void Game_Snake_Grow() {
  // Important tractic here.
  // it put 16 piece last item at the end of list,it helps the last block stay the same old
  // place while snake_head keep move forward,because the 16 pice last item is the same,
  // it wont change the snake tail place, but snake head keep going. then the snake enlenghten.
  // I change it here as half width of the body_width, shorten the snake while the score increase
  // It helps user to play the game a little longer.
  for (int i = 0; i < SNAKE_BODY_WIDTH / 8; i++) {
    Snake_BodyList.push_back(Snake_BodyList.back());  // add 16 pice of the ordination at the end of list.
    // but with the same coodination,they are resource for pop_back().
  }
  Serial.println(Snake_BodyList.size());  // increase 16 piece every time,it will cost a lot of resource while draw them repeatedly.
}

#if 0
void Game_Render_Snake()
{
  int K = 0; // Only draw the block every 4 steps pasted. It helps a to promte the speed ,especially while the score is over ten.
  for (auto i : Snake_BodyList) {
    if (K % 4 == 0) {
      //    Screen.fillRoundRect(i.x - (SNAKE_BODY_WIDTH / 2) + 2, i.y - (SNAKE_BODY_WIDTH / 2) + 2, SNAKE_BODY_WIDTH, SNAKE_BODY_WIDTH, 1, COLOR_SNAKE_SHADOW);
      Screen.fillRoundRect(i.x - (SNAKE_BODY_WIDTH / 2), i.y - (SNAKE_BODY_WIDTH / 2), SNAKE_BODY_WIDTH, SNAKE_BODY_WIDTH, 1, COLOR_SNAKE);
    }
    K++;
  }
}
#endif

#if 1
void Game_Render_Snake() {
  int K = 0;  // Only draw the block every 4 steps pasted. It helps a to promte the speed ,especially while the score is over ten.
  for (auto i : Snake_BodyList) {
    if (K % 4 == 0) {
      //    Screen.fillRoundRect(i.x - (SNAKE_BODY_WIDTH / 2) + 2, i.y - (SNAKE_BODY_WIDTH / 2) + 2, SNAKE_BODY_WIDTH, SNAKE_BODY_WIDTH, 1, COLOR_SNAKE_SHADOW);
      Screen.fillCircle(i.x, i.y, SNAKE_BODY_WIDTH / 2, COLOR_SNAKE);
    }
    K++;
  }
}
#endif


void Game_Food_Update() {
  /* Get a random Y */
  int x = 1;  // can change it to other number, crazy mode.
  while (x--) {
    FoodInfo_t New_Food;
    while (1) {
      New_Food.x = random(0 + (SNAKE_BODY_WIDTH / 2), Screen.width() - (SNAKE_BODY_WIDTH / 2));  // center point of the food
      /* Check if fit grid */
      if (((New_Food.x + (SNAKE_BODY_WIDTH / 2)) % SNAKE_BODY_WIDTH) != 0)
        continue;
      /* Check if hit snake */
      //      for (auto i : Snake_BodyList) // x or y not the same at the same is okay.
      //      {
      //        if (New_Food.x == i.x)
      //          continue;
      //      }
      for (auto i : Food_List) {
        if (New_Food.x == i.x)
          continue;
      }
      break;
    }
    /* Get a random Y */
    while (1) {
      New_Food.y = random(0 + (SNAKE_BODY_WIDTH / 2), Screen.height() - (SNAKE_BODY_WIDTH / 2));
      /* Check if fit grid */
      if (((New_Food.y + (SNAKE_BODY_WIDTH / 2)) % SNAKE_BODY_WIDTH) != 0)
        continue;
      /* Check if hit snake */
      for (auto i : Snake_BodyList) {
        if (New_Food.y == i.y)
          continue;
      }
      //      for (auto i : Food_List)
      //      {
      //        if (New_Food.y == i.y)
      //          continue;
      //      }
      break;
    }
    New_Food.fsize = random(10) + 5;
    New_Food.color = random(0xFFFF);
    /* Resize food */
    Food_List.insert(Food_List.begin(), New_Food);
    //  Snake_BodyList.pop_back();
  }
  Food_Size = 0;
  //  vTaskDelay(500);
}


void Game_Render_Food() {
  for (auto i : Food_List) {
    Screen.fillRoundRect(i.x - (i.fsize / 2) + 2, i.y - (i.fsize / 2) + 2, i.fsize, i.fsize, 1, i.color - 50);  //COLOR_FOOD_SHADOW
    Screen.fillRoundRect(i.x - (i.fsize / 2), i.y - (i.fsize / 2), i.fsize, i.fsize, 1, i.color);               //COLOR_FOOD
                                                                                                                //        if (i.fsize < SNAKE_BODY_WIDTH) { // runing but it wont change the size of the food.
                                                                                                                //          i.fsize = i.fsize+1;
                                                                                                                //        }
  }
  /* Food grow up */  // it is working at this method ,but i dont want to use it ,cause i want it to have the different size.
  //  if ((millis() - Food_UpdateTime_Old) > 10) // if need to revise the item of the vector, must use the array.
  //  {
  //    Food_UpdateTime_Old = millis();
  //    for (int i =0;i<Food_List.size();i++) {
  //      if (Food_List[i].fsize < SNAKE_BODY_WIDTH) {
  //        Food_List[i].fsize ++;
  //        Serial.println(Food_List[i].fsize);
  //      }
  //    }
}



void Game_FoodExplode_Update()  // make a explode_list later ,will make all the explode happen at the same time.
{
  Explode_Exploding = true;
  Explode_Coor.x = Snake_BodyList.begin()->x;
  Explode_Coor.y = Snake_BodyList.begin()->y;
  Explode_Size = 2;
}

void Game_FoodExplode_Update2(uint16_t color) {  // vector mode for the explosion. allow the explosion happen at the same time.
  ExplodeInfo_t New_explode;
  New_explode.is_Exploding = true;
  New_explode.x = Snake_BodyList.begin()->x;
  New_explode.y = Snake_BodyList.begin()->y;
  New_explode._Size = 2;
  //    New_Food.fsize = random(10) + 5;
  New_explode.color = color;  // exchange the color while eating the food.
  /* Resize food */
  Explode_List.insert(Explode_List.begin(), New_explode);
  //  Snake_BodyList.pop_back();
}



void Game_Render_FoodExplode() {
  if (Explode_Exploding) {
    /* Render octagon explode */  // why is 7/10, 1/sqrt(2)
    uint16_t COLOR_EXPLODE_SHADOW = COLOR_EXPLODE - 50;
    int32_t LittleFood_Size = SNAKE_BODY_WIDTH / 2;
    for (int i = 0; i < 360; i += 45) {
      Screen.fillRoundRect(2 + Explode_Coor.x + Explode_Size * cos(i * Rad), 2 + Explode_Coor.y + Explode_Size * sin(i * Rad), LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE_SHADOW);
      Screen.fillRoundRect(Explode_Coor.x + Explode_Size * cos(i * Rad), Explode_Coor.y + Explode_Size * sin(i * Rad), LittleFood_Size, LittleFood_Size, 1, COLOR_EXPLODE);
    }

    if ((millis() - Explode_UpdateTime_Old) > 10) {
      Explode_UpdateTime_Old = millis();

      if (Explode_Size < (SNAKE_BODY_WIDTH * 2))
        Explode_Size++;
      else
        Explode_Exploding = false;
    }
  }
}


void Game_Render_FoodExplode2() {
  for (auto explode : Explode_List) {
    /* Render octagon explode */  // why is 7/10, 1/sqrt(2)
    uint16_t COLOR_EXPLODE_SHADOW = COLOR_EXPLODE - 50;
    int32_t LittleFood_Size = SNAKE_BODY_WIDTH / 2;
    for (int i = 0; i < 360; i += 45) {
      Screen.fillRoundRect(2 + explode.x + explode._Size * cos(i * Rad), 2 + explode.y + explode._Size * sin(i * Rad), LittleFood_Size, LittleFood_Size, 1, explode.color - 50);
      Screen.fillRoundRect(explode.x + explode._Size * cos(i * Rad), explode.y + explode._Size * sin(i * Rad), LittleFood_Size, LittleFood_Size, 1, explode.color);
    }
  }
  if ((millis() - Explode_UpdateTime_Old) > 10) {
    Explode_UpdateTime_Old = millis();

    for (int i = 0; i < Explode_List.size(); i++) {
      if (Explode_List[i]._Size < (SNAKE_BODY_WIDTH * 2)) {
        Explode_List[i]._Size++;
      } else {
        Explode_List.erase(Explode_List.begin() + i);  // a good operation for erase the item in the vector.
      }
    }
  }
}



void Game_Check_EatFood() {
  for (auto i = Food_List.begin(); i <= Food_List.end(); i++) {
    if ((i->x == Snake_BodyList[0].x) && (i->y == Snake_BodyList[0].y)) {
      COLOR_EXPLODE = COLOR_SNAKE = i->color;
      Game_Score++;
      Game_Snake_Grow();
      Food_List.erase(i);  // it is working too ,erase the item in a vector.
      //      swap(i,Food_List.end());
      //      Food_List.pop_back();
      //      Game_Food_Update();
      Game_FoodExplode_Update2(COLOR_EXPLODE);  // transfer the color the the explosion.
      break;
    }
  }
}


void Game_Check_EatItself() {
  /* ***Check bug, not yet figured out */
  if (Snake_BodyList.size() < (3 * (SNAKE_BODY_WIDTH / 2)))
    return;

  Coordinate_t Coor_Head = *Snake_BodyList.begin();
  for (auto i = Snake_BodyList.begin() + 1; i <= Snake_BodyList.end(); i++) {
    if ((Coor_Head.x == i->x) && (Coor_Head.y == i->y))
      Game_Over();
  }
}


void Game_Render_BackGround() {
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
