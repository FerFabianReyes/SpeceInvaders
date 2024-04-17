#include "fabgl.h"
#include "fabutils.h"
#include <Ps3Controller.h>
#include "sprites.h"
#include "sounds.h"
#include "WiFiGeneric.h"

using fabgl::iclamp;

fabgl::VGAController DisplayController;
fabgl::Canvas canvas(&DisplayController);
SoundGenerator soundGenerator;

//Las variables van a ser globales para que no cause proplema
    constexpr unsigned long TIEMPO_LIMITE = 90000;
    constexpr unsigned long TiempoInicio = 0;

// IntroScene

 void mostrarMensajeTiempoAgotado() {
  // Borra la pantalla, lo que este ahi
  canvas.setBrushColor(Color::Black);
  canvas.clear();

  // Configura el color y la fuente del mensaje
  canvas.setPenColor(Color::White);
  canvas.selectFont(&fabgl::FONT_8x8);

  // Muestra el mensaje en el centro de la pantalla
  //canvas.drawText(50, 100, "TIEMPO AGOTADO");
}

struct IntroScene : public Scene
{

  static const int TEXTROWS = 4;
  static const int TEXT_X = 130;
  static const int TEXT_Y = 122;

  int textRow_ = 0;
  int textCol_ = 0;
  int starting_ = 0;

  SamplesGenerator *music_ = nullptr;

  IntroScene()
      : Scene(0, 20, DisplayController.getViewPortWidth(), DisplayController.getViewPortHeight())
  {
  }

  void init()
  {
    canvas.setBrushColor(21, 26, 70);
    canvas.clear();
    canvas.setGlyphOptions(GlyphOptions().FillBackground(true));
    canvas.selectFont(&fabgl::FONT_8x8);
    canvas.setPenColor(217, 245, 255);
    canvas.setGlyphOptions(GlyphOptions().DoubleWidth(5));
    canvas.drawText(50, 20, "SPACE INVADERS");
    canvas.setGlyphOptions(GlyphOptions().DoubleWidth(0));

    canvas.setPenColor(59, 167, 204);
    canvas.drawText(85, 40, "con ESP32 por FIE");
    canvas.drawText(30, 55, "Facultad de Ingenieria Electrica.");

    canvas.setPenColor(248, 252, 167);
    canvas.setBrushColor(0, 0, 0);
    // canvas.fillRectangle(70, 92, 240, 110);
    canvas.drawRectangle(70, 92, 240, 110);
    canvas.setPenColor(248, 252, 167);
    canvas.drawText(72, 97, "  Tabla de puntajes  ");
    canvas.drawBitmap(TEXT_X - 40 - 2, TEXT_Y - 2, &bmpEnemyD);
    canvas.drawBitmap(TEXT_X - 40, TEXT_Y + 10, &bmpEnemyA[0]);
    canvas.drawBitmap(TEXT_X - 40, TEXT_Y + 25, &bmpEnemyB[0]);
    canvas.drawBitmap(TEXT_X - 40, TEXT_Y + 40, &bmpEnemyC[0]);

    canvas.setBrushColor(21, 26, 70);

    music_ = soundGenerator.playSamples(themeSoundSamples, sizeof(themeSoundSamples), 100, -1);
  }

  void update(int updateCount)
  {
    static const char *scoreText[] = {"= ? MISTERIOSO", "= 30 PUNTOS", "= 20 PUNTOS", "= 10 PUNTOS"};

    if (starting_)
    {

      if (starting_ > 50)
      {
        // stop music
        soundGenerator.detach(music_);
        // stop scene
        stop();
      }

      ++starting_;
      canvas.scroll(0, -5);
    }
    else
    {
      if (updateCount > 30 && updateCount % 5 == 0 && textRow_ < 4)
      {
        int x = TEXT_X + textCol_ * canvas.getFontInfo()->width - 9;
        int y = TEXT_Y + textRow_ * 15 - 4;
        canvas.setPenColor(255, 255, 255);
        canvas.drawChar(x, y, scoreText[textRow_][textCol_]);
        ++textCol_;
        if (scoreText[textRow_][textCol_] == 0)
        {
          textCol_ = 0;
          ++textRow_;
        }
      }

      if (updateCount % 20 == 0)
      {
        canvas.setPenColor(random(255), random(255), 255);
        canvas.drawText(50, 75, "Presiona [START] para jugar");
      }

      // handle keyboard or mouse (after two seconds)
      if (updateCount > 50)
      {
        if (Ps3.event.button_down.start)
          starting_ = true;
      }
    }
  }

  void collisionDetected(Sprite *spriteA, Sprite *spriteB, Point collisionPoint)
  {
  }
};

// GameScene
struct GameScene : public Scene
{

  enum SpriteType
  {
    TYPE_PLAYERFIRE,
    TYPE_PLAYERFIRE2,
    TYPE_ENEMIESFIRE,
    TYPE_ENEMY,
    TYPE_PLAYER,
    TYPE_PLAYER2,
    TYPE_SHIELD,
    TYPE_ENEMYMOTHER
  };

  struct SISprite : Sprite
  {
    SpriteType type;
    uint8_t enemyPoints;
  };

  enum GameState
  {
    GAMESTATE_PLAYING,
    GAMESTATE_PLAYERKILLED,
    GAMESTATE_PLAYER2KILLED,
    GAMESTATE_ENDGAME,
    GAMESTATE_GAMEOVER,
    GAMESTATE_LEVELCHANGING,
    GAMESTATE_LEVELCHANGED
  };
  unsigned long TiempoInicio;
  unsigned long TiempoTranscurrido;
  
  static const int PLAYERSCOUNT = 1;
  static const int SHIELDSCOUNT = 3;
  static const int ROWENEMIESCOUNT = 11;
  static const int PLAYERFIRECOUNT = 1;
  static const int ENEMIESFIRECOUNT = 1;
  static const int ENEMYMOTHERCOUNT = 1;
  static const int SPRITESCOUNT = 2 * PLAYERSCOUNT + SHIELDSCOUNT + 3 * ROWENEMIESCOUNT + 2 * PLAYERFIRECOUNT + ENEMIESFIRECOUNT + ENEMYMOTHERCOUNT;

  static const int ENEMIES_X_SPACE = 16; // Espacio entre enemigos
  static const int ENEMIES_Y_SPACE = 10;
  static const int ENEMIES_START_X = 0;
  static const int ENEMIES_START_Y = 30;
  static const int ENEMIES_STEP_X = 6;
  static const int ENEMIES_STEP_Y = 8;

  static const int PLAYER_Y = 170;
  static const int PLAYER2_Y = 170;

  static int score_;
  static int level_;
  static int hiScore_;

  SISprite *sprites_ = new SISprite[SPRITESCOUNT];
  SISprite *player_ = sprites_;
  SISprite *player2_ = player_ + PLAYERSCOUNT;
  SISprite *shields_ = player2_ + PLAYERSCOUNT;
  SISprite *enemies_ = shields_ + SHIELDSCOUNT;
  SISprite *enemiesR1_ = enemies_;
  SISprite *enemiesR2_ = enemiesR1_ + ROWENEMIESCOUNT;
  SISprite *enemiesR3_ = enemiesR2_ + ROWENEMIESCOUNT;
  SISprite *playerFire_ = enemiesR3_ + ROWENEMIESCOUNT;
  SISprite *playerFire2_ = playerFire_ + PLAYERFIRECOUNT;
  SISprite *enemiesFire_ = playerFire2_ + PLAYERFIRECOUNT;

  SISprite *enemyMother_ = enemiesFire_ + ENEMIESFIRECOUNT;

  int playerVelX_ = 0; // 0 = no move
  int player2VelX_ = 0;
  int enemiesX_ = ENEMIES_START_X;
  int enemiesY_ = ENEMIES_START_Y;

  // enemiesDir_
  //   bit 0 : if 1 moving left
  //   bit 1 : if 1 moving right
  //   bit 2 : if 1 moving down
  //   bit 3 : if 0 before was moving left, if 1 before was moving right
  // Allowed cases:
  //   1  = moving left
  //   2  = moving right
  //   4  = moving down (before was moving left)
  //   12 = moving down (before was moving right)

  static constexpr int ENEMY_MOV_LEFT = 1;
  static constexpr int ENEMY_MOV_RIGHT = 2;
  static constexpr int ENEMY_MOV_DOWN_BEFORE_LEFT = 4;
  static constexpr int ENEMY_MOV_DOWN_BEFORE_RIGHT = 12;

  int enemiesDir_ = ENEMY_MOV_RIGHT;

  int enemiesAlive_ = ROWENEMIESCOUNT * 3;
  int enemiesSoundCount_ = 0;
  SISprite *lastHitEnemy_ = nullptr;
  GameState gameState_ = GAMESTATE_PLAYING;

  bool updateScore_ = true;
  int64_t pauseStart_;

  Bitmap bmpShield[3] = {
      Bitmap(22, 16, shield_data, PixelFormat::Mask, RGB888(47, 93, 130), true),
      Bitmap(22, 16, shield_data, PixelFormat::Mask, RGB888(47, 93, 130), true),
      Bitmap(22, 16, shield_data, PixelFormat::Mask, RGB888(47, 93, 130), true),
  };

  GameScene()
      : Scene(SPRITESCOUNT, 20, DisplayController.getViewPortWidth(), DisplayController.getViewPortHeight())
  {
  }

  ~GameScene()
  {
    delete[] sprites_;
  }

  void initEnemy(Sprite *sprite, int points)
  {
    SISprite *s = (SISprite *)sprite;
    s->addBitmap(&bmpEnemyExplosion);
    s->type = TYPE_ENEMY;
    s->enemyPoints = points;
    addSprite(s);
  }

  void init()
  {
    TiempoInicio = millis();
    // setup player 1
    player_->addBitmap(&bmpPlayer)->addBitmap(&bmpPlayerExplosion[0])->addBitmap(&bmpPlayerExplosion[1]);
    player_->moveTo(225, PLAYER_Y);
    player_->type = TYPE_PLAYER;
    addSprite(player_);
    // setup player fire
    playerFire_->addBitmap(&bmpPlayerFire);
    playerFire_->visible = false;
    playerFire_->type = TYPE_PLAYERFIRE;
    //playerFire_->type = TYPE_PLAYERFIRE2;
    addSprite(playerFire_);

    // setup player 2
    player2_->addBitmap(&bmpPlayer2)->addBitmap(&bmpPlayerExplosion2[0])->addBitmap(&bmpPlayerExplosion2[1]);
    player2_->moveTo(75, PLAYER_Y);
    player2_->type = TYPE_PLAYER2;
    addSprite(player2_);
    // setup player fire 2
    playerFire2_->addBitmap(&bmpPlayerFire2);
    playerFire2_->visible = false;
    playerFire2_->type = TYPE_PLAYERFIRE2;
    addSprite(playerFire2_);

    // setup shields
    for (int i = 0; i < 3; ++i)
    {
      shields_[i].addBitmap(&bmpShield[i])->moveTo(70 + i * 75, 150);
      shields_[i].isStatic = true;
      shields_[i].type = TYPE_SHIELD;
      addSprite(&shields_[i]);
    }
    // setup enemies
    for (int i = 0; i < ROWENEMIESCOUNT; ++i)
    {
      initEnemy(enemiesR1_[i].addBitmap(&bmpEnemyA[0])->addBitmap(&bmpEnemyA[1]), 30);
      initEnemy(enemiesR2_[i].addBitmap(&bmpEnemyB[0])->addBitmap(&bmpEnemyB[1]), 20);
      initEnemy(enemiesR3_[i].addBitmap(&bmpEnemyC[0])->addBitmap(&bmpEnemyC[1]), 20);
    }
    // setup enemies fire
    enemiesFire_->addBitmap(&bmpEnemiesFire[0])->addBitmap(&bmpEnemiesFire[1]);
    enemiesFire_->visible = false;
    enemiesFire_->type = TYPE_ENEMIESFIRE;
    addSprite(enemiesFire_);
    // setup enemy mother ship
    enemyMother_->addBitmap(&bmpEnemyD)->addBitmap(&bmpEnemyExplosionRed);
    enemyMother_->visible = false;
    enemyMother_->type = TYPE_ENEMYMOTHER;
    enemyMother_->enemyPoints = 100;
    enemyMother_->moveTo(getWidth(), ENEMIES_START_Y);
    addSprite(enemyMother_);

    DisplayController.setSprites(sprites_, SPRITESCOUNT);

    canvas.setBrushColor(21, 26, 70);
    canvas.clear();

    canvas.setPenColor(47, 93, 130);
    canvas.drawLine(0, 180, 320, 180);

    canvas.setGlyphOptions(GlyphOptions().FillBackground(true));
    canvas.selectFont(&fabgl::FONT_8x8);
    canvas.setPenColor(248, 252, 167);
    canvas.drawText(133, 2, "TIEMPO");
    canvas.setPenColor(69, 142, 237);
    canvas.drawText(2, 2, "SCORE");
    canvas.setPenColor(248, 252, 167);
    canvas.drawText(254, 2, "HI-SCORE");
    canvas.setPenColor(255, 255, 255);
    canvas.drawTextFmt(256, 181, "Nivel %02d", level_);

  }


  void drawScore()
  {
    canvas.setPenColor(255, 255, 255);
    canvas.drawTextFmt(2, 14, "%05d", score_);
    /*canvas.setPenColor(255, 255, 255);
    canvas.drawTextFmt(133, 14, "%03d", TiempoTranscurrido);*/
    if (score_ > hiScore_)
      hiScore_ = score_;
    canvas.setPenColor(255, 255, 255);
    canvas.drawTextFmt(266, 14, "%05d", hiScore_);
  }

  void moveEnemy(SISprite *enemy, int x, int y, bool *touchSide)
  {
    if (enemy->visible)
    {
      if (x <= 0 || x >= getWidth() - enemy->getWidth())
        *touchSide = true;
      enemy->moveTo(x, y);
      enemy->setFrame(enemy->getFrameIndex() ? 0 : 1);
      updateSprite(enemy);
      if (y >= PLAYER_Y)
      {
        // enemies reach earth!
        gameState_ = GAMESTATE_ENDGAME;
      }
    }
  }

  void gameOver()
  {
    // disable enemies drawing, so text can be over them
    for (int i = 0; i < ROWENEMIESCOUNT * 5; ++i)
      enemies_[i].allowDraw = false;
    // show game over
    canvas.setPenColor(248, 252, 167);//248, 252, 67
    canvas.setBrushColor(28, 35, 92);
    canvas.fillRectangle(40, 60, 270, 130);
    canvas.drawRectangle(40, 60, 270, 130);
    canvas.setGlyphOptions(GlyphOptions().DoubleWidth(0));
    canvas.setPenColor(255, 255, 255);
    canvas.drawText(80, 72, "TIEMPO TERMINADO!");//55, 80
    canvas.setGlyphOptions(GlyphOptions().DoubleWidth(0));
    canvas.setPenColor(248, 252, 167);
    canvas.drawText(100, 110, "Presiona [START]");
    // change state
    gameState_ = GAMESTATE_GAMEOVER;
    level_ = 1;
    score_ = 0;
  }

  void levelChange()
  {
    ++level_;
    // show game over
    canvas.setPenColor(248, 252, 167);
    canvas.drawRectangle(80, 80, 240, 110);
    canvas.setGlyphOptions(GlyphOptions().DoubleWidth(1));
    canvas.drawTextFmt(105, 88, "NIVEL %d", level_);
    canvas.setGlyphOptions(GlyphOptions().DoubleWidth(0));
    // change state
    gameState_ = GAMESTATE_LEVELCHANGED;
    pauseStart_ = esp_timer_get_time();
  }

  void update(int updateCount)
  {

    if (updateScore_)
    {
      updateScore_ = false;
      drawScore();
    }

    if (gameState_ == GAMESTATE_PLAYING || gameState_ == GAMESTATE_PLAYERKILLED || gameState_ == GAMESTATE_PLAYER2KILLED)
    {

      // move enemies and shoot
      if ((updateCount % max(3, 21 - level_ * 2)) == 0)
      {
        // handle enemy explosion
        if (lastHitEnemy_)
        {
          lastHitEnemy_->visible = false;
          lastHitEnemy_ = nullptr;
        }
        // handle enemies movement
        enemiesX_ += (-1 * (enemiesDir_ & 1) + (enemiesDir_ >> 1 & 1)) * ENEMIES_STEP_X;
        enemiesY_ += (enemiesDir_ >> 2 & 1) * ENEMIES_STEP_Y;
        bool touchSide = false;
        for (int i = 0; i < ROWENEMIESCOUNT; ++i)
        {
          moveEnemy(&enemiesR1_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 0 * ENEMIES_Y_SPACE, &touchSide);
          moveEnemy(&enemiesR2_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 1 * ENEMIES_Y_SPACE, &touchSide);
          moveEnemy(&enemiesR3_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 2 * ENEMIES_Y_SPACE, &touchSide);
        }
        switch (enemiesDir_)
        {
        case ENEMY_MOV_DOWN_BEFORE_LEFT:
          enemiesDir_ = ENEMY_MOV_RIGHT;
          break;
        case ENEMY_MOV_DOWN_BEFORE_RIGHT:
          enemiesDir_ = ENEMY_MOV_LEFT;
          break;
        default:
          if (touchSide)
            enemiesDir_ = (enemiesDir_ == ENEMY_MOV_LEFT ? ENEMY_MOV_DOWN_BEFORE_LEFT : ENEMY_MOV_DOWN_BEFORE_RIGHT);
          break;
        }
        // sound
        ++enemiesSoundCount_;
        soundGenerator.playSamples(invadersSoundSamples[enemiesSoundCount_ % 4], invadersSoundSamplesSize[enemiesSoundCount_ % 4]);
        // handle enemies fire generation
        if (!enemiesFire_->visible)
        {
          int shottingEnemy = random(enemiesAlive_);
          for (int i = 0, a = 0; i < ROWENEMIESCOUNT * 3; ++i)
          {
            if (enemies_[i].visible)
            {
              if (a == shottingEnemy)
              {
                enemiesFire_->x = enemies_[i].x + enemies_[i].getWidth() / 2;
                enemiesFire_->y = enemies_[i].y + enemies_[i].getHeight() / 2;
                enemiesFire_->visible = true;
                break;
              }
              ++a;
            }
          }
        }
      }

     if (gameState_ == GAMESTATE_PLAYER2KILLED)
      {
        if ((updateCount % 20) == 0)
        {
          if (gameState_ == GAMESTATE_PLAYER2KILLED)
          {
            player2_->visible = true;
            gameState_ = GAMESTATE_PLAYING;
          }
        }
      }
      else if (gameState_ == GAMESTATE_PLAYERKILLED)
      {
        // animate player explosion or restart playing other lives
        if ((updateCount % 20) == 0)
        {
          if (gameState_ == GAMESTATE_PLAYER2KILLED)
          {
            player2_->visible = true;
            gameState_ = GAMESTATE_PLAYING;
          }
        }
      } else if (playerVelX_ != 0 || player2VelX_ != 0)
      {
        // Movimiento de la posición
        player_->x += playerVelX_;
        player_->x = iclamp(player_->x, 0, getWidth() - player_->getWidth());
        updateSprite(player_);

        player2_->x += player2VelX_;
        player2_->x = iclamp(player2_->x, 0, getWidth() - player2_->getWidth());
        updateSprite(player2_);
      }

      // move player fire
      if (playerFire_->visible)
      {
        playerFire_->y -= 3;
        if (playerFire_->y < ENEMIES_START_Y)
          playerFire_->visible = false;
        else
          updateSpriteAndDetectCollisions(playerFire_);
      }

      if (playerFire2_->visible)
      {
        playerFire2_->y -= 3;
        if (playerFire2_->y < ENEMIES_START_Y)
          playerFire2_->visible = false;
        else
          updateSpriteAndDetectCollisions(playerFire2_);
      }

      // move enemies fire
      if (enemiesFire_->visible)
      {
        enemiesFire_->y += 2;
        enemiesFire_->setFrame(enemiesFire_->getFrameIndex() ? 0 : 1);
        if (enemiesFire_->y > PLAYER_Y + player_->getHeight())
          enemiesFire_->visible = false;
        else
          updateSpriteAndDetectCollisions(enemiesFire_);
      }

      // move enemy mother ship
      if (enemyMother_->visible && enemyMother_->getFrameIndex() == 0)
      {
        enemyMother_->x -= 1;
        if (enemyMother_->x < -enemyMother_->getWidth())
          enemyMother_->visible = false;
        else
          updateSprite(enemyMother_);
      }

      // start enemy mother ship
      if ((updateCount % 800) == 0)
      {
        soundGenerator.playSamples(motherShipSoundSamples, sizeof(motherShipSoundSamples), 100, 7000);
        enemyMother_->x = getWidth();
        enemyMother_->setFrame(0);
        enemyMother_->visible = true;
      }

      // Uso del control de PS3 de jugador 1.
      if (Ps3.data.analog.stick.rx > 90 || Ps3.data.analog.stick.rx < -90)
      {
        if (Ps3.data.analog.stick.rx > 90)
        {
          playerVelX_ = +1;
        }
        else if (Ps3.data.analog.stick.rx < -90)
        {
          playerVelX_ = -1;
        }
      }
      else
      {
        playerVelX_ = 0;
      }

      if (abs(Ps3.event.analog_changed.button.cross) && !playerFire_->visible) // player fire?
        fire();

      // Uso del control de PS3 de jugador 2.
      if (Ps3.data.analog.stick.lx > 90 || Ps3.data.analog.stick.lx < -90)
      {
        if (Ps3.data.analog.stick.lx > 90)
        {
          player2VelX_ = +1;
        }
        else if (Ps3.data.analog.stick.lx < -90)
        {
          player2VelX_ = -1;
        }
      }
      else
      {
        player2VelX_ = 0;
      }

      if (abs(Ps3.event.analog_changed.button.down) && !playerFire2_->visible) // player fire?
        fire2();
    }

    if (gameState_ == GAMESTATE_ENDGAME)
      gameOver();

    if (gameState_ == GAMESTATE_LEVELCHANGING)
      levelChange();

    if (gameState_ == GAMESTATE_LEVELCHANGED && esp_timer_get_time() >= pauseStart_ + 2500000)
    {
      stop(); // restart from next level
      DisplayController.removeSprites();
    }

    if (gameState_ == GAMESTATE_GAMEOVER)
    {

      // animate player burning
      if ((updateCount % 20) == 0)
      {
        player_->setFrame(player_->getFrameIndex() == 1 ? 2 : 1);
        player_->setFrame(player_->getFrameIndex() == 1 ? 2 : 1);
      }

      if (Ps3.event.button_down.start)
      {
        stop();
        DisplayController.removeSprites();
      }
    }

    DisplayController.refreshSprites();

    // funcion del tiempo
    TiempoTranscurrido = (millis() - TiempoInicio)/1000;
    if (90 - TiempoTranscurrido == 0)
    {
      // Mostrar mensaje de tiempo agotado
      //mostrarMensajeTiempoAgotado();
      gameOver();
      TiempoTranscurrido = 0;
    }

    canvas.setPenColor(255, 255, 255);
    canvas.drawTextFmt(150, 14, "%2d", 90 - TiempoTranscurrido);
  }

  // player shoots
  void fire()
  {
    playerFire_->moveTo(player_->x + 7, player_->y - 1)->visible = true;
    soundGenerator.playSamples(fireSoundSamples, sizeof(fireSoundSamples));
  }

  void fire2()
  {
    playerFire2_->moveTo(player2_->x + 7, player2_->y - 1)->visible = true;
    soundGenerator.playSamples(fireSoundSamples, sizeof(fireSoundSamples));
  }

  //Cambios del tiempo aqui

  // shield has been damaged
  void damageShield(SISprite *shield, Point collisionPoint)
  {
    Bitmap *shieldBitmap = shield->getFrame();
    int x = collisionPoint.X - shield->x;
    int y = collisionPoint.Y - shield->y;
    shieldBitmap->setPixel(x, y, 0);
    for (int i = 0; i < 32; ++i)
    {
      int px = iclamp(x + random(-4, 5), 0, shield->getWidth() - 1);
      int py = iclamp(y + random(-4, 5), 0, shield->getHeight() - 1);
      shieldBitmap->setPixel(px, py, 0);
    }
  }

  void collisionDetected(Sprite *spriteA, Sprite *spriteB, Point collisionPoint)
  {
    SISprite *sA = (SISprite *)spriteA;
    SISprite *sB = (SISprite *)spriteB;
    if (!lastHitEnemy_ && sA->type == TYPE_PLAYERFIRE && sB->type == TYPE_ENEMY)
    {
      // player fire hits an enemy
      soundGenerator.playSamples(shootSoundSamples, sizeof(shootSoundSamples));
      sA->visible = false;
      sB->setFrame(2);
      lastHitEnemy_ = sB;
      --enemiesAlive_;
      score_ += sB->enemyPoints;
      updateScore_ = true;
      if (enemiesAlive_ == 0)
        gameState_ = GAMESTATE_LEVELCHANGING;
    }
 
    if (sB->type == TYPE_SHIELD)
    {
      // something hits a shield
      sA->visible = false;
      damageShield(sB, collisionPoint);
      sB->allowDraw = true;
    }

    if (gameState_ == GAMESTATE_PLAYING && sA->type == TYPE_ENEMIESFIRE && sB->type == TYPE_PLAYER)
    {
      // Golpe del enemigo
      soundGenerator.playSamples(explosionSoundSamples, sizeof(explosionSoundSamples));
      gameState_ = GAMESTATE_PLAYERKILLED;
      player_->visible = false;
    }
     if (!lastHitEnemy_ && sA->type == TYPE_PLAYERFIRE2 && sB->type == TYPE_ENEMY)
    {
      // player fire hits an enemy
      soundGenerator.playSamples(shootSoundSamples, sizeof(shootSoundSamples));
      sA->visible = false;
      sB->setFrame(2);
      lastHitEnemy_ = sB;
      --enemiesAlive_;
      score_ += sB->enemyPoints;
      updateScore_ = true;
      if (enemiesAlive_ == 0)
        gameState_ = GAMESTATE_LEVELCHANGING;
    }

    if (gameState_ == GAMESTATE_PLAYING && sA->type == TYPE_ENEMIESFIRE && sB->type == TYPE_PLAYER2)
    {
      //  Golpe de enemigo
      soundGenerator.playSamples(explosionSoundSamples, sizeof(explosionSoundSamples));
      gameState_ = GAMESTATE_PLAYER2KILLED;
      player_->visible = false;
    }

    if (!lastHitEnemy_ && sA->type == TYPE_PLAYERFIRE2 && sB->type == TYPE_ENEMY)
    {
      // player fire hits an enemy
      soundGenerator.playSamples(shootSoundSamples, sizeof(shootSoundSamples));
      sA->visible = false;
      sB->setFrame(2);
      lastHitEnemy_ = sB;
      --enemiesAlive_;
      score_ += sB->enemyPoints;
      updateScore_ = true;
      if (enemiesAlive_ == 0)
        gameState_ = GAMESTATE_LEVELCHANGING;
    }

    if (sB->type == TYPE_ENEMYMOTHER)
    {
      // player fire hits enemy mother ship
      soundGenerator.playSamples(mothershipexplosionSoundSamples, sizeof(mothershipexplosionSoundSamples));
      sA->visible = false;
      sB->setFrame(1);
      lastHitEnemy_ = sB;
      score_ += sB->enemyPoints;
      updateScore_ = true;
    }
  }
};

int GameScene::hiScore_ = 0;
int GameScene::level_ = 1;
int GameScene::score_ = 0;

void setup()
{
  //78:dd:08:4d:94:a4
  //24:6f:28:af:1c:66
  Ps3.begin("78:dd:08:4d:94:a4");
  DisplayController.begin();
  DisplayController.setResolution(VGA_320x200_75Hz);
}



void loop()
{
    // Inicia el tiempo
 

    // Inicia la escena de introducción solo en el primer nivel
    if (GameScene::level_ == 1) {
        IntroScene introScene;
        introScene.start();
    }

    GameScene gameScene;
    gameScene.start();

    // Bucle para verificar el tiempo transcurrido y detener el juego si es necesario
    
        // Calcula el tiempo transcurrido

            // Detener el juego
            // Puedes agregar aquí la lógica para detener el juego adecuadamente
            //break; // Salir del bucle de verificación de tiempo
        
        // Actualiza el juego u otras operaciones si es necesario
    
}