#pragma once

#include "./NoteStatus.hpp"
#include "./OscillatorType.hpp"

NS_HWM_BEGIN

struct TestSynth
{
    struct WaveTable;
    using WaveTablePtr = std::shared_ptr<WaveTable>;
    
    //! This function calculates the PolyBLEPs
    //! @param t 現在の位相。[0..1]で、[0..2*PI]を表す
    //! @param dt 目的のピッチに対応する角速度を正規化周波数で表したもの。
    static
    double poly_blep(double t, double dt);
    
    static
    double note_number_to_freq(int n);
    
    static
    WaveTablePtr GenerateSinTable();
    
    static
    WaveTable const * GetSinTable();
    
    struct Voice
    {
        Voice() = default;
        
        Voice(OscillatorType ot,
              double sample_rate,
              double freq,
              Int32 num_attack_samples,
              Int32 num_release_samples);
        
        double get_current_gain() const;
        
        //! generate one sample and advance one step.
        AudioSample generate();
        
        bool is_alive() const;
        void start();
        void force_stop();
        void request_to_stop();
        
    private:
        Int32 num_attack_samples_ = 0;
        Int32 attack_sample_pos_ = 0;
        Int32 num_release_samples_ = 0;
        Int32 release_sample_pos_ = 0;
        enum class State {
            kAttack,
            kSustain,
            kRelease,
            kFinished,
        };
        State state_ = State::kFinished;
        
        OscillatorType ot_;
        double freq_;
        double sample_rate_;
        double t_ = 0;
        double last_smp_ = 0;
        
        void advance_envelope();
    };
    
    TestSynth();
    ~TestSynth();
    
    std::array<Voice, 128> voices_;
    using KeyboardStatus = std::array<std::atomic<NoteStatus>, 128>;
    KeyboardStatus keys_;
    double sample_rate_ = 0;
    std::atomic<OscillatorType> ot_;
    Int32 num_attack_samples_ = 0;
    Int32 num_release_samples_ = 0;
    
    // デバイスダイアログを閉じて、デバイスを開始する前に段階で呼び出す
    void SetSampleRate(double sample_rate);
    
    void SetOscillatorType(OscillatorType ot);
    OscillatorType GetOscillatorType() const;
    
    void Process(AudioSample *dest, SampleCount length);
};

NS_HWM_END
