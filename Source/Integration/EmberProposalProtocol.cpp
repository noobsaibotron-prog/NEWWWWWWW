#include "EmberProposalProtocol.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sstream>

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>

namespace EmberProposal
{
namespace
{

constexpr std::uint64_t kMaxSafeInt = 9007199254740991ULL;

const char* kForbiddenVerbs[] = {
    "apply", "remote_apply", "set_parameter", "live_api_call", "live_api_set",
    "live_remote", "shell", "cmd", "popen", "exec", "system", "hid_inject",
    "synthetic_click", "create_track", "delete_track", "create_clip", "delete_clip",
    "create_device", "delete_device"
};

bool isDigit(char c) noexcept { return c >= '0' && c <= '9'; }

std::string hexEncode(const std::uint8_t* data, std::size_t n)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out(n * 2, '0');
    for (std::size_t i = 0; i < n; ++i)
    {
        out[i * 2] = kHex[(data[i] >> 4) & 0x0f];
        out[i * 2 + 1] = kHex[data[i] & 0x0f];
    }
    return out;
}

int hexNibble(char c) noexcept
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

struct JsonValue
{
    enum class Type { Null, Bool, Int, Number, String, Object, Array };
    Type type = Type::Null;
    bool boolValue = false;
    std::int64_t intValue = 0;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<std::pair<std::string, JsonValue>> object;
    std::vector<JsonValue> array;

    [[nodiscard]] const JsonValue* get(std::string_view key) const
    {
        if (type != Type::Object)
            return nullptr;
        for (const auto& kv : object)
            if (kv.first == key)
                return &kv.second;
        return nullptr;
    }

    [[nodiscard]] bool has(std::string_view key) const { return get(key) != nullptr; }

    [[nodiscard]] std::optional<std::string> asString() const
    {
        if (type == Type::String)
            return stringValue;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<bool> asBool() const
    {
        if (type == Type::Bool)
            return boolValue;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::int64_t> asInt() const
    {
        if (type == Type::Int)
            return intValue;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<double> asNumber() const
    {
        if (type == Type::Int)
            return static_cast<double>(intValue);
        if (type == Type::Number)
            return numberValue;
        return std::nullopt;
    }
};

struct ParseError
{
    ProtocolErrorCode code = ProtocolErrorCode::malformed_json;
};

class JsonParser
{
public:
    JsonParser(std::string_view text) : p(text.data()), end(text.data() + text.size()) {}

    JsonValue parse(ProtocolErrorCode& error)
    {
        try
        {
            skipWs();
            auto value = parseValue();
            skipWs();
            if (p != end)
                throw ParseError { ProtocolErrorCode::malformed_json };
            succeeded = true;
            return value;
        }
        catch (const ParseError& e)
        {
            error = e.code;
            succeeded = false;
            return {};
        }
    }

    bool succeeded = false;

private:
    const char* p;
    const char* end;
    int depth = 0;

    [[nodiscard]] bool atEnd() const noexcept { return p >= end; }

    void skipWs()
    {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'))
            ++p;
    }

    char peek() const
    {
        if (atEnd())
            throw ParseError { ProtocolErrorCode::malformed_json };
        return *p;
    }

    char take()
    {
        if (atEnd())
            throw ParseError { ProtocolErrorCode::malformed_json };
        return *p++;
    }

    void expect(char c)
    {
        if (take() != c)
            throw ParseError { ProtocolErrorCode::malformed_json };
    }

    JsonValue parseValue()
    {
        skipWs();
        const char c = peek();
        if (c == '{')
            return parseObject();
        if (c == '[')
            return parseArray();
        if (c == '"')
            return parseString();
        if (c == 't' || c == 'f')
            return parseLiteralBool();
        if (c == 'n')
            return parseNull();
        if (c == '-' || isDigit(c))
            return parseNumber();
        throw ParseError { ProtocolErrorCode::malformed_json };
    }

    JsonValue parseObject()
    {
        expect('{');
        ++depth;
        if (depth > kMaxNesting)
            throw ParseError { ProtocolErrorCode::nesting_exceeded };

        JsonValue value;
        value.type = JsonValue::Type::Object;
        skipWs();
        if (peek() == '}')
        {
            take();
            --depth;
            return value;
        }

        for (;;)
        {
            skipWs();
            if (peek() != '"')
                throw ParseError { ProtocolErrorCode::malformed_json };
            auto keyVal = parseString();
            skipWs();
            expect(':');
            auto child = parseValue();
            for (const auto& existing : value.object)
                if (existing.first == keyVal.stringValue)
                    throw ParseError { ProtocolErrorCode::duplicate_key };
            value.object.emplace_back(std::move(keyVal.stringValue), std::move(child));
            skipWs();
            const char c = take();
            if (c == '}')
                break;
            if (c != ',')
                throw ParseError { ProtocolErrorCode::malformed_json };
        }
        --depth;
        return value;
    }

    JsonValue parseArray()
    {
        expect('[');
        ++depth;
        if (depth > kMaxNesting)
            throw ParseError { ProtocolErrorCode::nesting_exceeded };

        JsonValue value;
        value.type = JsonValue::Type::Array;
        skipWs();
        if (peek() == ']')
        {
            take();
            --depth;
            return value;
        }
        for (;;)
        {
            value.array.push_back(parseValue());
            skipWs();
            const char c = take();
            if (c == ']')
                break;
            if (c != ',')
                throw ParseError { ProtocolErrorCode::malformed_json };
        }
        --depth;
        return value;
    }

    JsonValue parseString()
    {
        expect('"');
        std::string out;
        while (!atEnd())
        {
            unsigned char c = static_cast<unsigned char>(take());
            if (c == '"')
            {
                if (!isValidUtf8(out))
                    throw ParseError { ProtocolErrorCode::invalid_utf8 };
                JsonValue value;
                value.type = JsonValue::Type::String;
                value.stringValue = std::move(out);
                return value;
            }
            if (c == '\\')
            {
                if (atEnd())
                    throw ParseError { ProtocolErrorCode::malformed_json };
                const char e = take();
                switch (e)
                {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u':
                    {
                        unsigned code = 0;
                        for (int i = 0; i < 4; ++i)
                        {
                            const int n = hexNibble(take());
                            if (n < 0)
                                throw ParseError { ProtocolErrorCode::malformed_json };
                            code = (code << 4) | static_cast<unsigned>(n);
                        }
                        if (code <= 0x7f)
                            out.push_back(static_cast<char>(code));
                        else if (code <= 0x7ff)
                        {
                            out.push_back(static_cast<char>(0xc0 | (code >> 6)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
                        }
                        else
                        {
                            out.push_back(static_cast<char>(0xe0 | (code >> 12)));
                            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
                        }
                        break;
                    }
                    default:
                        throw ParseError { ProtocolErrorCode::malformed_json };
                }
                continue;
            }
            if (c < 0x20)
                throw ParseError { ProtocolErrorCode::malformed_json };
            out.push_back(static_cast<char>(c));
        }
        throw ParseError { ProtocolErrorCode::malformed_json };
    }

    JsonValue parseLiteralBool()
    {
        JsonValue value;
        value.type = JsonValue::Type::Bool;
        if (p + 4 <= end && std::memcmp(p, "true", 4) == 0)
        {
            p += 4;
            value.boolValue = true;
            return value;
        }
        if (p + 5 <= end && std::memcmp(p, "false", 5) == 0)
        {
            p += 5;
            value.boolValue = false;
            return value;
        }
        throw ParseError { ProtocolErrorCode::malformed_json };
    }

    JsonValue parseNull()
    {
        if (p + 4 <= end && std::memcmp(p, "null", 4) == 0)
        {
            p += 4;
            JsonValue value;
            value.type = JsonValue::Type::Null;
            return value;
        }
        throw ParseError { ProtocolErrorCode::malformed_json };
    }

    JsonValue parseNumber()
    {
        const char* start = p;
        if (peek() == '-')
            take();
        if (p + 8 <= end && (std::memcmp(p, "Infinity", 8) == 0 || std::memcmp(p, "NaN", 3) == 0))
            throw ParseError { ProtocolErrorCode::non_finite_number };
        if (atEnd() || !isDigit(peek()))
            throw ParseError { ProtocolErrorCode::malformed_json };
        if (peek() == '0')
        {
            take();
            if (!atEnd() && isDigit(peek()))
                throw ParseError { ProtocolErrorCode::malformed_json };
        }
        else
        {
            while (!atEnd() && isDigit(peek()))
                take();
        }
        bool isFloat = false;
        if (!atEnd() && peek() == '.')
        {
            isFloat = true;
            take();
            if (atEnd() || !isDigit(peek()))
                throw ParseError { ProtocolErrorCode::malformed_json };
            while (!atEnd() && isDigit(peek()))
                take();
        }
        if (!atEnd() && (peek() == 'e' || peek() == 'E'))
        {
            isFloat = true;
            take();
            if (!atEnd() && (peek() == '+' || peek() == '-'))
                take();
            if (atEnd() || !isDigit(peek()))
                throw ParseError { ProtocolErrorCode::malformed_json };
            while (!atEnd() && isDigit(peek()))
                take();
        }

        const std::string token(start, static_cast<std::size_t>(p - start));
        JsonValue value;
        if (!isFloat)
        {
            try
            {
                const auto parsed = std::stoll(token);
                if (parsed < 0 || static_cast<std::uint64_t>(parsed) > kMaxSafeInt)
                {
                    // Negative integers are legal JSON; seq/revision validators reject later.
                    if (parsed < 0 && parsed >= -static_cast<std::int64_t>(kMaxSafeInt))
                    {
                        value.type = JsonValue::Type::Int;
                        value.intValue = parsed;
                        return value;
                    }
                }
                value.type = JsonValue::Type::Int;
                value.intValue = parsed;
                return value;
            }
            catch (...)
            {
                throw ParseError { ProtocolErrorCode::malformed_json };
            }
        }

        char* endPtr = nullptr;
        const double d = std::strtod(token.c_str(), &endPtr);
        if (endPtr != token.c_str() + token.size() || !std::isfinite(d))
            throw ParseError { ProtocolErrorCode::non_finite_number };
        value.type = JsonValue::Type::Number;
        value.numberValue = d;
        return value;
    }
};

std::string jsonEscape(std::string_view s)
{
    std::string out;
    out.push_back('"');
    for (unsigned char c : s)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20)
                {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                }
                else
                    out.push_back(static_cast<char>(c));
                break;
        }
    }
    out.push_back('"');
    return out;
}

std::string jsonInt(std::uint64_t v) { return std::to_string(v); }
std::string jsonIntSigned(std::int64_t v) { return std::to_string(v); }
std::string jsonBool(bool v) { return v ? "true" : "false"; }

std::string jsonFloat(double v)
{
    if (!std::isfinite(v))
        return "null";
    std::ostringstream oss;
    oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
    oss.precision(std::numeric_limits<double>::max_digits10);
    oss << v;
    auto s = oss.str();
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos
        && s.find('E') == std::string::npos)
        s += ".0";
    return s;
}

std::string field(std::string_view key, std::string value)
{
    std::string out = jsonEscape(key);
    out += ':';
    out += std::move(value);
    return out;
}

std::string obj(std::vector<std::string> fields)
{
    std::string out = "{";
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        if (i > 0)
            out += ',';
        out += fields[i];
    }
    out += '}';
    return out;
}

std::string jsonNullOptString(const std::optional<std::string>& value)
{
    if (!value.has_value())
        return "null";
    return jsonEscape(*value);
}

AuditMetadata auditFromJson(const JsonValue* value, ProtocolErrorCode& error, bool& ok)
{
    AuditMetadata audit;
    if (value == nullptr || value->type != JsonValue::Type::Object)
    {
        ok = false;
        error = ProtocolErrorCode::missing_required;
        return audit;
    }
    static constexpr const char* kAllowed[] = { "byte_count", "sha256_hex", "latency_ms" };
    for (const auto& kv : value->object)
    {
        bool allowed = false;
        for (auto* a : kAllowed)
            if (kv.first == a)
                allowed = true;
        if (!allowed)
        {
            ok = false;
            error = ProtocolErrorCode::unknown_key;
            return audit;
        }
    }
    const auto* bytes = value->get("byte_count");
    const auto* sha = value->get("sha256_hex");
    if (bytes == nullptr || sha == nullptr)
    {
        ok = false;
        error = ProtocolErrorCode::missing_required;
        return audit;
    }
    auto b = bytes->asInt();
    auto s = sha->asString();
    if (!b || *b < 0 || *b > kMaxFrameBytes || !s || !isHex64(*s))
    {
        ok = false;
        error = ProtocolErrorCode::missing_required;
        return audit;
    }
    audit.byteCount = static_cast<int>(*b);
    audit.sha256Hex = *s;
    if (const auto* lat = value->get("latency_ms"))
    {
        if (lat->type == JsonValue::Type::Null)
            audit.latencyMs = std::nullopt;
        else
        {
            auto li = lat->asInt();
            if (!li || *li < 0 || *li > 86400000)
            {
                ok = false;
                error = ProtocolErrorCode::missing_required;
                return audit;
            }
            audit.latencyMs = static_cast<int>(*li);
        }
    }
    ok = true;
    return audit;
}

bool requireKeys(const JsonValue& obj,
                 const std::vector<std::string_view>& required,
                 const std::vector<std::string_view>& allowed,
                 ProtocolErrorCode& error)
{
    if (obj.type != JsonValue::Type::Object)
    {
        error = ProtocolErrorCode::missing_required;
        return false;
    }
    for (const auto& kv : obj.object)
    {
        bool ok = false;
        for (auto a : allowed)
            if (kv.first == a)
                ok = true;
        if (!ok)
        {
            error = ProtocolErrorCode::unknown_key;
            return false;
        }
    }
    for (auto r : required)
        if (!obj.has(r))
        {
            error = ProtocolErrorCode::missing_required;
            return false;
        }
    return true;
}

bool asUuid(const JsonValue& obj, std::string_view key, std::string& out, ProtocolErrorCode& error)
{
    const auto* v = obj.get(key);
    auto s = v ? v->asString() : std::nullopt;
    if (!s || !isUuidV4(*s))
    {
        error = ProtocolErrorCode::missing_required;
        return false;
    }
    out = *s;
    return true;
}

bool asHex64(const JsonValue& obj, std::string_view key, std::string& out, ProtocolErrorCode& error)
{
    const auto* v = obj.get(key);
    auto s = v ? v->asString() : std::nullopt;
    if (!s || !isHex64(*s))
    {
        error = ProtocolErrorCode::missing_required;
        return false;
    }
    out = *s;
    return true;
}

bool asNonNegInt(const JsonValue& obj, std::string_view key, std::uint64_t& out, ProtocolErrorCode& error)
{
    const auto* v = obj.get(key);
    auto i = v ? v->asInt() : std::nullopt;
    if (!i || *i < 0 || static_cast<std::uint64_t>(*i) > kMaxSafeInt)
    {
        error = ProtocolErrorCode::missing_required;
        return false;
    }
    out = static_cast<std::uint64_t>(*i);
    return true;
}

bool asBoolField(const JsonValue& obj, std::string_view key, bool& out, ProtocolErrorCode& error)
{
    const auto* v = obj.get(key);
    auto b = v ? v->asBool() : std::nullopt;
    if (!b)
    {
        error = ProtocolErrorCode::missing_required;
        return false;
    }
    out = *b;
    return true;
}

bool asStringField(const JsonValue& obj, std::string_view key, std::string& out, ProtocolErrorCode& error)
{
    const auto* v = obj.get(key);
    auto s = v ? v->asString() : std::nullopt;
    if (!s)
    {
        error = ProtocolErrorCode::missing_required;
        return false;
    }
    out = *s;
    return true;
}

bool parseLiveIdentity(const JsonValue& obj, LiveIdentity& out, ProtocolErrorCode& error)
{
    const auto* v = obj.get("live_identity");
    if (v == nullptr || v->type != JsonValue::Type::Object)
    {
        error = ProtocolErrorCode::missing_required;
        return false;
    }
    static const std::vector<std::string_view> req {
        "track_id", "device_id", "canonical_path", "identity_complete", "identity_best_effort"
    };
    if (!requireKeys(*v, req, req, error))
        return false;

    auto takeOpt = [&](std::string_view key, std::optional<std::string>& dest) {
        const auto* f = v->get(key);
        if (f == nullptr)
            return false;
        if (f->type == JsonValue::Type::Null)
        {
            dest = std::nullopt;
            return true;
        }
        auto s = f->asString();
        if (!s || s->empty())
            return false;
        dest = *s;
        return true;
    };
    if (!takeOpt("track_id", out.trackId) || !takeOpt("device_id", out.deviceId)
        || !takeOpt("canonical_path", out.canonicalPath))
    {
        error = ProtocolErrorCode::missing_required;
        return false;
    }
    if (!asBoolField(*v, "identity_complete", out.identityComplete, error))
        return false;
    if (!asBoolField(*v, "identity_best_effort", out.identityBestEffort, error))
        return false;
    return true;
}

bool parseOutcomeBase(const JsonValue& payload, OutcomeBase& out, ProtocolErrorCode& error, bool requireRequestId)
{
    static const std::vector<std::string_view> allowed {
        "request_id", "pair_binding_id", "target_runtime_instance_id", "reason_code",
        "control_revision", "projection_base_epoch", "audition_context_epoch", "audit"
    };
    std::vector<std::string_view> required {
        "pair_binding_id", "target_runtime_instance_id", "reason_code",
        "control_revision", "projection_base_epoch", "audition_context_epoch", "audit"
    };
    if (requireRequestId)
        required.insert(required.begin(), "request_id");
    // extra keys checked by caller for typed payloads
    if (requireRequestId && !asUuid(payload, "request_id", out.requestId, error))
        return false;
    if (!requireRequestId && payload.has("request_id"))
    {
        if (!asUuid(payload, "request_id", out.requestId, error))
            return false;
    }
    if (!asUuid(payload, "pair_binding_id", out.pairBindingId, error))
        return false;
    if (!asUuid(payload, "target_runtime_instance_id", out.targetRuntimeInstanceId, error))
        return false;
    if (!asNonNegInt(payload, "control_revision", out.controlRevision, error))
        return false;
    if (!asNonNegInt(payload, "projection_base_epoch", out.projectionBaseEpoch, error))
        return false;
    if (!asNonNegInt(payload, "audition_context_epoch", out.auditionContextEpoch, error))
        return false;
    bool auditOk = false;
    out.audit = auditFromJson(payload.get("audit"), error, auditOk);
    return auditOk;
}

std::string serializeAudit(const AuditMetadata& audit)
{
    std::vector<std::string> fields {
        field("byte_count", jsonInt(static_cast<std::uint64_t>(audit.byteCount))),
        field("sha256_hex", jsonEscape(audit.sha256Hex))
    };
    if (audit.latencyMs.has_value())
        fields.push_back(field("latency_ms", jsonIntSigned(*audit.latencyMs)));
    return obj(std::move(fields));
}

std::string serializeLiveIdentity(const LiveIdentity& id)
{
    return obj({
        field("track_id", jsonNullOptString(id.trackId)),
        field("device_id", jsonNullOptString(id.deviceId)),
        field("canonical_path", jsonNullOptString(id.canonicalPath)),
        field("identity_complete", jsonBool(id.identityComplete)),
        field("identity_best_effort", jsonBool(id.identityBestEffort))
    });
}

std::string payloadJson(const WireMessage& message)
{
    switch (message.type)
    {
        case MessageType::handshake:
            if (message.handshakePhase == HandshakePhase::hello)
            {
                std::string caps = "[";
                for (std::size_t i = 0; i < message.hello.capabilities.size(); ++i)
                {
                    if (i > 0) caps += ',';
                    caps += jsonEscape(message.hello.capabilities[i]);
                }
                caps += ']';
                return obj({
                    field("phase", jsonEscape("hello")),
                    field("client_nonce_hex", jsonEscape(message.hello.clientNonceHex)),
                    field("protocol_major", jsonIntSigned(message.hello.protocolMajor)),
                    field("protocol_minor", jsonIntSigned(message.hello.protocolMinor)),
                    field("capabilities", std::move(caps))
                });
            }
            if (message.handshakePhase == HandshakePhase::ack)
            {
                return obj({
                    field("phase", jsonEscape("ack")),
                    field("client_nonce_hex", jsonEscape(message.ack.clientNonceHex)),
                    field("server_nonce_hex", jsonEscape(message.ack.serverNonceHex)),
                    field("hmac_sha256_hex", jsonEscape(message.ack.hmacSha256Hex)),
                    field("session_uuid", jsonEscape(message.ack.sessionUuid)),
                    field("server_epoch", jsonInt(message.ack.serverEpoch)),
                    field("protocol_major", jsonIntSigned(message.ack.protocolMajor)),
                    field("protocol_minor", jsonIntSigned(message.ack.protocolMinor))
                });
            }
            return obj({
                field("phase", jsonEscape("confirm")),
                field("client_nonce_hex", jsonEscape(message.confirm.clientNonceHex)),
                field("server_nonce_hex", jsonEscape(message.confirm.serverNonceHex)),
                field("hmac_sha256_hex", jsonEscape(message.confirm.hmacSha256Hex))
            });
        case MessageType::pair_offer:
            return obj({
                field("runtime_instance_id", jsonEscape(message.pairOffer.runtimeInstanceId)),
                field("human_code", jsonEscape(message.pairOffer.humanCode)),
                field("control_revision", jsonInt(message.pairOffer.controlRevision)),
                field("projection_base_epoch", jsonInt(message.pairOffer.projectionBaseEpoch)),
                field("audition_context_epoch", jsonInt(message.pairOffer.auditionContextEpoch)),
                field("expires_at_monotonic_ns", jsonInt(message.pairOffer.expiresAtMonotonicNs)),
                field("editor_open", jsonBool(message.pairOffer.editorOpen))
            });
        case MessageType::pair_confirm:
            return obj({
                field("pair_binding_id", jsonEscape(message.pairConfirm.pairBindingId)),
                field("runtime_instance_id", jsonEscape(message.pairConfirm.runtimeInstanceId)),
                field("human_code", jsonEscape(message.pairConfirm.humanCode)),
                field("live_identity", serializeLiveIdentity(message.pairConfirm.liveIdentity)),
                field("control_revision", jsonInt(message.pairConfirm.controlRevision)),
                field("confirmed_at_monotonic_ns", jsonInt(message.pairConfirm.confirmedAtMonotonicNs))
            });
        case MessageType::unpair:
            return obj({
                field("pair_binding_id", jsonEscape(message.unpair.pairBindingId)),
                field("runtime_instance_id", jsonEscape(message.unpair.runtimeInstanceId)),
                field("reason_code", jsonEscape("unpaired")),
                field("unpaired_cause", jsonEscape(toString(message.unpair.unpairedCause)))
            });
        case MessageType::stage_semantic_request:
            return obj({
                field("target_runtime_instance_id", jsonEscape(message.stage.targetRuntimeInstanceId)),
                field("pair_binding_id", jsonEscape(message.stage.pairBindingId)),
                field("request_id", jsonEscape(message.stage.requestId)),
                field("expected_control_revision", jsonInt(message.stage.expectedControlRevision)),
                field("expected_projection_base_epoch", jsonInt(message.stage.expectedProjectionBaseEpoch)),
                field("expected_audition_context_epoch", jsonInt(message.stage.expectedAuditionContextEpoch)),
                field("phrase", jsonEscape(message.stage.phrase)),
                field("intensity", jsonFloat(message.stage.intensity)),
                field("expires_at_monotonic_ns", jsonInt(message.stage.expiresAtMonotonicNs)),
                field("request_hash", jsonEscape(message.stage.requestHash))
            });
        case MessageType::plan_staged:
            return obj({
                field("request_id", jsonEscape(message.planStaged.requestId)),
                field("pair_binding_id", jsonEscape(message.planStaged.pairBindingId)),
                field("target_runtime_instance_id", jsonEscape(message.planStaged.targetRuntimeInstanceId)),
                field("reason_code", jsonEscape("plan_staged")),
                field("control_revision", jsonInt(message.planStaged.controlRevision)),
                field("projection_base_epoch", jsonInt(message.planStaged.projectionBaseEpoch)),
                field("audition_context_epoch", jsonInt(message.planStaged.auditionContextEpoch)),
                field("plan_hash", jsonEscape(message.planStaged.planHash)),
                field("summary", jsonEscape(message.planStaged.summary)),
                field("audit", serializeAudit(message.planStaged.audit))
            });
        case MessageType::unpaired:
            return obj({
                field("request_id", jsonEscape(message.unpaired.requestId)),
                field("pair_binding_id", jsonEscape(message.unpaired.pairBindingId)),
                field("target_runtime_instance_id", jsonEscape(message.unpaired.targetRuntimeInstanceId)),
                field("reason_code", jsonEscape("unpaired")),
                field("unpaired_cause", jsonEscape(toString(message.unpaired.unpairedCause))),
                field("control_revision", jsonInt(message.unpaired.controlRevision)),
                field("projection_base_epoch", jsonInt(message.unpaired.projectionBaseEpoch)),
                field("audition_context_epoch", jsonInt(message.unpaired.auditionContextEpoch)),
                field("audit", serializeAudit(message.unpaired.audit))
            });
        case MessageType::protocol_error:
        {
            std::vector<std::string> fields;
            if (message.protocolError.requestId)
                fields.push_back(field("request_id", jsonEscape(*message.protocolError.requestId)));
            if (message.protocolError.pairBindingId)
                fields.push_back(field("pair_binding_id", jsonEscape(*message.protocolError.pairBindingId)));
            if (message.protocolError.targetRuntimeInstanceId)
                fields.push_back(field("target_runtime_instance_id",
                                       jsonEscape(*message.protocolError.targetRuntimeInstanceId)));
            fields.push_back(field("reason_code", jsonEscape("protocol_error")));
            fields.push_back(field("protocol_error_code",
                                   jsonEscape(toString(message.protocolError.protocolErrorCode))));
            fields.push_back(field("close_connection", jsonBool(message.protocolError.closeConnection)));
            fields.push_back(field("audit", serializeAudit(message.protocolError.audit)));
            return obj(std::move(fields));
        }
        case MessageType::user_applied:
            return obj({
                field("request_id", jsonEscape(message.userApplied.requestId)),
                field("pair_binding_id", jsonEscape(message.userApplied.pairBindingId)),
                field("target_runtime_instance_id", jsonEscape(message.userApplied.targetRuntimeInstanceId)),
                field("reason_code", jsonEscape("user_applied")),
                field("notification_only", jsonBool(true)),
                field("plan_hash", jsonEscape(message.userApplied.planHash)),
                field("control_revision", jsonInt(message.userApplied.controlRevision)),
                field("projection_base_epoch", jsonInt(message.userApplied.projectionBaseEpoch)),
                field("audition_context_epoch", jsonInt(message.userApplied.auditionContextEpoch)),
                field("audit", serializeAudit(message.userApplied.audit))
            });
        case MessageType::user_rejected:
            return obj({
                field("request_id", jsonEscape(message.userRejected.requestId)),
                field("pair_binding_id", jsonEscape(message.userRejected.pairBindingId)),
                field("target_runtime_instance_id", jsonEscape(message.userRejected.targetRuntimeInstanceId)),
                field("reason_code", jsonEscape("user_rejected")),
                field("notification_only", jsonBool(true)),
                field("control_revision", jsonInt(message.userRejected.controlRevision)),
                field("projection_base_epoch", jsonInt(message.userRejected.projectionBaseEpoch)),
                field("audition_context_epoch", jsonInt(message.userRejected.auditionContextEpoch)),
                field("audit", serializeAudit(message.userRejected.audit))
            });
        default:
        {
            const auto reason = reasonFromMessageType(message.type);
            return obj({
                field("request_id", jsonEscape(message.refusal.requestId)),
                field("pair_binding_id", jsonEscape(message.refusal.pairBindingId)),
                field("target_runtime_instance_id", jsonEscape(message.refusal.targetRuntimeInstanceId)),
                field("reason_code", jsonEscape(toString(reason))),
                field("control_revision", jsonInt(message.refusal.controlRevision)),
                field("projection_base_epoch", jsonInt(message.refusal.projectionBaseEpoch)),
                field("audition_context_epoch", jsonInt(message.refusal.auditionContextEpoch)),
                field("audit", serializeAudit(message.refusal.audit))
            });
        }
    }
}

ProtocolErrorCode payloadError(MessageType type, const JsonValue& payload, WireMessage& out)
{
    ProtocolErrorCode error = ProtocolErrorCode::missing_required;

    if (type == MessageType::handshake)
    {
        const auto* phaseV = payload.get("phase");
        auto phase = phaseV ? phaseV->asString() : std::nullopt;
        if (!phase)
            return ProtocolErrorCode::missing_required;
        if (*phase == "hello")
        {
            static const std::vector<std::string_view> keys {
                "phase", "client_nonce_hex", "protocol_major", "protocol_minor", "capabilities"
            };
            if (!requireKeys(payload, keys, keys, error))
                return error;
            if (!asHex64(payload, "client_nonce_hex", out.hello.clientNonceHex, error))
                return error;
            auto major = payload.get("protocol_major")->asInt();
            auto minor = payload.get("protocol_minor")->asInt();
            if (!major || !minor)
                return ProtocolErrorCode::missing_required;
            out.hello.protocolMajor = static_cast<int>(*major);
            out.hello.protocolMinor = static_cast<int>(*minor);
            const auto* caps = payload.get("capabilities");
            if (caps == nullptr || caps->type != JsonValue::Type::Array)
                return ProtocolErrorCode::missing_required;
            out.hello.capabilities.clear();
            for (const auto& item : caps->array)
            {
                auto s = item.asString();
                if (!s)
                    return ProtocolErrorCode::missing_required;
                out.hello.capabilities.push_back(*s);
            }
            if (out.hello.capabilities != std::vector<std::string> { "proposal" })
                return ProtocolErrorCode::unknown_write_capability;
            if (out.hello.protocolMajor != kProtocolMajor)
                return ProtocolErrorCode::incompatible_major;
            out.handshakePhase = HandshakePhase::hello;
            return ProtocolErrorCode::malformed_json; // sentinel unused
        }
        if (*phase == "ack")
        {
            static const std::vector<std::string_view> keys {
                "phase", "client_nonce_hex", "server_nonce_hex", "hmac_sha256_hex",
                "session_uuid", "server_epoch", "protocol_major", "protocol_minor"
            };
            if (!requireKeys(payload, keys, keys, error))
                return error;
            if (!asHex64(payload, "client_nonce_hex", out.ack.clientNonceHex, error)
                || !asHex64(payload, "server_nonce_hex", out.ack.serverNonceHex, error)
                || !asHex64(payload, "hmac_sha256_hex", out.ack.hmacSha256Hex, error)
                || !asUuid(payload, "session_uuid", out.ack.sessionUuid, error)
                || !asNonNegInt(payload, "server_epoch", out.ack.serverEpoch, error))
                return error;
            auto major = payload.get("protocol_major")->asInt();
            auto minor = payload.get("protocol_minor")->asInt();
            if (!major || !minor)
                return ProtocolErrorCode::missing_required;
            out.ack.protocolMajor = static_cast<int>(*major);
            out.ack.protocolMinor = static_cast<int>(*minor);
            out.handshakePhase = HandshakePhase::ack;
            return ProtocolErrorCode::malformed_json;
        }
        if (*phase == "confirm")
        {
            static const std::vector<std::string_view> keys {
                "phase", "client_nonce_hex", "server_nonce_hex", "hmac_sha256_hex"
            };
            if (!requireKeys(payload, keys, keys, error))
                return error;
            if (!asHex64(payload, "client_nonce_hex", out.confirm.clientNonceHex, error)
                || !asHex64(payload, "server_nonce_hex", out.confirm.serverNonceHex, error)
                || !asHex64(payload, "hmac_sha256_hex", out.confirm.hmacSha256Hex, error))
                return error;
            out.handshakePhase = HandshakePhase::confirm;
            return ProtocolErrorCode::malformed_json;
        }
        return ProtocolErrorCode::missing_required;
    }

    auto loadRefusal = [&](bool requireRequest) -> ProtocolErrorCode {
        static const std::vector<std::string_view> allowed {
            "request_id", "pair_binding_id", "target_runtime_instance_id", "reason_code",
            "control_revision", "projection_base_epoch", "audition_context_epoch", "audit"
        };
        std::vector<std::string_view> required = allowed;
        if (!requireRequest)
            required.erase(std::remove(required.begin(), required.end(), "request_id"), required.end());
        if (!requireKeys(payload, required, allowed, error))
            return error;
        std::string reason;
        if (!asStringField(payload, "reason_code", reason, error))
            return error;
        if (reason != toString(reasonFromMessageType(type)))
            return ProtocolErrorCode::missing_required;
        if (!parseOutcomeBase(payload, out.refusal, error, requireRequest))
            return error;
        return ProtocolErrorCode::malformed_json;
    };

    switch (type)
    {
        case MessageType::pair_offer:
        {
            static const std::vector<std::string_view> keys {
                "runtime_instance_id", "human_code", "control_revision", "projection_base_epoch",
                "audition_context_epoch", "expires_at_monotonic_ns", "editor_open"
            };
            if (!requireKeys(payload, keys, keys, error))
                return error;
            if (!asUuid(payload, "runtime_instance_id", out.pairOffer.runtimeInstanceId, error))
                return error;
            if (!asStringField(payload, "human_code", out.pairOffer.humanCode, error)
                || !isHumanCode(out.pairOffer.humanCode))
                return ProtocolErrorCode::missing_required;
            if (!asNonNegInt(payload, "control_revision", out.pairOffer.controlRevision, error)
                || !asNonNegInt(payload, "projection_base_epoch", out.pairOffer.projectionBaseEpoch, error)
                || !asNonNegInt(payload, "audition_context_epoch", out.pairOffer.auditionContextEpoch, error)
                || !asNonNegInt(payload, "expires_at_monotonic_ns", out.pairOffer.expiresAtMonotonicNs, error)
                || !asBoolField(payload, "editor_open", out.pairOffer.editorOpen, error))
                return error;
            return ProtocolErrorCode::malformed_json;
        }
        case MessageType::pair_confirm:
        {
            static const std::vector<std::string_view> keys {
                "pair_binding_id", "runtime_instance_id", "human_code", "live_identity",
                "control_revision", "confirmed_at_monotonic_ns"
            };
            if (!requireKeys(payload, keys, keys, error))
                return error;
            if (!asUuid(payload, "pair_binding_id", out.pairConfirm.pairBindingId, error)
                || !asUuid(payload, "runtime_instance_id", out.pairConfirm.runtimeInstanceId, error)
                || !asStringField(payload, "human_code", out.pairConfirm.humanCode, error)
                || !isHumanCode(out.pairConfirm.humanCode)
                || !parseLiveIdentity(payload, out.pairConfirm.liveIdentity, error)
                || !asNonNegInt(payload, "control_revision", out.pairConfirm.controlRevision, error)
                || !asNonNegInt(payload, "confirmed_at_monotonic_ns", out.pairConfirm.confirmedAtMonotonicNs, error))
                return error;
            return ProtocolErrorCode::malformed_json;
        }
        case MessageType::unpair:
        {
            static const std::vector<std::string_view> keys {
                "pair_binding_id", "runtime_instance_id", "reason_code", "unpaired_cause"
            };
            if (!requireKeys(payload, keys, keys, error))
                return error;
            if (!asUuid(payload, "pair_binding_id", out.unpair.pairBindingId, error)
                || !asUuid(payload, "runtime_instance_id", out.unpair.runtimeInstanceId, error))
                return error;
            std::string reason, cause;
            if (!asStringField(payload, "reason_code", reason, error) || reason != "unpaired")
                return ProtocolErrorCode::missing_required;
            if (!asStringField(payload, "unpaired_cause", cause, error))
                return error;
            auto parsedCause = unpairedCauseFromString(cause);
            if (!parsedCause)
                return ProtocolErrorCode::missing_required;
            out.unpair.unpairedCause = *parsedCause;
            return ProtocolErrorCode::malformed_json;
        }
        case MessageType::stage_semantic_request:
        {
            static const std::vector<std::string_view> keys {
                "target_runtime_instance_id", "pair_binding_id", "request_id",
                "expected_control_revision", "expected_projection_base_epoch",
                "expected_audition_context_epoch", "phrase", "intensity",
                "expires_at_monotonic_ns", "request_hash"
            };
            if (!requireKeys(payload, keys, keys, error))
                return error;
            if (!asUuid(payload, "target_runtime_instance_id", out.stage.targetRuntimeInstanceId, error)
                || !asUuid(payload, "pair_binding_id", out.stage.pairBindingId, error)
                || !asUuid(payload, "request_id", out.stage.requestId, error)
                || !asNonNegInt(payload, "expected_control_revision", out.stage.expectedControlRevision, error)
                || !asNonNegInt(payload, "expected_projection_base_epoch", out.stage.expectedProjectionBaseEpoch, error)
                || !asNonNegInt(payload, "expected_audition_context_epoch", out.stage.expectedAuditionContextEpoch, error)
                || !asStringField(payload, "phrase", out.stage.phrase, error)
                || !asNonNegInt(payload, "expires_at_monotonic_ns", out.stage.expiresAtMonotonicNs, error)
                || !asHex64(payload, "request_hash", out.stage.requestHash, error))
                return error;
            const auto* intensity = payload.get("intensity");
            if (intensity == nullptr || intensity->type == JsonValue::Type::Bool)
                return ProtocolErrorCode::missing_required;
            auto num = intensity->asNumber();
            if (!num)
                return ProtocolErrorCode::missing_required;
            if (!std::isfinite(*num))
                return ProtocolErrorCode::non_finite_number;
            if (*num < 0.0 || *num > 1.0)
                return ProtocolErrorCode::missing_required;
            out.stage.intensity = *num;
            const auto phraseBytes = out.stage.phrase.size();
            if (phraseBytes < 1 || phraseBytes > static_cast<std::size_t>(kPhraseMaxBytes)
                || !isValidUtf8(out.stage.phrase))
                return ProtocolErrorCode::oversized_frame;
            return ProtocolErrorCode::malformed_json;
        }
        case MessageType::plan_staged:
        {
            static const std::vector<std::string_view> keys {
                "request_id", "pair_binding_id", "target_runtime_instance_id", "reason_code",
                "control_revision", "projection_base_epoch", "audition_context_epoch",
                "plan_hash", "summary", "audit"
            };
            if (!requireKeys(payload, keys, keys, error))
                return error;
            if (!parseOutcomeBase(payload, out.planStaged, error, true))
                return error;
            std::string reason;
            if (!asStringField(payload, "reason_code", reason, error) || reason != "plan_staged")
                return ProtocolErrorCode::missing_required;
            if (!asHex64(payload, "plan_hash", out.planStaged.planHash, error)
                || !asStringField(payload, "summary", out.planStaged.summary, error)
                || out.planStaged.summary.empty()
                || out.planStaged.summary.size() > static_cast<std::size_t>(kSummaryMaxChars))
                return ProtocolErrorCode::missing_required;
            return ProtocolErrorCode::malformed_json;
        }
        case MessageType::unpaired:
        {
            static const std::vector<std::string_view> allowed {
                "request_id", "pair_binding_id", "target_runtime_instance_id", "reason_code",
                "unpaired_cause", "control_revision", "projection_base_epoch",
                "audition_context_epoch", "audit"
            };
            static const std::vector<std::string_view> required {
                "pair_binding_id", "target_runtime_instance_id", "reason_code",
                "unpaired_cause", "control_revision", "projection_base_epoch",
                "audition_context_epoch", "audit"
            };
            if (!requireKeys(payload, required, allowed, error))
                return error;
            if (!parseOutcomeBase(payload, out.unpaired, error, false))
                return error;
            std::string reason, cause;
            if (!asStringField(payload, "reason_code", reason, error) || reason != "unpaired")
                return ProtocolErrorCode::missing_required;
            if (!asStringField(payload, "unpaired_cause", cause, error))
                return error;
            auto parsedCause = unpairedCauseFromString(cause);
            if (!parsedCause)
                return ProtocolErrorCode::missing_required;
            out.unpaired.unpairedCause = *parsedCause;
            return ProtocolErrorCode::malformed_json;
        }
        case MessageType::protocol_error:
        {
            static const std::vector<std::string_view> allowed {
                "request_id", "pair_binding_id", "target_runtime_instance_id", "reason_code",
                "protocol_error_code", "close_connection", "audit"
            };
            static const std::vector<std::string_view> required {
                "reason_code", "protocol_error_code", "close_connection", "audit"
            };
            if (!requireKeys(payload, required, allowed, error))
                return error;
            std::string reason, code;
            if (!asStringField(payload, "reason_code", reason, error) || reason != "protocol_error")
                return ProtocolErrorCode::missing_required;
            if (!asStringField(payload, "protocol_error_code", code, error))
                return error;
            auto parsed = protocolErrorFromString(code);
            if (!parsed)
                return ProtocolErrorCode::missing_required;
            out.protocolError.protocolErrorCode = *parsed;
            if (!asBoolField(payload, "close_connection", out.protocolError.closeConnection, error))
                return error;
            if (payload.has("request_id"))
            {
                std::string id;
                if (!asUuid(payload, "request_id", id, error))
                    return error;
                out.protocolError.requestId = id;
            }
            if (payload.has("pair_binding_id"))
            {
                std::string id;
                if (!asUuid(payload, "pair_binding_id", id, error))
                    return error;
                out.protocolError.pairBindingId = id;
            }
            if (payload.has("target_runtime_instance_id"))
            {
                std::string id;
                if (!asUuid(payload, "target_runtime_instance_id", id, error))
                    return error;
                out.protocolError.targetRuntimeInstanceId = id;
            }
            bool auditOk = false;
            out.protocolError.audit = auditFromJson(payload.get("audit"), error, auditOk);
            if (!auditOk)
                return error;
            return ProtocolErrorCode::malformed_json;
        }
        case MessageType::user_applied:
        {
            static const std::vector<std::string_view> keys {
                "request_id", "pair_binding_id", "target_runtime_instance_id", "reason_code",
                "notification_only", "plan_hash", "control_revision", "projection_base_epoch",
                "audition_context_epoch", "audit"
            };
            if (!requireKeys(payload, keys, keys, error))
                return error;
            if (!parseOutcomeBase(payload, out.userApplied, error, true))
                return error;
            std::string reason;
            if (!asStringField(payload, "reason_code", reason, error) || reason != "user_applied")
                return ProtocolErrorCode::missing_required;
            bool notification = false;
            if (!asBoolField(payload, "notification_only", notification, error) || !notification)
                return ProtocolErrorCode::missing_required;
            if (!asHex64(payload, "plan_hash", out.userApplied.planHash, error))
                return error;
            return ProtocolErrorCode::malformed_json;
        }
        case MessageType::user_rejected:
        {
            static const std::vector<std::string_view> keys {
                "request_id", "pair_binding_id", "target_runtime_instance_id", "reason_code",
                "notification_only", "control_revision", "projection_base_epoch",
                "audition_context_epoch", "audit"
            };
            if (!requireKeys(payload, keys, keys, error))
                return error;
            if (!parseOutcomeBase(payload, out.userRejected, error, true))
                return error;
            std::string reason;
            if (!asStringField(payload, "reason_code", reason, error) || reason != "user_rejected")
                return ProtocolErrorCode::missing_required;
            bool notification = false;
            if (!asBoolField(payload, "notification_only", notification, error) || !notification)
                return ProtocolErrorCode::missing_required;
            return ProtocolErrorCode::malformed_json;
        }
        case MessageType::unknown_intent:
        case MessageType::contradictory_intent:
        case MessageType::no_safe_move:
        case MessageType::internal_error:
        case MessageType::stale_revision:
        case MessageType::target_ui_unavailable:
        case MessageType::expired:
            return loadRefusal(true);
        default:
            return ProtocolErrorCode::forbidden_verb;
    }
}

} // namespace

const char* toString(MessageType type) noexcept
{
    switch (type)
    {
        case MessageType::handshake: return "handshake";
        case MessageType::pair_offer: return "pair_offer";
        case MessageType::pair_confirm: return "pair_confirm";
        case MessageType::unpair: return "unpair";
        case MessageType::stage_semantic_request: return "stage_semantic_request";
        case MessageType::plan_staged: return "plan_staged";
        case MessageType::unknown_intent: return "unknown_intent";
        case MessageType::contradictory_intent: return "contradictory_intent";
        case MessageType::no_safe_move: return "no_safe_move";
        case MessageType::internal_error: return "internal_error";
        case MessageType::stale_revision: return "stale_revision";
        case MessageType::target_ui_unavailable: return "target_ui_unavailable";
        case MessageType::unpaired: return "unpaired";
        case MessageType::expired: return "expired";
        case MessageType::protocol_error: return "protocol_error";
        case MessageType::user_applied: return "user_applied";
        case MessageType::user_rejected: return "user_rejected";
    }
    return "protocol_error";
}

const char* toString(ReasonCode code) noexcept
{
    return toString(messageTypeFromReason(code));
}

const char* toString(ProtocolErrorCode code) noexcept
{
    switch (code)
    {
        case ProtocolErrorCode::incompatible_major: return "incompatible_major";
        case ProtocolErrorCode::unknown_write_capability: return "unknown_write_capability";
        case ProtocolErrorCode::replay: return "replay";
        case ProtocolErrorCode::reorder: return "reorder";
        case ProtocolErrorCode::oversized_frame: return "oversized_frame";
        case ProtocolErrorCode::duplicate_key: return "duplicate_key";
        case ProtocolErrorCode::non_finite_number: return "non_finite_number";
        case ProtocolErrorCode::unknown_key: return "unknown_key";
        case ProtocolErrorCode::invalid_utf8: return "invalid_utf8";
        case ProtocolErrorCode::hmac_mismatch: return "hmac_mismatch";
        case ProtocolErrorCode::nonce_reuse: return "nonce_reuse";
        case ProtocolErrorCode::nesting_exceeded: return "nesting_exceeded";
        case ProtocolErrorCode::malformed_json: return "malformed_json";
        case ProtocolErrorCode::missing_required: return "missing_required";
        case ProtocolErrorCode::unauthenticated: return "unauthenticated";
        case ProtocolErrorCode::expired_secret: return "expired_secret";
        case ProtocolErrorCode::stale_rendezvous: return "stale_rendezvous";
        case ProtocolErrorCode::forbidden_verb: return "forbidden_verb";
        case ProtocolErrorCode::seq_gap: return "seq_gap";
    }
    return "malformed_json";
}

const char* toString(UnpairedCause cause) noexcept
{
    switch (cause)
    {
        case UnpairedCause::user_unpair: return "user_unpair";
        case UnpairedCause::duplicate_instance: return "duplicate_instance";
        case UnpairedCause::duplicate_human_code: return "duplicate_human_code";
        case UnpairedCause::stale_live_identity: return "stale_live_identity";
        case UnpairedCause::disconnect: return "disconnect";
        case UnpairedCause::reload: return "reload";
        case UnpairedCause::delete_recreate: return "delete_recreate";
        case UnpairedCause::epoch_change: return "epoch_change";
        case UnpairedCause::ambiguity: return "ambiguity";
        case UnpairedCause::kill_switch: return "kill_switch";
        case UnpairedCause::ember_link_disabled: return "ember_link_disabled";
        case UnpairedCause::editor_closed: return "editor_closed";
    }
    return "disconnect";
}

std::optional<MessageType> messageTypeFromString(std::string_view text)
{
    for (int i = 0; i <= static_cast<int>(MessageType::user_rejected); ++i)
    {
        const auto type = static_cast<MessageType>(i);
        if (text == toString(type))
            return type;
    }
    return std::nullopt;
}

std::optional<ProtocolErrorCode> protocolErrorFromString(std::string_view text)
{
    static const ProtocolErrorCode kAll[] = {
        ProtocolErrorCode::incompatible_major, ProtocolErrorCode::unknown_write_capability,
        ProtocolErrorCode::replay, ProtocolErrorCode::reorder, ProtocolErrorCode::oversized_frame,
        ProtocolErrorCode::duplicate_key, ProtocolErrorCode::non_finite_number,
        ProtocolErrorCode::unknown_key, ProtocolErrorCode::invalid_utf8, ProtocolErrorCode::hmac_mismatch,
        ProtocolErrorCode::nonce_reuse, ProtocolErrorCode::nesting_exceeded,
        ProtocolErrorCode::malformed_json, ProtocolErrorCode::missing_required,
        ProtocolErrorCode::unauthenticated, ProtocolErrorCode::expired_secret,
        ProtocolErrorCode::stale_rendezvous, ProtocolErrorCode::forbidden_verb,
        ProtocolErrorCode::seq_gap
    };
    for (auto c : kAll)
        if (text == toString(c))
            return c;
    return std::nullopt;
}

std::optional<UnpairedCause> unpairedCauseFromString(std::string_view text)
{
    for (int i = 0; i <= static_cast<int>(UnpairedCause::editor_closed); ++i)
    {
        const auto cause = static_cast<UnpairedCause>(i);
        if (text == toString(cause))
            return cause;
    }
    return std::nullopt;
}

ReasonCode reasonFromMessageType(MessageType type) noexcept
{
    switch (type)
    {
        case MessageType::plan_staged: return ReasonCode::plan_staged;
        case MessageType::unknown_intent: return ReasonCode::unknown_intent;
        case MessageType::contradictory_intent: return ReasonCode::contradictory_intent;
        case MessageType::no_safe_move: return ReasonCode::no_safe_move;
        case MessageType::internal_error: return ReasonCode::internal_error;
        case MessageType::stale_revision: return ReasonCode::stale_revision;
        case MessageType::target_ui_unavailable: return ReasonCode::target_ui_unavailable;
        case MessageType::unpaired: return ReasonCode::unpaired;
        case MessageType::expired: return ReasonCode::expired;
        case MessageType::user_applied: return ReasonCode::user_applied;
        case MessageType::user_rejected: return ReasonCode::user_rejected;
        default: return ReasonCode::protocol_error;
    }
}

MessageType messageTypeFromReason(ReasonCode code) noexcept
{
    switch (code)
    {
        case ReasonCode::plan_staged: return MessageType::plan_staged;
        case ReasonCode::unknown_intent: return MessageType::unknown_intent;
        case ReasonCode::contradictory_intent: return MessageType::contradictory_intent;
        case ReasonCode::no_safe_move: return MessageType::no_safe_move;
        case ReasonCode::internal_error: return MessageType::internal_error;
        case ReasonCode::stale_revision: return MessageType::stale_revision;
        case ReasonCode::target_ui_unavailable: return MessageType::target_ui_unavailable;
        case ReasonCode::unpaired: return MessageType::unpaired;
        case ReasonCode::expired: return MessageType::expired;
        case ReasonCode::protocol_error: return MessageType::protocol_error;
        case ReasonCode::user_applied: return MessageType::user_applied;
        case ReasonCode::user_rejected: return MessageType::user_rejected;
    }
    return MessageType::protocol_error;
}

bool isForbiddenMessageType(std::string_view text) noexcept
{
    for (auto* v : kForbiddenVerbs)
        if (text == v)
            return true;
    return false;
}

bool isOutcomeMessage(MessageType type) noexcept
{
    switch (type)
    {
        case MessageType::plan_staged:
        case MessageType::unknown_intent:
        case MessageType::contradictory_intent:
        case MessageType::no_safe_move:
        case MessageType::internal_error:
        case MessageType::stale_revision:
        case MessageType::target_ui_unavailable:
        case MessageType::unpaired:
        case MessageType::expired:
        case MessageType::user_applied:
        case MessageType::user_rejected:
            return true;
        default:
            return false;
    }
}

bool isHex64(std::string_view text) noexcept
{
    if (text.size() != static_cast<std::size_t>(kHex64Len))
        return false;
    for (char c : text)
        if (hexNibble(c) < 0 || (c >= 'A' && c <= 'F'))
            return false;
    return true;
}

bool isUuidV4(std::string_view text) noexcept
{
    if (text.size() != 36)
        return false;
    auto hex = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    };
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (text[i] != '-')
                return false;
            continue;
        }
        if (i == 14)
        {
            if (text[i] != '4')
                return false;
            continue;
        }
        if (i == 19)
        {
            if (text[i] != '8' && text[i] != '9' && text[i] != 'a' && text[i] != 'b')
                return false;
            continue;
        }
        if (!hex(text[i]))
            return false;
    }
    return true;
}

bool isHumanCode(std::string_view text) noexcept
{
    if (text.size() != static_cast<std::size_t>(kHumanCodeLen))
        return false;
    for (char c : text)
        if (std::strchr(kHumanAlphabet, c) == nullptr)
            return false;
    return true;
}

bool isValidUtf8(std::string_view text) noexcept
{
    const auto* p = reinterpret_cast<const unsigned char*>(text.data());
    const auto* e = p + text.size();
    while (p < e)
    {
        if (*p < 0x80)
        {
            ++p;
            continue;
        }
        int need = 0;
        if ((*p & 0xe0) == 0xc0) need = 1;
        else if ((*p & 0xf0) == 0xe0) need = 2;
        else if ((*p & 0xf8) == 0xf0) need = 3;
        else return false;
        if (p + need >= e)
            return false;
        for (int i = 1; i <= need; ++i)
            if ((p[i] & 0xc0) != 0x80)
                return false;
        p += need + 1;
    }
    return true;
}

std::string toHex(const std::uint8_t* data, std::size_t n)
{
    return hexEncode(data, n);
}

bool fromHex(std::string_view hex, std::uint8_t* out, std::size_t n)
{
    if (hex.size() != n * 2)
        return false;
    for (std::size_t i = 0; i < n; ++i)
    {
        const int hi = hexNibble(hex[i * 2]);
        const int lo = hexNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

void sha256(const std::uint8_t* data, std::size_t n, std::uint8_t out[32])
{
    CC_SHA256(data, static_cast<CC_LONG>(n), out);
}

void hmacSha256(const std::uint8_t* key, std::size_t keyLen,
                const std::uint8_t* data, std::size_t dataLen,
                std::uint8_t out[32])
{
    CCHmac(kCCHmacAlgSHA256, key, keyLen, data, dataLen, out);
}

bool constantTimeEquals(const std::uint8_t* a, const std::uint8_t* b, std::size_t n) noexcept
{
    unsigned diff = 0;
    for (std::size_t i = 0; i < n; ++i)
        diff |= static_cast<unsigned>(a[i] ^ b[i]);
    return diff == 0;
}

bool constantTimeHexEquals(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size())
        return false;
    unsigned diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned>(static_cast<unsigned char>(a[i])
                                      ^ static_cast<unsigned char>(b[i]));
    return diff == 0;
}

void fillCsprng(std::uint8_t* out, std::size_t n)
{
    arc4random_buf(out, n);
}

std::string randomHex64()
{
    std::uint8_t bytes[32];
    fillCsprng(bytes, sizeof(bytes));
    auto hex = toHex(bytes, sizeof(bytes));
    if (hex == kForbiddenSecret)
        return randomHex64();
    return hex;
}

std::string randomUuidV4()
{
    std::uint8_t bytes[16];
    fillCsprng(bytes, sizeof(bytes));
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3f) | 0x80);
    auto hex = toHex(bytes, sizeof(bytes));
    return hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" + hex.substr(12, 4)
         + "-" + hex.substr(16, 4) + "-" + hex.substr(20, 12);
}

std::string randomHumanCode()
{
    std::uint8_t bytes[kHumanCodeLen];
    fillCsprng(bytes, sizeof(bytes));
    std::string out(kHumanCodeLen, 'A');
    const auto alphaLen = std::strlen(kHumanAlphabet);
    for (int i = 0; i < kHumanCodeLen; ++i)
        out[static_cast<std::size_t>(i)] = kHumanAlphabet[bytes[i] % alphaLen];
    return out;
}

std::array<std::uint8_t, 32> secretFromHex(std::string_view hex, bool& ok)
{
    std::array<std::uint8_t, 32> out {};
    ok = isHex64(hex) && hex != kForbiddenSecret && fromHex(hex, out.data(), out.size());
    return out;
}

std::string canonicalRequestJson(const StageSemanticRequest& request)
{
    return obj({
        field("target_runtime_instance_id", jsonEscape(request.targetRuntimeInstanceId)),
        field("pair_binding_id", jsonEscape(request.pairBindingId)),
        field("request_id", jsonEscape(request.requestId)),
        field("expected_control_revision", jsonInt(request.expectedControlRevision)),
        field("expected_projection_base_epoch", jsonInt(request.expectedProjectionBaseEpoch)),
        field("expected_audition_context_epoch", jsonInt(request.expectedAuditionContextEpoch)),
        field("phrase", jsonEscape(request.phrase)),
        field("intensity", jsonFloat(request.intensity)),
        field("expires_at_monotonic_ns", jsonInt(request.expiresAtMonotonicNs))
    });
}

std::string requestHashHex(const StageSemanticRequest& request)
{
    const auto json = canonicalRequestJson(request);
    std::uint8_t digest[32];
    sha256(reinterpret_cast<const std::uint8_t*>(json.data()), json.size(), digest);
    return toHex(digest, 32);
}

AuditMetadata makeAudit(const std::uint8_t* data, std::size_t n, std::optional<int> latencyMs)
{
    AuditMetadata audit;
    audit.byteCount = static_cast<int>(std::min(n, static_cast<std::size_t>(kMaxFrameBytes)));
    std::uint8_t digest[32];
    sha256(data, n, digest);
    audit.sha256Hex = toHex(digest, 32);
    audit.latencyMs = latencyMs;
    return audit;
}

AuditMetadata makeAudit(std::string_view text, std::optional<int> latencyMs)
{
    return makeAudit(reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), latencyMs);
}

std::string serializeWireJson(const WireMessage& message)
{
    return obj({
        field("record_kind", jsonEscape("wire_message")),
        field("protocol", jsonEscape(kProtocolName)),
        field("protocol_major", jsonIntSigned(kProtocolMajor)),
        field("protocol_minor", jsonIntSigned(kProtocolMinor)),
        field("message_type", jsonEscape(toString(message.type))),
        field("seq", jsonInt(message.seq)),
        field("sent_at_monotonic_ns", jsonInt(static_cast<std::uint64_t>(std::max<std::int64_t>(0, message.sentAtMonotonicNs)))),
        field("payload", payloadJson(message))
    });
}

std::string serializeRendezvousJson(const RendezvousRecord& record)
{
    return obj({
        field("record_kind", jsonEscape("rendezvous_record")),
        field("protocol", jsonEscape(kProtocolName)),
        field("protocol_major", jsonIntSigned(kProtocolMajor)),
        field("protocol_minor", jsonIntSigned(kProtocolMinor)),
        field("listen_address", jsonEscape(record.listenAddress)),
        field("listen_port", jsonIntSigned(record.listenPort)),
        field("session_uuid", jsonEscape(record.sessionUuid)),
        field("session_secret_hex", jsonEscape(record.sessionSecretHex)),
        field("server_epoch", jsonInt(record.serverEpoch)),
        field("expires_at_unix_s", jsonIntSigned(record.expiresAtUnixS)),
        field("created_at_unix_s", jsonIntSigned(record.createdAtUnixS))
    });
}

IngestResult parseWireJson(std::string_view json)
{
    IngestResult result;
    if (json.size() > static_cast<std::size_t>(kMaxFrameBytes))
    {
        result.error = ProtocolErrorCode::oversized_frame;
        return result;
    }
    if (!isValidUtf8(json))
    {
        result.error = ProtocolErrorCode::invalid_utf8;
        return result;
    }

    ProtocolErrorCode parseError = ProtocolErrorCode::malformed_json;
    JsonParser parser(json);
    const auto root = parser.parse(parseError);
    if (!parser.succeeded || root.type != JsonValue::Type::Object)
    {
        result.error = parser.succeeded ? ProtocolErrorCode::malformed_json : parseError;
        return result;
    }

    static const std::vector<std::string_view> envelope {
        "record_kind", "protocol", "protocol_major", "protocol_minor",
        "message_type", "seq", "sent_at_monotonic_ns", "payload"
    };
    ProtocolErrorCode error = ProtocolErrorCode::missing_required;
    if (!requireKeys(root, envelope, envelope, error))
    {
        result.error = error;
        return result;
    }

    auto kind = root.get("record_kind")->asString();
    auto protocol = root.get("protocol")->asString();
    auto major = root.get("protocol_major")->asInt();
    if (!kind || *kind != "wire_message")
    {
        result.error = ProtocolErrorCode::forbidden_verb;
        return result;
    }
    if (!protocol || *protocol != kProtocolName || !major || *major != kProtocolMajor)
    {
        result.error = ProtocolErrorCode::incompatible_major;
        return result;
    }

    auto typeStr = root.get("message_type")->asString();
    if (!typeStr)
    {
        result.error = ProtocolErrorCode::missing_required;
        return result;
    }
    if (isForbiddenMessageType(*typeStr) || !messageTypeFromString(*typeStr))
    {
        result.error = ProtocolErrorCode::forbidden_verb;
        return result;
    }
    auto seq = root.get("seq")->asInt();
    auto ts = root.get("sent_at_monotonic_ns")->asInt();
    if (!seq || *seq < 0 || !ts || *ts < 0)
    {
        result.error = ProtocolErrorCode::missing_required;
        return result;
    }
    const auto* payload = root.get("payload");
    if (payload == nullptr || payload->type != JsonValue::Type::Object)
    {
        result.error = ProtocolErrorCode::missing_required;
        return result;
    }

    WireMessage message;
    message.type = *messageTypeFromString(*typeStr);
    message.seq = static_cast<std::uint64_t>(*seq);
    message.sentAtMonotonicNs = *ts;
    const auto payErr = payloadError(message.type, *payload, message);
    if (payErr != ProtocolErrorCode::malformed_json)
    {
        result.error = payErr;
        return result;
    }
    result.ok = true;
    result.message = std::move(message);
    return result;
}

std::optional<RendezvousRecord> parseRendezvousJson(std::string_view json, ProtocolErrorCode& error)
{
    if (json.size() > static_cast<std::size_t>(kMaxFrameBytes))
    {
        error = ProtocolErrorCode::oversized_frame;
        return std::nullopt;
    }
    JsonParser parser(json);
    ProtocolErrorCode parseError = ProtocolErrorCode::malformed_json;
    const auto root = parser.parse(parseError);
    if (root.type != JsonValue::Type::Object)
    {
        error = parseError;
        return std::nullopt;
    }
    static const std::vector<std::string_view> keys {
        "record_kind", "protocol", "protocol_major", "protocol_minor",
        "listen_address", "listen_port", "session_uuid", "session_secret_hex",
        "server_epoch", "expires_at_unix_s", "created_at_unix_s"
    };
    if (!requireKeys(root, keys, keys, error))
        return std::nullopt;

    RendezvousRecord record;
    auto kind = root.get("record_kind")->asString();
    auto protocol = root.get("protocol")->asString();
    auto major = root.get("protocol_major")->asInt();
    if (!kind || *kind != "rendezvous_record" || !protocol || *protocol != kProtocolName
        || !major || *major != kProtocolMajor)
    {
        error = ProtocolErrorCode::incompatible_major;
        return std::nullopt;
    }
    if (!asStringField(root, "listen_address", record.listenAddress, error)
        || !asUuid(root, "session_uuid", record.sessionUuid, error)
        || !asHex64(root, "session_secret_hex", record.sessionSecretHex, error))
        return std::nullopt;
    auto port = root.get("listen_port")->asInt();
    auto exp = root.get("expires_at_unix_s")->asInt();
    auto created = root.get("created_at_unix_s")->asInt();
    auto epoch = root.get("server_epoch")->asInt();
    if (!port || *port < 1 || *port > 65535 || !exp || *exp < 1 || !created || *created < 1 || !epoch || *epoch < 0)
    {
        error = ProtocolErrorCode::missing_required;
        return std::nullopt;
    }
    record.listenPort = static_cast<int>(*port);
    record.expiresAtUnixS = *exp;
    record.createdAtUnixS = *created;
    record.serverEpoch = static_cast<std::uint64_t>(*epoch);
    if (!validateRendezvous(record, record.createdAtUnixS, error))
        return std::nullopt;
    return record;
}

bool validateRendezvous(const RendezvousRecord& record, std::int64_t nowUnixS, ProtocolErrorCode& error)
{
    if (record.listenAddress != kLoopbackAddress)
    {
        error = ProtocolErrorCode::stale_rendezvous;
        return false;
    }
    if (record.sessionSecretHex == kForbiddenSecret || !isHex64(record.sessionSecretHex))
    {
        error = ProtocolErrorCode::expired_secret;
        return false;
    }
    if (record.expiresAtUnixS <= nowUnixS)
    {
        error = ProtocolErrorCode::expired_secret;
        return false;
    }
    if (record.listenPort < 1 || record.listenPort > 65535 || !isUuidV4(record.sessionUuid))
    {
        error = ProtocolErrorCode::stale_rendezvous;
        return false;
    }
    return true;
}

EncodedFrame frameFromBody(std::string_view body, const std::uint8_t* hmac)
{
    EncodedFrame frame;
    frame.body.assign(body.begin(), body.end());
    if (hmac != nullptr)
    {
        std::memcpy(frame.hmac.data(), hmac, 32);
        frame.hasHmac = true;
    }
    return frame;
}

void ProposalSession::reset()
{
    *this = ProposalSession {};
}

void ProposalSession::setSecret(const std::array<std::uint8_t, 32>& value)
{
    secret = value;
    hasSecretFlag = true;
}

void ProposalSession::clearSecret()
{
    secret.fill(0);
    hasSecretFlag = false;
    authenticated = false;
}

IngestResult ProposalSession::ingest(const EncodedFrame& frame)
{
    IngestResult result;
    if (frame.body.size() > static_cast<std::size_t>(kMaxFrameBytes))
    {
        result.error = ProtocolErrorCode::oversized_frame;
        return result;
    }
    if (authenticated)
    {
        if (!hasSecretFlag || !frame.hasHmac)
        {
            result.error = ProtocolErrorCode::hmac_mismatch;
            return result;
        }
        std::uint8_t expected[32];
        hmacSha256(secret.data(), secret.size(), frame.body.data(), frame.body.size(), expected);
        if (!constantTimeEquals(expected, frame.hmac.data(), 32))
        {
            result.error = ProtocolErrorCode::hmac_mismatch;
            return result;
        }
    }

    const std::string_view json(reinterpret_cast<const char*>(frame.body.data()), frame.body.size());
    result = parseWireJson(json);
    if (!result.ok)
        return result;

    const auto seq = result.message.seq;
    if (lastSeq.has_value() && seq == *lastSeq)
    {
        result.ok = false;
        result.error = ProtocolErrorCode::replay;
        return result;
    }
    if (seq < expectedSeq)
    {
        result.ok = false;
        result.error = ProtocolErrorCode::reorder;
        return result;
    }
    if (seq > expectedSeq)
    {
        result.ok = false;
        result.error = ProtocolErrorCode::seq_gap;
        return result;
    }
    lastSeq = seq;
    expectedSeq = seq + 1;

    if (result.message.type == MessageType::handshake
        && result.message.handshakePhase == HandshakePhase::hello)
    {
        const auto& nonce = result.message.hello.clientNonceHex;
        if (std::find(seenNonces.begin(), seenNonces.end(), nonce) != seenNonces.end())
        {
            result.ok = false;
            result.error = ProtocolErrorCode::nonce_reuse;
            return result;
        }
        seenNonces.push_back(nonce);
    }

    if (!authenticated && result.message.type != MessageType::handshake)
    {
        result.ok = false;
        result.error = ProtocolErrorCode::unauthenticated;
        return result;
    }
    return result;
}

bool ProposalSession::applyClientAck(const HandshakeAck& ack, HandshakeConfirm& confirm)
{
    if (!hasSecretFlag || !hasClientNonce)
        return false;
    std::uint8_t server[32];
    if (!fromHex(ack.serverNonceHex, server, 32))
        return false;
    std::uint8_t expected[32];
    std::uint8_t concat[64];
    std::memcpy(concat, clientNonce.data(), 32);
    std::memcpy(concat + 32, server, 32);
    hmacSha256(secret.data(), secret.size(), concat, 64, expected);
    std::uint8_t given[32];
    if (!fromHex(ack.hmacSha256Hex, given, 32) || !constantTimeEquals(expected, given, 32))
        return false;
    std::memcpy(serverNonce.data(), server, 32);
    hasServerNonce = true;
    std::memcpy(concat, server, 32);
    std::memcpy(concat + 32, clientNonce.data(), 32);
    std::uint8_t confirmMac[32];
    hmacSha256(secret.data(), secret.size(), concat, 64, confirmMac);
    confirm.clientNonceHex = lastClientNonceHex;
    confirm.serverNonceHex = ack.serverNonceHex;
    confirm.hmacSha256Hex = toHex(confirmMac, 32);
    return true;
}

EncodeResult ProposalSession::encode(const WireMessage& message, std::int64_t nowNs, bool macOverride)
{
    EncodeResult result;
    WireMessage copy = message;
    copy.seq = outboundSeq;
    copy.sentAtMonotonicNs = nowNs;
    const auto json = serializeWireJson(copy);
    if (json.size() > static_cast<std::size_t>(kMaxFrameBytes))
    {
        result.ok = false;
        return result;
    }
    EncodedFrame frame;
    frame.body.assign(json.begin(), json.end());
    const bool mac = macOverride;
    if (mac)
    {
        if (!hasSecretFlag)
            return result;
        hmacSha256(secret.data(), secret.size(), frame.body.data(), frame.body.size(), frame.hmac.data());
        frame.hasHmac = true;
    }
    ++outboundSeq;
    result.ok = true;
    result.frame = std::move(frame);
    result.message = std::move(copy);
    return result;
}

} // namespace EmberProposal
