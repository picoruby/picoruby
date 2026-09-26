/* regex_engine.h -- the regex engine's seam.
 *
 * The host compiles a pattern with regex_compile_words and runs the
 * program with regex_exec. The compiler's only need is regex_malloc,
 * regex_realloc and regex_free, which the host supplies; the VM needs
 * nothing. This header depends on the C library only. */
#ifndef REGEX_ENGINE_H
#define REGEX_ENGINE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define REGEX_ICASE  1u   /* ASCII-only case folding ("i") */
#define REGEX_DOTALL 2u   /* `.` also matches a newline ("m") */

/* "im" -> bits; -1 on a letter that is not a flag */
int regex_flag_bits(const char *flags, size_t len);

/* Compile a pattern to program words (regex_malloc'd; the caller frees
 * with regex_free). NULL on a bad pattern, with *err naming the reason
 * and *err_at the byte offset in the pattern where it was found. An
 * allocation failure is the error "out of memory" at offset 0. The
 * words are position independent: a copy in flash runs as well. */
uint32_t *regex_compile_words(const uint8_t *src, size_t len, int flags,
                                 size_t *nwords, const char **err,
                                 size_t *err_at);

/* The words of scratch one regex_exec over prog needs. A loop
 * over many matches of the same program allocates it once. */
size_t regex_scratch_words(const uint32_t *prog);

/* Leftmost-first search of s[0..len) from byte offset start. On a
 * match, out[] holds prog's 2 * groups capture positions as byte
 * offsets, [b0, e0, b1, e1, ...], -1 for a group that did not take
 * part; out needs prog[1] slots. first_only stops at the first thread
 * that reaches MATCH, which is enough for a yes/no answer; the
 * positions are then not the leftmost-first ones. scratch is
 * regex_scratch_words(prog) words; its content on entry does not
 * matter. The VM allocates nothing and never recurses. */
bool regex_exec(const uint32_t *prog, const uint8_t *s, size_t len,
                   size_t start, int32_t *out, bool first_only,
                   uint32_t *scratch);

#endif /* REGEX_ENGINE_H */
