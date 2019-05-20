#pragma once

#include <atomic>

NS_HWM_BEGIN

class Transient
{
public:
    //! コンストラクタ。指定されたパラメータから音量の推移する速度を計算して設定する。
    /*! @param sample_rate サンプリングレート
     *  @param duration_in_msec 音量が2倍や半分(= log10(2) * 2 =~ 6.02dB)に変換するのにかかる時間
     *  @param min_db 最小のdB値
     *  @param max_db 最大のdB値
     */
    Transient(double sample_rate,
              UInt32 duration_in_msec,
              double min_db,
              double max_db);
    
    Transient();
    
    Transient(Transient const &) = delete;
    Transient & operator=(Transient const &) = delete;
    
    Transient(Transient &&rhs);
    Transient & operator=(Transient &&rhs);
    
    //! 指定したstepが経過したあとtransient値を計算して設定する
    //! この関数は、get_current_transient_XXX()関数と同じスレッドから呼び出すこと
    void update_transient(Int32 step);
    
    //! 現在推移中の出力レベルをdB値として返す
    /*! @note この関数は、update_transient()関数と同じスレッドから呼び出すこと
     */
    double get_current_transient_db() const;
    
    //! 現在推移中の出力レベルを線形なゲイン値として返す
    /*! `get_current_transient_db() == get_min_db()`のときは0を返す
     *  @note この関数は、update_transient()関数と同じスレッドから呼び出すこと
     */
    double get_current_transient_linear_gain() const;
    
    //! コンストラクタで指定された、最小のdB値を返す。
    double get_min_db() const;
    //! コンストラクタで指定された、最大のdB値を返す。
    double get_max_db() const;

    //! `set_target_db()`関数で設定された出力レベルをdB値で返す。
    double get_target_db() const;
    
    //! 出力レベルを設定する
    /*! このクラスの現在の出力レベルは、コンストラクタで指定された推移速度で、このdB値に向かって推移する。
     */
    void set_target_db(double db);
    
private:
    double amount_;
    double min_db_;
    double max_db_;
    double current_db_ = 0;
    std::atomic<double> target_db_ = {0};
};

NS_HWM_END
