#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define MP_WEB_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define MP_WEB_EXPORT
#endif

#include "mppriv.h"

static kstring_t g_result = {0,0,0};
static kstring_t g_error = {0,0,0};

static int web_append(kstring_t *dst, const char *src, size_t len)
{
	char *p;
	size_t needed, new_m;
	if (dst == 0 || src == 0 || len == 0) return 0;
	needed = dst->l + len + 1;
	if (needed > dst->m) {
		new_m = dst->m? dst->m : 128;
		while (new_m < needed)
			new_m += (new_m >> 1) + 128;
		p = (char*)realloc(dst->s, new_m);
		if (p == 0) return -1;
		dst->s = p, dst->m = new_m;
	}
	memcpy(dst->s + dst->l, src, len);
	dst->l += len;
	dst->s[dst->l] = 0;
	return 0;
}

static void web_clear_string(kstring_t *s)
{
	if (s == 0) return;
	s->l = 0;
	if (s->s) s->s[0] = 0;
}

static void web_set_error(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	int n;
	web_clear_string(&g_error);
	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0) return;
	if ((size_t)n >= sizeof(buf)) n = (int)sizeof(buf) - 1;
	web_append(&g_error, buf, (size_t)n);
}

static int web_write_file(const char *path, const uint8_t *data, int32_t len)
{
	FILE *fp;
	if (path == 0 || data == 0 || len < 0) return -1;
	fp = fopen(path, "wb");
	if (fp == 0) return -1;
	if (len > 0 && fwrite(data, 1, (size_t)len, fp) != (size_t)len) {
		fclose(fp);
		return -1;
	}
	if (fclose(fp) != 0) return -1;
	return 0;
}

MP_WEB_EXPORT void *miniprot_web_alloc(int32_t len)
{
	if (len <= 0) return 0;
	return malloc((size_t)len);
}

MP_WEB_EXPORT void miniprot_web_dealloc(void *ptr, int32_t len)
{
	(void)len;
	free(ptr);
}

MP_WEB_EXPORT int32_t miniprot_ganflu_run(
	const uint8_t *genome_fasta,
	int32_t genome_len,
	const uint8_t *protein_fasta,
	int32_t protein_len,
	const char *prefix,
	int32_t intron_open_penalty)
{
	static const char *genome_path = "/miniprot_ganflu_genome.fa";
	static const char *protein_path = "/miniprot_ganflu_proteins.faa";
	mp_idxopt_t io;
	mp_mapopt_t mo;
	mp_idx_t *mi = 0;
	int32_t ret = 0;

	web_clear_string(&g_result);
	web_clear_string(&g_error);

	if (genome_fasta == 0 || genome_len <= 0) {
		web_set_error("missing genome FASTA input");
		return 2;
	}
	if (protein_fasta == 0 || protein_len <= 0) {
		web_set_error("missing protein FASTA input");
		return 2;
	}

	mp_start();
	mp_verbose = 0;
	mp_dbg_flag = 0;
	mp_idxopt_init(&io);
	mp_mapopt_init(&mo);
	mo.flag |= MP_F_GFF;
	mo.gff_prefix = prefix && prefix[0]? prefix : "MP";
	mo.io = intron_open_penalty > 0? intron_open_penalty : 15;
	if (mp_mapopt_check(&mo) < 0) {
		web_set_error("invalid miniprot mapping options");
		return 2;
	}

	unlink(genome_path);
	unlink(protein_path);
	if (web_write_file(genome_path, genome_fasta, genome_len) < 0) {
		web_set_error("failed to write genome FASTA to virtual filesystem: %s", strerror(errno));
		return 3;
	}
	if (web_write_file(protein_path, protein_fasta, protein_len) < 0) {
		web_set_error("failed to write protein FASTA to virtual filesystem: %s", strerror(errno));
		unlink(genome_path);
		return 3;
	}

	mi = mp_idx_load(genome_path, &io, 1);
	if (mi == 0) {
		web_set_error("failed to open or build the miniprot index");
		ret = 4;
		goto end;
	}
	if (mp_map_file_to_string(mi, protein_path, &mo, 1, &g_result) != 0) {
		web_set_error("failed to map protein FASTA");
		ret = 5;
		goto end;
	}

end:
	if (mi) mp_idx_destroy(mi);
	unlink(genome_path);
	unlink(protein_path);
	return ret;
}

MP_WEB_EXPORT const char *miniprot_ganflu_result_ptr(void)
{
	return g_result.s;
}

MP_WEB_EXPORT int32_t miniprot_ganflu_result_len(void)
{
	return (int32_t)g_result.l;
}

MP_WEB_EXPORT const char *miniprot_ganflu_error_ptr(void)
{
	return g_error.s;
}

MP_WEB_EXPORT int32_t miniprot_ganflu_error_len(void)
{
	return (int32_t)g_error.l;
}

MP_WEB_EXPORT void miniprot_ganflu_clear(void)
{
	web_clear_string(&g_result);
	web_clear_string(&g_error);
}
