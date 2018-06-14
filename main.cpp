#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>

#ifdef ENABLE_REAL_3D_LED_CUBE
#include <bcm2835.h>
#endif // ENABLE_REAL_3D_LED_CUBE

namespace led {
    int const width = 16;
    int const height = 32;
    int const depth = 8;
    int const rgb565 = (5 + 6 + 5) / 8;
    int const rgb24 = 24 / 8;
    int const spi_header_size = 3;
    int const udp_packet_size = width * height * depth * rgb565;
    int const spi_packet_size = width * height * depth * rgb24 + spi_header_size;

    typedef boost::array<char, udp_packet_size * 2> udp_buf_type;
    typedef boost::array<char, spi_packet_size> spi_buf_type;

    struct LookupTable
    {
        uint8_t m[256];

        LookupTable(double gamma) {
            for (size_t ix = 0; ix < sizeof(m); ++ix){
                m[ix] = round(255 * pow(ix / 255.0, 1.0 / gamma));
            }
        }
    };
}

namespace {
    void convert(led::udp_buf_type const & u, led::spi_buf_type & s, led::LookupTable const & lut) {
        s[0] = 2; // spi write command
        s[1] = s[2] = 0; // spi address
        for (int x = 0; x < led::width; ++x){
            for (int y = 0; y < led::height; ++y){
                for (int z = 0; z < led::depth; ++z){
                    int i0 = ((x * led::height + y) * led::depth + z) * led::rgb565;
                    int i1 = ((z * led::height + y) * led::width + x) * led::rgb24 + led::spi_header_size;
                    s[i1] = lut.m[(u[i0] & 0xF8)]; // R
                    s[i1 + 1] = lut.m[((u[i0] & 0x07) << 5) + (((u[i0 + 1] & 0xE0) >> 3))]; // G
                    s[i1 + 2] = lut.m[(u[i0 + 1] & 0x1F) << 3]; // B
                }
            }
        }
    }

    void write_spi(led::spi_buf_type const & s) {
#ifdef ENABLE_REAL_3D_LED_CUBE
        bcm2835_spi_begin();
        bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
        bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
        bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64);
        bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
        bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
        bcm2835_spi_writenb(const_cast<char*>(&s[0]), s.size());
        bcm2835_spi_end();
#endif // ENABLE_REAL_3D_LED_CUBE
    }
}

int main(int argc, const char * argv[]) {
#ifdef ENABLE_REAL_3D_LED_CUBE
    if (!bcm2835_init()){
        std::cerr << "failed to init bcm2835." << std::endl;
        return 1;
    }
#endif // ENABLE_REAL_3D_LED_CUBE
    namespace asio = boost::asio;
    namespace ip = asio::ip;
    led::LookupTable const lut(0.6);
    asio::io_service io_service;
    ip::udp::socket socket(io_service, ip::udp::endpoint(ip::udp::v4(), 9001));
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
