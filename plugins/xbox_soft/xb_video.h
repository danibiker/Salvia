#define PSX_WIDTH 1024
#define PSX_HEIGHT 512

#ifdef __cplusplus
extern "C" {
#endif
	unsigned int VideoInit();
	void DisplayUpdate();
	void UpdateScrenRes(int x,int y);
#ifdef __cplusplus
}
#endif
