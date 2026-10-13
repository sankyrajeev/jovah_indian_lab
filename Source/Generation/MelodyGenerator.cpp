#include "MelodyGenerator.h"
#include "../Core/Rng.h"

#include <algorithm>

namespace jgl
{

namespace
{
    struct RhythmNote
    {
        double start;    // beats, relative to the segment start
        double length;   // beats
        bool accent;
    };

    /** Parse "x--- x.x-" style cells into notes, limited to `beats` beats. */
    std::vector<RhythmNote> parseRhythm (const MelodyRhythmCell& cell, double beats)
    {
        const auto p = stripNotation (cell.pattern);
        const double stepBeats = 1.0 / cell.stepsPerBeat;
        const int maxSteps = juce::jmin (p.length(), (int) std::lround (beats * cell.stepsPerBeat));

        std::vector<RhythmNote> notes;
        for (int i = 0; i < maxSteps; ++i)
        {
            const auto c = p[i];
            if (c == 'x' || c == 'X')
            {
                notes.push_back ({ i * stepBeats, stepBeats, c == 'X' });
            }
            else if (c == '-' && ! notes.empty())
            {
                auto& last = notes.back();
                if (std::abs ((last.start + last.length) - i * stepBeats) < 1.0e-6)
                    last.length += stepBeats;
            }
        }
        return notes;
    }

    const MelodyRhythmCell& chooseRhythm (const GenerationContext& ctx, Rng& rng, bool riff, bool sustained)
    {
        const auto& cells = melodyRhythmCells();
        const auto sBit = styleBit (ctx.style->id);
        const bool custom = ctx.style->id == StyleId::Custom;
        const bool triplet = framework (ctx.framework).triplet;

        std::vector<double> w (cells.size(), 0.0);
        for (size_t i = 0; i < cells.size(); ++i)
        {
            const auto& c = cells[i];
            if (c.riff != riff) continue;
            double x = (custom || (c.styles & sBit)) ? 1.0 : 0.15;
            const float d = (c.energy - ctx.settings.energy) / 0.35f;
            x *= std::exp (-d * d);
            if (c.stepsPerBeat == 3) x *= triplet ? 3.0 : 0.25;
            if (sustained)
            {
                const auto p = stripNotation (c.pattern);
                const int sustains = p.length() - p.removeCharacters ("-").length();
                x *= 0.4 + sustains * 0.12;
            }
            w[i] = x;
        }

        int idx = rng.weightedIndex (w);
        if (idx < 0) idx = 0;
        return cells[(size_t) idx];
    }

    /** Index of `pitch` in an ordered list of scale pitches (nearest if absent). */
    int indexOf (const std::vector<int>& pitches, int pitch)
    {
        int best = 0;
        for (int i = 0; i < (int) pitches.size(); ++i)
            if (std::abs (pitches[(size_t) i] - pitch) < std::abs (pitches[(size_t) best] - pitch))
                best = i;
        return best;
    }

    int nearestInSet (const std::vector<int>& pitches, int target)
    {
        return pitches[(size_t) indexOf (pitches, target)];
    }

    struct WalkParams
    {
        const ScaleProfile* scale;
        std::vector<int> pitches;   // all scale pitches in range
        float leapiness;
        float repetition;
    };

    /** Weighted pitch choice honouring aroha/avaroha and emphasis on strong beats. */
    int choosePitch (const WalkParams& wp, int prev, double target, bool strongBeat, Rng& rng)
    {
        const auto& s = *wp.scale;
        const int prevIdx = indexOf (wp.pitches, prev);

        std::vector<double> w (wp.pitches.size(), 0.0);
        for (size_t i = 0; i < wp.pitches.size(); ++i)
        {
            const int c = wp.pitches[i];
            const int stepDist = std::abs ((int) i - prevIdx);
            if (stepDist > 5) continue;

            double x = std::exp (-std::abs (c - target) / 2.5);

            switch (stepDist)
            {
                case 0:  x *= 0.25 + wp.repetition * 0.6; break;
                case 1:  x *= 1.0; break;
                case 2:  x *= 0.6; break;
                case 3:  x *= 0.15 + 0.4 * wp.leapiness; break;
                default: x *= 0.04 + 0.2 * wp.leapiness; break;
            }

            if (c > prev && ! arohaContains (s, c))   x *= 0.02;
            if (c < prev && ! avarohaContains (s, c)) x *= 0.02;

            const int pc = pitchClass (c);
            if (strongBeat && (pc == 0 || pc == pitchClass (s.vadi) || pc == pitchClass (s.samvadi) || pc == 7))
                x *= 1.7;
            if (pc == pitchClass (s.vadi))
                x *= 1.25;

            w[i] = x;
        }

        const int idx = rng.weightedIndex (w);
        return idx >= 0 ? wp.pitches[(size_t) idx] : prev;
    }

    /** Ensure a descending/ascending step into `next` respects the raga direction; otherwise use a neighbour that does. */
    int fixDirection (const WalkParams& wp, int prev, int next)
    {
        const auto& s = *wp.scale;
        if (next > prev && ! arohaContains (s, next))
        {
            for (int d = 1; d < 4; ++d)
                for (int cand : { next + d, next - d })
                    if (cand > prev && scaleContains (s, cand) && arohaContains (s, cand)) return cand;
        }
        if (next < prev && ! avarohaContains (s, next))
        {
            for (int d = 1; d < 4; ++d)
                for (int cand : { next - d, next + d })
                    if (cand < prev && scaleContains (s, cand) && avarohaContains (s, cand)) return cand;
        }
        return next;
    }

    /** Direction-safe, in-range, in-scale step from `prev` towards `p`. Falls back to repeating `prev`. */
    int safeStep (const WalkParams& wp, int prev, int p)
    {
        const auto& s = *wp.scale;
        p = fixDirection (wp, prev, p);
        if (p < wp.pitches.front() || p > wp.pitches.back() || ! scaleContains (s, p))
            p = nearestInSet (wp.pitches, p);
        const bool legal = p == prev || (p > prev ? arohaContains (s, p) : avarohaContains (s, p));
        return legal ? p : prev;
    }

    int upperNeighbour (const ScaleProfile& s, int p) { return stepInScale (s, p, 1); }
    int lowerNeighbour (const ScaleProfile& s, int p) { return stepInScale (s, p, -1); }

    /** Pick a call's ending note: something open (Pa, samvadi, vadi, Re), in the octave closest to `near`. */
    int callEnding (const ScaleProfile& s, const std::vector<int>& pitches, int near, Rng& rng)
    {
        std::vector<int> options;
        std::vector<double> weights;
        for (int pc : { 7, s.samvadi, s.vadi, 2, 4, 3 })
            for (int oct : { -12, 0, 12 })
            {
                const int p = pc + oct;
                if (pc == 0 || ! scaleContains (s, p) || std::find (pitches.begin(), pitches.end(), p) == pitches.end())
                    continue;
                options.push_back (p);
                weights.push_back (std::exp (-std::abs (p - near) / 3.0) * (pc == 7 || pc == s.samvadi ? 1.5 : 1.0));
            }
        if (options.empty())
            return nearestInSet (pitches, 7);
        return options[(size_t) std::max (0, rng.weightedIndex (weights))];
    }

    //==========================================================================
    void addNote (Part& part, double beat, double length, int pitch, float vel, Dynamic dyn, Rng& rng)
    {
        NoteEvent e;
        e.beat = beat;
        e.length = std::max (0.05, length);
        e.pitch = pitch;
        e.velocity = juce::jlimit (0.1f, 1.0f, vel);
        e.dyn = dyn;
        e.swingable = std::abs (beat * 4.0 - std::round (beat * 4.0)) < 1.0e-6;
        e.seed = rng.next32();
        part.events.push_back (e);
    }

    /** Applies instrument-specific ornaments to an already-pitched phrase. */
    void ornament (Part& part, InstrumentId id, const ScaleProfile& s, float chance, Rng& rng)
    {
        auto& ev = part.events;
        for (size_t i = 0; i < ev.size(); ++i)
        {
            auto& e = ev[i];
            const bool hasPrev = i > 0 && (e.beat - (ev[i - 1].beat + ev[i - 1].length)) < 0.26;
            const int prevPitch = hasPrev ? ev[i - 1].pitch : e.pitch;
            const int interval = e.pitch - prevPitch;

            switch (id)
            {
                case InstrumentId::Sitar:
                    if (hasPrev && std::abs (interval) >= 2 && std::abs (interval) <= 7 && rng.chance (chance * 0.75))
                    {
                        e.glideFrom = (float) -interval;   // meend: pull from the previous note
                        e.glideBeats = (float) std::min (0.3, e.length * 0.45);
                    }
                    else if (e.length >= 0.75 && rng.chance (chance * 0.35))
                    {
                        e.ornament = OrnMurki;
                        e.ornamentPitch = upperNeighbour (s, e.pitch);
                    }
                    else if (e.length >= 0.5 && rng.chance (chance * 0.25))
                    {
                        const int below = lowerNeighbour (s, e.pitch);
                        e.glideFrom = (float) (below - e.pitch);
                        e.glideBeats = 0.12f;
                    }
                    break;

                case InstrumentId::Bansuri:
                    if ((! hasPrev || e.length >= 0.5) && rng.chance (chance * 0.6))
                    {
                        e.ornament = OrnKan;
                        e.ornamentPitch = upperNeighbour (s, e.pitch);
                    }
                    else if (hasPrev && std::abs (interval) <= 3 && interval != 0 && rng.chance (chance * 0.4))
                    {
                        e.glideFrom = (float) -interval;
                        e.glideBeats = 0.15f;
                    }
                    break;

                case InstrumentId::Nadaswaram:
                    if (e.length >= 0.75 && rng.chance (chance * 0.7))
                    {
                        e.ornament = OrnGamaka;      // shaking the note: the nadaswaram's signature
                        e.ornamentPitch = upperNeighbour (s, e.pitch);
                    }
                    else if (hasPrev && std::abs (interval) >= 1 && std::abs (interval) <= 7 && rng.chance (chance * 0.8))
                    {
                        e.glideFrom = (float) -interval;
                        e.glideBeats = (float) std::min (0.35, e.length * 0.5);
                    }
                    else if (rng.chance (chance * 0.4))
                    {
                        e.ornament = OrnKan;
                        e.ornamentPitch = upperNeighbour (s, e.pitch);
                    }
                    break;

                case InstrumentId::Tumbi:
                    if (e.dyn == Dynamic::Accent && rng.chance (chance * 0.4))
                    {
                        e.ornament = OrnSlideIn;
                        e.glideFrom = -2.0f;
                        e.glideBeats = 0.08f;
                    }
                    break;

                default:
                    break;
            }
        }
    }

    /** Final pass: every melodic move inside the loop obeys aroha (up) / avaroha (down). */
    void enforceDirection (Part& part, const WalkParams& wp)
    {
        auto& ev = part.events;
        for (size_t i = 1; i < ev.size(); ++i)
        {
            const int prev = ev[i - 1].pitch;
            auto& e = ev[i];
            if (e.pitch == prev) continue;
            const bool legal = e.pitch > prev ? arohaContains (*wp.scale, e.pitch) : avarohaContains (*wp.scale, e.pitch);
            if (! legal)
            {
                const int fixedPitch = safeStep (wp, prev, e.pitch);
                if (e.glideFrom != 0.0f)
                    e.glideFrom = (float) (prev - fixedPitch);
                e.pitch = fixedPitch;
            }
        }
    }

    //==========================================================================
    void generateDrone (Part& part, const GenerationContext& ctx, Rng& rng)
    {
        const auto& s = *ctx.scale;
        const int first = s.tanpuraString;
        const int strings[] = { first, 0, 0, -12 };
        const double times[] = { 0.0, 1.0, 2.0, 3.0 };

        for (int b = 0; b < ctx.settings.bars; ++b)
            for (int k = 0; k < 4; ++k)
                addNote (part, b * 4.0 + times[k], 4.0, strings[k],
                         k == 0 ? 0.68f : 0.58f + 0.06f * rng.uniformf(),
                         k == 0 ? Dynamic::Accent : Dynamic::Normal, rng);

        part.summary = "Tanpura cycle: " + juce::String (first == -5 ? "Pa" : first == -7 ? "Ma" : "Ni") + " Sa Sa Sa";
    }

    void generateRiff (Part& part, InstrumentId id, const GenerationContext& ctx, const WalkParams& wp, Rng& rng)
    {
        const auto& s = *ctx.scale;
        const auto& mb = ctx.style->melody;
        const auto& cell = chooseRhythm (ctx, rng, true, false);
        auto rhythm = parseRhythm (cell, 4.0);
        if (rhythm.empty())
            return;

        // Density: drop some unaccented onsets when sparse.
        const auto uDrop = stableDraws (hashCombine (part.seed, 31), (int) rhythm.size());
        const float density = ctx.settings.density * (0.6f + mb.noteDensity * 0.8f);
        std::vector<RhythmNote> kept;
        for (size_t i = 0; i < rhythm.size(); ++i)
            if (i == 0 || rhythm[i].accent || uDrop[i] > (0.55f - density) * 1.2f)
                kept.push_back (rhythm[i]);
        rhythm = kept;

        // Hook pitches: a pedal plus close neighbours, accents land on pedal / Pa.
        const int pedal = nearestInSet (wp.pitches, rng.chance (0.7) ? 0 : 7);
        std::vector<int> pool { pedal, stepInScale (s, pedal, 1), stepInScale (s, pedal, 2), stepInScale (s, pedal, -1) };
        if (scaleContains (s, pedal + 7) && pedal + 7 <= wp.pitches.back()) pool.push_back (pedal + 7);
        for (auto& p : pool) p = nearestInSet (wp.pitches, p);

        std::vector<int> hook;
        int prev = pedal;
        for (size_t i = 0; i < rhythm.size(); ++i)
        {
            int p;
            if (i == 0 || rhythm[i].accent)
                p = rng.chance (0.75) ? pedal : pool[(size_t) rng.integer ((int) pool.size())];
            else
            {
                std::vector<double> w;
                for (int c : pool)
                    w.push_back ((c == prev ? 0.5 : 1.0) * (c == pedal ? 1.4 : 1.0) * std::exp (-std::abs (c - prev) / 4.0));
                p = pool[(size_t) std::max (0, rng.weightedIndex (w))];
            }
            p = safeStep (wp, prev, p);
            hook.push_back (p);
            prev = p;
        }

        const bool staccato = id == InstrumentId::Tumbi;
        const float variation = ctx.settings.variation * (1.0f - 0.6f * ctx.style->repetition);
        const int bars = ctx.settings.bars;

        for (int b = 0; b < bars; ++b)
        {
            auto pitches = hook;
            Rng barRng = rng.fork ((uint64_t) b + 77);

            const bool phraseEnd = (b == bars - 1 && bars > 1) || (bars >= 4 && b % 4 == 3);
            const bool answerBar = (b % 2 == 1);

            if (answerBar && pitches.size() > 2 && barRng.chance (0.3 + variation * 0.6))
            {
                // Answer: change the tail of the hook.
                const size_t from = pitches.size() - std::min<size_t> (2, pitches.size() - 1);
                for (size_t i = from; i < pitches.size(); ++i)
                    pitches[i] = safeStep (wp, pitches[i - 1], choosePitch (wp, pitches[i - 1], pedal - 2, false, barRng));
            }
            if (phraseEnd && pitches.size() > 2 && barRng.chance (0.4 + variation * 0.5))
            {
                // Phrase-end lift: step up towards Pa / Sa'.
                const int target = nearestInSet (wp.pitches, pedal + 7);
                const size_t from = pitches.size() - std::min<size_t> (3, pitches.size() - 1);
                for (size_t i = from; i < pitches.size(); ++i)
                    pitches[i] = safeStep (wp, pitches[i - 1], choosePitch (wp, pitches[i - 1], target, false, barRng));
            }

            for (size_t i = 0; i < rhythm.size(); ++i)
            {
                const auto& r = rhythm[i];
                const double nextStart = i + 1 < rhythm.size() ? rhythm[i + 1].start : 4.0;
                double len = std::min (r.length, nextStart - r.start);
                if (staccato) len = std::min (len, 0.2);
                else          len = (nextStart - r.start) * (0.6 + 0.4 * mb.legato);

                addNote (part, b * 4.0 + r.start, len, pitches[i],
                         r.accent ? 0.9f : 0.68f + 0.1f * barRng.uniformf(),
                         r.accent ? Dynamic::Accent : Dynamic::Normal, barRng);
            }
        }

        part.summary = juce::String (staccato ? "Riff" : "Hypnotic hook") + " on "
                     + (pedal == 0 ? "Sa" : pedal == 7 ? "Pa" : "pedal")
                     + ", " + juce::String ((int) rhythm.size()) + " notes/bar";
    }

    void generatePhrases (Part& part, InstrumentId id, MelodicRole role, const GenerationContext& ctx,
                          const WalkParams& wp, Rng& rng)
    {
        const auto& s = *ctx.scale;
        const auto& mb = ctx.style->melody;
        const int bars = ctx.settings.bars;
        const double unit = bars == 1 ? 2.0 : 4.0;   // call / response segment length in beats
        const int segments = (int) std::lround (bars * 4.0 / unit);
        const auto [answerSlot, numAnswerers] = ctx.answerSlot (id);
        const bool hasAnswerer = role == MelodicRole::Lead && numAnswerers > 0;
        const float variation = ctx.settings.variation * (1.0f - 0.5f * ctx.style->repetition);
        const bool sustained = id == InstrumentId::Bansuri || id == InstrumentId::Nadaswaram || mb.legato > 0.6f;

        // Motif rhythm shared by calls and responses.
        const auto& cell = chooseRhythm (ctx, rng, false, sustained);
        auto motif = parseRhythm (cell, unit);
        if (motif.size() < 2)
            motif = { { 0.0, unit * 0.5, true }, { unit * 0.5, unit * 0.5, false } };

        // Density: merge notes when sparse, split long notes when dense.
        {
            const float d = ctx.settings.density * 0.5f + mb.noteDensity * 0.5f;
            const auto u = stableDraws (hashCombine (part.seed, 41), (int) motif.size() * 2);
            std::vector<RhythmNote> out;
            for (size_t i = 0; i < motif.size(); ++i)
            {
                auto n = motif[i];
                if (i > 0 && ! n.accent && u[i] < (0.45f - d) * 1.4f && ! out.empty())
                {
                    out.back().length = n.start + n.length - out.back().start;
                    continue;
                }
                if (n.length >= 1.0 && u[i + motif.size()] < (d - 0.55f) * 1.5f)
                {
                    const double half = std::round (n.length * 2.0) / 4.0;
                    out.push_back ({ n.start, half, n.accent });
                    out.push_back ({ n.start + half, n.length - half, false });
                    continue;
                }
                out.push_back (n);
            }
            motif = out;
        }

        int prev = nearestInSet (wp.pitches, rng.chance (0.5) ? 0 : (rng.chance (0.5) ? 7 : s.vadi));
        std::vector<int> callPitches;

        for (int seg = 0; seg < segments; ++seg)
        {
            const bool isCall = (seg % 2 == 0);
            const double segStart = seg * unit;
            Rng segRng = rng.fork ((uint64_t) seg + 1000);

            const bool plays = role == MelodicRole::Lead ? (isCall || ! hasAnswerer)
                                                         : (! isCall && (segments / 2 < numAnswerers || (seg / 2) % std::max (1, numAnswerers) == answerSlot));   // Answer role
            if (! plays)
                continue;

            auto rhythm = motif;
            // Later segments vary the rhythm slightly.
            if (seg >= 2 && segRng.chance (variation * 0.6) && rhythm.size() > 2)
            {
                const size_t k = 1 + (size_t) segRng.integer ((int) rhythm.size() - 1);
                rhythm.erase (rhythm.begin() + (long) k);
                if (k - 1 < rhythm.size())
                {
                    const double end = k < rhythm.size() ? rhythm[k].start : unit;
                    rhythm[k - 1].length = end - rhythm[k - 1].start;
                }
            }
            if (role == MelodicRole::Answer && rhythm.size() > 3)
                rhythm.resize (3 + (size_t) segRng.integer ((int) rhythm.size() - 2));

            const int n = (int) rhythm.size();
            const bool finalSegment = seg >= segments - 2;
            int endTarget;
            if (isCall && role == MelodicRole::Lead)
                endTarget = callEnding (s, wp.pitches, prev + 3, segRng);
            else
                endTarget = nearestInSet (wp.pitches, (prev > 9 && ! finalSegment) ? 12 : 0);

            // Response re-uses the call's opening for motivic coherence.
            const bool echo = ! isCall && ! callPitches.empty() && segRng.chance (0.7);

            std::vector<int> pitches;
            int startPitch = echo ? callPitches.front() : prev;
            const double archHeight = isCall ? 2.0 + 3.0 * segRng.uniform() : -1.0;

            // Raga-inspired profiles: sometimes open with a characteristic movement.
            std::vector<int> charPhrase;
            if (s.ragaInspired && ! s.phrases.empty() && segRng.chance (0.4))
                charPhrase = s.phrases[(size_t) segRng.integer ((int) s.phrases.size())];

            for (int i = 0; i < n; ++i)
            {
                int p;
                const bool strong = std::abs (rhythm[(size_t) i].start - std::round (rhythm[(size_t) i].start)) < 1.0e-6;

                if (i < (int) charPhrase.size() && i < n - 1)
                {
                    p = charPhrase[(size_t) i];
                    while (p < wp.pitches.front()) p += 12;
                    while (p > wp.pitches.back()) p -= 12;
                    p = nearestInSet (wp.pitches, p);
                }
                else if (echo && i < 2 && i < (int) callPitches.size() && i < n - 1)
                    p = callPitches[(size_t) i];
                else if (i == n - 1)
                {
                    // Cadence chosen from where the line actually is, so it lands by step.
                    if (isCall && role == MelodicRole::Lead)
                        p = callEnding (s, wp.pitches, prev, segRng);
                    else
                    {
                        const int saBelow = nearestInSet (wp.pitches, prev - pitchClass (prev));
                        const int saAbove = nearestInSet (wp.pitches, saBelow + 12);
                        p = (std::abs (saAbove - prev) < std::abs (saBelow - prev) && ! finalSegment) ? saAbove : saBelow;
                        if (! scaleContains (s, p) || pitchClass (p) != 0) p = endTarget;
                    }
                }
                else if (i == 0)
                    p = choosePitch (wp, startPitch, startPitch, strong, segRng);
                else
                {
                    const double t = (double) i / (double) std::max (1, n - 1);
                    const double target = startPitch + (endTarget - startPitch) * t
                                        + archHeight * std::sin (juce::MathConstants<double>::pi * t);
                    p = choosePitch (wp, prev, target, strong, segRng);
                }

                // Keep leaps singable: fold anything wider than a fifth back towards the previous note.
                while (p - prev > 7 && p - 12 >= wp.pitches.front()) p -= 12;
                while (prev - p > 7 && p + 12 <= wp.pitches.back()) p += 12;
                p = i > 0 ? safeStep (wp, prev, p) : nearestInSet (wp.pitches, p);

                pitches.push_back (p);
                prev = p;
            }

            if (isCall)
                callPitches = pitches;

            for (int i = 0; i < n; ++i)
            {
                const auto& r = rhythm[(size_t) i];
                const double nextStart = i + 1 < n ? rhythm[(size_t) i + 1].start : unit;
                const double gap = nextStart - (r.start + r.length);
                double len = r.length + (gap > 0 ? gap * mb.legato : 0.0);
                if (i == n - 1)
                    len = std::max (len, (unit - r.start) * (0.6 + 0.4 * mb.legato));

                float vel = r.accent ? 0.86f : 0.7f;
                if (role == MelodicRole::Answer) vel -= 0.06f;
                if (i == n - 1) vel -= 0.04f;
                vel += 0.06f * (segRng.uniformf() - 0.5f);

                addNote (part, segStart + r.start, len, pitches[(size_t) i], vel,
                         r.accent ? Dynamic::Accent : Dynamic::Normal, segRng);
            }
        }

        juce::String desc = role == MelodicRole::Answer ? "Answers the lead" : (hasAnswerer ? "Calls" : "Call & response");
        part.summary = desc + ", " + juce::String ((int) motif.size()) + "-note motif";
        if (s.ragaInspired) part.summary << ", " << s.name;
    }
}

//==============================================================================
juce::Range<int> MelodyGenerator::pitchRange (InstrumentId id, const GenerationContext& ctx)
{
    const int span = ctx.style->melody.rangeSemis;
    switch (id)
    {
        case InstrumentId::Sitar:   return { -5, juce::jlimit (7, 17, span) };
        case InstrumentId::Bansuri: return { -3, juce::jlimit (9, 19, span + 2) };
        case InstrumentId::Nadaswaram: return { -5, juce::jlimit (9, 17, span + 3) };
        case InstrumentId::Tumbi:   return { -5, 12 };
        case InstrumentId::Tanpura: return { -12, 0 };
        default:                    return { -5, 12 };
    }
}

Part MelodyGenerator::generate (InstrumentId id, uint64_t seed, const GenerationContext& ctx)
{
    Part part;
    part.instrument = id;
    part.seed = seed;

    Rng rng (seed);
    const auto role = ctx.roleFor (id);
    const auto& s = *ctx.scale;

    if (role == MelodicRole::Drone)
    {
        generateDrone (part, ctx, rng);
        return part;
    }

    const auto range = pitchRange (id, ctx);
    WalkParams wp;
    wp.scale = &s;
    wp.pitches = scalePitchesInRange (s, range.getStart(), range.getEnd());
    wp.leapiness = ctx.style->melody.leapiness * (0.6f + ctx.settings.complexity * 0.8f);
    wp.repetition = ctx.style->repetition;

    if (wp.pitches.size() < 3)
        return part;

    const bool riff = role == MelodicRole::Riff
                   || (ctx.style->melody.hypnoticRiff && role == MelodicRole::Lead && id == InstrumentId::Sitar && rng.chance (0.6));

    if (riff)
        generateRiff (part, id, ctx, wp, rng);
    else
        generatePhrases (part, id, role, ctx, wp, rng);

    std::sort (part.events.begin(), part.events.end(),
               [] (const NoteEvent& a, const NoteEvent& b) { return a.beat < b.beat; });
    enforceDirection (part, wp);
    ornament (part, id, s, ctx.style->melody.ornamentChance * (0.6f + ctx.settings.complexity * 0.8f), rng);
    return part;
}

} // namespace jgl
