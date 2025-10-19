/*
ESP32 PONG (esPONG)

Copyright (C) 2025 RafaG @ MINIBOTS <https://minibots.wordpress.com/>

Este programa es software libre: usted puede redistribuirlo y/o modificarlo
bajo los términos de la Licencia Pública General de GNU
publicada por la Free Software Foundation,
o (a su elección) cualquier versión posterior de la Licencia.

Este programa se distribuye con la esperanza de que sea útil,
pero SIN GARANTÍA ALGUNA; ni siquiera la garantía implícita
MERCANTIBILIDAD o APTITUD PARA UN PROPÓSITO PARTICULAR.
Consulte los detalles de la Licencia Pública General de GNU para más información.

Debería haber recibido una copia de la Licencia Pública General de GNU
junto con este programa. En caso contrario, consulte <http://www.gnu.org/licenses/>.
*/

#include <Arduino.h>
#include <ESP32Lib.h>
#include <Ressources/Font6x8.h>
#include <PS2KeyAdvanced.h>

// Constantes para dimensiones y configuración
const int LOGICAL_WIDTH = 256;
const int LOGICAL_HEIGHT = 248;
const int PADDLE_HEIGHT = 40;
const int PADDLE_WIDTH = 4;
const int BALL_SIZE = 4;
const int PADDLE_MARGIN = 20;
const int SCREEN_MARGIN = 1;
const int GAME_AREA_TOP = 28;
const int GAME_AREA_BOTTOM = 248;

// Velocidades
const int PADDLE_SPEED = 8;
const int BALL_SPEED_INITIAL = 1;

// Posiciones iniciales
const int BALL_START_X = 128;
const int BALL_START_Y = 124;
const int PADDLE_START_Y = 100;

// Scores y límites
const int MAX_SCORE = 10;
const int SCORE1_X = 95;
const int SCORE2_X = 145;
const int SCORE_Y = 5;
const int DIGIT_WIDTH = 16;
const int DIGIT_HEIGHT = 22;
const int SEGMENT_THICKNESS = 2;

// Espaciado de línea central
const int CENTER_LINE_SPACING = 8;

// Pines VGA
const int VGA_RED_PIN = 22;
const int VGA_GREEN_PIN = 19;
const int VGA_BLUE_PIN = 5;
const int VGA_HSYNC_PIN = 23;
const int VGA_VSYNC_PIN = 15;

// Pines PS/2
const int PS2_CLK_PIN = 33;
const int PS2_DATA_PIN = 32;

// Pines potenciómetros
const int POT1_PIN = 34;
const int POT2_PIN = 35;

// Intervalos
const unsigned long KEY_CHECK_INTERVAL = 1;

// VGA Device
VGA3Bit vga;

// Teclado PS/2
PS2KeyAdvanced teclado;

// Variables del juego
int paddle1Y = PADDLE_START_Y;
int paddle2Y = PADDLE_START_Y;
// Variables de la bola
int ballX, ballY;
float ballSpeedX, ballSpeedY;
int ballBounceCount = 0;
float ballSpeedMultiplier = 1.0;
int lastScoredBy = 0;
int score1 = 0;
int score2 = 0;

// Estados del juego
bool gamePaused = false;
bool gameStarted = false; // False hasta que se pulse una tecla
bool waitingForRestart = false; // True cuando el juego terminó y espera reinicio

// Control de teclado
unsigned long lastKeyCheck = 0;

// Aceleración de palas
bool paddle1WPressed = false;
unsigned long paddle1WPressStart = 0;
bool paddle1SPressed = false;
unsigned long paddle1SPressStart = 0;
bool paddle2UpPressed = false;
unsigned long paddle2UpPressStart = 0;
bool paddle2DownPressed = false;
unsigned long paddle2DownPressStart = 0;

// Variables para seguimiento
int prevBallX = BALL_START_X;
int prevBallY = BALL_START_Y;
int prevPaddle1Y = PADDLE_START_Y;
int prevPaddle2Y = PADDLE_START_Y;
bool paddle1Moved = false;
bool paddle2Moved = false;

void setup() {
  Serial.begin(115200);
  Serial.println("esPONG (ESP32 Pong) by MINIBOTS");
  Serial.println("-------------------------------");  
  Serial.println("Iniciando esPONG con PS/2...");
  
  // Inicializar VGA PRIMERO
  vga.init(VGAMode::MODE320x240.custom(LOGICAL_WIDTH, LOGICAL_HEIGHT), VGA_RED_PIN, VGA_GREEN_PIN, VGA_BLUE_PIN, VGA_HSYNC_PIN, VGA_VSYNC_PIN);
  
  // CORREGIDO: Orden correcto de pines DATA, CLK
  teclado.begin(PS2_DATA_PIN, PS2_CLK_PIN);
  delay(500); // Pausa más larga para inicialización del teclado
  
  Serial.println("Teclado PS/2 inicializado");
  Serial.println("Controles:");
  Serial.println("Jugador 1: W (arriba), S (abajo)");
  Serial.println("Jugador 2: Flecha Arriba, Flecha Abajo");
  Serial.println("Espacio: Pausa/Reanudar");
  Serial.println("Enter: Reiniciar juego");
  Serial.println("Cualquier tecla: Iniciar partida");
  
  // Inicializar pines de potenciómetros (como fallback)
  pinMode(POT1_PIN, INPUT);
  pinMode(POT2_PIN, INPUT);
  
  // Inicializar posición inicial de la bola
  ballX = BALL_START_X;
  ballY = (GAME_AREA_TOP + GAME_AREA_BOTTOM) / 2;
  
  // DIBUJAR INMEDIATAMENTE - pero juego pausado
  vga.clear(0);
  drawStaticElements();
  updateScore();
  drawInitialScreen();
  drawStartMessage();
  
  // Mensaje inicial
  Serial.println("Esperando tecla para iniciar...");
}

void loop() {
  if (!gameStarted) {
    waitForStart();
  } else if (!gamePaused && !waitingForRestart) {
    readControls();
    updateGame();
  }
  
  // Manejar reinicio si el juego terminó
  if (waitingForRestart) {
    if (teclado.available()) {
      int key = teclado.read();
      if (key == 0x5A || (key & 0xFF)) {
        resetGame();
        waitingForRestart = false;
      }
    }
  }
  
  drawGame();
  delay(16);
}

void waitForStart() {
  // Esperar a que se pulse cualquier tecla para empezar
  if (teclado.available()) {
    int key = teclado.read();
    if (key & 0xFF) {
      gameStarted = true;
      // Iniciar la pelota
      ballX = BALL_START_X;
      ballY = (GAME_AREA_TOP + GAME_AREA_BOTTOM) / 2;
      ballSpeedX = (random(0, 2) == 0) ? BALL_SPEED_INITIAL : -BALL_SPEED_INITIAL;
      ballSpeedY = random(-BALL_SPEED_INITIAL, BALL_SPEED_INITIAL + 1);
      ballSpeedX *= ballSpeedMultiplier;
      ballSpeedY *= ballSpeedMultiplier;
      vga.fillRect(60, 110, 140, 55, 0); // Borrar mensaje de inicio
      // Redibujar segmentos de línea central borrados
      for (int y = max(110, GAME_AREA_TOP); y < 165; y += CENTER_LINE_SPACING) {
        vga.fillRect(128 - 1, y, 2, 4, 7);
      }
      Serial.println("¡Juego iniciado!");
    }
  }
  // No leer potenciómetros en pantalla de inicio para evitar temblor de palas
  // readPotControls();
}

void drawStartMessage() {
  // Dibujar mensaje "esPONG" centrado y más grande
  vga.fillRect(60, 110, 140, 45, 0); // Limpiar área centrada y más amplia
  
  // e (minúscula, más grande y mejor definida, alineada con PONG)
  vga.fillRect(71, 118, 12, 4, 7); // Horizontal medio superior
  vga.fillRect(71, 132, 12, 4, 7); // Horizontal abajo
  vga.fillRect(71, 110, 12, 4, 7); // Horizontal arriba
  vga.fillRect(71, 110, 4, 26, 7); // Vertical izquierda completa
  vga.fillRect(79, 110, 4, 8, 7); // Vertical derecha arriba
  
  // s (minúscula, más grande y mejor definida, alineada con PONG)
  vga.fillRect(87, 110, 12, 4, 7); // Horizontal arriba
  vga.fillRect(87, 121, 12, 4, 7); // Horizontal medio
  vga.fillRect(87, 132, 12, 4, 7); // Horizontal abajo
  vga.fillRect(87, 110, 4, 11, 7); // Vertical izquierda arriba
  vga.fillRect(95, 121, 4, 11, 7); // Vertical derecha abajo
  
  // P (mayúscula, más grande)
  vga.fillRect(101, 110, 4, 30, 7);  // Vertical izquierda
  vga.fillRect(105, 110, 12, 4, 7); // Horizontal arriba
  vga.fillRect(105, 125, 12, 4, 7); // Horizontal medio
  vga.fillRect(113, 110, 4, 15, 7); // Vertical derecha arriba
  
  // O (más grande)
  vga.fillRect(121, 110, 4, 30, 7); // Vertical izquierda
  vga.fillRect(133, 110, 4, 30, 7); // Vertical derecha
  vga.fillRect(125, 110, 8, 4, 7); // Horizontal arriba
  vga.fillRect(125, 136, 8, 4, 7); // Horizontal abajo
  
  // N (más grande)
  vga.fillRect(141, 110, 4, 30, 7); // Vertical izquierda
  vga.fillRect(153, 110, 4, 30, 7); // Vertical derecha
  for (int i = 0; i < 30; i += 4) {
    vga.fillRect(145 + i/5, 110 + i, 3, 4, 7); // Diagonal hacia abajo
  }
  
  // G (más grande)
  vga.fillRect(161, 110, 4, 30, 7); // Vertical izquierda
  vga.fillRect(165, 110, 12, 4, 7); // Horizontal arriba
  vga.fillRect(165, 136, 12, 4, 7); // Horizontal abajo
  vga.fillRect(173, 125, 4, 11, 7); // Vertical derecha baja

	vga.setFont(Font6x8);
	//displaying the text
  vga.setCursor(97, 150);
	vga.println("by MINIBOTS");
}

void drawInitialScreen() {
  // Dibujar elementos iniciales inmediatamente
  drawPaddles();
  drawBall();
  redrawCenterLine();
}

void readControls() {
  // Guardar posiciones anteriores antes de actualizar
  prevPaddle1Y = paddle1Y;
  prevPaddle2Y = paddle2Y;
  
  // Leer teclado PS/2
  readPS2Controls();
  
  // Aplicar movimiento basado en teclas presionadas
  if (paddle1WPressed) {
    int timeHeld = millis() - paddle1WPressStart;
    int speed = 2 + timeHeld / 100;
    paddle1Y -= min(speed, 20);
    paddle1Moved = true;
  }
  if (paddle1SPressed) {
    int timeHeld = millis() - paddle1SPressStart;
    int speed = 2 + timeHeld / 100;
    paddle1Y += min(speed, 20);
    paddle1Moved = true;
  }
  if (paddle2UpPressed) {
    int timeHeld = millis() - paddle2UpPressStart;
    int speed = 2 + timeHeld / 100;
    paddle2Y -= min(speed, 20);
    paddle2Moved = true;
  }
  if (paddle2DownPressed) {
    int timeHeld = millis() - paddle2DownPressStart;
    int speed = 2 + timeHeld / 100;
    paddle2Y += min(speed, 20);
    paddle2Moved = true;
  }
  
  // Limitar palas al área de juego
  paddle1Y = constrain(paddle1Y, GAME_AREA_TOP + SCREEN_MARGIN, 
                      GAME_AREA_BOTTOM - PADDLE_HEIGHT - SCREEN_MARGIN);
  paddle2Y = constrain(paddle2Y, GAME_AREA_TOP + SCREEN_MARGIN, 
                      GAME_AREA_BOTTOM - PADDLE_HEIGHT - SCREEN_MARGIN);
}

void readPS2Controls() {
  // Leer teclado PS/2 de forma no bloqueante, procesar todas las teclas disponibles
  while (teclado.available()) {
    int key = teclado.read();
    
    // Eco de la tecla por Serial
    Serial.print("Tecla: 0x");
    Serial.println(key, HEX);
    
    // Procesar códigos de make (presión) y break (liberación)
    if (key == 0x1D || key == 0x57) { // W make
      if (!paddle1WPressed) {
        paddle1WPressed = true;
        paddle1WPressStart = millis();
      }
    } else if (key == 0x801D || key == 0x8057) { // W break
      paddle1WPressed = false;
      paddle1WPressStart = 0;
    } else if (key == 0x1B || key == 0x53) { // S make
      if (!paddle1SPressed) {
        paddle1SPressed = true;
        paddle1SPressStart = millis();
      }
    } else if (key == 0x801B || key == 0x8053) { // S break
      paddle1SPressed = false;
      paddle1SPressStart = 0;
    } else if (key == 0x117) { // Up make
      if (!paddle2UpPressed) {
        paddle2UpPressed = true;
        paddle2UpPressStart = millis();
      }
    } else if (key == 0x8117) { // Up break
      paddle2UpPressed = false;
      paddle2UpPressStart = 0;
    } else if (key == 0x118) { // Down make
      if (!paddle2DownPressed) {
        paddle2DownPressed = true;
        paddle2DownPressStart = millis();
      }
    } else if (key == 0x8118) { // Down break
      paddle2DownPressed = false;
      paddle2DownPressStart = 0;
    } else if (key == 0x11F) { // Space make
      gamePaused = !gamePaused;
      drawPauseMessage();
    } else if (key == 0x11E) { // Enter make
      resetGame();
    }
  }
}

void readPotControls() {
  // Fallback a potenciómetros cuando no hay entrada de teclado
  int pot1Value = analogRead(POT1_PIN);
  int pot2Value = analogRead(POT2_PIN);
  
  int oldPaddle1Y = paddle1Y;
  int oldPaddle2Y = paddle2Y;
  
  int newPaddle1Y = map(pot1Value, 0, 4095, GAME_AREA_TOP + SCREEN_MARGIN, 
                       GAME_AREA_BOTTOM - PADDLE_HEIGHT - SCREEN_MARGIN);
  int newPaddle2Y = map(pot2Value, 0, 4095, GAME_AREA_TOP + SCREEN_MARGIN, 
                       GAME_AREA_BOTTOM - PADDLE_HEIGHT - SCREEN_MARGIN);
  
  // Solo marcar como movido si hay cambio significativo y posición cambió
  if (abs(newPaddle1Y - paddle1Y) > 2) {
    paddle1Y = newPaddle1Y;
    paddle1Moved = (paddle1Y != oldPaddle1Y);
  }
  if (abs(newPaddle2Y - paddle2Y) > 2) {
    paddle2Y = newPaddle2Y;
    paddle2Moved = (paddle2Y != oldPaddle2Y);
  }
}

void updateGame() {
  // Solo actualizar el juego si ha empezado
  if (!gameStarted) return;
  
  static int ballFrame = 0;
  ballFrame++;
  
  prevBallX = ballX;
  prevBallY = ballY;
  
  // Mover bola cada 2 frames para velocidad más lenta
  if (ballFrame % 2 == 0) {
    ballX += ballSpeedX;
    ballY += ballSpeedY;
  }
  
  if (ballY <= GAME_AREA_TOP + SCREEN_MARGIN || ballY >= GAME_AREA_BOTTOM - BALL_SIZE - SCREEN_MARGIN) {
    ballSpeedY = -ballSpeedY;
    ballY = constrain(ballY, GAME_AREA_TOP + SCREEN_MARGIN + 1, GAME_AREA_BOTTOM - BALL_SIZE - SCREEN_MARGIN - 1);
    // Redibujar scores después del rebote superior para evitar borrado de segmentos
    if (ballY <= GAME_AREA_TOP + SCREEN_MARGIN + 5) {
      updateScore();
    }
  }
  
  if (ballX <= PADDLE_MARGIN + PADDLE_WIDTH && ballX >= PADDLE_MARGIN && 
      ballY + BALL_SIZE >= paddle1Y && ballY <= paddle1Y + PADDLE_HEIGHT) {
    ballSpeedX = abs(ballSpeedX);
    ballX = PADDLE_MARGIN + PADDLE_WIDTH + 1;
    int hitPos = ballY - (paddle1Y + PADDLE_HEIGHT/2);
    ballSpeedY = constrain(hitPos / 4, -3, 3);
    // Aumentar velocidad tras rebote
    ballBounceCount++;
    if (ballBounceCount == 1) ballSpeedMultiplier = 1.5;
    else if (ballBounceCount == 2) ballSpeedMultiplier = 2.0;
    else if (ballBounceCount == 3) ballSpeedMultiplier = 3.0;
    float prevMultiplier = (ballBounceCount == 1) ? 1.0 : (ballBounceCount == 2) ? 1.5 : 2.0;
    ballSpeedX *= ballSpeedMultiplier / prevMultiplier;
    ballSpeedY *= ballSpeedMultiplier / prevMultiplier;
  }
  
  if (ballX + BALL_SIZE >= LOGICAL_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH && ballX <= LOGICAL_WIDTH - PADDLE_MARGIN && 
      ballY + BALL_SIZE >= paddle2Y && ballY <= paddle2Y + PADDLE_HEIGHT) {
    ballSpeedX = -abs(ballSpeedX);
    ballX = LOGICAL_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH - BALL_SIZE - 1;
    int hitPos = ballY - (paddle2Y + PADDLE_HEIGHT/2);
    ballSpeedY = constrain(hitPos / 4, -3, 3);
    // Aumentar velocidad solo si la pala se movió
    if (paddle2Moved) {
      ballBounceCount++;
      if (ballBounceCount == 1) ballSpeedMultiplier = 1.5;
      else if (ballBounceCount == 2) ballSpeedMultiplier = 2.0;
      else if (ballBounceCount == 3) ballSpeedMultiplier = 3.0;
      float prevMultiplier = (ballBounceCount == 1) ? 1.0 : (ballBounceCount == 2) ? 1.5 : 2.0;
      ballSpeedX *= ballSpeedMultiplier / prevMultiplier;
      ballSpeedY *= ballSpeedMultiplier / prevMultiplier;
    }
  }
  
  if (ballX < -BALL_SIZE) {
    score2++;
    lastScoredBy = 2;
    resetBall();
    updateScore();
    // Pausa breve para mostrar el punto
    delay(500);
  } else if (ballX > 256) {
    score1++;
    lastScoredBy = 1;
    resetBall();
    updateScore();
    // Pausa breve para mostrar el punto
    delay(500);
  }
  
  if (score1 >= MAX_SCORE || score2 >= MAX_SCORE) {
    gameOver();
  }
}

void resetBall() {
  eraseBall();
  
  prevBallX = ballX;
  prevBallY = ballY;
  ballX = BALL_START_X;
  ballY = (GAME_AREA_TOP + GAME_AREA_BOTTOM) / 2;
  // Resetear velocidad a inicial para nuevos saques
  ballBounceCount = 0;
  ballSpeedMultiplier = 1.0;
  // Dirección hacia el lado que anotó
  if (lastScoredBy == 1) {
    ballSpeedX = BALL_SPEED_INITIAL; // hacia derecha (jugador 1)
  } else if (lastScoredBy == 2) {
    ballSpeedX = -BALL_SPEED_INITIAL; // hacia izquierda (jugador 2)
  } else {
    ballSpeedX = (random(0, 2) == 0) ? BALL_SPEED_INITIAL : -BALL_SPEED_INITIAL;
  }
  ballSpeedY = random(-BALL_SPEED_INITIAL, BALL_SPEED_INITIAL + 1);
  ballSpeedX *= ballSpeedMultiplier;
  ballSpeedY *= ballSpeedMultiplier;
}

void resetGame() {
  score1 = 0;
  score2 = 0;
  // No resetear bounceCount y multiplier aquí, mantener velocidad
  resetBall();
  gamePaused = false;
  gameStarted = true;
  waitingForRestart = false; // Salir del estado de espera
  ballSpeedX = (random(0, 2) == 0) ? BALL_SPEED_INITIAL : -BALL_SPEED_INITIAL;
  ballSpeedY = random(-BALL_SPEED_INITIAL, BALL_SPEED_INITIAL + 1);
  updateScore();
  vga.fillRect(50, 100, 156, 40, 0); // Borrar mensajes anteriores
  Serial.println("Juego reiniciado");
}

void gameOver() {
  // Dibujar mensaje de victoria inmediatamente
  drawWinMessage();
  
  // Mostrar mensaje en serial
  Serial.println("JUEGO TERMINADO - Presiona ENTER para reiniciar");
  
  // Establecer estado de espera para reinicio (no bloqueante)
  waitingForRestart = true;
}

void drawStaticElements() {
  drawCenterLine();
}

void drawCenterLine() {
  for (int y = 0; y < GAME_AREA_BOTTOM; y += CENTER_LINE_SPACING) {
    vga.fillRect(128 - 1, y, 2, 4, 7);
  }
}

void drawPauseMessage() {
  vga.fillRect(100, 120, 60, 20, 0);
  if (gamePaused) {
    // Dibujar "PAUSA" con rectángulos
    // Letra P
    vga.fillRect(105, 122, 3, 12, 7);
    vga.fillRect(108, 122, 6, 3, 7);
    vga.fillRect(108, 127, 6, 3, 7);
    // Letra A
    vga.fillRect(118, 122, 3, 12, 7);
    vga.fillRect(128, 122, 3, 12, 7);
    vga.fillRect(121, 122, 6, 3, 7);
    vga.fillRect(121, 127, 6, 3, 7);
  }
}

void drawGame() {
  // MEJORA: Borrado más agresivo de palas
  eraseBall();
  erasePaddles();
  drawPaddles();
  drawBall();
  redrawCenterLine();
  
  // Reset flags de movimiento después del dibujo
  paddle1Moved = false;
  paddle2Moved = false;
}

void eraseBall() {
  int margin = 2;
  if (prevBallY >= GAME_AREA_TOP && prevBallY <= GAME_AREA_BOTTOM) {
    vga.fillRect(prevBallX - margin, prevBallY - margin, 
                 BALL_SIZE + margin * 2, BALL_SIZE + margin * 2, 0);
  }
}

void erasePaddles() {
  // Borrar las posiciones anteriores de las palas
  if (paddle1Moved) {
    vga.fillRect(PADDLE_MARGIN, prevPaddle1Y, PADDLE_WIDTH, PADDLE_HEIGHT, 0);
  }
  
  if (paddle2Moved) {
    vga.fillRect(LOGICAL_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH, prevPaddle2Y, PADDLE_WIDTH, PADDLE_HEIGHT, 0);
  }
}

void drawPaddles() {
  vga.fillRect(PADDLE_MARGIN, paddle1Y, PADDLE_WIDTH, PADDLE_HEIGHT, 7);
  vga.fillRect(LOGICAL_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH, paddle2Y, PADDLE_WIDTH, PADDLE_HEIGHT, 7);
  
  paddle1Moved = false;
  paddle2Moved = false;
}

void drawBall() {
  if (ballX >= 0 && ballX <= LOGICAL_WIDTH - BALL_SIZE &&
      ballY >= GAME_AREA_TOP && ballY <= GAME_AREA_BOTTOM - BALL_SIZE) {
    vga.fillRect(ballX, ballY, BALL_SIZE, BALL_SIZE, 7);
  }
}

void redrawCenterLine() {
  int ballAreaTop = min(prevBallY, ballY) - 3;
  int ballAreaBottom = max(prevBallY + BALL_SIZE, ballY + BALL_SIZE) + 3;
  
  int paddleAreaTop = GAME_AREA_BOTTOM;
  int paddleAreaBottom = GAME_AREA_TOP;
  
  if (paddle1Moved) {
    paddleAreaTop = min(paddleAreaTop, min(prevPaddle1Y, paddle1Y));
    paddleAreaBottom = max(paddleAreaBottom, max(prevPaddle1Y + PADDLE_HEIGHT, paddle1Y + PADDLE_HEIGHT));
  }
  if (paddle2Moved) {
    paddleAreaTop = min(paddleAreaTop, min(prevPaddle2Y, paddle2Y));
    paddleAreaBottom = max(paddleAreaBottom, max(prevPaddle2Y + PADDLE_HEIGHT, paddle2Y + PADDLE_HEIGHT));
  }
  
  int affectedTop = min(ballAreaTop, paddleAreaTop);
  int affectedBottom = max(ballAreaBottom, paddleAreaBottom);
  
  affectedTop = max(affectedTop, GAME_AREA_TOP);
  affectedBottom = min(affectedBottom, GAME_AREA_BOTTOM);
  
  for (int y = 0; y < GAME_AREA_BOTTOM; y += CENTER_LINE_SPACING) {
    if (y + 4 >= affectedTop && y <= affectedBottom && y >= GAME_AREA_TOP) {
      vga.fillRect(128 - 1, y, 2, 4, 7);
    }
  }
}

void updateScore() {
  // MEJORA: Borrado más amplio de los dígitos
  vga.fillRect(SCORE1_X - 4, SCORE_Y - 4, DIGIT_WIDTH * 2 + 8, DIGIT_HEIGHT + 8, 0);
  vga.fillRect(SCORE2_X - 4, SCORE_Y - 4, DIGIT_WIDTH * 2 + 8, DIGIT_HEIGHT + 8, 0);
  
  if (score1 < 10) {
    drawDigit(SCORE1_X, SCORE_Y, score1, 7);
  } else {
    drawTwoDigits(SCORE1_X - 8, SCORE_Y, 10, 7);
  }
  
  if (score2 < 10) {
    drawDigit(SCORE2_X, SCORE_Y, score2, 7);
  } else {
    drawTwoDigits(SCORE2_X - 8, SCORE_Y, 10, 7);
  }
  
  // Redraw center line segments that might have been erased by score clears
  int scoreTop = SCORE_Y - 4;
  int scoreBottom = SCORE_Y + DIGIT_HEIGHT + 8;
  for (int y = max(0, scoreTop); y < min(GAME_AREA_BOTTOM, scoreBottom); y += CENTER_LINE_SPACING) {
    vga.fillRect(128 - 1, y, 2, 4, 7);
  }
}

void drawTwoDigits(int x, int y, int number, int color) {
  drawDigit(x, y, 1, color);
  drawDigit(x + DIGIT_WIDTH, y, 0, color);
}

void drawDigit(int x, int y, int digit, int color) {
  vga.fillRect(x, y, DIGIT_WIDTH, DIGIT_HEIGHT, 0);
  
  switch(digit) {
    case 0:
      drawSegmentA(x, y, color); drawSegmentB(x, y, color);
      drawSegmentC(x, y, color); drawSegmentD(x, y, color);
      drawSegmentE(x, y, color); drawSegmentF(x, y, color);
      break;
    case 1:
      drawSegmentB(x, y, color); drawSegmentC(x, y, color);
      break;
    case 2:
      drawSegmentA(x, y, color); drawSegmentB(x, y, color);
      drawSegmentD(x, y, color); drawSegmentE(x, y, color);
      drawSegmentG(x, y, color);
      break;
    case 3:
      drawSegmentA(x, y, color); drawSegmentB(x, y, color);
      drawSegmentC(x, y, color); drawSegmentD(x, y, color);
      drawSegmentG(x, y, color);
      break;
    case 4:
      drawSegmentB(x, y, color); drawSegmentC(x, y, color);
      drawSegmentF(x, y, color); drawSegmentG(x, y, color);
      break;
    case 5:
      drawSegmentA(x, y, color); drawSegmentC(x, y, color);
      drawSegmentD(x, y, color); drawSegmentF(x, y, color);
      drawSegmentG(x, y, color);
      break;
    case 6:
      drawSegmentA(x, y, color); drawSegmentC(x, y, color);
      drawSegmentD(x, y, color); drawSegmentE(x, y, color);
      drawSegmentF(x, y, color); drawSegmentG(x, y, color);
      break;
    case 7:
      drawSegmentA(x, y, color); drawSegmentB(x, y, color);
      drawSegmentC(x, y, color);
      break;
    case 8:
      drawSegmentA(x, y, color); drawSegmentB(x, y, color);
      drawSegmentC(x, y, color); drawSegmentD(x, y, color);
      drawSegmentE(x, y, color); drawSegmentF(x, y, color);
      drawSegmentG(x, y, color);
      break;
    case 9:
      drawSegmentA(x, y, color); drawSegmentB(x, y, color);
      drawSegmentC(x, y, color); drawSegmentD(x, y, color);
      drawSegmentF(x, y, color); drawSegmentG(x, y, color);
      break;
  }
}

// Funciones para los segmentos - ACTUALIZADAS para nuevo tamaño
void drawSegmentA(int x, int y, int color) {
  vga.fillRect(x + 4, y, 9, SEGMENT_THICKNESS, color);
}

void drawSegmentB(int x, int y, int color) {
  vga.fillRect(x + 13, y + 2, SEGMENT_THICKNESS, 8, color);
}

void drawSegmentC(int x, int y, int color) {
  vga.fillRect(x + 13, y + 12, SEGMENT_THICKNESS, 8, color);
}

void drawSegmentD(int x, int y, int color) {
  vga.fillRect(x + 4, y + 20, 9, SEGMENT_THICKNESS, color);
}

void drawSegmentE(int x, int y, int color) {
  vga.fillRect(x + 1, y + 12, SEGMENT_THICKNESS, 8, color);
}

void drawSegmentF(int x, int y, int color) {
  vga.fillRect(x + 1, y + 2, SEGMENT_THICKNESS, 8, color);
}

void drawSegmentG(int x, int y, int color) {
  vga.fillRect(x + 4, y + 10, 9, SEGMENT_THICKNESS, color);
}

void drawWinMessage() {
  vga.fillRect(50, 100, 156, 40, 0);
  
  if (score1 >= MAX_SCORE) {
    drawW(50, 100, 7);
    drawI(80, 100, 7);
    drawN(100, 100, 7);
    drawDigit(130, 100, 1, 7);
  } else {
    drawW(140, 100, 7);
    drawI(170, 100, 7);
    drawN(190, 100, 7);
    drawDigit(220, 100, 2, 7);
  }
}

void drawW(int x, int y, int color) {
  vga.fillRect(x, y, 3, 20, color);
  vga.fillRect(x + 8, y, 3, 20, color);
  vga.fillRect(x + 4, y + 15, 3, 5, color);
}

void drawI(int x, int y, int color) {
  vga.fillRect(x + 2, y, 3, 20, color);
}

void drawN(int x, int y, int color) {
  vga.fillRect(x, y, 3, 20, color);
  vga.fillRect(x + 8, y, 3, 20, color);
  for (int i = 0; i < 20; i += 3) {
    vga.fillRect(x + 3 + i/4, y + i, 2, 3, color);
  }
}