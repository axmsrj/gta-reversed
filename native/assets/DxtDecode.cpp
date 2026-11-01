#include "DxtDecode.h"

#include <array>
#include <algorithm>
#include <limits>

namespace gtasa::assets {
namespace {
std::uint16_t Read16(const std::uint8_t* p) { return p[0] | (std::uint16_t(p[1]) << 8); }
std::uint32_t Read32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
using Color=std::array<std::uint8_t,4>;
Color Unpack565(std::uint16_t p) {
    return {std::uint8_t(((p>>11)&31)*255/31),std::uint8_t(((p>>5)&63)*255/63),
            std::uint8_t((p&31)*255/31),255};
}
}

std::vector<std::uint8_t> DecodeDxt(const std::uint8_t* blocks, std::size_t size,
                                    int width, int height, std::uint32_t fourcc) {
    constexpr std::uint32_t dxt1=0x31545844, dxt2=0x32545844, dxt3=0x33545844,
                            dxt4=0x34545844, dxt5=0x35545844;
    if (!blocks || width<=0 || height<=0 || width>8192 || height>8192) return {};
    if (fourcc!=dxt1 && fourcc!=dxt2 && fourcc!=dxt3 && fourcc!=dxt4 && fourcc!=dxt5) return {};
    const std::size_t blockBytes=fourcc==dxt1 ? 8 : 16;
    const std::size_t nx=(std::size_t(width)+3)/4, ny=(std::size_t(height)+3)/4;
    if (nx > std::numeric_limits<std::size_t>::max()/ny/blockBytes || size<nx*ny*blockBytes)
        return {};
    std::vector<std::uint8_t> rgba(std::size_t(width)*height*4);
    for (std::size_t by=0;by<ny;++by) for (std::size_t bx=0;bx<nx;++bx) {
        const auto* block=blocks+(by*nx+bx)*blockBytes;
        const auto* colors=block+(fourcc==dxt1 ? 0 : 8);
        const auto c0=Read16(colors), c1=Read16(colors+2);
        std::array<Color,4> palette{Unpack565(c0),Unpack565(c1),{}, {}};
        const bool fourColors=fourcc!=dxt1 || c0>c1;
        for (int channel=0;channel<3;++channel) {
            if (fourColors) {
                palette[2][channel]=(2*int(palette[0][channel])+palette[1][channel])/3;
                palette[3][channel]=(palette[0][channel]+2*int(palette[1][channel]))/3;
            } else {
                palette[2][channel]=(int(palette[0][channel])+palette[1][channel])/2;
            }
        }
        palette[2][3]=255;
        palette[3][3]=fourColors ? 255 : 0;
        const auto indices=Read32(colors+4);
        std::array<std::uint8_t,8> alphas{};
        std::uint64_t alphaBits=0;
        if (fourcc==dxt4 || fourcc==dxt5) {
            alphas[0]=block[0]; alphas[1]=block[1];
            for (int i=1;i<7;++i) alphaBits|=std::uint64_t(block[i+1])<<(8*(i-1));
            for (int i=2;i<8;++i) {
                if (alphas[0]>alphas[1])
                    alphas[i]=((8-i)*int(alphas[0])+(i-1)*int(alphas[1]))/7;
                else if (i<6)
                    alphas[i]=((6-i)*int(alphas[0])+(i-1)*int(alphas[1]))/5;
                else alphas[i]=i==6 ? 0 : 255;
            }
        }
        for (int py=0;py<4;++py) for (int px=0;px<4;++px) {
            const int x=int(bx*4)+px,y=int(by*4)+py;
            if (x>=width || y>=height) continue;
            const int i=py*4+px;
            auto color=palette[(indices>>(i*2))&3];
            if (fourcc==dxt2 || fourcc==dxt3)
                color[3]=std::uint8_t(((block[i/2]>>(4*(i%2)))&15)*17);
            if (fourcc==dxt4 || fourcc==dxt5)
                color[3]=alphas[(alphaBits>>(i*3))&7];
            if ((fourcc==dxt2 || fourcc==dxt4) && color[3])
                for (int c=0;c<3;++c) color[c]=std::uint8_t(std::min(255,int(color[c])*255/int(color[3])));
            const auto offset=(std::size_t(y)*width+x)*4;
            std::copy(color.begin(),color.end(),rgba.begin()+offset);
        }
    }
    return rgba;
}
}
