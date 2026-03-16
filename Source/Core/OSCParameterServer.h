#pragma once

#include <juce_osc/juce_osc.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <thread>
#include <atomic>

/**
 * Lightweight OSC server embedded in the plugin.
 * Listens on UDP port 11100 using a dedicated thread (no JUCE message loop needed).
 * Replies on port 11101.
 *
 * Protocol:
 *   /aieq/ping                       → replies /aieq/pong
 *   /aieq/list                       → replies with all param IDs + values
 *   /aieq/get  <paramID>             → replies with paramID + value
 *   /aieq/set  <paramID> <value>     → sets normalised value (0-1)
 *   /aieq/set/denorm <paramID> <val> → sets denormalised (real) value
 */
class OSCParameterServer
{
public:
    OSCParameterServer(juce::AudioProcessorValueTreeState& apvts, int port = 11100)
        : apvtsRef(apvts), listenPort(port)
    {
    }

    ~OSCParameterServer()
    {
        stop();
    }

    bool isRunning() const { return running.load(); }

    bool start()
    {
        if (running.load())
            return true;

        // Log to file for debugging
        juce::File logFile(juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                           .getChildFile("aieq_osc_log.txt"));
        logFile.appendText("OSCParameterServer::start() called at " + juce::Time::getCurrentTime().toString(true, true) + "\n");

        socket = std::make_unique<juce::DatagramSocket>(false);
        bool bound = socket->bindToPort(listenPort);
        logFile.appendText("bindToPort(" + juce::String(listenPort) + ") = " + (bound ? "OK" : "FAILED") + "\n");

        if (!bound)
        {
            // Try alternative ports
            for (int alt = listenPort + 10; alt < listenPort + 50; alt += 10)
            {
                bound = socket->bindToPort(alt);
                if (bound)
                {
                    logFile.appendText("Bound to alternative port " + juce::String(alt) + "\n");
                    listenPort = alt;
                    break;
                }
            }
            if (!bound)
            {
                logFile.appendText("All ports failed\n");
                return false;
            }
        }

        running.store(true);
        serverThread = std::make_unique<std::thread>([this]() { run(); });
        logFile.appendText("Server thread started on port " + juce::String(listenPort) + "\n");
        return true;
    }

    void stop()
    {
        running.store(false);
        if (socket)
            socket->shutdown();
        if (serverThread && serverThread->joinable())
            serverThread->join();
        serverThread.reset();
        socket.reset();
    }

private:
    void run()
    {
        char buf[4096];
        while (running.load())
        {
            juce::String senderIP;
            int senderPort = 0;
            int bytesRead = socket->read(buf, sizeof(buf), false, senderIP, senderPort);

            if (bytesRead <= 0)
            {
                if (running.load())
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            handleRawOSC(buf, bytesRead, senderIP, senderPort);
        }
    }

    void handleRawOSC(const char* data, int len, const juce::String& senderIP, int senderPort)
    {
        // Parse OSC address (null-terminated, padded to 4 bytes)
        juce::String address;
        int offset = 0;
        while (offset < len && data[offset] != '\0')
            address += data[offset++];
        offset++; // skip null
        offset += (4 - offset % 4) % 4; // align to 4

        // Parse type tag string
        juce::String typetag;
        if (offset < len && data[offset] == ',')
        {
            while (offset < len && data[offset] != '\0')
                typetag += data[offset++];
            offset++;
            offset += (4 - offset % 4) % 4;
        }

        // Parse arguments
        std::vector<std::variant<float, int, juce::String>> args;
        for (int i = 1; i < typetag.length(); ++i) // skip ','
        {
            char t = typetag[i];
            if (t == 'f' && offset + 4 <= len)
            {
                union { char c[4]; float f; } u;
                // big-endian to host
                u.c[0] = data[offset+3]; u.c[1] = data[offset+2];
                u.c[2] = data[offset+1]; u.c[3] = data[offset];
                args.push_back(u.f);
                offset += 4;
            }
            else if (t == 'i' && offset + 4 <= len)
            {
                int32_t v = (static_cast<uint8_t>(data[offset]) << 24)
                          | (static_cast<uint8_t>(data[offset+1]) << 16)
                          | (static_cast<uint8_t>(data[offset+2]) << 8)
                          | (static_cast<uint8_t>(data[offset+3]));
                args.push_back(v);
                offset += 4;
            }
            else if (t == 's')
            {
                juce::String s;
                while (offset < len && data[offset] != '\0')
                    s += data[offset++];
                offset++;
                offset += (4 - offset % 4) % 4;
                args.push_back(s);
            }
        }

        // Reply port: always send back to sender on port 11101
        int replyPort = listenPort + 1;

        if (address == "/aieq/ping")
        {
            sendOSCReply(senderIP, replyPort, "/aieq/pong", {});
            return;
        }

        if (address == "/aieq/list")
        {
            // Build reply with all param IDs and denormalised values
            std::vector<uint8_t> reply;
            writeOSCString(reply, "/aieq/list");

            // First pass: build type tag and collect data
            std::vector<std::pair<juce::String, float>> paramData;
            for (auto* param : apvtsRef.processor.getParameters())
            {
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(param))
                {
                    paramData.push_back({rp->getParameterID(),
                                         rp->convertFrom0to1(rp->getValue())});
                }
            }

            juce::String tt = ",";
            for (size_t i = 0; i < paramData.size(); ++i)
                tt += "sf";
            writeOSCString(reply, tt);

            for (auto& [pid, val] : paramData)
            {
                writeOSCString(reply, pid);
                writeOSCFloat(reply, val);
            }

            sendRaw(senderIP, replyPort, reply.data(), static_cast<int>(reply.size()));
            return;
        }

        if (address == "/aieq/get" && !args.empty())
        {
            auto paramID = std::get<juce::String>(args[0]);
            if (auto* p = apvtsRef.getRawParameterValue(paramID))
            {
                std::vector<uint8_t> reply;
                writeOSCString(reply, "/aieq/get");
                writeOSCString(reply, ",sf");
                writeOSCString(reply, paramID);
                writeOSCFloat(reply, p->load());
                sendRaw(senderIP, replyPort, reply.data(), static_cast<int>(reply.size()));
            }
            return;
        }

        if (address == "/aieq/set" && args.size() >= 2)
        {
            auto paramID = std::get<juce::String>(args[0]);
            float normVal = std::get<float>(args[1]);
            if (auto* param = apvtsRef.getParameter(paramID))
            {
                // Use MessageManager to call on message thread (safe for gesture)
                juce::MessageManager::callAsync([param, normVal]() {
                    param->beginChangeGesture();
                    param->setValueNotifyingHost(normVal);
                    param->endChangeGesture();
                });
            }
            return;
        }

        if (address == "/aieq/set/denorm" && args.size() >= 2)
        {
            auto paramID = std::get<juce::String>(args[0]);
            float realVal = std::get<float>(args[1]);
            if (auto* param = apvtsRef.getParameter(paramID))
            {
                float normVal = param->convertTo0to1(realVal);
                juce::MessageManager::callAsync([param, normVal]() {
                    param->beginChangeGesture();
                    param->setValueNotifyingHost(normVal);
                    param->endChangeGesture();
                });
            }
            return;
        }
    }

    // ── OSC encoding helpers ───────────────────────────────────────────
    static void writeOSCString(std::vector<uint8_t>& buf, const juce::String& s)
    {
        auto utf8 = s.toRawUTF8();
        size_t len = std::strlen(utf8) + 1; // include null
        buf.insert(buf.end(), utf8, utf8 + len);
        size_t pad = (4 - len % 4) % 4;
        for (size_t i = 0; i < pad; ++i)
            buf.push_back(0);
    }

    static void writeOSCFloat(std::vector<uint8_t>& buf, float f)
    {
        union { float fv; uint8_t b[4]; } u;
        u.fv = f;
        // big-endian
        buf.push_back(u.b[3]);
        buf.push_back(u.b[2]);
        buf.push_back(u.b[1]);
        buf.push_back(u.b[0]);
    }

    void sendOSCReply(const juce::String& host, int port, const juce::String& address,
                      std::initializer_list<float> values)
    {
        std::vector<uint8_t> buf;
        writeOSCString(buf, address);
        juce::String tt = ",";
        for (size_t i = 0; i < values.size(); ++i)
            tt += "f";
        writeOSCString(buf, tt);
        for (auto v : values)
            writeOSCFloat(buf, v);
        sendRaw(host, port, buf.data(), static_cast<int>(buf.size()));
    }

    void sendRaw(const juce::String& host, int port, const void* data, int size)
    {
        juce::DatagramSocket sender(false);
        sender.write(host, port, data, size);
    }

    juce::AudioProcessorValueTreeState& apvtsRef;
    int listenPort;
    std::unique_ptr<juce::DatagramSocket> socket;
    std::unique_ptr<std::thread> serverThread;
    std::atomic<bool> running { false };
};
