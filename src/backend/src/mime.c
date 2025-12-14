#include "mime.h"
#include <string.h>
#include <strings.h>

typedef struct {
  const char *extension;
  const char *mime_type;
} mime_entry_t;

static const mime_entry_t mime_types[] = {
    // Web essentials
    {".html", "text/html; charset=utf-8"},
    {".htm", "text/html; charset=utf-8"},
    {".css", "text/css; charset=utf-8"},
    {".js", "application/javascript; charset=utf-8"},
    {".mjs", "application/javascript; charset=utf-8"},
    {".json", "application/json; charset=utf-8"},
    {".xml", "application/xml; charset=utf-8"},
    {".txt", "text/plain; charset=utf-8"},

    // Images
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".webp", "image/webp"},

    // Fonts
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".ttf", "font/ttf"},
    {".otf", "font/otf"},
    {".eot", "application/vnd.ms-fontobject"},

    // Other
    {".pdf", "application/pdf"},
    {".zip", "application/zip"},
    {".map", "application/json"},

    {NULL, NULL}};

const char *mime_type_for_extension(const char *path) {
  const char *ext = strrchr(path, '.');
  if (!ext) {
    return "application/octet-stream";
  }

  for (const mime_entry_t *entry = mime_types; entry->extension; entry++) {
    if (strcasecmp(ext, entry->extension) == 0) {
      return entry->mime_type;
    }
  }

  return "application/octet-stream";
}
