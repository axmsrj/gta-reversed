#include "DxtDecode.h"

#include <array>
#include <cassert>
#include <cstdint>

int main() {
    using gtasa::assets::DecodeDxt;
    constexpr std::uint32_t dxt1=0x31545844,dxt3=0x33545844,dxt5=0x35545844;
    // Red endpoint, blue endpoint; first pixel red and second blue.
    const std::array<std::uint8_t,8> bc1{0x00,0xf8,0x1f,0x00,0x04,0,0,0};
    const auto red=DecodeDxt(bc1.data(),bc1.size(),4,4,dxt1);
    assert(red.size()==64 && red[0]==255 && red[1]==0 && red[2]==0 && red[3]==255);
    assert(red[4]==0 && red[5]==0 && red[6]==255 && red[7]==255);
    assert(DecodeDxt(bc1.data(),7,4,4,dxt1).empty());
    assert(DecodeDxt(bc1.data(),bc1.size(),0,4,dxt1).empty());
    assert(DecodeDxt(bc1.data(),bc1.size(),4,4,0).empty());
    const std::array<std::uint8_t,8> bc1Alpha{0,0,0xff,0xff,3,0,0,0};
    const auto alpha=DecodeDxt(bc1Alpha.data(),bc1Alpha.size(),4,4,dxt1);
    assert(alpha.size()==64 && alpha[3]==0);
    std::array<std::uint8_t,16> bc2{};
    bc2[0]=0xf0; // pixel 0 transparent, pixel 1 opaque
    for (int i=0;i<8;++i) bc2[8+i]=bc1[i];
    const auto explicitAlpha=DecodeDxt(bc2.data(),bc2.size(),4,4,dxt3);
    assert(explicitAlpha.size()==64 && explicitAlpha[3]==0 && explicitAlpha[7]==255);
    std::array<std::uint8_t,16> bc3{};
    bc3[0]=255; bc3[1]=0; bc3[2]=1; // pixel 0 alpha index 1 => zero
    for (int i=0;i<8;++i) bc3[8+i]=bc1[i];
    const auto interpolatedAlpha=DecodeDxt(bc3.data(),bc3.size(),4,4,dxt5);
    assert(interpolatedAlpha.size()==64 && interpolatedAlpha[3]==0 && interpolatedAlpha[7]==255);
}
