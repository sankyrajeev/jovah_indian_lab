#include "RhythmGenerator.h"
#include "../Core/Rng.h"

#include <algorithm>

namespace jgl
{

namespace
{
    using Step = RhythmGenerator::Step;
    using Bar  = RhythmGenerator::Bar;

    float clamp01 (float v) { return juce::jlimit (0.0f, 1.0f, v); }

    float closeness (float a, float b, float width)
    {
        const float d = (a - b) / width;
        return std::exp (-d * d);
    }

    float metricWeight (const Framework& fw, int step, int stepsPerBar)
    {
        if (stepsPerBar == 12) return fw.accent12[(size_t) (step % 12)];
        if (stepsPerBar == 16) return fw.accent16[(size_t) (step % 16)];
        return fw.accent16[(size_t) ((step * 16 / stepsPerBar) % 16)];
    }

    bool isTexture (InstrumentId id)
    {
        return id == InstrumentId::Manjira || id == InstrumentId::Ghungroo || id == InstrumentId::Taali;
    }

    float ghostAffinity (InstrumentId id)
    {
        switch (id)
        {
            case InstrumentId::Tabla:    return 1.0f;
            case InstrumentId::Dholak:   return 0.85f;
            case InstrumentId::Dhol:     return 0.55f;
            case InstrumentId::Manjira:  return 0.5f;
            case InstrumentId::Ghungroo: return 0.35f;
            case InstrumentId::Taali:    return 0.1f;
            default:                     return 0.0f;
        }
    }

    bool canRoll (InstrumentId id)
    {
        return id == InstrumentId::Tabla || id == InstrumentId::Dhol || id == InstrumentId::Dholak;
    }

    /** Musically related alternative stroke used by mutations. */
    int siblingArticulation (InstrumentId id, int a)
    {
        switch (id)
        {
            case InstrumentId::Tabla:
                switch (a)
                {
                    case TablaArt::Na:   return TablaArt::Tin;
                    case TablaArt::Tin:  return TablaArt::Na;
                    case TablaArt::Dha:  return TablaArt::Dhin;
                    case TablaArt::Dhin: return TablaArt::Dha;
                    case TablaArt::Ge:   return TablaArt::Ghe;
                    case TablaArt::Ghe:  return TablaArt::Ge;
                    case TablaArt::Te:   return TablaArt::Re;
                    case TablaArt::Re:   return TablaArt::Te;
                    case TablaArt::Tun:  return TablaArt::Tin;
                    default:             return a;
                }
            case InstrumentId::Dhol:
                switch (a)
                {
                    case DholArt::Tilli:      return DholArt::TilliEdge;
                    case DholArt::TilliEdge:  return DholArt::Tilli;
                    case DholArt::Dagga:      return DholArt::DaggaMuted;
                    case DholArt::DaggaMuted: return DholArt::Dagga;
                    default:                  return a;
                }
            case InstrumentId::Dholak:
                switch (a)
                {
                    case DholakArt::Ge:      return DholakArt::GeSlide;
                    case DholakArt::GeSlide: return DholakArt::Ge;
                    case DholakArt::Na:      return DholakArt::Ta;
                    case DholakArt::Ta:      return DholakArt::Na;
                    default:                 return a;
                }
            case InstrumentId::Manjira:  return a == ManjiraArt::Open ? ManjiraArt::Closed : ManjiraArt::Open;
            case InstrumentId::Ghungroo: return a == GhungrooArt::Shake ? GhungrooArt::LongShake : GhungrooArt::Shake;
            case InstrumentId::Taali:    return a == TaaliArt::Clap ? TaaliArt::Group : TaaliArt::Clap;
            default:                     return a;
        }
    }

    /** Stroke used when density adds non-ghost hits. */
    int fillerArticulation (InstrumentId id, float posWeight, int alt)
    {
        switch (id)
        {
            case InstrumentId::Tabla:    return posWeight > 0.6f ? TablaArt::Dha : ((alt & 1) ? TablaArt::Na : TablaArt::Tin);
            case InstrumentId::Dhol:     return posWeight > 0.6f ? DholArt::Dagga : DholArt::Tilli;
            case InstrumentId::Dholak:   return posWeight > 0.6f ? DholakArt::Ge : DholakArt::Na;
            case InstrumentId::Manjira:  return ManjiraArt::Closed;
            case InstrumentId::Ghungroo: return GhungrooArt::Shake;
            case InstrumentId::Taali:    return TaaliArt::Clap;
            default:                     return 0;
        }
    }

    float effectiveDensity (InstrumentId id, const GenerationContext& ctx)
    {
        float d = ctx.settings.density;
        const bool support = isTexture (id) || (id == InstrumentId::Dholak && ctx.has (InstrumentId::Tabla));
        if (support && ctx.numPercussion >= 4)
            d -= 0.07f * (float) (ctx.numPercussion - 3);
        if (id == InstrumentId::Tabla && ctx.numMelodic >= 2)
            d -= 0.05f;
        return juce::jlimit (0.05f, 1.0f, d);
    }

    //==========================================================================
    const PatternCell* chooseCell (InstrumentId id, const GenerationContext& ctx, Rng& rng,
                                   const PatternCell* avoid, int requireSpb)
    {
        const auto& style = *ctx.style;
        const auto& fw = framework (ctx.framework);
        const auto sBit = styleBit (style.id);
        const bool custom = style.id == StyleId::Custom;
        const auto fBit = frameworkBit (ctx.framework);
        const float complexity = ctx.settings.complexity;
        const float energy = ctx.settings.energy;

        std::vector<const PatternCell*> cands;
        std::vector<double> weights;

        auto collect = [&] (bool ignoreStyle)
        {
            for (auto& c : percussionCells())
            {
                if (c.instrument != id || &c == avoid) continue;
                if (requireSpb > 0 && c.stepsPerBeat != requireSpb) continue;
                if (! ignoreStyle && ! custom && (c.styles & sBit) == 0) continue;

                double w = c.weight;
                w *= closeness (c.energy, energy, 0.32f);
                w *= closeness (c.complexity, complexity, 0.38f);

                if (c.frameworks != 0)
                    w *= (c.frameworks & fBit) ? (id == InstrumentId::Tabla ? 6.0 : 3.0) : 0.35;
                else if (id == InstrumentId::Tabla && (custom || ctx.settings.frameworkIndex > 0))
                    w *= 0.6;   // an explicit framework should steer the lead drum to its theka

                if (c.stepsPerBeat == 3)
                {
                    if (fw.triplet)                       w *= 2.5;
                    else if (id == InstrumentId::Tabla)   w *= 0.1;
                    else                                  w *= 0.2 + style.tripletChance * 2.0;
                }
                else if (fw.triplet && id == InstrumentId::Tabla)
                {
                    w *= 0.4;
                }

                cands.push_back (&c);
                weights.push_back (w);
            }
        };

        collect (false);
        if (cands.empty())
            collect (true);
        if (cands.empty())
            return nullptr;

        const int idx = rng.weightedIndex (weights);
        return idx >= 0 ? cands[(size_t) idx] : cands.front();
    }

    Bar barFromCell (const std::vector<ParsedStep>& parsed, int barIndex, int stepsPerBar)
    {
        Bar bar ((size_t) stepsPerBar);
        for (int s = 0; s < stepsPerBar; ++s)
        {
            const size_t src = (size_t) (barIndex * stepsPerBar + s);
            if (src >= parsed.size()) break;
            const auto& p = parsed[src];
            auto& d = bar[(size_t) s];
            d.hit = p.hit;
            d.articulation = p.articulation;
            d.dyn = p.dyn;
            d.roll = p.roll;
        }
        return bar;
    }

    //==========================================================================
    /** Density / complexity shaping with fixed per-step draws (morphs smoothly with the knobs). */
    void embellish (Bar& bar, InstrumentId id, const GenerationContext& ctx, const Framework& fw,
                    float density, uint64_t drawSeed)
    {
        const int n = (int) bar.size();
        const auto uThin  = stableDraws (hashCombine (drawSeed, 1), n);
        const auto uGhost = stableDraws (hashCombine (drawSeed, 2), n);
        const auto uAdd   = stableDraws (hashCombine (drawSeed, 3), n);
        const auto uRoll  = stableDraws (hashCombine (drawSeed, 4), n);
        const auto uAcc   = stableDraws (hashCombine (drawSeed, 5), n);

        const float complexity = ctx.settings.complexity;
        const float affinity = ghostAffinity (id);
        const auto& style = *ctx.style;
        int ghostAlt = 0;

        const Bar original = bar;

        for (int s = 0; s < n; ++s)
        {
            auto& st = bar[(size_t) s];
            const float w = metricWeight (fw, s, n);
            const bool nextHit = original[(size_t) ((s + 1) % n)].hit;
            const bool nextAccent = nextHit && original[(size_t) ((s + 1) % n)].dyn == Dynamic::Accent;

            if (st.hit)
            {
                // Thinning: low density removes weak, non-accented hits (never the downbeat).
                if (s != 0 && st.dyn != Dynamic::Accent)
                {
                    const float pRemove = std::max (0.0f, 0.5f - density) * 1.6f * (1.0f - w);
                    if (uThin[(size_t) s] < pRemove)
                    {
                        st = Step();
                        continue;
                    }
                }

                // Syncopated accent shifts on weak positions.
                if (st.dyn == Dynamic::Normal && w < 0.35f
                    && uAcc[(size_t) s] < std::max (0.0f, complexity - 0.45f) * 0.5f)
                    st.dyn = Dynamic::Accent;

                // Rolls leading into accents.
                if (canRoll (id) && nextAccent && st.dyn != Dynamic::Accent
                    && uRoll[(size_t) s] < style.rollChance * complexity * 0.7f)
                    st.roll = true;

                continue;
            }

            // Ghost notes: anticipate the next hit, avoid strong positions.
            const float pGhost = complexity * (0.12f + density * 0.5f) * affinity
                               * (nextHit ? 1.5f : 0.55f) * (1.0f - 0.7f * w);
            if (uGhost[(size_t) s] < pGhost)
            {
                st.hit = true;
                st.dyn = Dynamic::Ghost;
                st.articulation = ghostArticulation (id, ghostAlt++);
                continue;
            }

            // High density adds real hits on stronger positions.
            const float pAdd = std::max (0.0f, density - 0.62f) * 1.5f * w * (1.0f - complexity * 0.3f);
            if (uAdd[(size_t) s] < pAdd)
            {
                st.hit = true;
                st.dyn = Dynamic::Normal;
                st.articulation = fillerArticulation (id, w, s);
            }
        }

        // Never thin a bar to silence: keep the original's strongest hit.
        bool any = false;
        for (auto& st : bar) any = any || st.hit;
        if (! any)
        {
            int best = -1;
            float bestW = -1.0f;
            for (int s = 0; s < n; ++s)
                if (original[(size_t) s].hit && metricWeight (fw, s, n) > bestW)
                {
                    best = s;
                    bestW = metricWeight (fw, s, n);
                }
            if (best >= 0)
                bar[(size_t) best] = original[(size_t) best];
        }
    }

    /** Small musical edits used to create A', A'' variations. Counts only edits that change something. */
    void mutate (Bar& bar, InstrumentId id, float amount, float complexity, Rng& rng, const Framework& fw)
    {
        const int n = (int) bar.size();
        const int wanted = (int) std::floor (amount * (1.0f + complexity * 2.5f) + rng.uniform() * 0.8 + (amount > 0.15f ? 0.5 : 0.0));
        int done = 0;

        for (int attempt = 0; attempt < wanted * 8 && done < wanted; ++attempt)
        {
            const int s = 1 + rng.integer (n - 1);   // never touch the downbeat
            auto& st = bar[(size_t) s];
            const float w = metricWeight (fw, s, n);
            const int op = rng.integer (5);

            switch (op)
            {
                case 0: // displace a weak hit by one step
                    if (st.hit && st.dyn != Dynamic::Accent)
                    {
                        const int t = s + (rng.chance (0.5) ? 1 : -1);
                        if (t > 0 && t < n && ! bar[(size_t) t].hit)
                        {
                            std::swap (bar[(size_t) t], st);
                            ++done;
                        }
                    }
                    break;

                case 1: // change stroke colour
                    if (st.hit)
                    {
                        const int sib = siblingArticulation (id, st.articulation);
                        if (sib != st.articulation) { st.articulation = sib; ++done; }
                    }
                    break;

                case 2: // add a ghost
                    if (! st.hit && w < 0.6f)
                    {
                        st.hit = true;
                        st.dyn = Dynamic::Ghost;
                        st.articulation = ghostArticulation (id, s);
                        ++done;
                    }
                    break;

                case 3: // drop a weak hit (space is part of the groove)
                    if (st.hit && st.dyn != Dynamic::Accent && w < 0.5f)
                    {
                        st = Step();
                        ++done;
                    }
                    break;

                default: // syncopated accent
                    if (st.hit && st.dyn == Dynamic::Normal && w < 0.4f)
                    {
                        st.dyn = Dynamic::Accent;
                        ++done;
                    }
                    else if (! st.hit && w < 0.3f && rng.chance (0.5))
                    {
                        st.hit = true;
                        st.dyn = Dynamic::Normal;
                        st.articulation = fillerArticulation (id, w, s);
                        ++done;
                    }
                    break;
            }
        }
    }

    /**
        Tihai: a short motif played three times so the final stroke lands on the next downbeat's
        doorstep (3m + 2g = steps, m = motif length, g = gap). Returns false if no shape fits.
    */
    bool applyTihai (Bar& bar, InstrumentId id, int fillSteps, float energy, Rng& rng)
    {
        std::vector<std::vector<int>> motifs;
        switch (id)
        {
            case InstrumentId::Tabla:
                motifs = { { TablaArt::Dha }, { TablaArt::Dha, TablaArt::Te }, { TablaArt::Dha, TablaArt::Te, TablaArt::Te },
                           { TablaArt::Te, TablaArt::Re, TablaArt::Ka, TablaArt::Dha }, { TablaArt::Na, TablaArt::Dha } };
                break;
            case InstrumentId::Dhol:
                motifs = { { DholArt::Both }, { DholArt::Both, DholArt::Tilli }, { DholArt::Dagga, DholArt::Tilli, DholArt::Tilli } };
                break;
            case InstrumentId::Dholak:
                motifs = { { DholakArt::Ge }, { DholakArt::Ge, DholakArt::Na }, { DholakArt::Ta, DholakArt::Na, DholakArt::Ge } };
                break;
            case InstrumentId::Taali:
                motifs = { { TaaliArt::Clap }, { TaaliArt::Clap, TaaliArt::Clap } };
                break;
            case InstrumentId::Manjira:
                motifs = { { ManjiraArt::Open }, { ManjiraArt::Closed, ManjiraArt::Open } };
                break;
            default:
                return false;
        }

        std::vector<std::pair<int, int>> shapes;   // (motif index, gap)
        for (int mi = 0; mi < (int) motifs.size(); ++mi)
        {
            const int m = (int) motifs[(size_t) mi].size();
            const int rest = fillSteps - 3 * m;
            if (rest >= 0 && rest % 2 == 0 && rest / 2 <= 3)
                shapes.push_back ({ mi, rest / 2 });
        }
        if (shapes.empty())
            return false;

        const auto shape = shapes[(size_t) rng.integer ((int) shapes.size())];
        const auto& motif = motifs[(size_t) shape.first];
        const int m = (int) motif.size(), g = shape.second;
        const int n = (int) bar.size();
        const int start = n - fillSteps;

        for (int s = start; s < n; ++s)
            bar[(size_t) s] = Step();

        for (int rep = 0; rep < 3; ++rep)
            for (int k = 0; k < m; ++k)
            {
                auto& st = bar[(size_t) (start + rep * (m + g) + k)];
                st.hit = true;
                st.articulation = motif[(size_t) k];
                const bool lastStrike = rep == 2 && k == m - 1;
                st.dyn = (k == 0 || lastStrike) ? Dynamic::Accent : Dynamic::Normal;
                st.velocity = juce::jlimit (0.3f, 1.0f, 0.62f + 0.12f * (float) rep + (k == 0 ? 0.1f : 0.0f) + 0.1f * energy);
            }
        return true;
    }

    /** Phrase-end fill over the last `fillSteps` steps, crescendo into the next downbeat. */
    void applyFill (Bar& bar, InstrumentId id, int fillSteps, int spb, float energy, float complexity, Rng& rng)
    {
        const int n = (int) bar.size();
        fillSteps = juce::jlimit (1, n - 1, fillSteps);

        // Tihai cadences for the drums that speak bols.
        if (fillSteps >= 5 && rng.chance (0.3 + 0.35 * complexity) && applyTihai (bar, id, fillSteps, energy, rng))
            return;

        const int start = n - fillSteps;
        const int variant = rng.integer (3);   // fill shape

        for (int s = start; s < n; ++s)
            bar[(size_t) s] = Step();

        auto set = [&] (int s, int art, Dynamic dyn, float vel, bool roll = false)
        {
            auto& st = bar[(size_t) s];
            st.hit = true;
            st.articulation = art;
            st.dyn = dyn;
            st.velocity = vel;
            st.roll = roll;
        };

        for (int i = 0; i < fillSteps; ++i)
        {
            const int s = start + i;
            const float t = (float) (i + 1) / (float) fillSteps;
            const float vel = 0.45f + 0.5f * t * (0.7f + 0.3f * energy);
            const bool last = (i == fillSteps - 1);
            const bool groupStart = (i % spb) == 0;
            const bool allowRoll = complexity > 0.55f && rng.chance (complexity * 0.5);

            switch (id)
            {
                case InstrumentId::Tabla:
                {
                    static const int tirakita[] = { TablaArt::Te, TablaArt::Re, TablaArt::Ka, TablaArt::Na };
                    static const int dhere[]    = { TablaArt::Te, TablaArt::Re, TablaArt::Te, TablaArt::Re };
                    static const int gumki[]    = { TablaArt::Ghe, TablaArt::Na, TablaArt::Na, TablaArt::Tin };
                    const int* seq = variant == 0 ? tirakita : (variant == 1 ? dhere : gumki);
                    if (last)                                        set (s, TablaArt::Dha, Dynamic::Accent, 0.95f);
                    else if (groupStart && rng.chance (0.35 + complexity * 0.4))
                                                                     set (s, variant == 2 ? TablaArt::Ghe : TablaArt::Dha, Dynamic::Accent, vel);
                    else                                             set (s, seq[i % 4], Dynamic::Normal, vel, allowRoll && (i % 2 == 1));
                    break;
                }
                case InstrumentId::Dhol:
                {
                    if (last)                                              set (s, DholArt::Both, Dynamic::Accent, 1.0f);
                    else if (variant == 0 && i % 3 == 0 && energy > 0.45f) set (s, DholArt::Dagga, Dynamic::Normal, vel);
                    else if (variant == 1 && i % 2 == 0)                   set (s, DholArt::Both, Dynamic::Normal, vel);
                    else if (variant == 2)                                 set (s, DholArt::Tilli, Dynamic::Normal, vel, i >= fillSteps / 2);
                    else                                                   set (s, DholArt::Tilli, Dynamic::Normal, vel, allowRoll && i >= fillSteps - 3);
                    break;
                }
                case InstrumentId::Dholak:
                {
                    if (last)               set (s, DholakArt::GeSlide, Dynamic::Accent, 0.95f);
                    else if (groupStart)    set (s, DholakArt::Ge, Dynamic::Normal, vel);
                    else                    set (s, (i & 1) ? DholakArt::Na : DholakArt::Ta, Dynamic::Normal, vel, allowRoll && (i & 1));
                    break;
                }
                case InstrumentId::Manjira:
                {
                    if (last)                                     set (s, ManjiraArt::Open, Dynamic::Accent, 0.9f);
                    else if (fillSteps <= 4 || (i & 1) == 0)      set (s, ManjiraArt::Closed, Dynamic::Normal, vel * 0.85f);
                    break;
                }
                case InstrumentId::Ghungroo:
                {
                    if (i == 0)                 set (s, GhungrooArt::LongShake, Dynamic::Normal, vel);
                    else if (last)              set (s, GhungrooArt::Shake, Dynamic::Accent, 0.9f);
                    else if ((i & 1) == 0)      set (s, GhungrooArt::Shake, Dynamic::Normal, vel);
                    break;
                }
                case InstrumentId::Taali:
                {
                    if (last)                                    set (s, TaaliArt::Group, Dynamic::Accent, 0.95f);
                    else if (energy > 0.7f || (i & 1) == 1)      set (s, TaaliArt::Clap, Dynamic::Normal, vel);
                    break;
                }
                default:
                    break;
            }
        }
    }

    //==========================================================================
    /** Texture instrument on a short cycle that runs against the bar (3, 5, 6 or 7 sixteenths). */
    std::vector<Bar> buildPolymetric (InstrumentId id, const GenerationContext& ctx, Rng& rng,
                                      float density, juce::String& label)
    {
        const auto& style = *ctx.style;
        std::vector<int> options;
        switch (style.id)
        {
            case StyleId::AfroIndian:        options = { 3, 3, 6 }; break;
            case StyleId::IndianUnderground: options = { 5, 7, 3 }; break;
            case StyleId::TribalRitual:      options = { 3, 5 }; break;
            case StyleId::MinimalHypnosis:   options = { 3, 5, 7 }; break;
            case StyleId::PsychedelicIndia:  options = { 3, 5, 7 }; break;
            default:                         options = { 3 }; break;
        }
        int len = options[(size_t) rng.integer ((int) options.size())];
        if (id == InstrumentId::Taali && len < 5)
            len = 5;

        const int bars = ctx.settings.bars;
        const int reset = bars >= 2 ? 32 : 16;   // cycle realigns every 1-2 bars, so the loop stays seamless
        const auto uThin = stableDraws (hashCombine (rng.next(), 7), 64);

        // Which cycle positions sound (beyond the accented first one).
        std::vector<bool> inner ((size_t) len, false);
        for (int c = 2; c < len; c += 2)
            inner[(size_t) c] = density > 0.35f;

        std::vector<Bar> out;
        for (int b = 0; b < bars; ++b)
        {
            Bar bar (16);
            for (int s = 0; s < 16; ++s)
            {
                const int g = (b * 16 + s) % reset;
                const int c = g % len;
                auto& st = bar[(size_t) s];

                if (id == InstrumentId::Ghungroo)
                {
                    // Continuous shaker with accents on the cycle: the classic hypnotic 3-over-4.
                    if (c == 0)                         { st.hit = true; st.dyn = Dynamic::Accent; st.articulation = GhungrooArt::Shake; }
                    else if (uThin[(size_t) g] < 0.25f + density * 0.75f)
                                                        { st.hit = true; st.dyn = Dynamic::Ghost; st.articulation = GhungrooArt::Shake; }
                }
                else if (c == 0)
                {
                    st.hit = true;
                    st.dyn = Dynamic::Accent;
                    st.articulation = id == InstrumentId::Manjira ? (int) ManjiraArt::Open : (int) TaaliArt::Clap;
                }
                else if (inner[(size_t) c] && id != InstrumentId::Taali)
                {
                    st.hit = true;
                    st.dyn = Dynamic::Normal;
                    st.articulation = id == InstrumentId::Manjira ? (int) ManjiraArt::Closed : (int) TaaliArt::Clap;
                }
            }
            out.push_back (bar);
        }

        label = "Polymeter " + juce::String (len) + " over 16";
        return out;
    }

    char phraseLabel (int bar, int bars, float variation, float fillDrive)
    {
        switch (bars)
        {
            case 1:  return fillDrive > 0.5f ? 'P' : 'A';
            case 2:  return bar == 0 ? 'A' : (fillDrive > 0.25f ? 'P' : '1');
            case 4:
            {
                static const char l[] = { 'A', '1', 'A', 'F' };
                if (bar == 2 && variation > 0.55f) return 'B';
                return l[bar];
            }
            default:
            {
                static const char l[] = { 'A', '1', 'A', 'P', 'A', '1', '2', 'F' };
                if (bar == 6 && variation > 0.45f) return 'B';
                return l[bar % 8];
            }
        }
    }
}

//==============================================================================
Part RhythmGenerator::generate (InstrumentId id, uint64_t seed, const GenerationContext& ctx)
{
    Part part;
    part.instrument = id;
    part.seed = seed;

    const auto& cs = ctx.settings;
    const auto& style = *ctx.style;
    const auto& fw = framework (ctx.framework);
    Rng rng (seed);

    const int bars = cs.bars;
    const float density = effectiveDensity (id, ctx);
    const float complexity = cs.complexity;
    const float energy = cs.energy;
    const float variation = clamp01 (cs.variation * (1.0f - 0.7f * style.repetition) + 0.05f);
    const float fillDrive = style.fillIntensity * (0.4f + energy * 0.8f);

    std::vector<Bar> phrase;
    int spb = 4;
    juce::StringArray tags;

    const bool usePoly = isTexture (id) && ! fw.triplet
                      && rng.chance (style.polymeterChance * (0.35f + complexity * 0.8f));

    if (usePoly)
    {
        juce::String label;
        phrase = buildPolymetric (id, ctx, rng, density, label);
        tags.add (label);

        if (bars >= 4 && fillDrive > 0.3f)
            applyFill (phrase.back(), id, 2, 4, energy, complexity, rng);
    }
    else
    {
        const PatternCell* cell = chooseCell (id, ctx, rng, nullptr, 0);
        if (cell == nullptr)
            return part;

        spb = cell->stepsPerBeat;
        const int stepsPerBar = 4 * spb;
        const auto parsed = parseCell (*cell);
        const int cBars = cellBars (*cell);

        std::vector<Bar> base;
        for (int i = 0; i < cBars; ++i)
        {
            base.push_back (barFromCell (parsed, i, stepsPerBar));
            embellish (base.back(), id, ctx, fw, density, hashCombine (seed, 100 + (uint64_t) i));
        }

        // A related cell supplies a contrasting second half ("B" bars).
        Bar altBar = base[0];
        if (const auto* alt = chooseCell (id, ctx, rng, cell, spb))
        {
            altBar = barFromCell (parseCell (*alt), 0, stepsPerBar);
            embellish (altBar, id, ctx, fw, density, hashCombine (seed, 200));
        }

        for (int b = 0; b < bars; ++b)
        {
            Bar bar = base[(size_t) (b % cBars)];
            Rng barRng = rng.fork ((uint64_t) b);
            const char label = phraseLabel (b, bars, variation, fillDrive);

            switch (label)
            {
                case '1':
                    mutate (bar, id, variation, complexity, barRng, fw);
                    break;
                case '2':
                    mutate (bar, id, variation * 1.6f, complexity, barRng, fw);
                    break;
                case 'B':
                    for (int s = stepsPerBar / 2; s < stepsPerBar; ++s)
                        bar[(size_t) s] = altBar[(size_t) s];
                    mutate (bar, id, variation * 0.5f, complexity, barRng, fw);
                    break;
                case 'P':
                {
                    mutate (bar, id, variation * 0.7f, complexity, barRng, fw);
                    const int steps = energy > 0.6f ? spb : std::max (1, spb / 2);
                    applyFill (bar, id, steps, spb, energy * 0.8f, complexity, barRng);
                    break;
                }
                case 'F':
                {
                    mutate (bar, id, variation * 0.8f, complexity, barRng, fw);
                    const int maxBeats = bars <= 2 ? 2 : 3;
                    const int beats = juce::jlimit (1, maxBeats, (int) std::lround (0.5f + fillDrive * 2.0f));
                    applyFill (bar, id, beats * spb, spb, energy, complexity, barRng);
                    break;
                }
                default:
                    break;
            }

            phrase.push_back (bar);
        }

        tags.add (cell->label);
    }

    //--------------------------------------------------------------------------
    // Grid -> events
    const int stepsPerBar = 4 * spb;
    const double stepBeats = 1.0 / spb;
    int ghostCount = 0, accentCount = 0;

    for (int b = 0; b < (int) phrase.size(); ++b)
    {
        const auto& bar = phrase[(size_t) b];
        for (int s = 0; s < stepsPerBar && s < (int) bar.size(); ++s)
        {
            const auto& st = bar[(size_t) s];
            if (! st.hit)
                continue;

            const float w = metricWeight (fw, s, stepsPerBar);
            float vel = st.velocity;
            if (vel < 0.0f)
            {
                switch (st.dyn)
                {
                    case Dynamic::Ghost:  vel = 0.26f + 0.1f * rng.uniformf(); break;
                    case Dynamic::Accent: vel = 0.9f + 0.1f * energy; break;
                    default:              vel = 0.58f + 0.2f * w + 0.1f * energy; break;
                }
            }

            ghostCount += st.dyn == Dynamic::Ghost ? 1 : 0;
            accentCount += st.dyn == Dynamic::Accent ? 1 : 0;

            NoteEvent e;
            e.beat = b * Arrangement::beatsPerBar + s * stepBeats;
            e.length = stepBeats;
            e.velocity = juce::jlimit (0.05f, 1.0f, vel);
            e.dyn = st.dyn;
            e.articulation = st.articulation;
            e.swingable = (spb == 4);
            e.seed = rng.next32();

            if (st.roll)
            {
                // Two strokes in the space of one step (32nds / triplet 16ths).
                NoteEvent first = e;
                first.velocity *= 0.8f;
                first.length = stepBeats * 0.5;
                part.events.push_back (first);

                NoteEvent second = e;
                second.beat += stepBeats * 0.5;
                second.length = stepBeats * 0.5;
                second.velocity *= 0.7f;
                second.dyn = Dynamic::Normal;
                second.articulation = siblingArticulation (id, e.articulation);
                second.seed = rng.next32();
                part.events.push_back (second);
            }
            else
            {
                part.events.push_back (e);
            }
        }
    }

    std::sort (part.events.begin(), part.events.end(),
               [] (const NoteEvent& a, const NoteEvent& b) { return a.beat < b.beat; });

    if (spb == 3) tags.add ("triplet grid");
    if (complexity > 0.6f && ghostCount > bars * 2) tags.add ("ghosts");
    if (accentCount > bars * 4) tags.add ("accented");
    part.summary = tags.joinIntoString (" | ");
    return part;
}

} // namespace jgl
