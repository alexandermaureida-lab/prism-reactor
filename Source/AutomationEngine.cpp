#include "AutomationEngine.h"
#include <cmath>

using namespace juce;

AutomationEngine::AutomationEngine (AudioProcessorValueTreeState& a) : apvts (a)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (p))
        {
            params.add (r);
            idByIndex[r->getParameterIndex()] = r->paramID;
            r->addListener (this);
        }
}

AutomationEngine::~AutomationEngine()
{
    stopTimer();
    for (auto* p : params) p->removeListener (this);
}

RangedAudioParameter* AutomationEngine::paramFor (const String& id) const
{
    for (auto* p : params) if (p->paramID == id) return p;
    return nullptr;
}

AutoLane* AutomationEngine::find (const String& id)
{
    for (auto& l : lanes) if (l->id == id) return l.get();
    return nullptr;
}

AutoLane* AutomationEngine::addLane (const String& id, bool seed)
{
    if (auto* e = find (id)) return e;
    auto* p = paramFor (id);
    if (p == nullptr) return nullptr;
    auto l = std::make_unique<AutoLane>();
    l->param = p; l->id = id;
    if (seed) l->pts.push_back ({ 0.0, p->getValue() });
    lanes.push_back (std::move (l));
    if (onLanesChanged) onLanesChanged();
    return lanes.back().get();
}

void AutomationEngine::removeLane (const String& id)
{
    for (size_t i = 0; i < lanes.size(); ++i)
        if (lanes[i]->id == id) { lanes.erase (lanes.begin() + (long) i); break; }
    touch.erase (id);
    if (onLanesChanged) onLanesChanged();
}

void AutomationEngine::clearAll()
{
    lanes.clear(); touch.clear();
    if (onLanesChanged) onLanesChanged();
}

void AutomationEngine::setPlaying (bool p)
{
    if (p == playing) return;
    playing = p;
    if (playing) { lastTick = Time::getMillisecondCounterHiRes() * 0.001; dirty = true; startTimerHz (60); }
    else { stopTimer(); touch.clear(); }
}

void AutomationEngine::setTime (double t)
{
    time = jlimit (0.0, loopLen, t);
    touch.clear();
    apply();
}

void AutomationEngine::setLoopLength (double s)
{
    loopLen = jmax (5.0, s);
    if (time >= loopLen) time = 0.0;
}

float AutomationEngine::evaluate (const AutoLane& l, double t) const
{
    const auto& p = l.pts;
    const size_t n = p.size();
    if (n == 0) return l.param->getValue();
    if (t <= p.front().t) return p.front().v;
    if (t >= p.back().t) return p.back().v;
    size_t lo = 0, hi = n - 1;
    while (hi - lo > 1) { const size_t mid = (lo + hi) / 2; if (p[mid].t <= t) lo = mid; else hi = mid; }
    if (l.discrete()) return p[lo].v;
    const double f = (t - p[lo].t) / jmax (1.0e-9, p[hi].t - p[lo].t);
    return (float) (p[lo].v + (p[hi].v - p[lo].v) * f);
}

float AutomationEngine::quantise (const AutoLane& l, float v) const
{
    auto* p = l.param;
    const float real = p->convertFrom0to1 (jlimit (0.0f, 1.0f, v));
    return p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (real));
}

void AutomationEngine::markChanged()
{
    dirty = true;
    if (! playing) apply();
}

void AutomationEngine::apply()
{
    const double nowMs = (double) Time::getMillisecondCounter();
    ignoreUnused (nowMs);
    applying = true;
    for (auto& l : lanes)
    {
        if (l->mute || l->pts.empty()) continue;
        if (touch.count (l->id) || holding[l->id]) continue;
        const float v = quantise (*l, evaluate (*l, time));
        if (std::abs (l->param->getValue() - v) > 1.0e-4f)
            l->param->setValueNotifyingHost (v);
    }
    applying = false;
    dirty = false;
}

void AutomationEngine::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    double dt = now - lastTick; lastTick = now;
    if (dt > 0.25) dt = 0.0;
    time += dt;
    if (time >= loopLen) { time = std::fmod (time, loopLen); touch.clear(); }

    if (recording)
        for (auto& kv : holding)
            if (kv.second)
                if (auto* l = find (kv.first)) write (*l, l->param->getValue(), time);

    const auto ms = Time::currentTimeMillis();
    for (auto it = touch.begin(); it != touch.end();)
        it = (ms > it->second.untilMs && ! holding[it->first]) ? touch.erase (it) : std::next (it);

    apply();
}

void AutomationEngine::write (AutoLane& l, float v, double t)
{
    auto& tc = touch[l.id];
    if (tc.untilMs == 0 || t < tc.last - 0.001) { tc.last = t; tc.lastPtT = -1.0; }
    tc.untilMs = Time::currentTimeMillis() + 350;
    const double a = tc.last, b = t;
    bool replaced = false;
    if (tc.lastPtT >= 0.0 && t - tc.lastPtT < 0.05)
    {
        for (auto& p : l.pts)
            if (p.t == tc.lastPtT) { p.t = t; p.v = v; replaced = true; break; }
        if (replaced) tc.lastPtT = t;
    }
    if (! replaced)
    {
        l.pts.erase (std::remove_if (l.pts.begin(), l.pts.end(), [a, b] (const AutoPoint& p) { return p.t > a + 0.0005 && p.t <= b + 0.0005; }), l.pts.end());
        l.pts.push_back ({ t, v });
        std::sort (l.pts.begin(), l.pts.end(), [] (const AutoPoint& x, const AutoPoint& y) { return x.t < y.t; });
        tc.lastPtT = t;
    }
    tc.last = t;
}

void AutomationEngine::parameterValueChanged (int index, float newValue)
{
    if (applying || ! recording || ! playing || ! onMessageThread()) return;
    auto it = idByIndex.find (index);
    if (it == idByIndex.end()) return;
    bool created = find (it->second) == nullptr;
    if (auto* l = addLane (it->second, false)) write (*l, newValue, time);
    ignoreUnused (created);
}

void AutomationEngine::parameterGestureChanged (int index, bool starting)
{
    if (! onMessageThread()) return;
    auto it = idByIndex.find (index);
    if (it != idByIndex.end()) holding[it->second] = starting;
}

var AutomationEngine::toVar() const
{
    auto* root = new DynamicObject();
    root->setProperty ("loop", loopLen);
    Array<var> arr;
    for (auto& l : lanes)
    {
        auto* o = new DynamicObject();
        o->setProperty ("id", l->id);
        o->setProperty ("mute", l->mute);
        Array<var> pts;
        for (auto& p : l->pts) { Array<var> pr; pr.add (std::round (p.t * 1000.0) / 1000.0); pr.add (std::round (p.v * 10000.0f) / 10000.0); pts.add (var (pr)); }
        o->setProperty ("pts", pts);
        arr.add (var (o));
    }
    root->setProperty ("lanes", arr);
    return var (root);
}

void AutomationEngine::fromVar (const var& v)
{
    lanes.clear(); touch.clear();
    if (auto* o = v.getDynamicObject())
    {
        if (o->hasProperty ("loop")) setLoopLength ((double) o->getProperty ("loop"));
        if (auto* arr = o->getProperty ("lanes").getArray())
            for (auto& item : *arr)
            {
                auto id = item.getProperty ("id", {}).toString();
                if (find (id) != nullptr) continue;
                auto* p = paramFor (id);
                if (p == nullptr) continue;
                auto l = std::make_unique<AutoLane>();
                l->param = p; l->id = id; l->mute = (bool) item.getProperty ("mute", false);
                if (auto* pts = item.getProperty ("pts", {}).getArray())
                    for (auto& pr : *pts)
                        if (auto* a = pr.getArray(); a != nullptr && a->size() >= 2 && (double) (*a)[0] >= 0.0)
                            l->pts.push_back ({ (double) (*a)[0], jlimit (0.0f, 1.0f, (float) (double) (*a)[1]) });
                std::sort (l->pts.begin(), l->pts.end(), [] (const AutoPoint& x, const AutoPoint& y) { return x.t < y.t; });
                lanes.push_back (std::move (l));
            }
    }
    dirty = true;
    if (onLanesChanged) onLanesChanged();
}
