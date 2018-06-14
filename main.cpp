#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <bcm2835.h>

namespace asio = boost::asio;
namespace ip = asio::ip;

namespace led {
    int const width = 16;
    int const height = 32;
    int const depth = 8;
    int const udp_packet_size = width * height * depth * 2;
    int const spi_write_size = width * height * depth * 3 + 3;

    typedef boost::array<char, udp_packet_size * 2> udp_buf_type;
    typedef boost::array<char, spi_write_size> spi_buf_type;

    struct LookupTable
    {
        uint8_t m[256];

        LookupTable(double gamma) {
            for (int ix = 0; ix < sizeof(m); ++ix){
                m[ix] = round(255 * pow(ix / 255.0, 1.0 / gamma));
            }
        }
    };
}



namespace 
{
    void convert(led::udp_buf_type const & u, led::spi_buf_type & s, led::LookupTable const & lut) {
        s[0] = 2;
        s[1] = s[2] = 0;
        for (int x = 0, i = 0; x < led::width; ++x){
            for (int y = 0; y < led::height; ++y){
                for (int z = 0; z < led::depth; ++z, ++i){
                    char r = lut.m[(u[i * 2] & 0xF8)];
                    char g = lut.m[((u[i * 2] & 0x07) << 5) + (((u[i * 2 + 1] & 0xE0) >> 3))];
                    char b = lut.m[(u[i * 2 + 1] << 3)];
                    int i1 = (z * led::width * led::height + y * led::width + x) * 3 + 3;
                    s[i1] = r;
                    s[i1 + 1] = g;
                    s[i1 + 2] = b;
                }
            }
        }
    }

    void write_spi(led::spi_buf_type const & s) {
        bcm2835_spi_begin();
        bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
        bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
        bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64);
        bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
        bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
        bcm2835_spi_writenb(const_cast<char*>(&s[0]), s.size());
        bcm2835_spi_end();
    }
}

int main(int argc, const char * argv[]) {
    if (!bcm2835_init()){
        std::cerr << "failed to init bcm2835." << std::endl;
        return 1;
    }
	led::LookupTable const lut(0.6);
    asio::io_service io_service;
    ushort const port = 9001;
    ip::udp::socket socket(io_service, ip::udp::endpoint(ip::udp::v4(), port));
    led::udp_buf_type udp_buf;
    led::spi_buf_type spi_buf;
    for (;;){
        ip::udp::endpoint ep;
        boost::system::error_code er;
        size_t len = socket.receive_from(boost::asio::buffer(udp_buf), ep, 0, er);
        if (len != led::udp_packet_size) {
            std::cerr << "size error : " << len << "\n" << std::endl;
            continue;
        }
        convert(udp_buf, spi_buf, lut);
        write_spi(spi_buf);
    }
    return 0;
}
