#include <stdint.h>
#include <beans/structures.h>

namespace Filter { namespace HQ2x {
    // Solo declaramos que existen
    extern uint32_t *yuvTable;
    void render(uint16_t* output, unsigned int outpitch, const uint16_t* input, unsigned int pitch, unsigned int width, unsigned int height);
	void scale2x_to_3x_565(const uint16_t* src, unsigned srcW, unsigned srcH, unsigned srcPitch, uint16_t* dst, unsigned dstPitch);
}
}
