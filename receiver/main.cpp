#include "port.h"
#include "led.h"
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>

#if defined(ENABLE_REAL_3D_LED_CUBE)
#include "spi.h"
#else
#include "show.hpp"
#include <opencv2/opencv.hpp>
#endif

using namespace makerfaire::fxat;

int main(int argc, const char * argv[]) {

	uint8_t lut[256] = { 0 };
	double gamma = 0.6;
	for (int ix = 0; ix < 256; ++ix){
		lut[ix] = round(255 * pow(ix / 255.0, 1.0 / gamma));
	}
	namespace asio = boost::asio;
	namespace ip = asio::ip;
	asio::io_service io_service;
	ip::udp::socket socket(io_service, ip::udp::endpoint(ip::udp::v4(), static_cast<ushort>(makerfaire::fxat::Port)));
	std::cout << "Port:" << makerfaire::fxat::Port << std::endl;
	for (;;){
		boost::array<char, 32 * 1024> buf;
        ip::udp::endpoint ep;
        boost::system::error_code er;
        size_t len = socket.receive_from(boost::asio::buffer(buf), ep, 0, er);
        // Led::Width*Led::Height*Led::Depth
		if (len != 8192) {
			std::cerr << "size error : " << len << "\n" << std::endl;
			continue;
		}
		int m[Led::Width][Led::Height][Led::Depth] = { 0 };
		for (int x = 0, i = 0; x < Led::Width; ++x){
			for (int y = 0; y < Led::Height; ++y){
				for (int z = 0; z < Led::Depth; ++z, ++i){
                    char r = (buf[i * 2] & 0xF8);
                    char g = ((buf[i * 2] & 0x07) << 5) + (((buf[i * 2 + 1] & 0xE0) >> 3));
                    char b = (buf[i * 2 + 1] << 3);
					m[x][y][z] = (lut[(std::uint8_t)r] << 16) + (lut[(std::uint8_t)g] << 8) + (lut[(std::uint8_t)b] << 0);
				}
			}
		}
#if defined(ENABLE_REAL_3D_LED_CUBE)
		SendSpi(m);
#else
		::ShowWindow("Receiver", m);
#endif
	}
	return 0;
}
