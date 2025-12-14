#ifndef MIME_H
#define MIME_H

// Get MIME type for file extension
// Returns "application/octet-stream" for unknown types
const char *mime_type_for_extension(const char *path);

#endif // MIME_H
