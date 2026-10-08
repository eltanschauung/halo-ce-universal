#include "xgpu.h"
#include "../include/halo_spray.h"
#include <SDL3/SDL.h>
#include <stdlib.h>

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_MAX_DIMENSIONS 2048
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include "../../third_party/stb/stb_image.h"

static GLuint spray_texture, spray_program, spray_array, spray_buffer;
void console_printf(BOOL warning, const char *format, ...);

int spray_image_load(float *aspect)
{
	char path[4096];
	const char *base = SDL_GetBasePath();
	SDL_IOStream *file;
	Sint64 length;
	size_t size = 0;
	void *data;
	unsigned char *pixels;
	int width, height, channels;
	GLuint texture;
	GLint active, bound, unpack_buffer, alignment, row_length, skip_rows, skip_pixels;
	if (!base || SDL_snprintf(path, sizeof(path), "%sspray.png", base) >= (int)sizeof(path))
		return 0;
	file = SDL_IOFromFile(path, "rb");
	if (!file)
	{
		console_printf(FALSE, "Spray Image: put spray.png beside halo.exe");
		return 0;
	}
	length = SDL_GetIOSize(file);
	if (length <= 0 || length > 8 * 1024 * 1024)
	{
		SDL_CloseIO(file);
		console_printf(FALSE, "Spray Image: PNG must be at most 8 MiB");
		return 0;
	}
	data = SDL_LoadFile_IO(file, &size, TRUE);
	/* A changing file must not bypass the encoded-size limit. */
	if (size > 8 * 1024 * 1024)
	{
		SDL_free(data);
		return 0;
	}
	pixels = data ? stbi_load_from_memory(data, (int)size, &width, &height, &channels, 4) : NULL;
	SDL_free(data);
	if (!pixels)
	{
		console_printf(FALSE, "Spray Image: invalid PNG (maximum 2048 x 2048)");
		return 0;
	}
	/* Uploads must not inherit a previous draw's pixel-unpack buffer or strides. */
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
	glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack_buffer);
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
	glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
	glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skip_rows);
	glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skip_pixels);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, bound);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack_buffer);
	glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, skip_rows);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, skip_pixels);
	glActiveTexture(active);
	stbi_image_free(pixels);
	if (spray_texture)
		glDeleteTextures(1, &spray_texture);
	spray_texture = texture;
	*aspect = (float)height / width;
	xgpu_gl_state_invalidate();
	return 1;
}

void halo_spray_image_forget(void)
{
	if (spray_texture)
	{
		glDeleteTextures(1, &spray_texture);
		spray_texture = 0;
		xgpu_gl_state_invalidate();
	}
}

static int spray_prepare(void)
{
	GLuint vertex, fragment;
	if (spray_program)
		return 1;
#ifdef HALO_ANDROID
	/* GLES has no native upper-left / zero-to-one clip control. */
	vertex = xgpu_compile_shader(GL_VERTEX_SHADER,
		"#version 300 es\nprecision highp float;\n"
		"layout(location=0) in vec4 p; layout(location=1) in vec2 t; out vec2 uv;\n"
		"void main(){gl_Position=vec4(p.x,-p.y,2.0*p.z-p.w,p.w); uv=t;}\n", "spray");
	fragment = xgpu_compile_shader(GL_FRAGMENT_SHADER,
		"#version 300 es\nprecision highp float;\n"
#else
	vertex = xgpu_compile_shader(GL_VERTEX_SHADER,
		"#version 450 core\n"
		"layout(location=0) in vec4 p; layout(location=1) in vec2 t; out vec2 uv;\n"
		"void main(){gl_Position=p; uv=t;}\n", "spray");
	fragment = xgpu_compile_shader(GL_FRAGMENT_SHADER,
		"#version 450 core\n"
#endif
		"uniform sampler2D image; in vec2 uv; out vec4 color;\n"
		"void main(){color=texture(image,uv); if(color.a==0.0) discard;}\n", "spray");
	spray_program = vertex && fragment ? xgpu_link_program(vertex, fragment, "spray") : 0;
	if (vertex) glDeleteShader(vertex);
	if (fragment) glDeleteShader(fragment);
	if (!spray_program)
		return 0;
	glGenVertexArrays(1, &spray_array);
	glGenBuffers(1, &spray_buffer);
	return 1;
}

void halo_spray_image_draw(struct halo_spray_clip_vertex const *vertices, int count)
{
	GLint program, array, buffer, active, texture, sampler, function;
	GLint src_rgb, dst_rgb, src_alpha, dst_alpha, equation_rgb, equation_alpha;
	GLboolean depth, blend, stencil, cull, mask[4], depth_mask;
	if (!spray_texture || count <= 0 || count > HALO_SPRAY_MAXIMUM_VERTICES || !spray_prepare())
		return;
	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &array);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
	glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
	glGetIntegerv(GL_DEPTH_FUNC, &function);
	glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb); glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation_rgb); glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equation_alpha);
	depth = glIsEnabled(GL_DEPTH_TEST); blend = glIsEnabled(GL_BLEND);
	stencil = glIsEnabled(GL_STENCIL_TEST); cull = glIsEnabled(GL_CULL_FACE);
	glGetBooleanv(GL_COLOR_WRITEMASK, mask);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
	glUseProgram(spray_program);
	glUniform1i(glGetUniformLocation(spray_program, "image"), 0);
	glBindVertexArray(spray_array);
	glBindBuffer(GL_ARRAY_BUFFER, spray_buffer);
	glBufferData(GL_ARRAY_BUFFER, count * sizeof(*vertices), vertices, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(*vertices), (void *)0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(*vertices), (void *)(4 * sizeof(float)));
	glBindTexture(GL_TEXTURE_2D, spray_texture);
	glBindSampler(0, 0);
	glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
	glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE);
	/* Halo uses destination alpha for subsequent lighting and fog passes. */
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	glDrawArrays(GL_TRIANGLES, 0, count);
	glColorMask(mask[0], mask[1], mask[2], mask[3]); glDepthMask(depth_mask);
	glDepthFunc(function);
	glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
	glBlendEquationSeparate(equation_rgb, equation_alpha);
	if (!depth) glDisable(GL_DEPTH_TEST);
	if (!blend) glDisable(GL_BLEND);
	if (stencil) glEnable(GL_STENCIL_TEST);
	if (cull) glEnable(GL_CULL_FACE);
	glBindTexture(GL_TEXTURE_2D, texture); glBindSampler(0, sampler); glActiveTexture(active);
	glBindVertexArray(array); glBindBuffer(GL_ARRAY_BUFFER, buffer); glUseProgram(program);
	xgpu_gl_state_invalidate();
}
