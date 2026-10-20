#pragma once
#include <atomic>
#include <cstdint>

// The render thread publishes ownership; the GUI thread checks queued notifications.
class VideoRenderLifetime {
public:
    using Token = std::uint64_t;

    Token activate() {
        const Token token = nextToken.fetch_add(1) + 1;
        activeToken.store(token);
        return token;
    }

    void release(Token token) { activeToken.compare_exchange_strong(token, 0); }

    bool owns(Token token) const { return token != 0 && activeToken.load() == token; }
    bool available() const { return activeToken.load() != 0; }

private:
    std::atomic<Token> nextToken{0};
    std::atomic<Token> activeToken{0};
};
