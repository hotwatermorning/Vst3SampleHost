#include "TestSynth.hpp"
#include "../misc/MathUtil.hpp"

NS_HWM_BEGIN

struct TestSynth::WaveTable
{
    WaveTable(int length_as_shift_size)
    {
        assert(length_as_shift_size <= 31);
        
        shift_ = length_as_shift_size;
        wave_data_.resize(1 << shift_);
    }
    
    //! Get the table value of the specified position.
    //! @param pos is a normalized pos [0.0 - 1.0)
    AudioSample GetValueSimple(double pos) const
    {
        auto const k = 1 << shift_;
        auto const sample_pos = (SampleCount)std::round(pos * k) & (k - 1);
        assert(0 <= sample_pos && sample_pos < wave_data_.size());
        return wave_data_[sample_pos];
    }
    
    std::vector<AudioSample> wave_data_;
    int shift_;
};

double TestSynth::poly_blep(double t, double dt)
{
    // t-t^2/2 +1/2
    // 0 < t <= 1
    // discontinuities between 0 & 1
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    
    // t^2/2 +t +1/2
    // -1 <= t <= 0
    // discontinuities between -1 & 0
    else if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    
    // no discontinuities
    // 0 otherwise
    else return 0.0;
}

double TestSynth::note_number_to_freq(int n) {
    return 440 * pow(2, (n - 69) / 12.0);
};

TestSynth::WaveTablePtr TestSynth::GenerateSinTable()
{
    auto table = std::make_unique<WaveTable>(16);
    auto data = table->wave_data_.data();
    auto const len = table->wave_data_.size();
    for(int smp = 0; smp < len; ++smp) {
        data[smp] = sin(2 * M_PI * smp / (double)len);
    }
    
    return table;
}

TestSynth::WaveTable const * TestSynth::GetSinTable()
{
    static WaveTablePtr table = GenerateSinTable();
    return table.get();
}

TestSynth::Voice::Voice(OscillatorType ot,
                        double sample_rate,
                        double freq,
                        Int32 num_attack_samples,
                        Int32 num_release_samples)
{
    num_attack_samples_ = num_attack_samples;
    num_release_samples_ = num_release_samples;
    ot_ = ot;
    freq_ = freq;
    sample_rate_ = sample_rate;
}

double TestSynth::Voice::get_current_gain() const
{
    double const kVolumeRange = 48.0;
    
    double gain = 1.0;
    if(state_ == State::kAttack) {
        assert(attack_sample_pos_ < num_attack_samples_);
        if(attack_sample_pos_ == 0) {
            return 0; // gain == 0
        } else {
            gain = DBToLinear((attack_sample_pos_ / (double)num_attack_samples_) * kVolumeRange - kVolumeRange);
        }
    } else if(state_ == State::kSustain) {
        // do nothing.
    } else if(state_ == State::kRelease) {
        assert(release_sample_pos_ < num_release_samples_);
        auto attack_pos_ratio = attack_sample_pos_ / (double)num_attack_samples_;
        auto release_pos_ratio = Clamp<double>((release_sample_pos_ / (double)num_release_samples_) + (1 - attack_pos_ratio),
                                               -1.0, 1.0);
        gain = DBToLinear(release_pos_ratio * -kVolumeRange);
    }
    
    return gain;
}

//! generate one sample and advance one step.
AudioSample TestSynth::Voice::generate()
{
    if(is_alive() == false) { return 0; }
    
    auto const gain = get_current_gain();
    
    auto const dt = freq_ / sample_rate_;
    auto const t = t_;
    
    double smp = 0;
    
    // based on http://www.martin-finke.de/blog/articles/audio-plugins-018-polyblep-oscillator/
    if(ot_ == OscillatorType::kSine) {
        auto const wave = GetSinTable();
        assert(wave);
        smp = wave->GetValueSimple(t);
    } else if(ot_ == OscillatorType::kSaw) {
        smp = ((2 * t) - 1.0) - poly_blep(t, dt);
    } else if(ot_ == OscillatorType::kSquare) {
        smp = t < 0.5 ? 1.0 : -1.0;
        smp += poly_blep(t, dt);
        smp -= poly_blep(fmod(t + 0.5, 1.0), dt);
    } else if(ot_ == OscillatorType::kTriangle) {
        smp = t < 0.5 ? 1.0 : -1.0;
        smp += poly_blep(t, dt);
        smp -= poly_blep(fmod(t + 0.5, 1.0), dt);
        
        auto dt2pi = dt * 2 * M_PI;
        smp = dt2pi * smp + (1 - dt2pi) * last_smp_;
        last_smp_ = smp;
    }
    
    // advance one step
    t_ += dt;
    while(t_ >= 1.0) {
        t_ -= 1.0;
    }
    
    advance_envelope();
    
    double const master_volume = 0.5;
    return smp * gain * master_volume;
}

bool TestSynth::Voice::is_alive() const { return state_ != State::kFinished; }

void TestSynth::Voice::start()
{
    t_ = 0;
    last_smp_ = 0;
    attack_sample_pos_ = 0;
    release_sample_pos_ = 0;
    state_ = State::kAttack;
}

void TestSynth::Voice::force_stop()
{
    state_ = State::kFinished;
}

void TestSynth::Voice::request_to_stop()
{
    if(state_ != State::kFinished) {
        state_ = State::kRelease;
    }
}

void TestSynth::Voice::advance_envelope()
{
    if(state_ == State::kAttack) {
        attack_sample_pos_ += 1;
        if(attack_sample_pos_ == num_attack_samples_) {
            state_ = State::kSustain;
        }
    } else if(state_ == State::kSustain) {
        // do nothing.
    } else if(state_ == State::kRelease) {
        release_sample_pos_ += 1;
        if(release_sample_pos_ == num_release_samples_) {
            state_ = State::kFinished;
        }
    }
}

TestSynth::TestSynth()
{
    ot_ = OscillatorType::kSine;
    
    auto null = NoteStatus::CreateNull();
    for(auto &key: keys_) {
        key = null;
    }
}

TestSynth::~TestSynth()
{}

void TestSynth::SetSampleRate(double sample_rate)
{
    sample_rate_ = sample_rate;
    num_attack_samples_ = std::round(sample_rate * 0.03);
    num_release_samples_ = std::round(sample_rate * 0.03);
    
    for(auto &v: voices_) { v.force_stop(); }
    
    GetSinTable(); // force generate the sin table.
}

void TestSynth::SetOscillatorType(OscillatorType ot)
{
    ot_.store(ot);
}

OscillatorType TestSynth::GetOscillatorType() const
{
    return ot_.load();
}

void TestSynth::Process(AudioSample *dest, SampleCount length)
{
    for(int n = 0; n < 128; ++n) {
        auto st = keys_[n].load();
        if(st.is_note_on() && voices_[n].is_alive() == false) {
            voices_[n] = Voice {
                ot_, sample_rate_, note_number_to_freq(n),
                num_attack_samples_, num_release_samples_
            };
            voices_[n].start();
        } else if(st.is_note_off()) {
            voices_[n].request_to_stop();
        }
        
        if(voices_[n].is_alive() == false) { continue; }
        
        auto &v = voices_[n];
        for(int smp = 0; smp < length; ++smp) {
            dest[smp] += v.generate();
        }
    }
}

NS_HWM_END
