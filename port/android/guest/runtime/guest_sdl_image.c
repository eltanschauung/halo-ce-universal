/* Guest-owned file streams and RGBA screenshot surfaces. Host SDL objects
 * and their 64-bit pointers must never be exposed as guest SDL_Surface data. */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <SDL3/SDL.h>
#include "guest_host.h"
_Static_assert(sizeof(SDL_DateTime) == 36, "SDL_DateTime guest ABI");

bool SDL_SetError(const char *format, ...)
{
 char message[256]; va_list args;
 va_start(args, format); vsnprintf(message, sizeof(message), format, args); va_end(args);
 host_sdl_set_error(message);
 return false;
}
int SDL_snprintf(char *text, size_t size, const char *format, ...)
{
 int result; va_list args;
 va_start(args, format); result = vsnprintf(text, size, format, args); va_end(args);
 return result;
}
const char *SDL_GetBasePath(void)
{
 static char path[4096];
 return host_sdl_base_path(path, sizeof(path)) ? path : NULL;
}
/* Only SDL_IOFromFile creates streams in this guest runtime. */
SDL_IOStream *SDL_IOFromFile(const char *path, const char *mode)
{
 return (SDL_IOStream *)fopen(path, mode);
}
Sint64 SDL_GetIOSize(SDL_IOStream *stream)
{
 FILE *file = (FILE *)stream;
 long position = ftell(file), length;
 if (position < 0 || fseek(file, 0, SEEK_END)) return -1;
 length = ftell(file);
 if (fseek(file, position, SEEK_SET)) return -1;
 return length;
}
bool SDL_CloseIO(SDL_IOStream *stream)
{
 return fclose((FILE *)stream) == 0;
}
void *SDL_LoadFile_IO(SDL_IOStream *stream, size_t *size, bool closeio)
{
 Sint64 length = SDL_GetIOSize(stream);
 void *data = NULL;
 if (size) *size = 0;
 if (length >= 0 && length < SIZE_MAX)
 {
  data = malloc((size_t)length + 1);
  if (data)
  {
   size_t read = fread(data, 1, (size_t)length, (FILE *)stream);
   if (ferror((FILE *)stream)) { free(data); data = NULL; }
   else { ((char *)data)[read] = 0; if (size) *size = read; }
  }
 }
 if (closeio) SDL_CloseIO(stream);
 return data;
}
bool SDL_CreateDirectory(const char *path) { return host_sdl_create_directory(path) != 0; }
bool SDL_GetCurrentTime(SDL_Time *ticks) { return host_sdl_current_time(ticks) != 0; }
bool SDL_TimeToDateTime(SDL_Time ticks, SDL_DateTime *date, bool local)
{
 return host_sdl_date_time(ticks, date, local) != 0;
}
bool SDL_GetPathInfo(const char *path, SDL_PathInfo *info)
{
 int type; long long values[4];
 if (!host_sdl_path_info(path, info ? &type : NULL, info ? values : NULL)) return false;
 if (info) { info->type = (SDL_PathType)type; info->size = values[0];
             info->create_time = values[1]; info->modify_time = values[2]; info->access_time = values[3]; }
 return true;
}
/* The screenshot caller needs only RGBA32 pixels, dimensions and pitch. */
SDL_Surface *SDL_CreateSurface(int width, int height, SDL_PixelFormat format)
{
 SDL_Surface *surface;
 if (format != SDL_PIXELFORMAT_RGBA32 || width <= 0 || height <= 0 ||
     width > INT_MAX / 4 || (size_t)height > SIZE_MAX / ((size_t)width * 4))
 { SDL_SetError("Invalid guest screenshot dimensions or format"); return NULL; }
 surface = calloc(1, sizeof(*surface));
 if (!surface) return NULL;
 surface->pixels = malloc((size_t)width * 4 * height);
 if (!surface->pixels) { free(surface); return NULL; }
 surface->w = width; surface->h = height; surface->pitch = width * 4;
 surface->format = format; surface->refcount = 1;
 return surface;
}
void SDL_DestroySurface(SDL_Surface *surface)
{
 if (surface) { free(surface->pixels); free(surface); }
}
bool SDL_SavePNG(SDL_Surface *surface, const char *path)
{
 if (!surface || surface->format != SDL_PIXELFORMAT_RGBA32) return false;
 return host_sdl_save_rgba_png(surface->w, surface->h, surface->pitch, surface->pixels, path) != 0;
}
