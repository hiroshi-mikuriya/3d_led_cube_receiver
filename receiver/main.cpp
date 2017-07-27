#include "port.h"
#include "led.h"
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <map>
#include <chrono>

#if defined(ENABLE_REAL_3D_LED_CUBE)
#include "spi.h"
#else
#include "show.hpp"
#include <opencv2/opencv.hpp>
#endif

using namespace makerfaire::fxat;
namespace asio = boost::asio;
namespace ip = asio::ip;

/** ルックアップテーブルのサイズ */
const size_t lut_size = 256;

/** LEDパケットのサイズ */
const size_t packet_size = 8192;

/** パケット型 */
typedef std::vector<uint8_t> packet_type;

/** 履歴の保存期間 */
const size_t time_limit_ms = 500;

/** 履歴のバリュー */
typedef std::pair<std::chrono::system_clock::time_point, packet_type> history_value_type;

/** 履歴のキー */
typedef ip::address history_key_type;

/** 履歴の型 */
typedef std::map<history_key_type, history_value_type> history_type;

/*!
 履歴にパケットを追加する
 @param[in] packet 追加するパケット
 @param[out] history 履歴
 */
template<size_t N>
void add_history(ip::address const & address, boost::array<char, N> const & received, history_type & history)
{
    packet_type packet(packet_size);
    for(int i = 0; i < packet.size(); ++i){
        packet[i] = received[i];
    }
    history[address] = std::make_pair(std::chrono::system_clock::now(), packet);
}

/*!
 古い履歴を削除する
 @param[out] history 履歴
 */
void erase_old_history(history_type & history)
{
    auto now = std::chrono::system_clock::now();
    std::vector<ip::address> erase_list;
    for(auto it = history.begin(); it != history.end(); ++it){
        auto time = it->second.first;
        auto d = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - time).count());
        if(time_limit_ms < d){
            erase_list.push_back(it->first);
        }
    }
    for(auto it = erase_list.begin(); it != erase_list.end(); ++it){
        history.erase(*it);
    }
}

/*!
 3D LED CUBEへデータを転送する
 @param[in] history 履歴
 @param[in] lut ルックアップテーブル
 */
void send_led(history_type const & history, uint8_t const (&lut)[lut_size])
{
    packet_type packet(packet_size);
    for(size_t i = 0; i < packet.size(); ++i){
        packet[i] = 0;
        for(auto it = history.cbegin(); it != history.cend(); ++it){
            packet[i] = std::max(packet[i], it->second.second[i]);
        }
    }
    int m[Led::Width][Led::Height][Led::Depth] = { 0 };
    for (int x = 0, i = 0; x < Led::Width; ++x){
        for (int y = 0; y < Led::Height; ++y){
            for (int z = 0; z < Led::Depth; ++z, ++i){
                char r = (packet[i * 2] & 0xF8);
                char g = ((packet[i * 2] & 0x07) << 5) + (((packet[i * 2 + 1] & 0xE0) >> 3));
                char b = (packet[i * 2 + 1] << 3);
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

/*!
 ルックアップテーブルを作る
 @param[out] lut
 */
void make_lut(uint8_t (&lut)[lut_size])
{
    double gamma = 0.6;
    for (int ix = 0; ix < lut_size; ++ix){
        lut[ix] = round(255 * pow(ix / 255.0, 1.0 / gamma));
    }
}

int main(int argc, const char * argv[]) {
    
    uint8_t lut[lut_size] = { 0 };
    make_lut(lut);
    history_type history;
    
    asio::io_service io_service;
    ip::udp::socket socket(io_service, ip::udp::endpoint(ip::udp::v4(), static_cast<ushort>(makerfaire::fxat::Port)));
    std::cout << "Port:" << makerfaire::fxat::Port << std::endl;
    for (;;){
        boost::array<char, packet_size * 4> buf;
        ip::udp::endpoint ep;
        boost::system::error_code er;
        size_t len = socket.receive_from(boost::asio::buffer(buf), ep, 0, er);
        if (len != packet_size) {
            std::cerr << "size error : " << len << "\n" << std::endl;
            continue;
        }
        add_history(ep.address(), buf, history);
        erase_old_history(history);
        send_led(history, lut);
    }
    return 0;
}
