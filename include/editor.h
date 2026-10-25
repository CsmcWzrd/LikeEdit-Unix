#ifndef LIKEEDIT_EDITOR_H
#define LIKEEDIT_EDITOR_H
#include <stddef.h>
#include <stdint.h>

#define LIKEEDIT_MAX_FILE_SIZE (128u * 1024u * 1024u)
#define LIKEEDIT_MAX_BUFFERS 1024u

typedef struct {
    char *text;
    size_t bytes;
    size_t capacity;
} LE_Line;

typedef struct {
    LE_Line *lines;
    size_t count;
    size_t capacity;
    char *path;
    int dirty;
    int bom;
    int crlf;
    size_t cx, cy;       /* Unicode scalar-column, logical line */
    size_t sx, sy;       /* display column / logical line scroll */
} LE_Buffer;

typedef struct {
    LE_Buffer **buffers;
    size_t count;
    size_t capacity;
    size_t active;
} LE_Editor;

int le_utf8_decode(const char *s, size_t n, uint32_t *cp, size_t *used);
int le_utf8_validate(const char *s, size_t n);
size_t le_line_columns(const LE_Line *line);
size_t le_line_byte_at(const LE_Line *line, size_t column);
void le_editor_init(LE_Editor *ed);
void le_editor_free(LE_Editor *ed);
LE_Buffer *le_current(LE_Editor *ed);
int le_editor_new(LE_Editor *ed);
int le_editor_open(LE_Editor *ed, const char *path, char *error, size_t error_size);
int le_editor_switch(LE_Editor *ed, size_t index);
int le_editor_close(LE_Editor *ed, int force);
int le_editor_dirty(const LE_Editor *ed);
int le_buffer_save(LE_Buffer *buf, const char *path, char *error, size_t error_size);
int le_insert(LE_Buffer *buf, const char *utf8, size_t length);
int le_split(LE_Buffer *buf);
int le_backspace(LE_Buffer *buf);
int le_delete(LE_Buffer *buf);
int le_delete_line(LE_Buffer *buf);
int le_open_line(LE_Buffer *buf, int above);
void le_move(LE_Buffer *buf, long x, long y);
const char *le_language(const char *path);
int le_lex_class(const char *lang, const char *s, size_t len, size_t offset,
                 int *block_comment, int *string_state, size_t *token_bytes);
#endif
