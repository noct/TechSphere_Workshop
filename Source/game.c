#include "game.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// Random
uint32_t rand_range(uint32_t min, uint32_t max)
{
    return ((float)rand() / RAND_MAX) * (max - min) + min;
}

float rand_rangef(float min, float max)
{
    return ((float)rand() / RAND_MAX) * (max - min) + min;
}

// Math
typedef struct
{
    float x, y;
} Vec2;

Vec2 vec2_add(Vec2 l, Vec2 r)
{
    return (Vec2){ .x = l.x + r.x, .y = l.y + r.y };
}

Vec2 vec2_sub(Vec2 l, Vec2 r)
{
    return (Vec2) { .x = l.x - r.x, .y = l.y - r.y };
}

Vec2 vec2_mul(Vec2 v, float s)
{
    return (Vec2){ .x = v.x * s, .y = v.y * s };
}

float vec2_mag(Vec2 v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}

Vec2 vec2_norm(Vec2 v)
{
    float mag = vec2_mag(v);
    if (mag == 0.0f) return (Vec2) {.x = 0.0f, .y = 0.0f};

    return vec2_mul(v, 1.0f / mag);
}

float math_maxf(float l, float r)
{
    return l > r ? l : r;
}

float math_minf(float l, float r)
{
    return l < r ? l : r;
}

uint32_t math_max(uint32_t l, uint32_t r)
{
    return l > r ? l : r;
}

// Crops
typedef enum
{
    FieldStage_Arable = 0,
    FieldStage_Fallow,
    FieldStage_Planted,
    FieldStage_Grown,
    FieldState_Max
} Field_Stage;

typedef struct
{
    float grown;
    float lifetime;
} Field_Crop;

typedef struct
{
    Field_Crop* crop;
    Field_Stage stage;
    Vec2 pos;
} Field_Tile;

const uint32_t Field_Width  = 500;
const uint32_t Field_Height = 500;
static Field_Tile* Field_Tiles = NULL;

const float Crop_MinLifetime = 1.0f;
const float Crop_MaxLifetime = 10.0f;

void field_tick(float delta)
{
    for (uint32_t i = 0; i < Field_Width * Field_Height; ++i)
    {
        if (Field_Tiles[i].stage == FieldStage_Planted)
        {
            Field_Crop* crop = Field_Tiles[i].crop;
            crop->lifetime += delta;

            if (crop->lifetime >= crop->grown)
            {
                Field_Tiles[i].stage = FieldStage_Grown;
            }
        }
    }
}


// AI
typedef enum
{
    FarmerState_Search,
    FarmerState_Move,
    FarmerState_Farm
} AI_FarmerState;

typedef struct
{
    Vec2 pos;
    AI_FarmerState state;

    union
    {
        struct
        {
            Vec2 vel;
            Field_Tile* tile;
        } moveState;

        struct
        {
            float farmTimer;
            Field_Tile* tile;
        } farmState;

        struct
        {
            float searchTimer;
        } searchState;
    };
} AI_Farmer;

const float AI_FarmerSpeed = 0.5f;
const float AI_FarmerCropRadius = 0.02f;
const float AI_FarmerSearchSpeedMin = 0.0f;
const float AI_FarmerSearchSpeedMax = 1.0f;
const float AI_FarmerFarmSpeedMin = 3.0f;
const float AI_FarmerFarmSpeedMax = 5.0f;
const uint32_t AI_FarmerCount = 100000;
static AI_Farmer* AI_Farmers = NULL;

void ai_tick(float delta)
{
    for (uint32_t ai = 0; ai < AI_FarmerCount; ++ai)
    {
        AI_Farmer* farmer = &AI_Farmers[ai];
        
        switch (farmer->state)
        {
        case FarmerState_Search:
            {
                farmer->searchState.searchTimer -= delta;
                farmer->searchState.searchTimer = math_maxf(farmer->searchState.searchTimer, 0.0f);

                if (farmer->searchState.searchTimer <= 0.0f)
                {
                    uint32_t tileIndex = rand_range(0U, Field_Width * Field_Height);
                    Field_Tile* tile = &Field_Tiles[tileIndex];

                    if (tile->stage != FieldStage_Planted)
                    {
                        farmer->state = FarmerState_Move;
                        farmer->moveState.tile = tile;

                        farmer->moveState.vel = vec2_mul(vec2_norm(vec2_sub(tile->pos, farmer->pos)), AI_FarmerSpeed);
                    }
                }

                break;
            }
        case FarmerState_Move:
            {
                Vec2 vel = farmer->moveState.vel;
                farmer->pos = vec2_add(farmer->pos, vec2_mul(vel, delta));

                Field_Tile* tile = farmer->moveState.tile;
                float dist = vec2_mag(vec2_sub(tile->pos, farmer->pos));
                if (dist < AI_FarmerCropRadius)
                {
                    farmer->state = FarmerState_Farm;
                    farmer->farmState.farmTimer = rand_rangef(AI_FarmerFarmSpeedMin, AI_FarmerFarmSpeedMax);
                    farmer->farmState.tile = tile;
                }

                break;
            }
        case FarmerState_Farm:
            {
                farmer->farmState.farmTimer -= delta;
                if (farmer->farmState.farmTimer <= 0.0f)
                {
                    Field_Tile* tile = farmer->farmState.tile;

                    if (tile->stage == FieldStage_Grown)
                    {
                        free(tile->crop);
                        tile->crop = NULL;
                    }

                    tile->stage = math_max(tile->stage + 1, FieldStage_Fallow);

                    if (tile->stage == FieldStage_Planted)
                    {
                        tile->crop = (Field_Crop*)malloc(sizeof(Field_Crop));
                        tile->crop->grown = rand_rangef(Crop_MinLifetime, Crop_MaxLifetime);
                    }

                    farmer->state = FarmerState_Search;
                    farmer->searchState.searchTimer = rand_rangef(AI_FarmerSearchSpeedMin, AI_FarmerSearchSpeedMax);
                }
                break;
            }
        }
    }
}

// Game

float Game_FarmerImageTable[] =
    {
        [FarmerState_Search] = 0.0f,
        [FarmerState_Move] = 1.0f,
        [FarmerState_Farm] = 2.0f
    };

float Game_FieldImageTable[] =
    {
        [FieldStage_Arable] = 3.0f,
        [FieldStage_Fallow] = 4.0f,
        [FieldStage_Planted] = 5.0f,
        [FieldStage_Grown] = 6.0f
    };

void game_init(void)
{
    srand((unsigned int)time(NULL));

    Field_Tiles = (Field_Tile*)malloc(sizeof(Field_Tile) * Field_Width * Field_Height);
    memset(Field_Tiles, 0, sizeof(Field_Tile) * Field_Width * Field_Height);

    for (uint32_t y = 0; y < Field_Height; ++y)
    {
        for (uint32_t x = 0; x < Field_Width; ++x)
        {
            Field_Tiles[y * Field_Width + x].pos = (Vec2) { .x = (float)x / Field_Width, .y = (float)y / Field_Height };
            Field_Tiles[y * Field_Width + x].pos = vec2_sub(vec2_mul(Field_Tiles[y * Field_Width + x].pos, 2.0f), (Vec2) { .x = 1.0f, .y = 1.0f });
        }
    }

    AI_Farmers = (AI_Farmer*)malloc(sizeof(AI_Farmer) * AI_FarmerCount);
    memset(AI_Farmers, 0, sizeof(AI_Farmer) * AI_FarmerCount);

    for (uint32_t ai = 0; ai < AI_FarmerCount; ++ai)
    {
        AI_Farmer* farmer = &AI_Farmers[ai];
        farmer->searchState.searchTimer = rand_rangef(AI_FarmerSearchSpeedMin, AI_FarmerSearchSpeedMax);
    }
}

void game_tick(float delta)
{
    delta = math_minf(delta, 0.02f);

    field_tick(delta);
    ai_tick(delta);
}

void game_kill(void)
{
    for (uint32_t i = 0; i < Field_Width * Field_Height; ++i)
    {
        free(Field_Tiles[i].crop);
    }

    free(Field_Tiles);
    free(AI_Farmers);
}

uint32_t game_gen_instance_buffer(Game_InstanceBuffer* buffer)
{
    uint32_t writeIndex = 0;
    for (uint32_t i = 0; i < Field_Width * Field_Height; ++i)
    {
        uint32_t writeLoc = writeIndex++;
        buffer->instances[writeLoc].spriteIndex = Game_FieldImageTable[Field_Tiles[i].stage];
        buffer->instances[writeLoc].scale = 1.0f;
        buffer->instances[writeLoc].pos[0] = Field_Tiles[i].pos.x;
        buffer->instances[writeLoc].pos[1] = Field_Tiles[i].pos.y;

        if (Field_Tiles[i].crop != NULL)
        {
            uint32_t cropWriteIndex = writeIndex++;
            buffer->instances[cropWriteIndex].spriteIndex = 0; // TODO: Get a crop image index
            buffer->instances[cropWriteIndex].scale = 1.1f;

            buffer->instances[cropWriteIndex].pos[0] = Field_Tiles[i].pos.x;
            buffer->instances[cropWriteIndex].pos[1] = Field_Tiles[i].pos.y;
        }
    }

    for (uint32_t i = 0; i < AI_FarmerCount; ++i)
    {
        uint32_t writeLoc = writeIndex++;
        buffer->instances[writeLoc].spriteIndex = Game_FarmerImageTable[AI_Farmers[i].state];
        buffer->instances[writeLoc].scale = 1.5f;
        buffer->instances[writeLoc].pos[0] = AI_Farmers[i].pos.x;
        buffer->instances[writeLoc].pos[1] = AI_Farmers[i].pos.y;
    }

    return writeIndex;
}
