#ifndef MAGLAVA_I18N_H
#define MAGLAVA_I18N_H
#ifdef __cplusplus
extern "C" {
#endif
const char *ml_text(const char *english);
void ml_set_language(const char *code);
int ml_language_index(void);
int ml_language_count(void);
const char *ml_language_code(int index);
const char *ml_language_name(int index);
const char *ml_locale_characters(void);
#ifdef __cplusplus
}
#endif
#endif
