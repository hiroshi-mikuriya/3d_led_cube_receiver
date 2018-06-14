#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>

#if defined(ENABLE_REAL_3D_LED_CUBE)
#include "spi.h"
#endif

namespace asio = boost::asio;
namespace ip = asio::ip;

/** ルックアップテーブル */
struct Lut
{
    uint8_t m[256];

    Lut() {
        double gamma = 0.6;
        for (int ix = 0; ix < sizeof(m); ++ix){
            m[ix] = round(255 * pow(ix / 255.0, 1.0 / gamma));
        }
    }
};

/** LEDパケットのサイズ */
const size_t led_packet_size = 8192;

/*!
 3D LED CUBEへデータを転送する
 @param[in] history 履歴
 @param[in] lut ルックアップテーブル
 */
template<size_t N>
void send_led(boost::array<char, N> const & buf, Lut const & lut)
{
    int m[16][32][8] = { 0 };
    for (int x = 0, i = 0; x < 16; ++x){
        for (int y = 0; y < 32; ++y){
            for (int z = 0; z < 8; ++z, ++i){
                char r = (buf[i * 2] & 0xF8);
                char g = ((buf[i * 2] & 0x07) << 5) + (((buf[i * 2 + 1] & 0xE0) >> 3));
                char b = (buf[i * 2 + 1] << 3);
                m[x][y][z] = (lut.m[(std::uint8_t)r] << 16) + (lut.m[(std::uint8_t)g] << 8) + (lut.m[(std::uint8_t)b] << 0);
            }
        }
    }
#if defined(ENABLE_REAL_3D_LED_CUBE)
    SendSpi(m);
#endif
}

int main(int argc, const char * argv[]) {
    Lut lut;
    asio::io_service io_service;
    ushort const port = 9001;
    ip::udp::socket socket(io_service, ip::udp::endpoint(ip::udp::v4(), port));
    boost::array<char, led_packet_size * 4> buf;
    for (;;){
        ip::udp::endpoint ep;
        boost::system::error_code er;
        size_t len = socket.receive_from(boost::asio::buffer(buf), ep, 0, er);
        if (len != led_packet_size) {
            std::cerr << "size error : " << len << "\n" << std::endl;
            continue;
        }
        send_led(buf, lut);
    }
    return 0;
}
