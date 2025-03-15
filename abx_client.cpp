#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <array>
#include <algorithm>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

using boost::asio::ip::tcp;
using json = nlohmann::json;

constexpr int PORT = 3000;
constexpr char HOST[] = "127.0.0.1";

// Logging utility
#define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl;
#define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl;

// Packet structure with improvements
struct Packet {
    std::array<char, 4> symbol; // Fixed-size array for symbol
    enum class Side : char { Buy = 'B', Sell = 'S' } side;
    uint32_t quantity;
    uint32_t price;
    uint32_t sequence;

    // Convert Packet to JSON
    json toJSON() const {
        return {
            {"symbol", std::string(symbol.begin(), symbol.end())},
            {"side", std::string(1, static_cast<char>(side))},
            {"quantity", quantity},
            {"price", price},
            {"sequence", sequence}
        };
    }
};

// Convert a 4-byte big-endian value to an int
uint32_t readInt32(const std::vector<char>& data, size_t offset) {
    return (static_cast<unsigned char>(data[offset]) << 24) |
           (static_cast<unsigned char>(data[offset + 1]) << 16) |
           (static_cast<unsigned char>(data[offset + 2]) << 8) |
           (static_cast<unsigned char>(data[offset + 3]));
}

// Send request to server
void sendRequest(tcp::socket& socket, uint8_t callType, uint8_t sequence = 0) {
    std::vector<char> request = {static_cast<char>(callType), static_cast<char>(sequence)};
    boost::asio::write(socket, boost::asio::buffer(request));
    LOG_INFO("Sent request: CallType = " << static_cast<int>(callType) << ", Sequence = " << static_cast<int>(sequence));
}

// Receive response from server
std::vector<char> receiveResponse(tcp::socket& socket) {
    std::vector<char> buffer(17); // Fixed packet size
    boost::system::error_code error;
    size_t length = socket.read_some(boost::asio::buffer(buffer), error);

    if (error == boost::asio::error::eof) {
        LOG_INFO("Connection closed by server.");
        return {};
    } else if (error) {
        LOG_ERROR("Receive error: " << error.message());
        throw boost::system::system_error(error);
    }

    buffer.resize(length);
    return buffer;
}

// Parse a packet from raw binary data
Packet parsePacket(const std::vector<char>& data) {
    Packet packet;
    std::copy_n(data.begin(), 4, packet.symbol.begin()); // Copy symbol
    packet.side = static_cast<Packet::Side>(data[4]);    // Convert side to enum
    packet.quantity = readInt32(data, 5);
    packet.price = readInt32(data, 9);
    packet.sequence = readInt32(data, 13);

    LOG_INFO("Parsed packet: " << std::string(packet.symbol.begin(), packet.symbol.end()) 
             << " | Side: " << static_cast<char>(packet.side) 
             << " | Qty: " << packet.quantity 
             << " | Price: " << packet.price 
             << " | Seq: " << packet.sequence);
    
    return packet;
}

// Main function to request, validate, and save data
void fetchAndSaveData() {
    boost::asio::io_context io_context;
    tcp::socket socket(io_context);
    tcp::resolver resolver(io_context);
    boost::asio::connect(socket, resolver.resolve(HOST, std::to_string(PORT)));
    LOG_INFO("Connected to server.");

    // Send request to stream all packets
    sendRequest(socket, 1);

    std::vector<Packet> packets;
    std::set<uint32_t> receivedSequences;

    while (true) {
        auto data = receiveResponse(socket);
        if (data.empty()) break; // Server closed connection

        Packet packet = parsePacket(data);
        packets.push_back(packet);
        receivedSequences.insert(packet.sequence);
    }

    // Detect missing sequences
    if (packets.empty()) {
        LOG_ERROR("No packets received.");
        return;
    }

    uint32_t lastSeq = packets.back().sequence;
    std::set<uint32_t> missingSequences;
    for (uint32_t i = 1; i < lastSeq; ++i) {
        if (receivedSequences.find(i) == receivedSequences.end()) {
            missingSequences.insert(i);
        }
    }

    LOG_INFO("Missing sequences detected: " << missingSequences.size());

    // Request missing sequences
    for (uint32_t seq : missingSequences) {
        tcp::socket retry_socket(io_context);
        boost::asio::connect(retry_socket, resolver.resolve(HOST, std::to_string(PORT)));
        sendRequest(retry_socket, 2, seq);

        auto data = receiveResponse(retry_socket);
        if (!data.empty()) {
            packets.push_back(parsePacket(data));
        }
    }

    // Sort packets by sequence number
    std::sort(packets.begin(), packets.end(), [](const Packet& a, const Packet& b) {
        return a.sequence < b.sequence;
    });

    // Convert to JSON and save to file
    json output = json::array();
    for (const auto& packet : packets) {
        output.push_back(packet.toJSON());
    }

    std::ofstream outFile("output.json");
    outFile << output.dump(4);
    outFile.close();

    LOG_INFO("Data saved to output.json");
}

int main() {
    try {
        fetchAndSaveData();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception: " << e.what());
    }
    return 0;
}
