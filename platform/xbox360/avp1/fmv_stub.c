/* avp360: substitui os modulos que dependem de FFmpeg e do OpenGL de PC.
 * - movie.c (Bink/Smacker via FFmpeg): sem decodificador por enquanto; as FMVs sao puladas.
 * - postprocess.c (bloom GLSL): desligado.
 * - cdplayer.c (musica via FFmpeg): silencioso ate o tocador de musica do Xbox entrar.
 * - oglfunc.c: os ponteiros pgl* sao preenchidos por xenos_gl.c. */
#include <stddef.h>
#include <stdint.h>

typedef struct AVPMovie AVPMovie;

AVPMovie *AVPMovie_Open(const char *filename, int with_audio) { (void)filename; (void)with_audio; return NULL; }
void AVPMovie_Close(AVPMovie *movie) { (void)movie; }
int AVPMovie_Width(const AVPMovie *movie) { (void)movie; return 0; }
int AVPMovie_Height(const AVPMovie *movie) { (void)movie; return 0; }
int AVPMovie_FrameNumber(const AVPMovie *movie) { (void)movie; return 0; }
int AVPMovie_FrameCount(const AVPMovie *movie) { (void)movie; return 0; }
int AVPMovie_FrameDurationMs(const AVPMovie *movie) { (void)movie; return 40; }
int AVPMovie_NextVideoFrame(AVPMovie *movie, uint8_t *dst, int dst_width, int dst_height, int dst_stride)
{ (void)movie; (void)dst; (void)dst_width; (void)dst_height; (void)dst_stride; return 0; }
int AVPMovie_FeedAudio(AVPMovie *movie) { (void)movie; return 0; }
int AVPMovie_SeekFrame(AVPMovie *movie, int frame) { (void)movie; (void)frame; return 0; }
int AVPMovie_PresentSurface(AVPMovie *movie, const uint8_t *rgb, int width, int height, int stride)
{ (void)movie; (void)rgb; (void)width; (void)height; (void)stride; return 0; }
int AVPMovie_PlayFullscreen(const char *filename) { (void)filename; return 0; }

int AVP_BloomEnabled = 0;
void AVP_PostProcess_Init(void) {}
void AVP_PostProcess_Render(int width, int height) { (void)width; (void)height; }
void AVP_PostProcess_InvalidateContext(void) {}
void AVP_PostProcess_Shutdown(void) {}

int CDPlayerVolume;
int CDTrackMax = 0;
void CheckCDVolume(void) {}
void CDDA_Start(void) {}
void CDDA_End(void) {}
void CDDA_ChangeVolume(int volume) { CDPlayerVolume = volume; }
int CDDA_GetCurrentVolumeSetting(void) { return CDPlayerVolume; }
int CDDA_CheckNumberOfTracks(void) { return 0; }
int CDDA_IsOn(void) { return 0; }
int CDDA_IsPlaying(void) { return 0; }
void CDDA_Play(int track) { (void)track; }
void CDDA_PlayLoop(int track) { (void)track; }
void CDDA_Stop(void) {}
void CDDA_SwitchOn(void) {}
void CDDA_SwitchOff(void) {}
void PlatCDDAManagement(void) {}
void CDDA_Management(void) {}

int check_for_errors_(const char *file, int line) { (void)file; (void)line; return 0; }
