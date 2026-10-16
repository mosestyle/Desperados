// SBPictureJpeg (SBLibNG/SBPictureJpeg.cpp): the JPEG pictures inside the level files (the
// patches' pictures). The original reads them with libjpeg straight from the file and steps back
// over what the decoder read ahead; here the stream is walked marker by marker to its end (EOI),
// then decoded with stb_image, and turned into the engine's 16-bit pixels (RGB565).
#include "sb/SBPictureJpeg.h"

#include "sb/SBFile.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG  // (the app's menu background)
#define STBI_NO_STDIO
#include "stb_image.h"

bool SBPictureJpeg::LoadFromStream(SBFile& f) {
    std::vector<uint8_t> buf;
    auto get = [&](uint8_t& b) {
        if (f.Tell() >= f.Size()) return false;
        b = f.U8();
        buf.push_back(b);
        return true;
    };
    uint8_t a = 0, b = 0;
    if (!get(a) || !get(b) || a != 0xff || b != 0xd8) return false;
    for (;;) {
        // the next marker
        if (!get(a)) return false;
        if (a != 0xff) return false;
        do {
            if (!get(b)) return false;
        } while (b == 0xff);
        if (b == 0xd9) break;
        if (b == 0x01 || (b >= 0xd0 && b <= 0xd7)) continue;
        uint8_t hi, lo;
        if (!get(hi) || !get(lo)) return false;
        int len = (hi << 8 | lo) - 2;
        for (int i = 0; i < len; ++i)
            if (!get(a)) return false;
        if (b == 0xda) {
            // the entropy-coded data: up to a marker that is not a stuffed 0 or a restart
            for (;;) {
                if (!get(a)) return false;
                if (a != 0xff) continue;
                if (!get(b)) return false;
                while (b == 0xff)
                    if (!get(b)) return false;
                if (b == 0 || (b >= 0xd0 && b <= 0xd7)) continue;
                if (b == 0xd9) goto done;
                // another marker (a second scan, tables): its segment
                if (!get(hi) || !get(lo)) return false;
                len = (hi << 8 | lo) - 2;
                for (int i = 0; i < len; ++i)
                    if (!get(a)) return false;
                if (b != 0xda) break;
            }
        }
    }
done:
    int w = 0, h = 0, n = 0;
    unsigned char* rgb = stbi_load_from_memory(buf.data(), (int)buf.size(), &w, &h, &n, 3);
    if (!rgb) return false;
    width = (uint16_t)w;
    height = (uint16_t)h;
    pixels.resize((size_t)w * h);
    for (size_t i = 0; i < pixels.size(); ++i) {
        const unsigned char* q = rgb + i * 3;
        pixels[i] = (uint16_t)((q[0] & 0xf8) << 8 | (q[1] & 0xfc) << 3 | q[2] >> 3);
    }
    stbi_image_free(rgb);
    return true;
}
