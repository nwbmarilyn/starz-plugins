#pragma once
#include <string>

struct Host {
    virtual float getNorm(int id) = 0;
    virtual void edit(int id, double norm, int phase) = 0;
    virtual std::wstring text(int id) = 0;
    virtual ~Host() {}
};

namespace gui {
struct Bridge : Host {
    virtual float pitchIn() = 0;
    virtual float pitchOut() = 0;
    virtual float refHz() = 0;
    virtual float latencyMs() = 0;
    virtual int keyRoot() = 0;
    virtual int scaleType() = 0;
    virtual ~Bridge() {}
};
}

namespace fxgui {
struct Bridge : Host {
    virtual double getPlain(int id) = 0;
    virtual float effectiveDelayMs() = 0;
    virtual ~Bridge() {}
};
}