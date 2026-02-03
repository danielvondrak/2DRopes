#include "DV.h"
#include "DV_Random.h"

#define append_rope(ropes, rope)                                                                                                                                                                       \
  do                                                                                                                                                                                                   \
  {                                                                                                                                                                                                    \
    if (ropes.count >= ropes.capacity)                                                                                                                                                                 \
    {                                                                                                                                                                                                  \
      if (ropes.capacity == 0)                                                                                                                                                                         \
        ropes.capacity = 1024;                                                                                                                                                                         \
      else                                                                                                                                                                                             \
        ropes.capacity *= 2;                                                                                                                                                                           \
      ropes.items = (Rope *)realloc(ropes.items, ropes.capacity * sizeof(*ropes.items));                                                                                                               \
    }                                                                                                                                                                                                  \
    ropes.items[ropes.count++] = rope;                                                                                                                                                                 \
  } while (0)

internal void DrawPoint(game_offscreen_buffer *Buffer, int32 x, int32 y, uint32 Color)
{
  if (x >= 0 && x < Buffer->Width && y > 0 && y < Buffer->Height)
  {
    uint8 *Row = ((uint8 *)Buffer->Memory + x * Buffer->BytesPerPixel + y * Buffer->Pitch);
    uint32 *Pixel = (uint32 *)Row;
    *Pixel = Color;
  }
}

internal void DrawLineEx(game_offscreen_buffer *Buffer, v2 startPos, v2 endPos, float thick, Color color)
{
  int32 x1 = RoundReal32ToInt32(startPos.X);
  int32 y1 = RoundReal32ToInt32(startPos.Y);
  int32 x2 = RoundReal32ToInt32(endPos.X);
  int32 y2 = RoundReal32ToInt32(endPos.Y);

  uint32 C = ((RoundReal32ToUInt32(color.r * 255.0f) << 16) | (RoundReal32ToUInt32(color.g * 255.0f) << 8) | (RoundReal32ToUInt32(color.b * 255.0f) << 0));

  int32 dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
  int32 dy = abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
  int32 err = (dx > dy ? dx : -dy) / 2, e2;

  for (;;)
  {
    DrawPoint(Buffer, x1, y1, C);
    if (x1 == x2 && y1 == y2)
      break;
    e2 = err;
    if (e2 > -dx)
    {
      err -= dy;
      x1 += sx;
    }
    if (e2 < dy)
    {
      err += dx;
      y1 += sy;
    }
  }
}

void DisplayRope(game_offscreen_buffer *Buffer, game_state *GameState)
{
  for (int i = 0; i < GameState->ROPES.count; i++)
  {
    for (int j = 0; j < MAX_PARTICLE_COUNT + 1; j++)
    {
      if (j != MAX_PARTICLE_COUNT + 1 - 1)
      {
        DrawLineEx(Buffer, GameState->ROPES.items[i].PARTICLES[j].position, GameState->ROPES.items[i].PARTICLES[j + 1].position, 5.0, GameState->ROPES.items[i].PARTICLES[j].color);
      }
      else
      {
        DrawLineEx(Buffer, GameState->ROPES.items[i].PARTICLES[j].position, GameState->ROPES.items[i].PARTICLES[j].position, 5, GameState->ROPES.items[i].PARTICLES[j].color);
      }
    }
  }
}

void UpdateRope(game_state *GameState, float deltaTime, int32 mX, int32 mY)
{
  v2 m = {(real32)mX, (real32)mY};
  for (int i = 0; i < GameState->ROPES.count; i++)
  {
    for (int j = 0; j < MAX_PARTICLE_COUNT + 1; j++)
    {
      Particle *particle = &GameState->ROPES.items[i].PARTICLES[j];
      if (!particle->pinned)
      {
        if (j == GameState->PINNABLE)
        {
          particle->position = m;
          particle->previous_position = particle->position;
          continue;
        }

        particle->acceleration.Y += GRAVITY;

        v2 velocity = {particle->position.X - particle->previous_position.X, particle->position.Y - particle->previous_position.Y};

        velocity.X *= DAMPING;
        velocity.Y *= DAMPING;

        particle->previous_position = particle->position;
        particle->position = ((particle->position + velocity) + (particle->acceleration * (deltaTime * deltaTime)));
        v2 z = {0, 0};
        particle->acceleration = z;
      }
    }
  }
}

void UpdateConstraintRope(game_state *GameState)
{
  for (int i = 0; i < GameState->ROPES.count; i++)
  {
    for (int j = 0; j < MAX_PARTICLE_COUNT + 1 - 1; j++)
    {
      Particle *P = &GameState->ROPES.items[i].PARTICLES[j];
      Particle *nextP = &GameState->ROPES.items[i].PARTICLES[j + 1];

      v2 d = {nextP->position.X - P->position.X, nextP->position.Y - P->position.Y};

      real32 l = sqrtf(powf(d.X, 2) + powf(d.Y, 2));
      if (l < 0.0001f)
        continue;

      v2 n = {d.X / l, d.Y / l};

      float difference = l - MAX_PARTICLE_DISTANCE;

      v2 c = {n.X * difference * 0.5f, n.Y * difference * 0.5f};

      if (!P->pinned)
      {
        P->position.X += c.X;
        P->position.Y += c.Y;
      }

      if (!nextP->pinned)
      {
        nextP->position.X -= c.X;
        nextP->position.Y -= c.Y;
      }
    }
  }
}

internal void DrawRectangle(game_offscreen_buffer *Buffer, v2 vMin, v2 vMax, real32 R, real32 G, real32 B)
{
  int32 MinX = RoundReal32ToInt32(vMin.X);
  int32 MinY = RoundReal32ToInt32(vMin.Y);
  int32 MaxX = RoundReal32ToInt32(vMax.X);
  int32 MaxY = RoundReal32ToInt32(vMax.Y);

  if (MinX < 0)
  {
    MinX = 0;
  }

  if (MinY < 0)
  {
    MinY = 0;
  }

  if (MaxX > Buffer->Width)
  {
    MaxX = Buffer->Width;
  }

  if (MaxY > Buffer->Height)
  {
    MaxY = Buffer->Height;
  }

  uint32 Color = ((RoundReal32ToUInt32(R * 255.0f) << 16) | (RoundReal32ToUInt32(G * 255.0f) << 8) | (RoundReal32ToUInt32(B * 255.0f) << 0));

  uint8 *Row = ((uint8 *)Buffer->Memory + MinX * Buffer->BytesPerPixel + MinY * Buffer->Pitch);
  for (int Y = MinY; Y < MaxY; ++Y)
  {
    uint32 *Pixel = (uint32 *)Row;
    for (int X = MinX; X < MaxX; ++X)
    {
      *Pixel++ = Color;
    }

    Row += Buffer->Pitch;
  }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
  Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
  Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

  // Initialization
  game_state *GameState = (game_state *)Memory->PermanentStorage;
  if (!Memory->IsInitialized)
  {
    GameState->ROPES = {NULL, 0, 0};
    GameState->ACTIVE_ROPE = -1;
    GameState->PINNABLE = 0;

    Memory->IsInitialized = true;
  }

  // Clear Buffer
  DrawRectangle(Buffer, v2{0, 0}, v2{(real32)Buffer->Width, (real32)Buffer->Height}, 0.25f, 0.25f, 0.25f);

  if (Input->MouseButtons[0].EndedDown && GameState->ACTIVE_ROPE == -1)
  {
    Rope ROPE = {0};

    for (int i = 0; i < MAX_PARTICLE_COUNT + 1; i++)
    {
      v2 position = {(real32)Input->MouseX, (real32)Input->MouseY + MAX_PARTICLE_DISTANCE * i};
      v2 previous_position = position;

      Color color = {(unsigned char)(RandomNumberTable[i] % 256), (unsigned char)(RandomNumberTable[i + 1] % 256), (unsigned char)(RandomNumberTable[i + 2] % 256), (unsigned char)255};
      Particle p = {false, position, previous_position, v2{0,0}, color};

      ROPE.PARTICLES[i] = p;
    }

		//TODO: cap the rope
    append_rope(GameState->ROPES, ROPE);

    GameState->ACTIVE_ROPE = (int)(GameState->ROPES.count - 1);
    GameState->PINNABLE = 0;
  }

  if (Input->MouseButtons[2].EndedDown && Input->MouseButtons[2].HalfTransitionCount > 0)
  {
    if (GameState->ACTIVE_ROPE >= 0)
    {
      if (!GameState->ROPES.items[GameState->ACTIVE_ROPE].PARTICLES[0].pinned)
      {
        GameState->ROPES.items[GameState->ACTIVE_ROPE].PARTICLES[0].pinned = true;
        GameState->PINNABLE = MAX_PARTICLE_COUNT;
      }
      else
      {
        GameState->ROPES.items[GameState->ACTIVE_ROPE].PARTICLES[MAX_PARTICLE_COUNT].pinned = true;
        GameState->ACTIVE_ROPE = -1;
      }
    }
  }

  if (GameState->ROPES.count > 0)
  {
    DisplayRope(Buffer, GameState);
    UpdateRope(GameState, Input->dtForFrame, Input->MouseX, Input->MouseY);
    for (int i = 0; i < MAX_CONSTRAINT_ITERATION; i++)
    {
      UpdateConstraintRope(GameState);
    }
  }
}

internal void GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer)
{
  // unused;
}
extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
  game_state *GameState = (game_state *)Memory->PermanentStorage;
  GameOutputSound(GameState, SoundBuffer);
}
