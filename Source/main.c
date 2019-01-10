#include "game.h"

#define SOKOL_IMPL
#define SOKOL_GLCORE33
#define SOKOL_DEBUG
#include "3rd/sokol_app.h"
#include "3rd/sokol_gfx.h"
#include "3rd/sokol_time.h"

#define STB_IMAGE_IMPLEMENTATION
#include "3rd/stb_image.h"

#include <stdint.h>
#include <stdbool.h>

#include "3rd/Mist_Profiler.h"
MIST_PROFILE_DEFINE_GLOBALS

#include <time.h>

const uint16_t Window_Width = 1024;
const uint16_t Window_Height = 720;
const char* Window_Title = "Holy Cheese";

static uint64_t Time_LastFrame = 0;

const uint8_t Render_SampleCount = 4;

static Game_InstanceBuffer* Render_InstanceBuffer;
static sg_draw_state Render_DrawState;

extern const char *Render_VS, *Render_FS;

typedef struct
{
    float aspect;
} Render_VSParams;

void core_init(void)
{
	Mist_ProfileInit();

    stm_setup();

    sg_desc desc = {0};
    sg_setup(&desc);
    
    sg_buffer instanceBuffer = sg_make_buffer(&(sg_buffer_desc)
        {
            .size = sizeof(Game_Instance) * GAME_MAX_INSTANCE_COUNT,
            .usage = SG_USAGE_STREAM
        });

    uint16_t indices[] = { 0, 1, 2, 2, 1, 3 };
    sg_buffer indexBuffer = sg_make_buffer(&(sg_buffer_desc)
        {
            .type = SG_BUFFERTYPE_INDEXBUFFER,
            .size = sizeof(uint16_t) * 6,
            .content = indices
        });

    sg_shader shader = sg_make_shader(&(sg_shader_desc)
        {
        .fs = { .images[0] = {.type = SG_IMAGETYPE_2D, .name = "tex0"}, .source = Render_FS },
            .vs = 
                { 
                    .uniform_blocks[0] = 
                        {
                            .size = sizeof(Render_VSParams),
                            .uniforms[0] = {.name = "aspect", .type = SG_UNIFORMTYPE_FLOAT }
                    },
                    .source = Render_VS
                }
        });

    int width, height, comp;
    stbi_uc* imageData = stbi_load("../../../Assets/Sprites.png", &width, &height, &comp, 4);
    sg_image image = sg_make_image(&(sg_image_desc)
        {
            .width = width,
            .height = height,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            .content.subimage[0][0] = { .ptr = imageData, .size = width * height * 4 }
        });

    stbi_image_free(imageData);

    sg_pipeline pipeline = sg_make_pipeline(&(sg_pipeline_desc)
        {
            .layout = 
                {
                    .buffers[0] = {.step_func = SG_VERTEXSTEP_PER_INSTANCE, .stride = sizeof(Game_Instance) },
                    .attrs = 
                        {
                            [0] = { .name = "sprite", .format = SG_VERTEXFORMAT_FLOAT },
                            [1] = { .name = "scale", .format = SG_VERTEXFORMAT_FLOAT },
                            [2] = { .name = "position", .format = SG_VERTEXFORMAT_FLOAT2 }
                        }
                },
            .shader = shader,
            .index_type = SG_INDEXTYPE_UINT16,
            .depth_stencil = 
                {
                    .depth_write_enabled = false
                },

            .rasterizer = {.cull_mode = SG_CULLMODE_NONE, .sample_count = Render_SampleCount },
            .blend = 
                {
                    .enabled = true,
                    .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                    .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
                    .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
                }
        });

    Render_DrawState = (sg_draw_state)
        {
            .pipeline = pipeline,
            .vertex_buffers[0] = instanceBuffer,
            .index_buffer = indexBuffer,
            .fs_images[0] = image
        };

    Render_InstanceBuffer = malloc(sizeof(Game_InstanceBuffer));
    game_init();
}

void core_frame(void)
{
	MIST_BEGIN_PROFILE("main", "core_frame");
    int width = sapp_width();
    int height = sapp_height();

    uint64_t delta = stm_laptime(&Time_LastFrame);
    game_tick((float)stm_sec(delta));

    uint32_t instanceCount = game_gen_instance_buffer(Render_InstanceBuffer);

	{
		MIST_BEGIN_PROFILE("main", "sg_update_buffer");
		sg_update_buffer(Render_DrawState.vertex_buffers[0], Render_InstanceBuffer, sizeof(Game_Instance) * instanceCount);
		MIST_END_PROFILE("main", "sg_update_buffer");
	}

	{
		MIST_BEGIN_PROFILE("main", "sg_pass");
		sg_pass_action passAction =
		{
			.colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.1f, 0.1f, 0.1f, 1.0f } }
		};
		sg_begin_default_pass(&passAction, (int)width, (int)height);
		sg_apply_draw_state(&Render_DrawState);
		sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &(Render_VSParams){.aspect = (float)width / height}, sizeof(Render_VSParams));
		if (instanceCount > 0)
		{
			sg_draw(0, 6, instanceCount);
		}
		sg_end_pass();
		MIST_END_PROFILE("main", "sg_pass");
	}
	{
		MIST_BEGIN_PROFILE("main", "sg_commit");
		sg_commit();
		MIST_END_PROFILE("main", "sg_commit");
	}
	MIST_END_PROFILE("main", "core_frame");
}

void core_trace(void)
{
	char timestamp[32] = { 0 };
	char filename[32] = { 0 };
	time_t t = time(0);
	struct tm* tm = localtime(&t);
	strftime(timestamp, 32, "%Y%m%d-%H%M%S", tm);
	sprintf(filename, "%s.trace.json", timestamp);

	if(Mist_ProfileListSize() == 0)
	{
		// Adds the current buffer to the list of buffers even if it hasn't been filled up yet.
		Mist_FlushThreadBuffer();
	}

	FILE* tracefile = fopen(filename, "w");
	if(tracefile)
	{
		char* samples = Mist_Flush();
		fprintf(tracefile, "%s", mist_ProfilePreface);
		fprintf(tracefile, "%s", samples);
		fprintf(tracefile, "%s", mist_ProfilePostface);
		fclose(tracefile);
	}
	Mist_ProfileTerminate();
}

void core_cleanup(void)
{
    game_kill();
    free(Render_InstanceBuffer);

    sg_shutdown();
	core_trace();
}

sapp_desc sokol_main(int argc, char* argv[])
{
    return (sapp_desc) 
        {
            .init_cb = core_init,
            .frame_cb = core_frame,
            .cleanup_cb = core_cleanup,
            .width = Window_Width,
            .height = Window_Height,
            .window_title = Window_Title,
        };
}

const char* Render_VS = 
    "#version 330\n"
    "uniform float aspect;\n"
    "in float sprite;\n"
    "in float scale;\n"
    "in vec2 position;\n"
    "out vec2 uv;\n"
    "void main()\n"
    "{\n"
    "  const float kImageCount = 11.0;\n"
    "  vec2 vertexPos = vec2(gl_VertexID / 2, gl_VertexID & 1);\n"
    "  gl_Position = vec4((position + vertexPos * scale / vec2(aspect, 1.0)), 0.0, 1.0);\n"
    "  uv = vec2(vertexPos.x / kImageCount + sprite / kImageCount, 1.0 - vertexPos.y);\n"
    "}\n";

const char* Render_FS = 
    "#version 330\n"
    "in vec2 uv;\n"
    "uniform sampler2D tex0;\n"
    "out vec4 frag_color;\n"
    "void main()\n"
    "{\n"
    "  frag_color = texture(tex0, uv);\n"
    "}\n";
