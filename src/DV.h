#if !defined(DV_H)

#include "DV_Platform.h"
#include "DV_Math.h"
#include "DV_Intrinsics.h"

typedef struct Color {
    unsigned char r;        // Color red value
    unsigned char g;        // Color green value
    unsigned char b;        // Color blue value
    unsigned char a;        // Color alpha value
} Color;

typedef struct Particle {
  bool32 pinned;

  v2 position;
  v2 previous_position;

  v2 acceleration;

  Color color;
} Particle;

#define MAX_PARTICLE_COUNT 20
#define MAX_PARTICLE_DISTANCE 20

#define MAX_CONSTRAINT_ITERATION 5

#define GRAVITY 980.0
#define DAMPING 0.98

typedef struct Rope {
  Particle PARTICLES[MAX_PARTICLE_COUNT + 1];
} Rope;

typedef struct Ropes {
  Rope *items;
  size_t count;
  size_t capacity;
} Ropes;

typedef struct game_state
{
	Ropes ROPES;
	int32 ACTIVE_ROPE;
	int32 PINNABLE;
}game_state;


struct game_clocks {
  real32 SecondsElapsed;
};


#define DV_H
#endif
