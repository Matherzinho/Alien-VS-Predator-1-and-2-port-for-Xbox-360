/* avp360: log do AvP1 em game:\avp1_log.txt (ou cache:\; e no log do xsdk), para diagnosticar travamentos no console.
 * Cada linha e gravada e descarregada na hora: se o console travar, a ultima linha mostra onde. */
#include <stdarg.h>
#include <stdio.h>
#include <xsdk/log.h>

static FILE *log_file;
static int log_tried;

void avp1_log(const char *fmt, ...)
{
	char line[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line, sizeof line, fmt, ap);
	va_end(ap);
	xsdk_log_write("avp1: %s", line);
	if (!log_tried) {
		log_tried = 1;
		/* Ao lado do .xex primeiro (onde o jogador procura); cache:\ se game:\ for so leitura. */
		log_file = fopen("game:\\avp1_log.txt", "w");
		if (!log_file) log_file = fopen("cache:\\avp1_log.txt", "w");
	}
	if (log_file) {
		fputs(line, log_file);
		fputc('\n', log_file);
		fflush(log_file);
	}
}
