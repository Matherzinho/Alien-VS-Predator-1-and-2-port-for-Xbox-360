/* avp360: ponto de entrada do avp1.xex.
 * - O motor (main do NakedAVP, renomeado para avp1_game_main pelo prefixo) roda numa thread com 16 MB de pilha:
 *   a thread principal de um .xex do xsdk so tem 1 MB.
 * - Cada etapa fica no log (avp1_log: cache:\ ou game:\avp1_log.txt; o xsdk tambem grava game:\avp1.log e, com
 *   net_log.txt ao lado do .xex, manda o log por UDP para o PC), para um travamento mostrar onde parou. */
#undef main
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <xsdk/terminate.h>

int avp1_game_main(int argc, char **argv);

#define GAME_STACK_BYTES (16u * 1024u * 1024u)

static int game_argc;
static char **game_argv;
static int game_result;

static void *game_thread(void *unused)
{
	(void)unused;
	avp1_log("entrada: thread do jogo iniciada");
	game_result = avp1_game_main(game_argc, game_argv);
	avp1_log("entrada: jogo terminou (%d)", game_result);
	return NULL;
}

static void on_terminate(void)
{
	avp1_log("entrada: o sistema esta encerrando o titulo");
}

int main(int argc, char **argv)
{
	static char *default_argv[] = { (char *)"avp1.xex", NULL };
	if (argc <= 0 || !argv) {
		argc = 1;
		argv = default_argv;
	}
	game_argc = argc;
	game_argv = argv;
	avp1_log("entrada: avp1.xex carregado");
	xsdk_on_terminate(on_terminate);

	pthread_attr_t attr;
	pthread_t thread;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, GAME_STACK_BYTES);
	if (pthread_create(&thread, &attr, game_thread, NULL) != 0) {
		avp1_log("entrada: nao consegui criar a thread de 16 MB; rodando na thread principal");
		return avp1_game_main(argc, argv);
	}
	pthread_attr_destroy(&attr);
	pthread_join(thread, NULL);
	return game_result;
}
